#include "client.hxx"
#include "window_launcher.hxx"
#include "include/internal/cef_types.h"
#include "resource_handler.hxx"

#include "include/cef_parser.h"

#include <fcntl.h>
#include <fmt/core.h>
#include <fstream>
#include <sstream>

#define BOLTDOMAIN "bolt-internal"
#define BOLTURI "https://" BOLTDOMAIN
#define DEFAULTURL BOLTURI "/index.html"

#if defined(__linux__)
#define URLPLATFORM "linux"
#endif

#if defined(_WIN32)
#define URLPLATFORM "windows"
#endif

#if defined(__APPLE__)
#define URLPLATFORM "mac"
#endif

CefRefPtr<CefResourceRequestHandler> SaveFileFromPost(CefRefPtr<CefRequest>, const std::filesystem::path::value_type*);

struct FilePicker: public CefRunFileDialogCallback, Browser::ResourceHandler {
	FilePicker(CefRefPtr<CefBrowser> browser, std::vector<CefString> accept_filters):
		accept_filters(accept_filters), callback(nullptr), browser_host(browser->GetHost()), ResourceHandler("text/plain") { }

	bool Open(CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) override {
		this->callback = callback;
		CefString title = "Select a JAR file...";
		CefString default_file_path = "";
		this->browser_host->RunFileDialog(FILE_DIALOG_OPEN, title, "", this->accept_filters, this);
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
		std::vector<CefString> accept_filters;
		CefRefPtr<CefCallback> callback;
		std::string data_;
		CefRefPtr<CefBrowserHost> browser_host;
		IMPLEMENT_REFCOUNTING(FilePicker);
		DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

Browser::Launcher::Launcher(
	CefRefPtr<Browser::Client> client,
	Details details,
	bool show_devtools,
	CefRefPtr<FileManager::FileManager> file_manager,
	std::filesystem::path config_dir,
	std::filesystem::path data_dir
): Window(client, details, show_devtools), data_dir(data_dir), file_manager(file_manager) {
	this->creds_path = data_dir;
	this->creds_path.append("creds");

	this->config_path = config_dir;
	this->config_path.append("launcher.json");

	this->rs3_elf_path = data_dir;
	this->rs3_elf_path.append("rs3linux");

	this->rs3_elf_hash_path = data_dir;
	this->rs3_elf_hash_path.append("rs3linux.sha256");

	this->rs3_exe_path = data_dir;
	this->rs3_exe_path.append("rs3windows.exe");

	this->rs3_exe_hash_path = data_dir;
	this->rs3_exe_hash_path.append("rs3windows.sha256");

	this->rs3_app_path = data_dir;
	this->rs3_app_path.append("rs3mac");

	this->rs3_app_hash_path = data_dir;
	this->rs3_app_hash_path.append("rs3mac.sha256");

	this->osrs_exe_path = data_dir;
	this->osrs_exe_path.append("osrswindows.exe");

	this->osrs_exe_hash_path = data_dir;
	this->osrs_exe_hash_path.append("osrswindows.sha256");

	this->osrs_app_path = data_dir;
	this->osrs_app_path.append("osrsmac");

	this->osrs_app_hash_path = data_dir;
	this->osrs_app_hash_path.append("osrsmac.sha256");

	this->runelite_path = data_dir;
	this->runelite_path.append("runelite.jar");

	this->runelite_id_path = data_dir;
	this->runelite_id_path.append("runelite_id.bin");

	this->hdos_path = data_dir;
	this->hdos_path.append("hdos.jar");

	this->hdos_version_path = data_dir;
	this->hdos_version_path.append("hdos_version.bin");

#if defined(BOLT_PLUGINS)
	this->plugin_config_path = config_dir;
	this->plugin_config_path.append("plugins.json");
#endif

	CefString url = this->BuildURL();
	this->Init(this, details, url, show_devtools);
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
		const std::string_view query(request_url.begin() + colon + 1, request_url.end());
		bool has_code = false;
		CefString code;
		bool has_state = false;
		CefString state;
		this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
			PQCEFSTRING(code)
			PQCEFSTRING(state)
		}, ',');
		if (has_code && has_state) {
			disable_default_handling = true;
			QSENDMOVED(CefString(std::string(DEFAULTURL "?code=") + code.ToString() + "&state=" + state.ToString()));
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
		QSENDMOVED(CefString(std::string(DEFAULTURL "?") + std::string(query)));
	}

	// handler for another custom request thing, but this one uses localhost, for whatever reason
	if (domain == "localhost" && path == "/" && comment.starts_with("code=")) {
		disable_default_handling = true;
		QSENDMOVED(CefString(std::string(DEFAULTURL "?") + std::string(comment)));
	}

	const bool is_internal_target = domain == BOLTDOMAIN;
	const bool is_internal_initiator = request_initiator == BOLTURI;

	if (is_internal_target) {
		disable_default_handling = true;
	}

