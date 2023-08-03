#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include "include/cef_parser.h"

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>
#include <sys/stat.h>

#if defined(__linux__)
constexpr std::string_view URI = "launcher.html?platform=linux";
#endif

#if defined(_WIN32)
constexpr std::string_view URI = "launcher.html?platform=windows";
#endif

#if defined(__APPLE__)
constexpr std::string_view URI = "launcher.html?platform=mac";
#endif

Browser::Launcher::Launcher(
	CefRefPtr<CefClient> client,
	Details details,
	bool show_devtools,
	const std::map<std::string, InternalFile>* const internal_pages,
	std::filesystem::path data_dir
): Window(details, show_devtools), data_dir(data_dir), internal_pages(internal_pages) {
	std::string url = this->internal_url + std::string(URI);

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
			return this->LaunchDeb(request, query);
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
