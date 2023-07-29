#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include "include/cef_parser.h"

#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <fmt/core.h>
#include <spawn.h>

extern char **environ;

constexpr std::string_view env_key_home = "HOME=";
constexpr std::string_view env_key_access_token = "JX_ACCESS_TOKEN=";
constexpr std::string_view env_key_refresh_token = "JX_REFRESH_TOKEN=";
constexpr std::string_view env_key_session_id = "JX_SESSION_ID=";
constexpr std::string_view env_key_character_id = "JX_CHARACTER_ID=";
constexpr std::string_view env_key_display_name = "JX_DISPLAY_NAME=";

Browser::Launcher::Launcher(
	CefRefPtr<CefClient> client,
	Details details,
	bool show_devtools,
	const std::map<std::string, InternalFile>* const internal_pages,
	std::filesystem::path data_dir
): Window(details, show_devtools), data_dir(data_dir), internal_pages(internal_pages) {
	std::string url = this->internal_url + std::string("index.html?platform=linux");

	this->creds_path = data_dir;
	this->creds_path.append("creds");

	this->rs3_path = data_dir;
	this->rs3_path.append("rs3linux");

	this->rs3_hash_path = data_dir;
	this->rs3_hash_path.append("rs3linux.sha256");

	int file = open(this->rs3_hash_path.c_str(), O_RDONLY);
	if (file != -1) {
		char buf[64];
		ssize_t r = read(file, buf, 64);
		if (r <= 64) {
			url += "&rs3_linux_installed_hash=";
			url.append(buf, r);
		}
	}
	close(file);

	struct stat file_status;
	if (stat(this->creds_path.c_str(), &file_status) >= 0) {
		file = open(this->creds_path.c_str(), O_RDONLY);
		if (file != -1) {
			char* buf = new char[file_status.st_size];
			size_t written = 0;
			while (written < file_status.st_size) {
				written += read(file, buf + written, file_status.st_size - written);
			}
			CefString str = CefURIEncode(CefString(buf, file_status.st_size), true);
			url += "&credentials=";
			url += str.ToString();
			delete[] buf;
			close(file);
		}
	}

	this->Init(client, details, url, show_devtools);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling
) {
	// Some of the dreaded phrases that I don't want to show up in a grep/search
	constexpr char provider[] = {106, 97, 103, 101, 120, 0};
	constexpr char tar_xz_inner_path[] = {
		46, 47, 117, 115, 114, 47, 115, 104, 97, 114, 101, 47, 103, 97, 109, 101,
		115, 47, 114, 117, 110, 101, 115, 99, 97, 112, 101, 45, 108, 97, 117,
		110, 99, 104, 101, 114, 47, 114, 117, 110, 101, 115, 99,97, 112, 101, 0
	};
	constexpr char launcher_redirect_domain[] = {
		115, 101, 99, 117, 114, 101, 46, 114, 117, 110, 101, 115, 99, 97, 112, 101,
		46, 99, 111, 109, 0
	};
	constexpr char launcher_redirect_path[] = {
		47, 109, 61, 119, 101, 98, 108, 111, 103, 105, 110, 47, 108, 97, 117, 110,
		99, 104, 101, 114, 45, 114, 101, 100, 105, 114, 101, 99, 116, 0
	};

	// Parse URL
	const std::string request_url = request->GetURL().ToString();
	const std::string::size_type colon = request_url.find_first_of(':');
	if (colon == std::string::npos) {
		return nullptr;
	}
	const std::string_view schema(request_url.begin(), request_url.begin() + colon);
	if (schema == provider) {
		// parser for custom schema thing
		bool has_code = false;
		std::string_view code;
		bool has_state = false;
		std::string_view state;
		std::string::size_type cursor = colon + 1;
		while (true) {
			std::string::size_type next_eq = request_url.find_first_of('=', cursor);
			std::string::size_type next_comma = request_url.find_first_of(',', cursor);
			if (next_eq == std::string::npos || next_comma < next_eq) return nullptr;
			const bool last_pair = next_comma == std::string::npos;
			std::string_view key(request_url.begin() + cursor, request_url.begin() + next_eq);
			std::string_view value(request_url.begin() + next_eq + 1, last_pair ? request_url.end() : request_url.begin() + next_comma);
			if (key == "code") {
				code = value;
				has_code = true;
			}
			if (key == "state") {
				state = value;
				has_state = true;
			}
			if (last_pair) {
				break;
			} else {
				cursor = next_comma + 1;
			}
		}
		if (has_code && has_state) {
			disable_default_handling = true;
			const char* data = "Moved\n";
			CefString location = CefString(this->internal_url + "oauth.html?code=" + std::string(code) + "&state=" + std::string(state));
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 302, "text/plain", location);
		} else {
			return nullptr;
		}
	}

	if ((schema != "http" && schema != "https") || request_url.size() < colon + 4
		|| request_url.at(colon + 1) != '/' || request_url.at(colon + 2) != '/')
	{
		return nullptr;
	}

	// parse all the important parts of the URL as string_views
	const std::string::size_type next_hash = request_url.find_first_of('#', colon + 3);
	const auto url_end = next_hash == std::string::npos ? request_url.end() : request_url.begin() + next_hash;
	const std::string::size_type next_sep = request_url.find_first_of('/', colon + 3);
	const std::string::size_type next_question = request_url.find_first_of('?', colon + 3);
	const auto domain_end = next_sep == std::string::npos && next_question == std::string::npos ? url_end : request_url.begin() + std::min(next_sep, next_question);
	const std::string_view domain(request_url.begin() + colon + 3, domain_end);
	const std::string_view path(domain_end, next_question == std::string::npos ? url_end : request_url.begin() + next_question);
	const std::string_view query(request_url.begin() + next_question + 1, next_question == std::string::npos ? request_url.begin() + next_question + 1 : url_end);
	const std::string_view comment(next_hash == std::string::npos ? request_url.end() : request_url.begin() + next_hash + 1, request_url.end());

	// handler for launcher-redirect url
	if (domain == launcher_redirect_domain && path == launcher_redirect_path) {
		disable_default_handling = true;
		const char* data = "Moved\n";
		CefString location = CefString(this->internal_url + "oauth.html?" + std::string(query));
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 302, "text/plain", location);
	}

	// handler for another custom request thing, but this one uses localhost, for whatever reason
	if (domain == "localhost" && path == "/" && comment.starts_with("code=")) {
		disable_default_handling = true;
		const char* data = "Moved\n";
		CefString location = CefString(this->internal_url + "game_auth.html?" + std::string(comment));
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 302, "text/plain", location);
	}

	// internal pages
	if (domain == "bolt-internal") {
		disable_default_handling = true;

		// instruction to launch RS3 .deb
		if (path == "/launch-deb") {
			CefRefPtr<CefPostData> post_data = request->GetPostData();
			auto cursor = 0;
			bool has_hash = false;
			bool should_set_access_token = false;
			bool should_set_refresh_token = false;
			bool should_set_session_id = false;
			bool should_set_character_id = false;
			bool should_set_display_name = false;
			std::string hash;
			std::string env_access_token;
			std::string env_refresh_token;
			std::string env_session_id;
			std::string env_character_id;
			std::string env_display_name;
				
			while (true) {
				const std::string::size_type next_eq = query.find_first_of('=', cursor);
				const std::string::size_type next_amp = query.find_first_of('&', cursor);
				if (next_eq == std::string::npos) break;
				if (next_eq >= next_amp) {
					cursor = next_amp + 1;
					continue;
				}
				std::string_view key(query.begin() + cursor, query.begin() + next_eq);
				if (key == "hash") {
					auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
					CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
					hash = CefURIDecode(value, true, UU_SPACES).ToString();
					has_hash = true;
				}
				if (key == "jx_access_token") {
					auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
					CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
					env_access_token = std::string(env_key_access_token) + CefURIDecode(value, true, UU_SPACES).ToString();
					should_set_access_token = true;
				}
				if (key == "jx_refresh_token") {
					auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
					CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
					env_refresh_token = std::string(env_key_refresh_token) + CefURIDecode(value, true, UU_SPACES).ToString();
					should_set_refresh_token = true;
				}
				if (key == "jx_session_id") {
					auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
					CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
					env_session_id = std::string(env_key_session_id) + CefURIDecode(value, true, UU_SPACES).ToString();
					should_set_session_id = true;
				}
				if (key == "jx_character_id") {
					auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
					CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
					env_character_id = std::string(env_key_character_id) + CefURIDecode(value, true, UU_SPACES).ToString();
					should_set_character_id = true;
				}
				if (key == "jx_display_name") {
					auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
					CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
					env_display_name = std::string(env_key_display_name) + CefURIDecode(value, true, UU_SPACES).ToString();
					should_set_display_name = true;
				}

				if (next_amp == std::string::npos) break;
				cursor = next_amp + 1;
			}

			if (has_hash) {
				CefRefPtr<CefPostData> post_data = request->GetPostData();
				if (post_data == nullptr || post_data->GetElementCount() != 1) {
					const char* data = "Bad Request";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
				}
				CefPostData::ElementVector vec;
				post_data->GetElements(vec);
				size_t deb_size = vec[0]->GetBytesCount();
				unsigned char* deb = new unsigned char[deb_size];
				vec[0]->GetBytes(deb_size, deb);
				struct archive* ar = archive_read_new();
				archive_read_support_format_ar(ar);
				archive_read_open_memory(ar, deb, deb_size);
				bool entry_found = false;
				struct archive_entry* entry;
				while (true) {
					int r = archive_read_next_header(ar, &entry);
					if (r == ARCHIVE_EOF) break;
					if (r != ARCHIVE_OK) {
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

				struct archive* xz = archive_read_new();
				archive_read_support_format_tar(xz);
				archive_read_support_filter_xz(xz);
				archive_read_open_memory(xz, tar_xz, entry_size);
				entry_found = false;
				while (true) {
					int r = archive_read_next_header(xz, &entry);
					if (r == ARCHIVE_EOF) break;
					if (r != ARCHIVE_OK) {
						archive_read_close(xz);
						archive_read_free(xz);
						delete[] tar_xz;
						const char* data = "Malformed .tar.xz file\n";
						return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
					}
					if (strcmp(archive_entry_pathname(entry), tar_xz_inner_path) == 0) {
						entry_found = true;
						break;
					}
				}
				if (!entry_found) {
					archive_read_close(xz);
					archive_read_free(xz);
					delete[] tar_xz;
					const char* data = "No target executable in .tar.xz file\n";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
				}

				const long game_size = archive_entry_size(entry);
				char* game = new char[game_size];
				written = 0;
				while (written < game_size) {
					written += archive_read_data(ar, game + written, game_size - written);
				}
				archive_read_close(xz);
				archive_read_free(xz);
				delete[] tar_xz;

				written = 0;
				int file = open(this->rs3_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
				if (file == -1) {
					delete[] game;
					const char* data = "Failed to save executable; if the game is already running, close it and try again\n";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
				}
				while (written < game_size) {
					written += write(file, game + written, game_size - written);
				}
				close(file);
				delete[] game;
			}

			char** e;
			for (e = environ; *e; e += 1);
			size_t env_count = e - environ;

			posix_spawn_file_actions_t file_actions;
			posix_spawnattr_t attributes;
			pid_t pid;
			posix_spawn_file_actions_init(&file_actions);
			posix_spawnattr_init(&attributes);
			posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID);
			std::string path_str(this->rs3_path.c_str());
			char* argv[2];
			argv[0] = path_str.data();
			argv[1] = nullptr;
			bool should_set_home = true;
			std::string env_home = std::string(env_key_home) + this->data_dir.string();
			char** env = new char*[env_count + 7];
			size_t i;
			for (i = 0; i < env_count; i += 1) {
				if (should_set_access_token && strncmp(environ[i], env_key_access_token.data(), env_key_access_token.size()) == 0) {
					should_set_access_token = false;
					env[i] = env_access_token.data();
				} else if (should_set_refresh_token && strncmp(environ[i], env_key_refresh_token.data(), env_key_refresh_token.size()) == 0) {
					should_set_refresh_token = false;
					env[i] = env_refresh_token.data();
				} else if (should_set_session_id && strncmp(environ[i], env_key_session_id.data(), env_key_session_id.size()) == 0) {
					should_set_session_id = false;
					env[i] = env_session_id.data();
				} else if (should_set_character_id && strncmp(environ[i], env_key_character_id.data(), env_key_character_id.size()) == 0) {
					should_set_character_id = false;
					env[i] = env_character_id.data();
				} else if (should_set_display_name && strncmp(environ[i], env_key_display_name.data(), env_key_display_name.size()) == 0) {
					should_set_display_name = false;
					env[i] = env_display_name.data();
				} else if (strncmp(environ[i], env_key_home.data(), env_key_home.size()) == 0) {
					should_set_home = false;
					env[i] = env_home.data();
				} else {
					env[i] = environ[i];
				}
			}
			if (should_set_access_token) {
				env[i] = env_access_token.data();
				i += 1;
			}
			if (should_set_refresh_token) {
				env[i] = env_refresh_token.data();
				i += 1;
			}
			if (should_set_session_id) {
				env[i] = env_session_id.data();
				i += 1;
			}
			if (should_set_character_id) {
				env[i] = env_character_id.data();
				i += 1;
			}
			if (should_set_display_name) {
				env[i] = env_display_name.data();
				i += 1;
			}
			if (should_set_home) {
				env[i] = env_home.data();
				i += 1;
			}
			env[i] = nullptr;

			int r = posix_spawn(&pid, path_str.c_str(), &file_actions, &attributes, argv, env);
			
			posix_spawnattr_destroy(&attributes);
			posix_spawn_file_actions_destroy(&file_actions);
			delete[] env;

			if (r == 0) {
				fmt::print("[B] Successfully spawned game process with pid {}\n", pid);

				if (has_hash) {
					size_t written = 0;
					int file = open(this->rs3_hash_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (file == -1) {
						const char* data = "OK, but unable to save hash file\n";
						return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
					}
					while (written < hash.size()) {
						written += write(file, hash.c_str() + written, hash.size() - written);
					}
					close(file);
				}

				const char* data = "OK\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
			} else {
				fmt::print("[B] Error from posix_spawn: {}\n", r);
				const char* data = "Error spawning process\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
			}
		}

		// instruction to save user credentials to disk
		if (path == "/save-credentials") {
			CefRefPtr<CefPostData> post_data = request->GetPostData();
			if (post_data->GetElementCount() != 1) {
				const char* data = "Bad request\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
			}

			CefPostData::ElementVector elements;
			post_data->GetElements(elements);
			size_t byte_count = elements[0]->GetBytesCount();
			size_t written = 0;
			int file = open(this->creds_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (file == -1) {
				const char* data = "Failed to open file\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
			}
			unsigned char* buf = new unsigned char[byte_count];
			elements[0]->GetBytes(byte_count, buf);
			while (written < byte_count) {
				written += write(file, buf + written, byte_count - written);
			}
			close(file);
			delete[] buf;

			const char* data = "OK\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
		}

		// respond using internal hashmap of filenames
		auto it = std::find_if(
			this->internal_pages->begin(),
			this->internal_pages->end(),
			[&path](const auto& e) { return e.first == path; }
		);
		if (it != this->internal_pages->end()) {
			if (it->second.success) {
				return new ResourceHandler(
					it->second.data.data(),
					it->second.data.size(),
					200,
					it->second.mime_type
				);
			} else {
				// this file failed to load during startup for some reason - 500
				const char* data = "Internal Server Error\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
			}
		} else {
			// no matching internal page - 404
			const char* data = "Not Found\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 404, "text/plain");
		}
	}

	return nullptr;
}