	// internal API endpoints - only allowed if it's a request from internal URL to internal URL
	if (is_internal_target && is_internal_initiator) {

		// instruction to launch RS3 .deb
		if (path == "/launch-rs3-deb") {
			return this->LaunchRs3Deb(request, query);
		}

		// instruction to launch RS3 .exe
		if (path == "/launch-rs3-exe") {
			return this->LaunchRs3Exe(request, query);
		}

		// instruction to launch RS3 app (mac)
		if (path == "/launch-rs3-app") {
			return this->LaunchRs3App(request, query);
		}

		// instruction to launch OSRS .exe
		if (path == "/launch-osrs-exe") {
			return this->LaunchOsrsExe(request, query);
		}

		// instruction to launch OSRS app (mac)
		if (path == "/launch-osrs-app") {
			return this->LaunchOsrsApp(request, query);
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

		// instruction to save plugin config to disk
		if (path == "/save-plugin-config") {
#if defined(BOLT_PLUGINS)
			return SaveFileFromPost(request, this->plugin_config_path.c_str());
#else
			QSENDNOTSUPPORTED();
#endif
		}

		// request for list of connected game clients
		if (path == "/list-game-clients") {
#if defined(BOLT_PLUGINS)
			this->UpdateClientList(true);
			QSENDOK();
#else
			QSENDNOTSUPPORTED();
#endif
		}

		// request for the contents of a JSON file - doesn't actually validate the contents
		if (path == "/read-json-file") {
#if defined(BOLT_PLUGINS)
			QSTRING path;
			bool has_path = false;
			this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
				PQSTRING(path)
			});
			QREQPARAM(path);
			std::ifstream file(path, std::ios::in | std::ios::binary);
			if (!file.fail()) {
				std::stringstream ss;
				ss << file.rdbuf();
				std::string str = ss.str();
				file.close();
				return new Browser::ResourceHandler(str, 200, "application/json");
			} else {
				QSENDNOTFOUND();
			}
#else
			QSENDNOTSUPPORTED();
#endif
		}

		// request to send a message to a game client to start a plugin
		if (path == "/start-plugin") {
#if defined(BOLT_PLUGINS)
			CefString id;
			bool has_id = false;
			CefString path;
			bool has_path  = false;
			CefString main;
			bool has_main  = false;
			uint64_t client;
			bool has_client  = false;
			bool client_valid = false;
			this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
				PQCEFSTRING(id)
				PQCEFSTRING(path)
				PQCEFSTRING(main)
				PQINT(client)
			});
			QREQPARAM(id);
			QREQPARAM(path);
			QREQPARAM(main);
			QREQPARAMINT(client);
			this->client->StartPlugin(client, std::string(id), std::string(path), std::string(main));
			QSENDOK();
#else
			QSENDNOTSUPPORTED();
#endif
		}

		// request to stop a plugin that's currently running for a specific client
		if (path == "/stop-plugin") {
#if defined(BOLT_PLUGINS)
			uint64_t client;
			bool has_client = false;
			bool client_valid = false;
			uint64_t uid;
			bool has_uid = false;
			bool uid_valid = false;
			this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
				PQINT(client)
				PQINT(uid)
			});
			QREQPARAMINT(client);
			QREQPARAMINT(uid);
			this->client->StopPlugin(client, uid);
			QSENDOK();
#else
			QSENDNOTSUPPORTED();
#endif
		}

		// instruction to try to open an external URL in the user's browser
		if (path == "/open-external-url") {
			CefRefPtr<CefPostData> post_data = request->GetPostData();
			QSENDBADREQUESTIF(!post_data || post_data->GetElementCount() != 1);
			CefPostData::ElementVector elements;
			post_data->GetElements(elements);
			size_t byte_count = elements[0]->GetBytesCount();
			char* url = new char[byte_count + 1];
			elements[0]->GetBytes(byte_count, url);
			url[byte_count] = '\0';
			this->OpenExternalUrl(url);
			delete[] url;
			QSENDOK();
		}

		// instruction to try to open Bolt's data directory in the user's file explorer
		if (path == "/browse-data") {
			if (!this->BrowseData()) {
				QSENDSTR("Error creating process", 500);
			}
			QSENDOK();
		}

		// instruction to open a file picker for .jar files
		if (path == "/jar-file-picker") {
			return new FilePicker(browser, {".jar"});
		}
		
		// instruction to open a file picker for .json files
		if (path == "/json-file-picker") {
			return new FilePicker(browser, {".json"});
		}
	}

	// internal hashmap of filenames - allowed to fetch these either if the request is from an internal origin,
	// or is_navigation is true, since navigations have no origin
	if (is_internal_target && (is_internal_initiator || is_navigation)) {
		FileManager::File file = this->file_manager->get(path);
		if (file.contents) {
			return new ResourceHandler(file, this->file_manager);
		} else {
			this->file_manager->free(file);
			QSENDNOTFOUND();
		}
	}

	if (!is_internal_initiator) {
		return nullptr;
	}

	// default way to handle web requests from the main window is to make a CefURLRequest that's not subject to CORS safety
	disable_default_handling = true;
	CefRefPtr<CefRequest> new_request = CefRequest::Create();
	CefRequest::HeaderMap headers;
	request->GetHeaderMap(headers); // doesn't include Referer header
	headers.erase("Origin");
	new_request->Set(request->GetURL(), request->GetMethod(), request->GetPostData(), headers);
	return new DefaultURLHandler(new_request);
}

