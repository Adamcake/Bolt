#include "client.hxx"
#include "window_launcher.hxx"
#include "include/internal/cef_types.h"
#include "resource_handler.hxx"

#include "include/cef_parser.h"

#include <fcntl.h>
#include <fmt/core.h>
#include <fstream>
#include <sstream>

#if defined(__linux__)
constexpr std::string_view URI = "launcher.html?platform=linux";
#endif

#if defined(_WIN32)
constexpr std::string_view URI = "launcher.html?platform=windows";
#endif

#if defined(__APPLE__)
constexpr std::string_view URI = "launcher.html?platform=mac";
#endif

CefRefPtr<CefResourceRequestHandler> SaveFileFromPost(CefRefPtr<CefRequest>, const std::filesystem::path::value_type*);

struct JarFilePicker: public CefRunFileDialogCallback, Browser::ResourceHandler {
	JarFilePicker(CefRefPtr<CefBrowser> browser): callback(nullptr), browser_host(browser->GetHost()), ResourceHandler("text/plain") { }

	bool Open(CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) override {
		this->callback = callback;
		CefString title = "Select a JAR file...";
		CefString default_file_path = "";
		std::vector<CefString> accept_filters = { ".jar" };
		this->browser_host->RunFileDialog(FILE_DIALOG_OPEN, title, "", accept_filters, this);
		this->browser_host = nullptr;
		handle_request = false;
		return true;
	}

	void Cancel() override {
		this->browser_host = nullptr;
		this->callback = nullptr;
		Browser::ResourceHandler::Cancel();
	}

	void OnFileDialogDismissed(const std::vector<CefString>& file_paths) override {
		if (file_paths.size() == 0) {
			this->status = 204;
			this->data_len = 0;
			this->data = nullptr;
		} else {
			this->status = 200;
			this->data_ = file_paths[0].ToString();
			this->data_len = this->data_.size();
			this->data = reinterpret_cast<const unsigned char*>(this->data_.data());
		}

		if (this->callback) {
			this->callback->Continue();
			this->callback = nullptr;
		}
	}

	private:
		CefRefPtr<CefCallback> callback;
		std::string data_;
		CefRefPtr<CefBrowserHost> browser_host;
		IMPLEMENT_REFCOUNTING(JarFilePicker);
		DISALLOW_COPY_AND_ASSIGN(JarFilePicker);
};

Browser::Launcher::Launcher(
	CefRefPtr<Browser::Client> client,
	Details details,
	bool show_devtools,
	CefRefPtr<FileManager::FileManager> file_manager,
	std::filesystem::path config_dir,
	std::filesystem::path data_dir
): Window(client, details, show_devtools), data_dir(data_dir), file_manager(file_manager) {
	std::stringstream url;
	url << this->internal_url << URI << "&flathub=" << BOLT_FLATHUB_BUILD;

	this->creds_path = data_dir;
	this->creds_path.append("creds");

	this->config_path = config_dir;
	this->config_path.append("launcher.json");

	this->rs3_path = data_dir;
	this->rs3_path.append("rs3linux");

	this->rs3_hash_path = data_dir;
	this->rs3_hash_path.append("rs3linux.sha256");

	this->runelite_path = data_dir;
	this->runelite_path.append("runelite.jar");

	this->runelite_id_path = data_dir;
	this->runelite_id_path.append("runelite_id.bin");

	this->hdos_path = data_dir;
	this->hdos_path.append("hdos.jar");

	this->hdos_version_path = data_dir;
	this->hdos_version_path.append("hdos_version.bin");

	std::ifstream rs_deb_hashfile(this->rs3_hash_path.c_str(), std::ios::in | std::ios::binary);
	if (!rs_deb_hashfile.fail()) {
		url << "&rs3_linux_installed_hash=" << rs_deb_hashfile.rdbuf();
	}

	std::ifstream rl_hashfile(this->runelite_id_path.c_str(), std::ios::in | std::ios::binary);
	if (!rl_hashfile.fail()) {
		url << "&runelite_installed_id=" << rl_hashfile.rdbuf();
	}

	std::ifstream hdos_hashfile(this->hdos_version_path.c_str(), std::ios::in | std::ios::binary);
	if (!hdos_hashfile.fail()) {
		url << "&hdos_installed_version=" << hdos_hashfile.rdbuf();
	}

	std::ifstream creds_file(this->creds_path.c_str(), std::ios::in | std::ios::binary);
	std::string creds_str;
	if (!creds_file.fail()) {
		std::stringstream ss;
		ss << creds_file.rdbuf();
		creds_str = CefURIEncode(CefString(ss.str()), true).ToString();
		url << "&credentials=" << creds_str;
		creds_file.close();
	}

	std::ifstream config_file(this->config_path.c_str(), std::ios::in | std::ios::binary);
	std::string config_str;
	if (!config_file.fail()) {
		std::stringstream ss;
		ss << config_file.rdbuf();
		config_str = CefURIEncode(CefString(ss.str()), true).ToString();
		url << "&config=" << config_str;
		config_file.close();
	}

	this->Init(client, details, url.str(), show_devtools);
}

