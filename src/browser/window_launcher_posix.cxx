#include "window_launcher.hxx"
#include "resource_handler.hxx"
#include "request.hxx"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <filesystem>
#include <fmt/core.h>
#include <spawn.h>

// see #34 for why this function exists and why it can't be run between fork-exec or just run `env`.
// true on success, false on failure, out is undefined on failure, you know the drill.
// java_home should be the result of getenv("JAVA_HOME") and therefore is allowed to be null
bool FindJava(const char* java_home, std::string& out) {
	if (java_home) {
		std::filesystem::path java(java_home);
		java.append("bin");
		java.append("java");
		if (std::filesystem::exists(java)) {
			out = java.string();
			return true;
		}
	}
	const char* path = getenv("PATH");
	while (true) {
		const char* next_colon = strchr(path, ':');
		const bool is_last = next_colon == nullptr;
		std::string_view path_item = is_last ? std::string_view(path) : std::string_view(path, (size_t)(next_colon - path));
		std::filesystem::path java(path_item);
		java.append("java");
		if (std::filesystem::exists(java)) {
			out = java.string();
			return true;
		}
		if (is_last) break;
		path = next_colon + 1;
	}
	return false;
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Deb(CefRefPtr<CefRequest> request, std::string_view query) {
	/* strings that I don't want to be searchable, which also need to be mutable for passing to env functions */
	// PULSE_PROP_OVERRIDE=
	char env_pulse_prop_override[] = {
		97, 112, 112, 108, 105, 99, 97, 116, 105, 111, 110, 46, 110,
		97, 109, 101, 61, 39, 82, 117, 110, 101, 83, 99, 97, 112, 101, 39, 32,
		97, 112, 112, 108, 105, 99, 97, 116, 105, 111, 110, 46, 105, 99, 111, 110,
		95, 110, 97, 109, 101, 61, 39, 114, 117, 110, 101, 115, 99, 97, 112, 101,
		39, 32, 109, 101, 100, 105, 97, 46, 114, 111, 108, 101, 61, 39, 103, 97,
		109, 101, 39, 0
	};
	// SDL_VIDEO_X11_WMCLASS=
	char env_wmclass[] = {82, 117, 110, 101, 83, 99, 97, 112, 101, 0};
	constexpr char tar_xz_inner_path[] = {
		46, 47, 117, 115, 114, 47, 115, 104, 97, 114, 101, 47, 103, 97, 109, 101,
		115, 47, 114, 117, 110, 101, 115, 99, 97, 112, 101, 45, 108, 97, 117,
		110, 99, 104, 101, 114, 47, 114, 117, 110, 101, 115, 99,97, 112, 101, 0
	};
	constexpr char tar_xz_icons_path[] = "./usr/share/icons/";

	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	// get local mutable copy of HOME env string - we redirect the game's HOME into our data dir
	const std::string env_home = this->data_dir.string();

	// parse query
	std::string hash;
	bool has_hash = false;
	std::string config_uri;
	bool has_config_uri = false;
	std::string jx_session_id;
	bool has_jx_session_id = false;
	std::string jx_character_id;
	bool has_jx_character_id = false;
	std::string jx_display_name;
	bool has_jx_display_name = false;
#if defined(BOLT_PLUGINS)
	bool plugin_loader = false;
#endif
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(hash)
		PQSTRING(config_uri)
		PQSTRING(jx_session_id)
		PQSTRING(jx_character_id)
		PQSTRING(jx_display_name)
#if defined(BOLT_PLUGINS)
		PQBOOL(plugin_loader)
#endif
	});

	// if there was a "hash" in the query string, we need to save the new game exe and the new hash
	if (has_hash) {
		QSENDBADREQUESTIF(!post_data || post_data->GetElementCount() != 1);
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
				QSENDSTR("Malformed .deb file", 400);
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
			QSENDSTR("No data in .deb file", 400);
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
				QSENDSTR("Malformed .tar.xz file", 400);
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
				int file = open(this->rs3_elf_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
				if (file == -1) {
					// failed to open game binary file on disk - probably in use or a permissions issue
					delete[] game;
					QSENDSTR("Failed to save executable; if the game is already running, close it and try again", 500);
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
			// data.tar.xz was valid but did not contain a game binary according to libarchive
			QSENDSTR("No target executable in .tar.xz file", 400);
		}
	}

	// setup argv for the new process
	std::string path_str(this->rs3_elf_path.c_str());
	char arg_configuri[] = "--configURI";
	char* argv[] = {
		path_str.data(),
		has_config_uri ? arg_configuri : nullptr,
		has_config_uri ? config_uri.data() : nullptr,
		nullptr,
	};

	pid_t pid = fork();
	if (pid == 0) {
		setpgrp();
		if (chdir(this->data_dir.c_str())) {
			fmt::print("[B] new process was unable to chdir: {}\n", errno);
			exit(1);
		}
		close(STDIN_FILENO);
		setenv("HOME", env_home.data(), true);
		setenv("PULSE_PROP_OVERRIDE", env_pulse_prop_override, false);
		setenv("SDL_VIDEO_X11_WMCLASS", env_wmclass, false);
		if (has_jx_session_id) setenv("JX_SESSION_ID", jx_session_id.data(), true);
		if (has_jx_character_id) setenv("JX_CHARACTER_ID", jx_character_id.data(), true);
		if (has_jx_display_name) setenv("JX_DISPLAY_NAME", jx_display_name.data(), true);
#if defined(BOLT_PLUGINS)
		if (plugin_loader) {
			setenv("LD_PRELOAD", "lib" BOLT_LIB_NAME ".so", true);
		}
#endif
		execv(*argv, argv);
	}
	fmt::print("[B] Successfully spawned game process with pid {}\n", pid);
	if (has_hash) {
		size_t written = 0;
		int file = open(this->rs3_elf_hash_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (file == -1) {
			QSENDSTR("OK, but unable to save hash file", 200);
		}
		while (written < hash.size()) {
			written += write(file, hash.c_str() + written, hash.size() - written);
		}
		close(file);
	}
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Exe(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR(".exe is not supported on this platform", 400);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3App(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR("Mac binaries are not supported on this platform", 400);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsExe(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR(".exe is not supported on this platform", 400);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsApp(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR("Mac binaries are not supported on this platform", 400);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query, bool configure) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	// value to override Java's user.home property with
	const std::string user_home = this->data_dir.string();

	// parse query
	std::string jar_path;
	bool has_jar_path = false;
	std::string id;
	bool has_id = false;
	std::string jx_session_id;
	bool has_jx_session_id = false;
	std::string jx_character_id;
	bool has_jx_character_id = false;
	std::string jx_display_name;
	bool has_jx_display_name = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(jar_path)
		PQSTRING(id)
		PQSTRING(jx_session_id)
		PQSTRING(jx_character_id)
		PQSTRING(jx_display_name)
	});

	// path to runelite.jar will either be a user-provided one or one in our data folder,
	// which we may need to overwrite with a new user-provided file
	std::filesystem::path rl_path;
	if (has_jar_path) {
		rl_path.assign(jar_path);
	} else {
		rl_path = this->runelite_path;

		// if there was an "id" in the query string, we need to save the new jar and hash
		if (has_id) {
			QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);
			CefPostData::ElementVector vec;
			post_data->GetElements(vec);
			size_t jar_size = vec[0]->GetBytesCount();
			unsigned char* jar = new unsigned char[jar_size];
			vec[0]->GetBytes(jar_size, jar);

			size_t written = 0;
			int file = open(rl_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
			if (file == -1) {
				// failed to open game binary file on disk - probably in use or a permissions issue
				delete[] jar;
				QSENDSTR("Failed to save JAR; if the game is already running, close it and try again", 500);
			}
			while (written < jar_size) {
				written += write(file, jar + written, jar_size - written);
			}
			close(file);
			delete[] jar;
		}
	}

	std::string java;
	if (!FindJava(getenv("JAVA_HOME"), java)) {
		QSENDSTR("Couldn't find Java: JAVA_HOME is either unset or does not point to a Java binary, and no binary named \"java\" exists in PATH.", 500);
	}
	std::string arg_home = "-Duser.home=" + user_home;
	std::string arg_jvm_argument_home = "-J" + arg_home;
	std::string path_str = rl_path.string();
	char arg_jar[] = "-jar";
	char arg_configure[] = "--configure";
	char* argv[] = {
		java.data(),
		arg_home.data(),
		arg_jar,
		path_str.data(),
		arg_jvm_argument_home.data(),
		configure ? arg_configure : nullptr,
		nullptr,
	};

	pid_t pid = fork();
	if (pid == 0) {
		setpgrp();
		if (chdir(this->data_dir.c_str())) {
			fmt::print("[B] new process was unable to chdir: {}\n", errno);
			exit(1);
		}
		close(STDIN_FILENO);
		setenv("HOME", user_home.data(), true);
		if (has_jx_session_id) setenv("JX_SESSION_ID", jx_session_id.data(), true);
		if (has_jx_character_id) setenv("JX_CHARACTER_ID", jx_character_id.data(), true);
		if (has_jx_display_name) setenv("JX_DISPLAY_NAME", jx_display_name.data(), true);
		execv(*argv, argv);
	}

	fmt::print("[B] Successfully spawned game process with pid {}\n", pid);
	if (has_id) {
		size_t written = 0;
		int file = open(this->runelite_id_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (file == -1) {
			QSENDSTR("OK, but unable to save id file", 200);
			const char* data = "OK, but unable to save id file\n";
		}
		while (written < id.size()) {
			written += write(file, id.c_str() + written, id.size() - written);
		}
		close(file);
	}
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchHdosJar(CefRefPtr<CefRequest> request, std::string_view query) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	const std::string user_home = this->data_dir.string();
	const char* env_key_user_home = "BOLT_ARG_HOME=";
	std::string arg_user_home = "-Duser.home=" + user_home;
	std::string arg_app_user_home = "-Dapp.user.home=" + user_home;

	std::string java;
	if (!FindJava(getenv("JAVA_HOME"), java)) {
		QSENDSTR("Couldn't find Java: JAVA_HOME does not point to a Java binary", 400);
	}

	// parse query
	std::string version;
	bool has_version = false;
	std::string jx_session_id;
	bool has_jx_session_id = false;
	std::string jx_character_id;
	bool has_jx_character_id = false;
	std::string jx_display_name;
	bool has_jx_display_name = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(version)
		PQSTRING(jx_session_id)
		PQSTRING(jx_character_id)
		PQSTRING(jx_display_name)
	});

	// if there was a "version" in the query string, we need to save the new jar and hash
	if (has_version) {
		QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);
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
			QSENDSTR("Failed to save JAR; if the game is already running, close it and try again", 500);
		}
		while (written < jar_size) {
			written += write(file, jar + written, jar_size - written);
		}
		close(file);
		delete[] jar;
	}

	// set up argv for the new process
	std::string path_str = this->hdos_path.string();
	char arg_jar[] = "-jar";
	char* argv[] = {
		java.data(),
		arg_user_home.data(),
		arg_app_user_home.data(),
		arg_jar,
		path_str.data(),
		nullptr,
	};

	pid_t pid = fork();
	if (pid == 0) {
		setpgrp();
		if (chdir(this->data_dir.c_str())) {
			fmt::print("[B] new process was unable to chdir: {}\n", errno);
			exit(1);
		}
		close(STDIN_FILENO);
		setenv("HOME", user_home.data(), true);
		setenv("BOLT_JAVA_PATH", java.data(), true);
		if (has_jx_session_id) setenv("JX_SESSION_ID", jx_session_id.data(), true);
		if (has_jx_character_id) setenv("JX_CHARACTER_ID", jx_character_id.data(), true);
		if (has_jx_display_name) setenv("JX_DISPLAY_NAME", jx_display_name.data(), true);
		execv(*argv, argv);
	}

	fmt::print("[B] Successfully spawned game process with pid {}\n", pid);
	if (has_version) {
		size_t written = 0;
		int file = open(this->hdos_version_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (file == -1) {
			QSENDSTR("OK, but unable to save version file", 200);
		}
		while (written < version.size()) {
			written += write(file, version.c_str() + written, version.size() - written);
		}
		close(file);
	}
	QSENDOK();
}

void Browser::Launcher::OpenExternalUrl(char* url) const {
	char arg_env[] = "/usr/bin/env";
	char arg_xdg_open[] = "xdg-open";
	char* argv[] { arg_env, arg_xdg_open, url, nullptr };
	pid_t pid = fork();
	if (pid == 0) execv(arg_env, argv);
}

bool BrowseFile(std::filesystem::path& dir) {
	char arg_env[] = "/usr/bin/env";
	char arg_xdg_open[] = "xdg-open";
	std::string p = dir;
	char* argv[] { arg_env, arg_xdg_open, p.data(), nullptr };
	pid_t pid = fork();
	if (pid == 0) execv(arg_env, argv);
	return pid > 0;
}