void Browser::Launcher::OnBrowserDestroyed(CefRefPtr<CefBrowserView> view, CefRefPtr<CefBrowser> browser) {
	Window::OnBrowserDestroyed(view, browser);
	this->file_manager = nullptr;
}

CefString Browser::Launcher::BuildURL() const {
	std::stringstream url;
	url << DEFAULTURL "?platform=" URLPLATFORM
#if defined(BOLT_FLATHUB_BUILD)
	"&flathub=1"
#endif
	;

	std::ifstream rs_elf_hashfile(this->rs3_elf_hash_path.c_str(), std::ios::in | std::ios::binary);
	if (!rs_elf_hashfile.fail()) {
		url << "&rs3_deb_installed_hash=" << rs_elf_hashfile.rdbuf();
	}

	std::ifstream rs_exe_hashfile(this->rs3_exe_hash_path.c_str(), std::ios::in | std::ios::binary);
	if (!rs_exe_hashfile.fail()) {
		url << "&rs3_exe_installed_hash=" << rs_exe_hashfile.rdbuf();
	}

	std::ifstream rs_app_hashfile(this->rs3_app_hash_path.c_str(), std::ios::in | std::ios::binary);
	if (!rs_app_hashfile.fail()) {
		url << "&rs3_app_installed_hash=" << rs_app_hashfile.rdbuf();
	}

	std::ifstream osrs_exe_hashfile(this->osrs_exe_hash_path.c_str(), std::ios::in | std::ios::binary);
	if (!osrs_exe_hashfile.fail()) {
		url << "&osrs_exe_installed_hash=" << osrs_exe_hashfile.rdbuf();
	}

	std::ifstream osrs_app_hashfile(this->osrs_app_hash_path.c_str(), std::ios::in | std::ios::binary);
	if (!osrs_app_hashfile.fail()) {
		url << "&osrs_app_installed_hash=" << osrs_app_hashfile.rdbuf();
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

#if defined(BOLT_PLUGINS)
	std::ifstream plugin_file(this->plugin_config_path.c_str(), std::ios::in | std::ios::binary);
	std::string plugins_str;
	if (!plugin_file.fail()) {
		std::stringstream ss;
		ss << plugin_file.rdbuf();
		plugins_str = CefURIEncode(CefString(ss.str()), true).ToString();
		url << "&plugins=" << plugins_str;
		plugin_file.close();
	} else {
		url << "&plugins=%7B%7D";
	}
#endif

	return url.str();
}

#if defined(BOLT_PLUGINS)
void Browser::Launcher::UpdateClientList(bool need_lock_mutex) const {
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("__bolt_clientlist");
	CefRefPtr<CefListValue> list = message->GetArgumentList();
	this->client->ListGameClients(list, need_lock_mutex);
	this->browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
}
#endif

void Browser::Launcher::Refresh() const {
	// override the default behaviour, which would be to call ReloadIgnoreCache() (a.k.a. ctrl+f5)
	// because if certain config files have changed, we need to set different URL params than before
	this->browser->GetMainFrame()->LoadURL(this->BuildURL());
}

void Browser::Launcher::ParseQuery(std::string_view query, std::function<void(const std::string_view&, const std::string_view&)> callback, char delim) {
	size_t pos = 0;
	while (true) {
		const size_t next_and = query.find(delim, pos);
		const size_t next_eq = query.find('=', pos);
		if (next_eq == std::string_view::npos) break;
		else if (next_and != std::string_view::npos && next_eq > next_and) {
			pos = next_and + 1;
			continue;
		}
		const bool is_last = next_and == std::string_view::npos;
		const auto end = is_last ? query.end() : query.begin() + next_and;
		const std::string_view key(query.begin() + pos, query.begin() + next_eq);
		const std::string_view val(query.begin() + next_eq + 1, end);
		callback(key, val);
		if (is_last) break;
		pos = next_and + 1;
	}
}

void Browser::Launcher::NotifyClosed() {
	this->client->OnLauncherClosed();
}

CefRefPtr<CefResourceRequestHandler> SaveFileFromPost(CefRefPtr<CefRequest> request, const std::filesystem::path::value_type* path) {
	CefRefPtr<CefPostData> post_data = request->GetPostData();
	QSENDBADREQUESTIF(!post_data || post_data->GetElementCount() != 1);

	CefPostData::ElementVector elements;
	post_data->GetElements(elements);
	size_t byte_count = elements[0]->GetBytesCount();
	
	std::ofstream file(path, std::ios::out | std::ios::binary);
	QSENDSYSTEMERRORIF(file.fail());
	char* buf = new char[byte_count];
	elements[0]->GetBytes(byte_count, buf);
	file.write(buf, byte_count);
	delete[] buf;
	QSENDOK();
}