bool Browser::Launcher::IsLauncher() const {
	return true;
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
	const auto domain_end = next_sep == std::string::npos && next_question == std::string::npos ? url_end : request_url.begin() + (next_sep < next_question ? next_sep : next_question);
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

		if (path == "/close") {
			frame->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_close"));
			const char* data = "OK\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
		}

		// instruction to launch RS3 .deb
		if (path == "/launch-rs3-deb") {
			return this->LaunchRs3Deb(request, query);
		}

		// instruction to launch RuneLite.jar
		if (path == "/launch-runelite-jar") {
			return this->LaunchRuneliteJar(request, query, false);
		}

		// instruction to launch RuneLite.jar with --configure
		if (path == "/launch-runelite-jar-configure") {
			return this->LaunchRuneliteJar(request, query, true);
		}

		// instruction to launch RuneLite.jar
		if (path == "/launch-hdos-jar") {
			return this->LaunchHdosJar(request, query);
		}

		// instruction to save user config file to disk
		if (path == "/save-config") {
			return SaveFileFromPost(request, this->config_path.c_str());
		}

		// instruction to save user credentials to disk
		if (path == "/save-credentials") {
			return SaveFileFromPost(request, this->creds_path.c_str());
		}

		// instruction to try to open an external URL in the user's browser
		if (path == "/open-external-url") {
			CefRefPtr<CefPostData> post_data = request->GetPostData();
			if (post_data->GetElementCount() != 1) {
				const char* data = "Bad request\n";
				return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
			}
			CefPostData::ElementVector elements;
			post_data->GetElements(elements);
			size_t byte_count = elements[0]->GetBytesCount();
			char* url = new char[byte_count + 1];
			elements[0]->GetBytes(byte_count, url);
			url[byte_count] = '\0';
			this->OpenExternalUrl(url);
			delete[] url;
			const char* data = "OK\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
		}

		// instruction to open a file picker for .jar files
		if (path == "/jar-file-picker") {
			return new JarFilePicker(browser);
		}

		// respond using internal hashmap of filenames
		FileManager::File file = this->file_manager->get(path);
		if (file.contents) {
			return new ResourceHandler(file, this->file_manager);
		} else {
			this->file_manager->free(file);
			const char* data = "Not Found\n";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 404, "text/plain");
		}
	}

	return nullptr;
}

void Browser::Launcher::OnBrowserDestroyed(CefRefPtr<CefBrowserView> view, CefRefPtr<CefBrowser> browser) {
	Window::OnBrowserDestroyed(view, browser);
	this->file_manager = nullptr;
}

CefRefPtr<CefResourceRequestHandler> SaveFileFromPost(CefRefPtr<CefRequest> request, const std::filesystem::path::value_type* path) {
	CefRefPtr<CefPostData> post_data = request->GetPostData();
	if (post_data->GetElementCount() != 1) {
		const char* data = "Bad request\n";
		return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
	}

	CefPostData::ElementVector elements;
	post_data->GetElements(elements);
	size_t byte_count = elements[0]->GetBytesCount();
	
	std::ofstream file(path, std::ios::out | std::ios::binary);
	if (file.fail()) {
		const char* data = "Failed to open file\n";
		return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
	}
	char* buf = new char[byte_count];
	elements[0]->GetBytes(byte_count, buf);
	file.write(buf, byte_count);
	delete[] buf;

	const char* data = "OK\n";
	return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
}
