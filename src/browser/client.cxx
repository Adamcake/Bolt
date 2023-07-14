#include "client.hxx"
#include "include/cef_parser.h"
#include "include/cef_life_span_handler.h"
#include "include/internal/cef_types.h"
#include "include/internal/cef_types_wrappers.h"

#if defined(__linux__)
#include <archive.h>
#include <archive_entry.h>
#endif

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>
#include <fstream>
#include <regex>
#include <spawn.h>

/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_resource_request_handler.h
/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_resource_handler.h
struct ResourceHandler: public CefResourceRequestHandler, CefResourceHandler {
	ResourceHandler(const unsigned char* data, size_t len, int status, CefString mime):
		data(data), data_len(len), status(status), mime(mime), has_location(false), cursor(0) { }
	ResourceHandler(const unsigned char* data, size_t len, int status, CefString mime, CefString location):
		data(data), data_len(len), status(status), mime(mime), location(location), has_location(true), cursor(0) { }

	bool Open(CefRefPtr<CefRequest>, bool& handle_request, CefRefPtr<CefCallback>) override {
		handle_request = true;
		return true;
	}

	void GetResponseHeaders(CefRefPtr<CefResponse> response, int64& response_length, CefString& redirectUrl) override {
		response->SetStatus(this->status);
		response->SetMimeType(this->mime);
		if (this->has_location) {
			response->SetHeaderByName("Location", this->location, false);
		}
		response_length = this->data_len;
	}

	bool Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback>) override {
		if (this->cursor == this->data_len) {
			// "To indicate response completion set |bytes_read| to 0 and return false."
			bytes_read = 0;
			return false;
		}
		if (this->cursor + bytes_to_read <= this->data_len) {
			// requested read is entirely in bounds
			bytes_read = bytes_to_read;
			memcpy(data_out, this->data + this->cursor, bytes_read);
			this->cursor += bytes_to_read;
		} else {
			// read only to end of string
			bytes_read = this->data_len - this->cursor;
			memcpy(data_out, this->data + this->cursor, bytes_read);
			this->cursor = this->data_len;
		}
		return true;
	}

	bool Skip(int64 bytes_to_skip, int64& bytes_skipped, CefRefPtr<CefResourceSkipCallback>) override {
		if (this->cursor + bytes_to_skip <= this->data_len) {
			// skip in bounds
			bytes_skipped = bytes_to_skip;
			this->cursor += bytes_to_skip;
		} else {
			// skip to end of string
			bytes_skipped = this->data_len - this->cursor;
			this->cursor = this->data_len;
		}
		return true;
	}

	void Cancel() override {
		this->cursor = this->data_len;
	}

	CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>) override {
		return this;
	}

	private:
		const unsigned char* data;
		size_t data_len;
		int status;
		CefString mime;
		CefString location;
		bool has_location;
		size_t cursor;
		IMPLEMENT_REFCOUNTING(ResourceHandler);
		DISALLOW_COPY_AND_ASSIGN(ResourceHandler);
};

_InternalFile allocate_file(const char* filename, CefString mime_type) {
	// little helper function for Client constructor
	std::ifstream file(filename, std::ios::binary);
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();

	if (size < 0) {
		return _InternalFile { .success = false };
	}

	file.seekg(0, std::ios::beg);
	std::vector<unsigned char> buf(size);
	if (file.read(reinterpret_cast<char*>(buf.data()), size)) {
		return _InternalFile {
			.success = true,
			.data = buf,
			.mime_type = mime_type,
		};
	} else {
		return _InternalFile { .success = false };
	}
}

Browser::Client::Client(CefRefPtr<Browser::App> app, std::filesystem::path config_dir): show_devtools(true), config_dir(config_dir) {
	CefString mime_type_html = "text/html";
	CefString mime_type_js = "application/javascript";
	app->SetBrowserProcessHandler(this);
	this->internal_pages["/index.html"] = allocate_file("html/index.html", mime_type_html);
	this->internal_pages["/oauth.html"] = allocate_file("html/oauth.html", mime_type_html);
	this->internal_pages["/game_auth.html"] = allocate_file("html/game_auth.html", mime_type_html);
	this->internal_pages["/frame.html"] = allocate_file("html/frame.html", mime_type_html);
}

CefRefPtr<CefLifeSpanHandler> Browser::Client::GetLifeSpanHandler() {
	return this;
}

CefRefPtr<CefRequestHandler> Browser::Client::GetRequestHandler() {
	return this;
}

void Browser::Client::OnContextInitialized() {
	fmt::print("[B] OnContextInitialized\n");
	// After main() enters its event loop, this function will be called on the main thread when CEF
	// context is ready to go, so, as suggested by CEF examples, Bolt treats this as an entry point.
	Browser::Details details = {
		.preferred_width = 800,
		.preferred_height = 608,
		.center_on_open = true,
		.resizeable = true,
		.frame = true,
		.controls_overlay = false,
	};
	std::string url = this->internal_url + this->launcher_uri;
	this->windows_lock.lock();
	CefRefPtr<Browser::Window> w = new Browser::Window(Browser::Kind::Launcher, this, details, url, this->show_devtools);
	this->windows.push_back(w);
	this->windows_lock.unlock();
}

bool Browser::Client::OnBeforePopup(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	const CefString& target_url,
	const CefString& target_frame_name,
	CefLifeSpanHandler::WindowOpenDisposition target_disposition,
	bool user_gesture,
	const CefPopupFeatures& popup_features,
	CefWindowInfo& window_info,
	CefRefPtr<CefClient>& client,
	CefBrowserSettings& settings,
	CefRefPtr<CefDictionaryValue>& extra_info,
	bool* no_javascript_access
) {
	std::lock_guard<std::mutex> _(this->windows_lock);
	for (CefRefPtr<Browser::Window>& window: this->windows) {
		window->SetPopupFeaturesForBrowser(browser, popup_features);
	}
	return false;
}

