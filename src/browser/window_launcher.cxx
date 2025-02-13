#include "client.hxx"
#include "window_launcher.hxx"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"
#include "resource_handler.hxx"
#include "request.hxx"

#include <fcntl.h>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <sstream>

#if defined(HAS_LIBARCHIVE) && defined(BOLT_PLUGINS)
#include <archive.h>
#include <archive_entry.h>
#endif

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

	this->plugins_data_dir = data_dir;
	this->plugins_data_dir.append("plugins");

	this->plugins_config_dir = config_dir;
	this->plugins_config_dir.append("plugins");
#endif

	CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
	dict->SetBool("launcher", true);
	this->Init(this->BuildURL(), dict);
}

CefRefPtr<CefRequestHandler> Browser::Launcher::GetRequestHandler() {
	return this;
}

#define ROUTE(API, FUNC) if (path == "/" API) { return this->FUNC(request, query); }
#if defined(BOLT_PLUGINS)
#define ROUTEIFPLUGINS(API, FUNC) if (path == "/" API) { return this->FUNC(request, query); }
#else
#define ROUTEIFPLUGINS(API, FUNC) if (path == "/" API) { QSENDNOTSUPPORTED(); }
#endif
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
		ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
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
		ROUTE("launch-rs3-deb", LaunchRs3Deb)
		ROUTE("launch-rs3-exe", LaunchRs3Exe)
		ROUTE("launch-rs3-app", LaunchRs3App)
		ROUTE("launch-osrs-exe", LaunchOsrsExe)
		ROUTE("launch-osrs-app", LaunchOsrsApp)
		ROUTE("launch-runelite-jar", LaunchRuneliteJarNormal)
		ROUTE("launch-runelite-jar-configure", LaunchRuneliteJarConfigure)
		ROUTE("launch-hdos-jar", LaunchHdosJar)
		ROUTE("save-config", SaveConfig)
		ROUTE("save-credentials", SaveCredentials)
		ROUTE("open-external-url", OpenExternalUrl)
		ROUTE("browse-data", BrowseData)
		ROUTE("jar-file-picker", JarFilePicker)
		ROUTE("json-file-picker", JsonFilePicker)
		ROUTE("close", Close)
		ROUTEIFPLUGINS("save-plugin-config", SavePluginConfig)
		ROUTEIFPLUGINS("list-game-clients", ListGameClients)
		ROUTEIFPLUGINS("read-json-file", ReadJsonFile)
		ROUTEIFPLUGINS("start-plugin", StartPlugin)
		ROUTEIFPLUGINS("stop-plugin", StopPlugin)
		ROUTEIFPLUGINS("install-plugin", InstallPlugin)
		ROUTEIFPLUGINS("uninstall-plugin", UninstallPlugin)
		ROUTEIFPLUGINS("get-plugindir-json", GetPluginDirJson)
		ROUTEIFPLUGINS("browse-plugin-data", BrowsePluginData)
		ROUTEIFPLUGINS("browse-plugin-config", BrowsePluginConfig)
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
#if defined(HAS_LIBARCHIVE)
	"&has_libarchive=1"
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

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJarNormal(CefRefPtr<CefRequest> request, std::string_view query) {
	return this->LaunchRuneliteJar(request, query, false);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJarConfigure(CefRefPtr<CefRequest> request, std::string_view query) {
	return this->LaunchRuneliteJar(request, query, true);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::SaveConfig(CefRefPtr<CefRequest> request, std::string_view _) {
	return SaveFileFromPost(request, this->config_path.c_str());
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::SaveCredentials(CefRefPtr<CefRequest> request, std::string_view _) {
	return SaveFileFromPost(request, this->creds_path.c_str());
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::OpenExternalUrl(CefRefPtr<CefRequest> request, std::string_view _) {
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

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::BrowseData(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSYSTEMERRORIF(BrowseFile(this->data_dir));
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::JarFilePicker(CefRefPtr<CefRequest> request, std::string_view query) {
	return new FilePicker(browser, {".jar"});
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::JsonFilePicker(CefRefPtr<CefRequest> request, std::string_view query) {
	return new FilePicker(browser, {".json"});
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::Close(CefRefPtr<CefRequest> request, std::string_view query) {
	browser->GetHost()->CloseBrowser(false);
	QSENDOK();
}


#if defined(BOLT_PLUGINS)
CefRefPtr<CefResourceRequestHandler> Browser::Launcher::SavePluginConfig(CefRefPtr<CefRequest> request, std::string_view _) {
	return SaveFileFromPost(request, this->plugin_config_path.c_str());
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::ListGameClients(CefRefPtr<CefRequest> request, std::string_view _) {
	this->UpdateClientList(true);
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::ReadJsonFile(CefRefPtr<CefRequest> request, std::string_view query) {
	QSTRING path;
	bool has_path = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
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
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::StartPlugin(CefRefPtr<CefRequest> request, std::string_view query) {
	CefString id;
	bool has_id = false;
	CefString path;
	bool has_path  = false;
	CefString main;
	bool has_main  = false;
	uint64_t client;
	bool has_client  = false;
	bool client_valid = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQCEFSTRING(id)
		PQCEFSTRING(path)
		PQCEFSTRING(main)
		PQINT(client)
	});
	QREQPARAM(id);
	QREQPARAM(main);
	QREQPARAMINT(client);
	std::string sid = id;
	std::string spath;
	if (has_path) {
		spath = path;
	} else {
		std::filesystem::path p = this->plugins_data_dir;
		p.append(sid);
		spath = p.string() + '/';
	}
	this->client->StartPlugin(client, sid, spath, std::string(main));
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::StopPlugin(CefRefPtr<CefRequest> request, std::string_view query) {
	uint64_t client;
	bool has_client = false;
	bool client_valid = false;
	uint64_t uid;
	bool has_uid = false;
	bool uid_valid = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQINT(client)
		PQINT(uid)
	});
	QREQPARAMINT(client);
	QREQPARAMINT(uid);
	this->client->StopPlugin(client, uid);
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::InstallPlugin(CefRefPtr<CefRequest> request, std::string_view query) {
	CefRefPtr<CefPostData> post_data = request->GetPostData();
	QSENDBADREQUESTIF(!post_data || post_data->GetElementCount() != 1);

	QSTRING id;
	bool has_id = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(id)
	});
	QREQPARAM(id);
	QSENDBADREQUESTIF(id.size() == 0);
	std::filesystem::path plugin_dir = this->plugins_data_dir;
	plugin_dir.append(id);
	std::error_code err;
	QSENDSYSTEMERRORIF(std::filesystem::remove_all(plugin_dir, err) == -1);
	QSENDSYSTEMERRORIF(!std::filesystem::create_directories(plugin_dir, err));

	CefPostData::ElementVector elements;
	post_data->GetElements(elements);
	size_t file_length = elements[0]->GetBytesCount();
	char* archive_file = new char[file_length];
	elements[0]->GetBytes(file_length, archive_file);

	struct archive* archive = archive_read_new();
	archive_read_support_format_all(archive);
	archive_read_support_filter_all(archive);
	archive_read_open_memory(archive, archive_file, file_length);

	char buf[65536];
	struct archive_entry* entry;
	while (true) {
		const int r = archive_read_next_header(archive, &entry);
		if (r == ARCHIVE_EOF) break;
		if (r != ARCHIVE_OK) {
			archive_read_close(archive);
			archive_read_free(archive);
			delete[] archive_file;
			QSENDSTR("file is malformed", 400);
		}
		std::filesystem::path p = plugin_dir;
		const char* pathname = archive_entry_pathname(entry);
		if (pathname[0] == '\0' || pathname[strlen(pathname) - 1] == '/') continue;
		p.append(pathname);
		std::filesystem::create_directories(p.parent_path(), err);
		std::ofstream ofs(p, std::ofstream::binary);
		if (ofs.fail()) {
			archive_read_close(archive);
			archive_read_free(archive);
			delete[] archive_file;
			QSENDSTR("failed to create files", 500);
		}

		const la_int64_t entry_size = archive_entry_size(entry);
		la_int64_t written = 0;
		while (written < entry_size) {
			const la_ssize_t w = archive_read_data(archive, buf, sizeof(buf));
			ofs.write(buf, w);
			written += w;
		}
		ofs.close();
	}

	archive_read_close(archive);
	archive_read_free(archive);
	delete[] archive_file;
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::UninstallPlugin(CefRefPtr<CefRequest> request, std::string_view query) {
	QSTRING id;
	bool has_id = false;
	bool delete_data_dir = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(id)
		PQBOOL(delete_data_dir)
	});
	QREQPARAM(id);
	QSENDBADREQUESTIF(id.size() == 0);

	if (delete_data_dir) {
		std::filesystem::path plugin_dir = this->plugins_data_dir;
		plugin_dir.append(id);
		std::filesystem::remove_all(plugin_dir);
	}
	std::filesystem::path config_dir = this->plugins_config_dir;
	config_dir.append(id);
	std::filesystem::remove_all(config_dir);
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::BrowsePluginData(CefRefPtr<CefRequest> request, std::string_view query) {
	QSTRING id;
	bool has_id = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(id)
	});
	QREQPARAM(id);
	QSENDBADREQUESTIF(id.size() == 0);
	std::filesystem::path plugin_dir = this->plugins_data_dir;
	plugin_dir.append(id);
	QSENDSYSTEMERRORIF(BrowseFile(plugin_dir));
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::BrowsePluginConfig(CefRefPtr<CefRequest> request, std::string_view query) {
	QSTRING id;
	bool has_id = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(id)
	});
	QREQPARAM(id);
	QSENDBADREQUESTIF(id.size() == 0);
	std::filesystem::path plugin_dir = this->plugins_config_dir;
	plugin_dir.append(id);
	QSENDSYSTEMERRORIF(BrowseFile(plugin_dir));
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::GetPluginDirJson(CefRefPtr<CefRequest> request, std::string_view query) {
	QSTRING id;
	bool has_id = false;
	ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(id)
	});
	QREQPARAM(id);
	QSENDBADREQUESTIF(id.size() == 0);
	std::filesystem::path plugin_dir = this->plugins_data_dir;
	plugin_dir.append(id);
	plugin_dir.append("bolt.json");
	std::ifstream file(plugin_dir, std::ios::in | std::ios::binary);
	if (!file.fail()) {
		std::stringstream ss;
		ss << file.rdbuf();
		std::string str = ss.str();
		file.close();
		return new Browser::ResourceHandler(str, 200, "application/json");
	} else {
		QSENDNOTFOUND();
	}
}

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
