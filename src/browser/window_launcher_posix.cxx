#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include "include/cef_parser.h"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <filesystem>
#include <fmt/core.h>
#include <signal.h>
#include <spawn.h>

extern char **environ;

// standard set of JX_ environment/query parameters, for use in an env_params list
#define JX_ENV_PARAMS \
	{.should_set = false, .prepend_env_key = true, .env_key = "JX_ACCESS_TOKEN=", .key = "jx_access_token"}, \
	{.should_set = false, .prepend_env_key = true, .env_key = "JX_REFRESH_TOKEN=", .key = "jx_refresh_token"}, \
	{.should_set = false, .prepend_env_key = true, .env_key = "JX_SESSION_ID=", .key = "jx_session_id"}, \
	{.should_set = false, .prepend_env_key = true, .env_key = "JX_CHARACTER_ID=", .key = "jx_character_id"}, \
	{.should_set = false, .prepend_env_key = true, .env_key = "JX_DISPLAY_NAME=", .key = "jx_display_name"}

// iterates the key-value pairs in an HTTP query, performing ACTION on them
#define ITERATE_QUERY(ACTION, QUERY) { \
	size_t cursor = 0; \
	while (true) { \
		const std::string::size_type next_eq = QUERY.find_first_of('=', cursor); \
		const std::string::size_type next_amp = QUERY.find_first_of('&', cursor); \
		if (next_eq == std::string::npos) break; \
		if (next_eq >= next_amp) { \
			cursor = next_amp + 1; \
			continue; \
		} \
		const std::string_view key(QUERY.begin() + cursor, QUERY.begin() + next_eq); \
		const auto value_end = next_amp == std::string::npos ? QUERY.end() : QUERY.begin() + next_amp; \
		const CefString value = std::string(std::string_view(QUERY.begin() + next_eq + 1,  value_end)); \
		ACTION \
		if (next_amp == std::string::npos) break; \
		cursor = next_amp + 1; \
	} \
}