void Browser::Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnAfterCreated for browser {}\n", browser->GetIdentifier());
}

bool Browser::Client::DoClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] DoClose for browser {}\n", browser->GetIdentifier());
	std::lock_guard<std::mutex> _(this->windows_lock);
	this->windows.erase(
		std::remove_if(
			this->windows.begin(),
			this->windows.end(),
			[&browser](const CefRefPtr<Browser::Window>& window){ return window->CloseBrowser(browser); }
		),
		this->windows.end()
	);
	return false;
}

void Browser::Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnBeforeClose for browser {}\n", browser->GetIdentifier());
}

bool Browser::Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	std::lock_guard<std::mutex> _(this->windows_lock);
	CefString name = message->GetName();

	if (name == "__bolt_app_settings") {
		fmt::print("[B] bolt_app_settings received for browser {}\n", browser->GetIdentifier());
		CefWindowInfo window_info; // ignored, because this is a BrowserView
		CefBrowserSettings browser_settings;
		browser->GetHost()->ShowDevTools(window_info, this, browser_settings, CefPoint());
		return true;
	}

	if (name == "__bolt_app_minify") {
		fmt::print("[B] bolt_app_minify received for browser {}\n", browser->GetIdentifier());
		return true;
	}

	if (name == "__bolt_app_begin_drag") {
		fmt::print("[B] bolt_app_begin_drag received for browser {}\n", browser->GetIdentifier());
		return true;
	}

	return false;
}

CefRefPtr<CefResourceRequestHandler> Browser::Client::GetResourceRequestHandler(
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
		110, 99, 104, 101, 114, 47, 114, 117, 110, 101, 115, 99,97, 112, 101
	};

	// Find the Window responsible for this request, if any
	this->windows_lock.lock();
	auto it = std::find_if(
		this->windows.begin(),
		this->windows.end(),
		[&browser](const CefRefPtr<Browser::Window>& window){ return window->HasBrowser(browser); }
	);
	if (it == this->windows.end()) {
		// Request came from no window?
		return nullptr;
	}
	const CefRefPtr<Browser::Window> window = *it;
	this->windows_lock.unlock();

	if (window->IsApp()) {
		// TODO: make Browser::Window a valid request handler for sandboxing purposes
		return nullptr;
	}

	if (window->IsLauncher()) {
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

#if defined(__linux__)
			// instruction to launch RS3 .deb
			if (path == "/launch-deb") {
				CefRefPtr<CefPostData> post_data = request->GetPostData();
				std::filesystem::path path = this->config_dir;
				path.append("rs3linux");

				auto cursor = 0;
				bool has_hash = false;
				bool has_access_token = false;
				bool has_refresh_token = false;
				bool has_session_id = false;
				bool has_character_id = false;
				bool has_display_name = false;
				CefString hash;
				CefString access_token;
				CefString refresh_token;
				CefString session_id;
				CefString character_id;
				CefString display_name;
					
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
						access_token = CefURIDecode(value, true, UU_SPACES);
						has_hash = true;
					}
					if (key == "jx_access_token") {
						auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
						CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
						access_token = CefURIDecode(value, true, UU_SPACES);
						has_access_token = true;
					}
					if (key == "jx_refresh_token") {
						auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
						CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
						access_token = CefURIDecode(value, true, UU_SPACES);
						has_refresh_token = true;
					}
					if (key == "jx_session_id") {
						auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
						CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
						access_token = CefURIDecode(value, true, UU_SPACES);
						has_session_id = true;
					}
					if (key == "jx_character_id") {
						auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
						CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
						access_token = CefURIDecode(value, true, UU_SPACES);
						has_character_id = true;
					}
					if (key == "jx_display_name") {
						auto value_end = next_amp == std::string::npos ? query.end() : query.begin() + next_amp;
						CefString value = std::string(std::string_view(query.begin() + next_eq + 1,  value_end));
						access_token = CefURIDecode(value, true, UU_SPACES);
						has_display_name = true;
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
					int file = open(path.c_str(), O_WRONLY | O_CREAT, 0755);
					while (written < game_size) {
						written += write(file, game + written, game_size - written);
					}
					close(file);
					delete[] game;
				}

				posix_spawn_file_actions_t file_actions;
				posix_spawnattr_t attributes;
				pid_t pid;
				posix_spawn_file_actions_init(&file_actions);
				posix_spawnattr_init(&attributes);
				std::string path_str(path.c_str());
				char* argv[2];
				argv[0] = path_str.data();
				argv[1] = nullptr;
				char* env[8];
				memset(env, 0, sizeof env);
				int r = posix_spawn(&pid, path_str.c_str(), &file_actions, &attributes, argv, env);

				if (r == 0) {
					fmt::print("Successfully spawned game process with pid {}\n", pid);
					const char* data = "OK\n";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
				} else {
					fmt::print("[B] Error from posix_spawn: {}\n", r);
					const char* data = "Error spawning process\n";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
				}
			}
#endif

			// respond using internal hashmap of filenames
			auto it = std::find_if(
				this->internal_pages.begin(),
				this->internal_pages.end(),
				[&path](const auto& e) { return e.first == path; }
			);
			if (it != this->internal_pages.end()) {
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
	}

	// route the request normally, to a website or whatever
	return nullptr;
}
