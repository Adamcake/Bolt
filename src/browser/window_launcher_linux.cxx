#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include "include/cef_parser.h"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <fmt/core.h>
#include <signal.h>
#include <spawn.h>

extern char **environ;

constexpr char tar_xz_inner_path[] = {
	46, 47, 117, 115, 114, 47, 115, 104, 97, 114, 101, 47, 103, 97, 109, 101,
	115, 47, 114, 117, 110, 101, 115, 99, 97, 112, 101, 45, 108, 97, 117,
	110, 99, 104, 101, 114, 47, 114, 117, 110, 101, 115, 99,97, 112, 101, 0
};
constexpr char tar_xz_icons_path[] = "./usr/share/icons/";

struct EnvQueryParam {
	bool should_set = false;
	bool prepend_env_key;
	bool allow_override = true;
	std::string_view env_key;
	std::string_view key;
	std::string value;

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
	char arg_configuri[] = "--configURI";

	const CefRefPtr<CefPostData> post_data = request->GetPostData();
	auto cursor = 0;

	// calculate HOME env strings - we redirect the game's HOME into our data dir
	const char* home_ = "HOME=";
	const std::string env_home = home_ + this->data_dir.string();
	const std::string_view env_key_home = std::string_view(env_home.begin(), env_home.begin() + strlen(home_));

	// array of structures for keeping track of which environment variables we want to set and have already set
	EnvQueryParam hash_param = {.should_set = false, .key = "hash"};
	EnvQueryParam config_uri_param = {.should_set = false, .key = "config_uri"};
	EnvQueryParam env_params[] = {
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_ACCESS_TOKEN=", .key = "jx_access_token"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_REFRESH_TOKEN=", .key = "jx_refresh_token"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_SESSION_ID=", .key = "jx_session_id"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_CHARACTER_ID=", .key = "jx_character_id"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_DISPLAY_NAME=", .key = "jx_display_name"},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_home, .value = env_home},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = "PULSE_PROP_OVERRIDE=", .value = env_pulse_prop_override},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = "SDL_VIDEO_X11_WMCLASS=", .value = env_wmclass},
	};
	const size_t env_param_count = sizeof(env_params) / sizeof(env_params[0]);
	
	// loop through and parse the query string from the HTTP request we intercepted
	while (true) {
		const std::string::size_type next_eq = query.find_first_of('=', cursor);
		const std::string::size_type next_amp = query.find_first_of('&', cursor);
		if (next_eq == std::string::npos) break;
		if (next_eq >= next_amp) {
			// found an invalid query param with no '=' in it - skip this
			cursor = next_amp + 1;
			continue;
		}

		// parse the next "key" and "value" from the query, then check if it's relevant to us
		const std::string_view key(query.begin() + cursor, query.begin() + next_eq);
		const auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
		const CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
		for (EnvQueryParam& param: env_params) {
			param.CheckAndUpdate(key, value);
		}
		hash_param.CheckAndUpdate(key, value);
		config_uri_param.CheckAndUpdate(key, value);

		// if there are no more instances of '&', we've read the last param, so stop here
		// otherwise continue from the next '&'
		if (next_amp == std::string::npos) break;
		cursor = next_amp + 1;
	}

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

	// count how many env vars we have - we need to do this every time as CEF or other modules
	// may call setenv and unsetenv
	char** e;
	for (e = environ; *e; e += 1);
	size_t env_count = e - environ;

	// set up some structs needed by posix_spawn, and our modified env array
	sigset_t set;
	sigfillset(&set);
	posix_spawn_file_actions_t file_actions;
	posix_spawnattr_t attributes;
	pid_t pid;
	posix_spawn_file_actions_init(&file_actions);
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setsigdefault(&attributes, &set);
	posix_spawnattr_setpgroup(&attributes, 0);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
	std::string path_str(this->rs3_path.c_str());
	char* argv[4];
	argv[0] = path_str.data();
	if (config_uri_param.should_set) {
		argv[1] = arg_configuri;
		argv[2] = config_uri_param.value.data();
		argv[3] = nullptr;
	} else {
		argv[1] = nullptr;
	}
	char** env = new char*[env_count + env_param_count + 1];

