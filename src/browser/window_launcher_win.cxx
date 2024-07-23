#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include <iostream>
#include <shellapi.h>

#if defined(BOLT_PLUGINS)
#include "../library/dll/stub_inject.hxx"
#endif

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Deb(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "Elf binaries are not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Exe(CefRefPtr<CefRequest> request, std::string_view query) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	// parse query
	std::wstring hash;
	bool has_hash = false;
	std::wstring config_uri;
	bool has_config_uri = false;
	std::wstring jx_session_id;
	bool has_jx_session_id = false;
	std::wstring jx_character_id;
	bool has_jx_character_id = false;
	std::wstring jx_display_name;
	bool has_jx_display_name = false;
#if defined(BOLT_PLUGINS)
	bool plugin_loader = false;
#endif
	this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQCHECK(hash)
		PQCHECK(config_uri)
		PQCHECK(jx_session_id)
		PQCHECK(jx_character_id)
		PQCHECK(jx_display_name)
#if defined(BOLT_PLUGINS)
		PQBOOL(plugin_loader)
#endif
	});

	// if there was a "hash" in the query string, we need to save the new game exe and the new hash
	if (has_hash) {
		if (post_data == nullptr || post_data->GetElementCount() != 1) {
			// hash param must be accompanied by POST data containing the file it's a hash of,
			// so hash but no POST is a bad request
			const char* data = "Bad Request";
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
		}
		std::ofstream file(this->rs3_exe_path, std::ios::out | std::ios::binary);
		if (file.fail()) {
			const char* data = "Failed to save executable; if the game is already running, close it and try again\n";
			return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
		}
		CefPostData::ElementVector vec;
		post_data->GetElements(vec);
		size_t exe_size = vec[0]->GetBytesCount();
		unsigned char* exe = new unsigned char[exe_size];
		vec[0]->GetBytes(exe_size, exe);
		file.write((const char*)exe, exe_size);
		file.close();
		delete[] exe;
	}

	// create the command line for the new process
	std::wstring command_line = std::wstring(L"\"") + this->rs3_exe_path.wstring() + L"\"";
	if (has_config_uri) {
		command_line += L" --configURI \"";
		command_line += config_uri;
		command_line += L"\"";
	}

	// create the environment block for the new process
	std::wstring session_key = L"JX_SESSION_ID=";
	std::wstring character_key = L"JX_CHARACTER_ID=";
	std::wstring name_key = L"JX_DISPLAY_NAME=";
	LPWCH env = GetEnvironmentStringsW();
	size_t env_size = 0;
	while (env[env_size] != '\0') {
		while (env[env_size] != '\0') env_size += 1;
		env_size += 1;
	}
	const size_t current_process_env_size = env_size;
	if (has_jx_session_id) env_size += session_key.size() + jx_session_id.size() + 1;
	if (has_jx_character_id) env_size += character_key.size() + jx_character_id.size() + 1;
	if (has_jx_display_name) env_size += name_key.size() + jx_display_name.size() + 1;
	wchar_t* new_env = new wchar_t[env_size + 1];
	memcpy(&new_env[0], env, current_process_env_size * sizeof(*env));
	FreeEnvironmentStringsW(env);
	wchar_t* cursor = &new_env[current_process_env_size];
	if (has_jx_session_id) {
		memcpy(cursor, &session_key[0], session_key.size() * sizeof(session_key[0]));
		cursor += session_key.size();
		memcpy(cursor, &jx_session_id[0], jx_session_id.size() * sizeof(jx_session_id[0]));
		cursor += jx_session_id.size();
		*cursor = '\0';
		cursor += 1;
	}
	if (has_jx_character_id) {
		memcpy(cursor, &character_key[0], character_key.size() * sizeof(character_key[0]));
		cursor += character_key.size();
		memcpy(cursor, &jx_character_id[0], jx_character_id.size() * sizeof(jx_character_id[0]));
		cursor += jx_character_id.size();
		*cursor = '\0';
		cursor += 1;
	}
	if (has_jx_display_name) {
		memcpy(cursor, &name_key[0], name_key.size() * sizeof(name_key[0]));
		cursor += name_key.size();
		memcpy(cursor, &jx_display_name[0], jx_display_name.size() * sizeof(jx_display_name[0]));
		cursor += jx_display_name.size();
		*cursor = '\0';
		cursor += 1;
	}
	*cursor = '\0';

	PROCESS_INFORMATION pi;
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT;
#if defined(BOLT_PLUGINS)
	if (plugin_loader) creation_flags |= CREATE_SUSPENDED;
#endif

	if (!CreateProcessW(
		NULL,
		&command_line[0],
		NULL,
		NULL,
		false,
		creation_flags,
		new_env,
		NULL,
		&si,
		&pi
	)) {
		delete[] new_env;
		const DWORD error = GetLastError();
		const std::string error_message = "CreateProcess failed with error " + std::to_string(error) + "\n";
		return new ResourceHandler(error_message, 500, "text/plain");
	}
	delete[] new_env;

#if defined(BOLT_PLUGINS)
	if (plugin_loader) {
		InjectStub(pi.hProcess);
		ResumeThread(pi.hThread);
	}
#endif

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	if (has_hash) {
		std::wofstream file(this->rs3_exe_hash_path, std::ios::out | std::ios::binary);
		if (file.fail()) {
			const char* data = "OK, but unable to save hash file\n";
			return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
		}
		file << hash;
		file.close();
	}

	const char* data = "OK\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 200, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3App(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "Mac binaries are not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsExe(CefRefPtr<CefRequest> request, std::string_view query) {
	// TODO
	const char* data = ".exe is not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsApp(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "Mac binaries are not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query, bool configure) {
	const char* data = "JAR files not yet supported on Windows\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchHdosJar(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "JAR files not yet supported on Windows\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
}

void Browser::Launcher::OpenExternalUrl(char* u) const {
	const char* url = u;
	size_t size = mbsrtowcs(NULL, &url, 0, NULL);
	wchar_t* buf = new wchar_t[size + 1]();
	size = mbsrtowcs(buf, &url, size + 1, NULL);
	ShellExecuteW(0, 0, buf, 0, 0, SW_SHOW);
	delete[] buf;
}

int Browser::Launcher::BrowseData() const {
	// TODO
	return -1;
}