// calls SpawnProcess using the given argv and an envp calculated from the given env_params,
// then attempts to save the related hash file, and finally returns an appropriate HTTP response
#define SPAWN_FROM_PARAMS_AND_RETURN(ARGV, ENV_PARAMS, HASH_PARAM, HASH_PATH) { \
	char** e; \
	for (e = environ; *e; e += 1); \
	size_t env_count = e - environ; \
	const size_t env_param_count = sizeof(ENV_PARAMS) / sizeof(ENV_PARAMS[0]); \
	char** env = new char*[env_count + env_param_count + 1]; \
	size_t i; \
	for (i = 0; i < env_count; i += 1) { \
		bool use_default = true; \
		for (EnvQueryParam& param: ENV_PARAMS) { \
			if (param.should_set && strncmp(environ[i], param.env_key.data(), param.env_key.size()) == 0) { \
				param.should_set = false; \
				use_default = false; \
				if (param.allow_append) { \
					param.value = std::string(param.env_key) + std::string(environ[i] + param.env_key.size()) + ' ' + (param.value.data() + param.env_key.size()); \
				} \
				env[i] = param.value.data(); \
				break; \
			} \
		} \
		if (use_default) { \
			env[i] = environ[i]; \
		} \
	} \
	for (EnvQueryParam& param: ENV_PARAMS) { \
		if (param.should_set) { \
			env[i] = param.value.data(); \
			i += 1; \
		} \
	} \
	env[i] = nullptr; \
	pid_t pid; \
	int r = SpawnProcess(ARGV, env, &pid); \
	delete[] env; \
	if (r == 0) { \
		fmt::print("[B] Successfully spawned game process with pid {}\n", pid); \
		if (HASH_PARAM.should_set) { \
			size_t written = 0; \
			int file = open(HASH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644); \
			if (file == -1) { \
				const char* data = "OK, but unable to save hash file\n"; \
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain"); \
			} \
			while (written < HASH_PARAM.value.size()) { \
				written += write(file, HASH_PARAM.value.c_str() + written, HASH_PARAM.value.size() - written); \
			} \
			close(file); \
		} \
		const char* data = "OK\n"; \
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain"); \
	} else { \
		fmt::print("[B] Error from posix_spawn: {}\n", r); \
		const char* data = "Error spawning process\n"; \
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain"); \
	} \
}

const std::string_view env_key_runtime_dir = "XDG_RUNTIME_DIR=";
#define SETUP_TEMP_DIR(TEMP_DIR) { \
	const char* tmpdir_prefix = "bolt-runelite-"; \
	const char* runtime_dir = getenv("XDG_RUNTIME_DIR"); \
	char path_buf[PATH_MAX]; \
	if (runtime_dir) snprintf(path_buf, sizeof(path_buf), "%s/%sXXXXXX", runtime_dir, tmpdir_prefix); \
	else snprintf(path_buf, sizeof(path_buf), "/tmp/%sXXXXXX", tmpdir_prefix); \
	char* dtemp = mkdtemp(path_buf); \
	if (!dtemp) { \
		const char* data = "Couldn't create temp directory (is XDG_RUNTIME_DIR set?)\n"; \
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain"); \
	} \
	TEMP_DIR = std::filesystem::canonical(dtemp); \
	fmt::print("[B] runelite temp_dir: {}\n", TEMP_DIR.c_str()); \
	if (TEMP_DIR.has_parent_path()) { \
		for (const auto& entry: std::filesystem::directory_iterator(TEMP_DIR.parent_path())) { \
			const std::filesystem::path path = entry.path(); \
			snprintf(path_buf, sizeof(path_buf), "%s/%s", TEMP_DIR.c_str(), path.filename().c_str()); \
			int _ = symlink(path.c_str(), path_buf); \
		} \
	} \
}

// spawns a process using posix_spawn, the given argc and argv, and the target specified at argv[0]
int SpawnProcess(char** argv, char** envp, pid_t* pid) {
	sigset_t set;
	sigfillset(&set);
	posix_spawn_file_actions_t file_actions;
	posix_spawnattr_t attributes;
	posix_spawn_file_actions_init(&file_actions);
	posix_spawn_file_actions_addclose(&file_actions, STDIN_FILENO);
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setsigdefault(&attributes, &set);
	posix_spawnattr_setpgroup(&attributes, 0);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
	int r = posix_spawn(pid, argv[0], &file_actions, &attributes, argv, envp);
	posix_spawnattr_destroy(&attributes);
	posix_spawn_file_actions_destroy(&file_actions);
	return r;
}

struct EnvQueryParam {
	bool should_set = false;    // is this present in the query?
	bool prepend_env_key;       // prepend this->env_key to the output value (if not overriding by append)?
	bool allow_override = true; // allow replacement of a variable that's already in the env?
	bool allow_append = false;  // allow appending to a variable that's already in the env?
	std::string_view env_key;   // used only if prepend_env_key is true
	std::string_view key;       // query param, if applicable
	std::string value;          // output value

	// Compares `key` to `key_name`, setting `out` to `prepend`+`value` and setting `should_set` to `true` if
	// they match, or altering nothing otherwise (including `should_set`).
	void CheckAndUpdate(std::string_view key, CefString value) {
		if ((!this->should_set || this->allow_override) && this->key == key) {
			this->should_set = true;
			this->value.clear();
			if (this->prepend_env_key) {
				this->value = this->env_key;
			}
			this->value += CefURIDecode(value, true, static_cast<cef_uri_unescape_rule_t>(0b1111)).ToString();
		}
	}
};

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Deb(CefRefPtr<CefRequest> request, std::string_view query) {
	// strings that I don't want to be searchable, which also need to be mutable for passing to env functions
	char env_pulse_prop_override[] = {
		80, 85, 76, 83, 69, 95, 80, 82, 79, 80, 95, 79, 86, 69, 82, 82, 73, 68,
		69, 61, 97, 112, 112, 108, 105, 99, 97, 116, 105, 111, 110, 46, 110,
		97, 109, 101, 61, 39, 82, 117, 110, 101, 83, 99, 97, 112, 101, 39, 32,
		97, 112, 112, 108, 105, 99, 97, 116, 105, 111, 110, 46, 105, 99, 111, 110,
		95, 110, 97, 109, 101, 61, 39, 114, 117, 110, 101, 115, 99, 97, 112, 101,
		39, 32, 109, 101, 100, 105, 97, 46, 114, 111, 108, 101, 61, 39, 103, 97,
		109, 101, 39, 0
	};
	char env_wmclass[] = {
		83, 68, 76, 95, 86, 73, 68, 69, 79, 95, 88, 49, 49, 95, 87, 77, 67,
		76, 65, 83, 83, 61, 82, 117, 110, 101, 83, 99, 97, 112, 101, 0
	};
	constexpr char tar_xz_inner_path[] = {
		46, 47, 117, 115, 114, 47, 115, 104, 97, 114, 101, 47, 103, 97, 109, 101,
		115, 47, 114, 117, 110, 101, 115, 99, 97, 112, 101, 45, 108, 97, 117,
		110, 99, 104, 101, 114, 47, 114, 117, 110, 101, 115, 99,97, 112, 101, 0
	};
	constexpr char tar_xz_icons_path[] = "./usr/share/icons/";

	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	// calculate HOME env strings - we redirect the game's HOME into our data dir
	const char* home_ = "HOME=";
	const std::string env_home = home_ + this->data_dir.string();
	const std::string_view env_key_home = std::string_view(env_home.begin(), env_home.begin() + strlen(home_));

	// array of structures for keeping track of which environment variables we want to set and have already set
	EnvQueryParam hash_param = {.should_set = false, .key = "hash"};
	EnvQueryParam config_uri_param = {.should_set = false, .key = "config_uri"};
	EnvQueryParam env_params[] = {
		JX_ENV_PARAMS,
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_home, .value = env_home},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = "PULSE_PROP_OVERRIDE=", .value = env_pulse_prop_override},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = "SDL_VIDEO_X11_WMCLASS=", .value = env_wmclass},
	};
	
	// loop through and parse the query string from the HTTP request we intercepted
	ITERATE_QUERY({
		for (EnvQueryParam& param: env_params) {
			param.CheckAndUpdate(key, value);
		}
		hash_param.CheckAndUpdate(key, value);
		config_uri_param.CheckAndUpdate(key, value);
	}, query)

	// if there was a "hash" in the query string, we need to save the new game exe and the new hash
	if (hash_param.should_set) {
		if (post_data == nullptr || post_data->GetElementCount() != 1) {
			// hash param must be accompanied by POST data containing the file it's a hash of,
			// so hash but no POST is a bad request
			const char* data = "Bad Request";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
		}
		std::filesystem::path icons_dir = this->data_dir.parent_path();
		icons_dir.append("icons");
		CefPostData::ElementVector vec;
		post_data->GetElements(vec);
		size_t deb_size = vec[0]->GetBytesCount();
		unsigned char* deb = new unsigned char[deb_size];
		vec[0]->GetBytes(deb_size, deb);

		// use libarchive to extract data.tar.xz into memory from the supplied .deb (ar compression format)
		struct archive* ar = archive_read_new();
		archive_read_support_format_ar(ar);
		archive_read_open_memory(ar, deb, deb_size);
		bool entry_found = false;
		struct archive_entry* entry;
		while (true) {
			int r = archive_read_next_header(ar, &entry);
			if (r == ARCHIVE_EOF) break;
			if (r != ARCHIVE_OK) {
				// POST data contained an invalid .deb file
				archive_read_close(ar);
				archive_read_free(ar);
				delete[] deb;
				const char* data = "Malformed .deb file\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
			}
			if (strcmp(archive_entry_pathname(entry), "data.tar.xz") == 0) {
				entry_found = true;
				break;
			}
		}
		if (!entry_found) {
			// The .deb file is valid but does not contain "data.tar.xz" according to libarchive
			archive_read_close(ar);
			archive_read_free(ar);
			delete[] deb;
			const char* data = "No data in .deb file\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
		}

		const long entry_size = archive_entry_size(entry);
		unsigned char* tar_xz = new unsigned char[entry_size];
		long written = 0;
		while (written < entry_size) {
			written += archive_read_data(ar, tar_xz + written, entry_size - written);
		}
		archive_read_close(ar);
		archive_read_free(ar);
		delete[] deb;

		// open data.tar.xz and extract any files into memory that we're interested in
		struct archive* xz = archive_read_new();
		archive_read_support_format_tar(xz);
		archive_read_support_filter_xz(xz);
		archive_read_open_memory(xz, tar_xz, entry_size);
		entry_found = false;
		while (true) {
			int r = archive_read_next_header(xz, &entry);
			if (r == ARCHIVE_EOF) break;
			if (r != ARCHIVE_OK) {
				// .deb file was valid but the data.tar.xz it contained was not
				archive_read_close(xz);
				archive_read_free(xz);
				delete[] tar_xz;
				const char* data = "Malformed .tar.xz file\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
			}

			const char* entry_pathname = archive_entry_pathname(entry);
			const size_t entry_pathname_len = strlen(entry_pathname);
			if (strcmp(entry_pathname, tar_xz_inner_path) == 0) {
				// found the game binary - we need to save this to disk so we can run it
				entry_found = true;
				const long game_size = archive_entry_size(entry);
				char* game = new char[game_size];
				written = 0;
				while (written < game_size) {
					written += archive_read_data(ar, game + written, game_size - written);
				}
				written = 0;
				int file = open(this->rs3_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
				if (file == -1) {
					// failed to open game binary file on disk - probably in use or a permissions issue
					delete[] game;
					const char* data = "Failed to save executable; if the game is already running, close it and try again\n";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
				}
				while (written < game_size) {
					written += write(file, game + written, game_size - written);
				}
				close(file);
				delete[] game;
			} else if (strncmp(entry_pathname, tar_xz_icons_path, strlen(tar_xz_icons_path)) == 0) {
				// found an icon - save this to the icons directory, maintaining its relative path
				std::filesystem::path icon_path = icons_dir;
				icon_path.append(entry_pathname + strlen(tar_xz_icons_path));
				if (entry_pathname[entry_pathname_len - 1] == '/') {
					mkdir(icon_path.c_str(), 0755);
				} else {
					std::filesystem::path icon_path = icons_dir;
					icon_path.append(entry_pathname + strlen(tar_xz_icons_path));
					const long icon_size = archive_entry_size(entry);
					char* icon = new char[icon_size];
					written = 0;
					while (written < icon_size) {
						written += archive_read_data(ar, icon + written, icon_size - written);
					}
					written = 0;
					int file = open(icon_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
					if (file == -1) {
						// failing to save an icon is not a fatal error, but probably something the
						// user should know about
						fmt::print("[B] [warning] failed to save an icon: {}\n", icon_path.c_str());
					} else {
						while (written < icon_size) {
							written += write(file, icon + written, icon_size - written);
						}
						close(file);
						delete[] icon;
					}
				}
			}
		}

		archive_read_close(xz);
		archive_read_free(xz);
		delete[] tar_xz;

		if (!entry_found) {
			// data.tar.tx was valid but did not contain a game binary according to libarchive
			const char* data = "No target executable in .tar.xz file\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
		}
	}

	// setup argv for the new process
	std::string path_str(this->rs3_path.c_str());
	char arg_configuri[] = "--configURI";
	char* argv[] = {
		path_str.data(),
		config_uri_param.should_set ? arg_configuri : nullptr,
		config_uri_param.should_set ? config_uri_param.value.data() : nullptr,
		nullptr,
	};

	SPAWN_FROM_PARAMS_AND_RETURN(argv, env_params, hash_param, this->rs3_hash_path.c_str())
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query, bool configure) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	const std::string user_home = this->data_dir.string();
	const char* home_ = "HOME=";
	const std::string env_home = home_ + user_home;
	const std::string_view env_key_home = std::string_view(env_home.begin(), env_home.begin() + strlen(home_));

	// set up a temp directory just for this run
	std::filesystem::path temp_dir;
	SETUP_TEMP_DIR(temp_dir)

	// array of structures for keeping track of which environment variables we want to set and have already set
	EnvQueryParam rl_path_param = {.should_set = false, .key = "jar_path"};
	EnvQueryParam id_param = {.should_set = false, .key = "id"};
	EnvQueryParam rich_presence_param = {.should_set = false, .key = "flatpak_rich_presence"};
	EnvQueryParam env_params[] = {
		JX_ENV_PARAMS,
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_home, .value = env_home},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_runtime_dir, .value = std::string(env_key_runtime_dir) + temp_dir.c_str()},
	};

	// loop through and parse the query string from the HTTP request we intercepted
	ITERATE_QUERY({
		for (EnvQueryParam& param: env_params) {
			param.CheckAndUpdate(key, value);
		}
		id_param.CheckAndUpdate(key, value);
		rl_path_param.CheckAndUpdate(key, value);
		rich_presence_param.CheckAndUpdate(key, value);
	}, query)

	// path to runelite.jar will either be a user-provided one or one in our data folder,
	// which we may need to overwrite with a new user-provided file
	std::filesystem::path jar_path;
	if (rl_path_param.should_set) {
		jar_path = rl_path_param.value;
	} else {
		jar_path = this->runelite_path;

		// if there was an "id" in the query string, we need to save the new jar and hash
		if (id_param.should_set) {
			if (post_data == nullptr || post_data->GetElementCount() != 1) {
				// hash param must be accompanied by POST data containing the file it's a hash of,
				// so hash but no POST is a bad request
				const char* data = "Bad Request";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
			}

			CefPostData::ElementVector vec;
			post_data->GetElements(vec);
			size_t jar_size = vec[0]->GetBytesCount();
			unsigned char* jar = new unsigned char[jar_size];
			vec[0]->GetBytes(jar_size, jar);

			size_t written = 0;
			int file = open(jar_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
			if (file == -1) {
				// failed to open game binary file on disk - probably in use or a permissions issue
				delete[] jar;
				const char* data = "Failed to save JAR; if the game is already running, close it and try again\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
			}
			while (written < jar_size) {
				written += write(file, jar + written, jar_size - written);
			}
			close(file);
			delete[] jar;
		}
	}

	// symlink discord-ipc for rich presence, if the user has opted into it
	if (rich_presence_param.should_set) {
		std::filesystem::path discord_ipc = temp_dir;
		discord_ipc.append("discord-ipc-0");
		if (symlink("../app/com.discordapp.Discord/discord-ipc-0", discord_ipc.c_str())) {
			fmt::print("[B] note: unable to create symlink to discord-ipc-0 at {}\n", discord_ipc.c_str());
		}
	}

	// set up argv for the new process
	const char* java_home = getenv("JAVA_HOME");
	std::string java_home_str;
	if (java_home) java_home_str = std::string(java_home) + "/bin/java";
	std::string arg_home = "-Duser.home=" + user_home;
	std::string arg_jvm_argument_home = "-J" + arg_home;
	std::string path_str = jar_path.string();
	size_t argv_offset = java_home ? 1 : 0;
	char arg_env[] = "/usr/bin/env";
	char arg_java[] = "java";
	char arg_jar[] = "-jar";
	char arg_configure[] = "--configure";
	char* argv[] = {
		arg_env,
		java_home ? java_home_str.data() : arg_java,
		arg_home.data(),
		arg_jar,
		path_str.data(),
		arg_jvm_argument_home.data(),
		configure ? arg_configure : nullptr,
		nullptr,
	};

	SPAWN_FROM_PARAMS_AND_RETURN(argv + argv_offset, env_params, id_param, this->runelite_id_path.c_str())
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchHdosJar(CefRefPtr<CefRequest> request, std::string_view query) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	const char* java_home = getenv("JAVA_HOME");
	if (!java_home) {
		const char* data = "JAVA_HOME environment variable is required to run HDOS\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
	}

	std::filesystem::path temp_dir;
	SETUP_TEMP_DIR(temp_dir)

	const std::string user_home = this->data_dir.string();
	const char* home_ = "HOME=";
	const std::string env_home = home_ + user_home;
	const std::string_view env_key_home = std::string_view(env_home.begin(), env_home.begin() + strlen(home_));

	const char* env_key_user_home = "BOLT_ARG_HOME=";
	std::string arg_user_home = "-Duser.home=" + user_home;

	const char* env_key_java_path = "BOLT_JAVA_PATH=";
	std::string java_path_str = std::string(java_home) + "/bin/java";
	
	std::filesystem::path java_proxy_bin_path = std::filesystem::current_path();
	java_proxy_bin_path.append("java-proxy");
	std::filesystem::path java_proxy_data_dir_path = this->data_dir;
	java_proxy_data_dir_path.append("java-proxy");
	std::filesystem::remove_all(java_proxy_data_dir_path);
	std::filesystem::create_directory(java_proxy_data_dir_path);
	std::filesystem::path java_proxy_lib_path = java_proxy_data_dir_path;
	java_proxy_lib_path.append("lib");
	std::filesystem::path java_proxy_conf_path = java_proxy_data_dir_path;
	java_proxy_conf_path.append("conf");
	std::filesystem::path java_proxy_java_path = java_proxy_data_dir_path;
	java_proxy_java_path.append("bin");
	std::filesystem::create_directory(java_proxy_java_path);
	java_proxy_java_path.append("java");
	const std::string java_lib_str = std::string(java_home) + "/lib";
	const std::string java_conf_str = std::string(java_home) + "/conf";
	int err = 0;
	err |= symlink(java_lib_str.c_str(), java_proxy_lib_path.c_str());
	err |= symlink(java_conf_str.c_str(), java_proxy_conf_path.c_str());
	err |= symlink(java_proxy_bin_path.c_str(), java_proxy_java_path.c_str());
	if (err) {
		const char* data = "Unable to create symlinks\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
	}

	// array of structures for keeping track of which environment variables we want to set and have already set
	EnvQueryParam version_param = {.should_set = false, .key = "version"};
	EnvQueryParam rich_presence_param = {.should_set = false, .key = "flatpak_rich_presence"};
	EnvQueryParam env_params[] = {
		JX_ENV_PARAMS,
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_home, .value = env_home},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_user_home, .value = env_key_user_home + arg_user_home},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_java_path, .value = env_key_java_path + java_path_str},
	};

	// loop through and parse the query string from the HTTP request we intercepted
	ITERATE_QUERY({
		for (EnvQueryParam& param: env_params) {
			param.CheckAndUpdate(key, value);
		}
		version_param.CheckAndUpdate(key, value);
		rich_presence_param.CheckAndUpdate(key, value);
	}, query)

	// if there was a "version" in the query string, we need to save the new jar and hash
	if (version_param.should_set) {
		if (post_data == nullptr || post_data->GetElementCount() != 1) {
			// hash param must be accompanied by POST data containing the file it's a hash of,
			// so hash but no POST is a bad request
			const char* data = "Bad Request";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
		}

		CefPostData::ElementVector vec;
		post_data->GetElements(vec);
		size_t jar_size = vec[0]->GetBytesCount();
		unsigned char* jar = new unsigned char[jar_size];
		vec[0]->GetBytes(jar_size, jar);

		size_t written = 0;
		int file = open(this->hdos_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
		if (file == -1) {
			// failed to open game binary file on disk - probably in use or a permissions issue
			delete[] jar;
			const char* data = "Failed to save JAR; if the game is already running, close it and try again\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
		}
		while (written < jar_size) {
			written += write(file, jar + written, jar_size - written);
		}
		close(file);
		delete[] jar;
	}

	// symlink discord-ipc for rich presence, if the user has opted into it
	if (rich_presence_param.should_set) {
		std::filesystem::path discord_ipc = temp_dir;
		discord_ipc.append("discord-ipc-0");
		if (symlink("../app/com.discordapp.Discord/discord-ipc-0", discord_ipc.c_str())) {
			fmt::print("[B] note: unable to create symlink to discord-ipc-0 at {}\n", discord_ipc.c_str());
		}
	}

	// set up argv for the new process
	std::string path_str = this->hdos_path.string();
	char arg_jar[] = "-jar";
	std::string arg_java_home = "-Djava.home=" + java_proxy_data_dir_path.string();
	char* argv[] = {
		java_path_str.data(),
		arg_user_home.data(),
		arg_java_home.data(),
		arg_jar,
		path_str.data(),
		nullptr,
	};

	SPAWN_FROM_PARAMS_AND_RETURN(argv, env_params, version_param, this->hdos_version_path.c_str())
}

void Browser::Launcher::OpenExternalUrl(char* url) const {
	char arg_env[] = "/usr/bin/env";
	char arg_xdg_open[] = "xdg-open";
	char* argv[] { arg_env, arg_xdg_open, url, nullptr };
	pid_t pid;
	int r = SpawnProcess(argv, environ, &pid);
	if (r != 0) {
		fmt::print("[B] Error in OpenExternalUrl from posix_spawn: {}\n", strerror(r));
	}
}