	// first, loop through the existing env vars
	// if its key is one we want to set, overwrite it, otherwise copy the pointer as-is
	size_t i;
	for (i = 0; i < env_count; i += 1) {
		bool use_default = true;
		for (EnvQueryParam& param: env_params) {
			if (param.should_set && strncmp(environ[i], param.env_key.data(), param.env_key.size()) == 0) {
				param.should_set = false;
				use_default = false;
				env[i] = param.value.data();
				break;
			}
		}
		if (use_default) {
			env[i] = environ[i];
		}
	}
	// ... next, look for any env vars we want to set that we haven't already, and put them at the end...
	for (EnvQueryParam& param: env_params) {
		if (param.should_set) {
			env[i] = param.value.data();
			i += 1;
		}
	}
	// ... and finally null-terminate the list as required by POSIX
	env[i] = nullptr;

	int r = posix_spawn(&pid, path_str.c_str(), &file_actions, &attributes, argv, env);
	
	posix_spawnattr_destroy(&attributes);
	posix_spawn_file_actions_destroy(&file_actions);
	delete[] env;

	if (r == 0) {
		fmt::print("[B] Successfully spawned game process with pid {}\n", pid);

		if (hash_param.should_set) {
			size_t written = 0;
			int file = open(this->rs3_hash_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (file == -1) {
				const char* data = "OK, but unable to save hash file\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
			}
			while (written < hash_param.value.size()) {
				written += write(file, hash_param.value.c_str() + written, hash_param.value.size() - written);
			}
			close(file);
		}

		const char* data = "OK\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
	} else {
		// posix_spawn failed - this is extremely unlikely to happen in the field, since the only failure
		// case is EINVAL, which would indicate a trivial mistake in writing this function
		fmt::print("[B] Error from posix_spawn: {}\n", r);
		const char* data = "Error spawning process\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
	}
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	const std::string user_home = this->data_dir.string();
	const char* home_ = "HOME=";
	const std::string env_home = home_ + user_home;
	const std::string_view env_key_home = std::string_view(env_home.begin(), env_home.begin() + strlen(home_));

	// array of structures for keeping track of which environment variables we want to set and have already set
	EnvQueryParam rl_path_param = {.should_set = false, .key = "jar_path"};
	EnvQueryParam hash_param = {.should_set = false, .key = "hash"};
	EnvQueryParam env_params[] = {
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_ACCESS_TOKEN=", .key = "jx_access_token"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_REFRESH_TOKEN=", .key = "jx_refresh_token"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_SESSION_ID=", .key = "jx_session_id"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_CHARACTER_ID=", .key = "jx_character_id"},
		{.should_set = false, .prepend_env_key = true, .env_key = "JX_DISPLAY_NAME=", .key = "jx_display_name"},
		{.should_set = true, .prepend_env_key = false, .allow_override = false, .env_key = env_key_home, .value = env_home},
	};
	const size_t env_param_count = sizeof(env_params) / sizeof(env_params[0]);

	// loop through and parse the query string from the HTTP request we intercepted
	size_t cursor = 0;
	while (true) {
		const std::string::size_type next_eq = query.find_first_of('=', cursor);
		const std::string::size_type next_amp = query.find_first_of('&', cursor);
		if (next_eq == std::string::npos) break;
		if (next_eq >= next_amp) {
			// found an invalid query param with no '=' in it - skip this
			cursor = next_amp + 1;
			continue;
		}

		// parse the next "key" and "value" from the query, then check if it's relevant to us
		const std::string_view key(query.begin() + cursor, query.begin() + next_eq);
		const auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
		const CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
		for (EnvQueryParam& param: env_params) {
			param.CheckAndUpdate(key, value);
		}
		hash_param.CheckAndUpdate(key, value);
		rl_path_param.CheckAndUpdate(key, value);

		// if there are no more instances of '&', we've read the last param, so stop here
		// otherwise continue from the next '&'
		if (next_amp == std::string::npos) break;
		cursor = next_amp + 1;
	}

	// path to runelite.jar will either be a user-provided one or one in our data folder,
	// which we may need to overwrite with a new user-provided file
	std::filesystem::path jar_path;
	if (rl_path_param.should_set) {
		jar_path = rl_path_param.value;
	} else {
		jar_path = this->runelite_path;

		// if there was a "hash" in the query string, we need to save the new jar and hash
		if (hash_param.should_set) {
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

	// count how many env vars we have - we need to do this every time as CEF or other modules
	// may call setenv and unsetenv
	char** e;
	for (e = environ; *e; e += 1);
	size_t env_count = e - environ;

	// set up some structs needed by posix_spawn, and our modified env array
	const char* java_home = getenv("JAVA_HOME");
	std::string java_home_str;
	sigset_t set;
	sigfillset(&set);
	posix_spawn_file_actions_t file_actions;
	posix_spawnattr_t attributes;
	pid_t pid;
	posix_spawn_file_actions_init(&file_actions);
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setsigdefault(&attributes, &set);
	posix_spawnattr_setpgroup(&attributes, 0);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
	std::string arg_home = "-Duser.home=" + user_home;
	std::string arg_jvm_argument_home = "-J" + arg_home;
	std::string path_str = jar_path.string();

	char* argv[7];
	size_t argv_offset;
	char arg_env[] = "/usr/bin/env";
	char arg_java[] = "java";
	char arg_jar[] = "-jar";
	if (java_home) {
		java_home_str = std::string(java_home) + "/bin/java";
		argv[1] = java_home_str.data();
		argv_offset = 1;
	} else {
		argv[0] = arg_env;
		argv[1] = arg_java;
		argv_offset = 0;
	}
	argv[2] = arg_home.data();
	argv[3] = arg_jar;
	argv[4] = path_str.data();
	argv[5] = arg_jvm_argument_home.data();
	argv[6] = nullptr;

	char** env = new char*[env_count + env_param_count + 1];

	// first, loop through the existing env vars
	// if its key is one we want to set, overwrite it, otherwise copy the pointer as-is
	size_t i;
	for (i = 0; i < env_count; i += 1) {
		bool use_default = true;
		for (EnvQueryParam& param: env_params) {
			if (param.should_set && strncmp(environ[i], param.env_key.data(), param.env_key.size()) == 0) {
				param.should_set = false;
				use_default = false;
				env[i] = param.value.data();
				break;
			}
		}
		if (use_default) {
			env[i] = environ[i];
		}
	}
	// ... next, look for any env vars we want to set that we haven't already, and put them at the end...
	for (EnvQueryParam& param: env_params) {
		if (param.should_set) {
			env[i] = param.value.data();
			i += 1;
		}
	}
	// ... and finally null-terminate the list as required by POSIX
	env[i] = nullptr;

	int r = posix_spawn(&pid, argv[argv_offset], &file_actions, &attributes, argv + argv_offset, env);
	
	posix_spawnattr_destroy(&attributes);
	posix_spawn_file_actions_destroy(&file_actions);
	delete[] env;

	if (r == 0) {
		fmt::print("[B] Successfully spawned game process with pid {}\n", pid);

		if (hash_param.should_set) {
			size_t written = 0;
			int file = open(this->runelite_hash_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (file == -1) {
				const char* data = "OK, but unable to save hash file\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
			}
			while (written < hash_param.value.size()) {
				written += write(file, hash_param.value.c_str() + written, hash_param.value.size() - written);
			}
			close(file);
		}

		const char* data = "OK\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
	} else {
		// posix_spawn failed - this is extremely unlikely to happen in the field, since the only failure
		// case is EINVAL, which would indicate a trivial mistake in writing this function
		fmt::print("[B] Error from posix_spawn: {}\n", strerror(r));
		const char* data = "Error spawning process\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
	}
}

void Browser::Launcher::OpenExternalUrl(char* url) const {
	sigset_t set;
	sigfillset(&set);
	posix_spawn_file_actions_t file_actions;
	posix_spawnattr_t attributes;
	pid_t pid;
	posix_spawn_file_actions_init(&file_actions);
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setsigdefault(&attributes, &set);
	posix_spawnattr_setpgroup(&attributes, 0);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
	char arg_env[] = "/usr/bin/env";
	char arg_xdg_open[] = "xdg-open";
	char* argv[4];
	argv[0] = arg_env;
	argv[1] = arg_xdg_open;
	argv[2] = url;
	argv[3] = nullptr;
	int r = posix_spawn(&pid, argv[0], &file_actions, &attributes, argv, environ);
	if (r != 0) {
		fmt::print("[B] Error in OpenExternalUrl from posix_spawn: {}\n", strerror(r));
	}
}
