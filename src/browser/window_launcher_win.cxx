#include "window_launcher.hxx"
#include "resource_handler.hxx"
#include "request.hxx"

#include <iostream>
#include <shellapi.h>
#include <fmt/core.h>
#include <fmt/xchar.h>

#if defined(BOLT_PLUGINS)
#include "../library/dll/stub_inject.h"
#endif

/**
 * Attempts to find the java executable in the given `java_home` directory, or
 * in the PATH environment variable if `java_home` is null. If the executable is
 * found, the path to the executable is written to `out` and the function returns
 * true. If the executable is not found, the function returns false.
 * 
 * Javaw is used instead of java because it doesn't open a console window.
 * 
 * @param java_home The path to the jdk/jre directory to search for the java executable.
 * @param out A reference to a string that will be written to if the executable is found.
 * 
 * @return True if the executable was found, false otherwise.
 */
static bool FindJava(const wchar_t* java_home, std::wstring& out) {
	if (java_home) {
		std::filesystem::path java(java_home);
		java.append("bin");
		java.append("javaw.exe");
		if (std::filesystem::exists(java)) {
			out = java;
			return true;
		}
	}
	const char* path = getenv("PATH");
	while (true) {
		const char* next_semicolon = strchr(path, ';');
		const bool is_last = next_semicolon == nullptr;
		std::string_view path_item = is_last ? std::string_view(path) : std::string_view(path, (size_t)(next_semicolon - path));
		std::filesystem::path java(path_item);
		java.append("javaw.exe");
		if (std::filesystem::exists(java)) {
			out = java;
			return true;
		}
		if (is_last) break;
		path = next_semicolon + 1;
	}
	return false;
}

/**
 * Creates a new environment block by adding the given additional environment
 * variables to the current environment block.
 * 
 * The caller is responsible for freeing the memory allocated for the new environment block.
 * 
 * @param additional_env_vars A map of additional environment variables to add to the environment block.
 * 
 * @return A pointer to a new environment block containing the additional environment variables. Null if the function fails.
 */
static wchar_t* CreateEnvironmentString(const std::map<std::wstring, std::wstring>& additional_env_vars) {
	wchar_t* currentEnv = GetEnvironmentStringsW();
	if (currentEnv == nullptr) {
		return nullptr;
	}

	std::vector<wchar_t> updatedEnvBlock;

	wchar_t* envPtr = currentEnv;
	while (*envPtr) {
		size_t len = wcslen(envPtr) + 1;
		updatedEnvBlock.insert(updatedEnvBlock.end(), envPtr, envPtr + len);
		envPtr += len;
	}

	FreeEnvironmentStringsW(currentEnv);

	for (const auto& [key, value] : additional_env_vars) {
		std::wstring envVar = key + L"=" + value;
		updatedEnvBlock.insert(updatedEnvBlock.end(), envVar.begin(), envVar.end());
		updatedEnvBlock.push_back(L'\0');
	}

	// Append the final null terminator for the environment block. Environment blocks are double-null terminated.
	updatedEnvBlock.push_back(L'\0');

	wchar_t* resultEnv = new wchar_t[updatedEnvBlock.size()];
	std::copy(updatedEnvBlock.begin(), updatedEnvBlock.end(), resultEnv);

	return resultEnv;
}


/**
 * Launches a java process using the java executable at `java` with the given
 * jvm arguments, running the jar at `jar_path` with the given application
 * arguments, and with the working directory set to `working_dir`.
 *
 * @param java The path to the jdk/jre java executable.
 * @param jar_path The path to the jar file to be executed.
 * @param working_dir The working directory for the new process.
 * @param jvm_args The JVM arguments to be used. K/V pairs with nullopt values are treated as flags.
 * @param application_args The arguments to be passed to the application. K/V pairs with nullopt values are treated as flags.
 * @param env_vars The environment variables to be set for the new process
 * 
 * @return An exit code for the new process. ULONG_MAX if the env vars could not be created.
 */
static DWORD LaunchJavaProcess(
	const std::wstring& java, 
	const std::wstring& jar_path,
	const std::wstring& working_dir,
	const std::map<std::wstring, std::optional<std::wstring>>& jvm_args, 
	const std::map<std::wstring, std::optional<std::wstring>>& application_args, 
	const std::map<std::wstring, std::wstring>& env_vars
) {
	std::wstring jvm_args_string;
	for (const auto& [key, maybe_value] : jvm_args) {
		jvm_args_string += key;
		if (maybe_value.has_value()) jvm_args_string += L"=" + maybe_value.value();
		jvm_args_string += L" ";
	}

	std::wstring application_args_string;
	for (const auto& [key, maybe_value] : application_args) {
		application_args_string += key;
		if(maybe_value.has_value()) application_args_string += L"=" + maybe_value.value();
		application_args_string += L" ";
	}

	LPWSTR env_str(CreateEnvironmentString(env_vars));
	if (env_str == nullptr) {
		fmt::println(stderr, "Failed to create environment block for new process");
		return ULONG_MAX;
	}

	const std::wstring command = std::format(L"\"{}\" {} -jar \"{}\" {}", java, jvm_args_string, jar_path, application_args_string);
	
	fmt::print(L"Launching Java process with command: {}", command);

	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(STARTUPINFOW);

	DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT;
	
	// Copy the command, just in case CreateProcessW modifies it. This will help us log it later on.
	std::wstring mutable_command = command;
	bool create_process_result = CreateProcessW(
		java.c_str(), 
		mutable_command.data(), 
		NULL, 
		NULL, 
		FALSE, 
		creation_flags, 
		(LPVOID) env_str, 
		working_dir.c_str(), 
		&si, 
		&pi
	);

	if (!create_process_result) {
		DWORD err = GetLastError();
		fmt::println(stderr, L"CreateProcess failed with error: {}\nCommand: {}\nWorking Directory: {}", err, command, working_dir);
		delete[] env_str;
		return err;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	std::wcout << L"Successfully spawned Java process with PID " << pi.dwProcessId << std::endl;

	delete[] env_str;
	return 0;
}

static bool WriteStringToFile(const std::filesystem::path path, const std::wstring& data) {
    std::wofstream file(path, std::ios::out | std::ios::binary);

    if (file.fail()) {
        return false;
    }

    file << data;
    file.close();

	return true;
}

static bool WriteBytesToFile(const std::filesystem::path& path, const unsigned char* data, size_t size) {
    std::ofstream file(path, std::ios::out | std::ios::binary);

    if (file.fail()) {
        return false;
    }

    file.write((const char*)data, size);
    file.close();
    return true;
}


CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Deb(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR("Elf binaries are not supported on this platform", 400);
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
		QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);
		CefPostData::ElementVector vec;
		post_data->GetElements(vec);
		size_t exe_size = vec[0]->GetBytesCount();
		std::unique_ptr<unsigned char[]> exe(new unsigned char[exe_size]);
		vec[0]->GetBytes(exe_size, exe.get());
		if (!WriteBytesToFile(this->rs3_exe_path, exe.get(), exe_size)) {
			QSENDSTR("Failed to save executable; if the game is already running, close it and try again", 500);
		}
	}

	// create the command line for the new process
	std::wstring command_line = std::wstring(L"\"") + this->rs3_exe_path.wstring() + L"\"";
	if (has_config_uri) {
		command_line += L" --configURI \"";
		command_line += config_uri;
		command_line += L"\"";
	}

	// create the environment block for the new process
	std::map<std::wstring, std::wstring> env_vars;
	if (has_jx_session_id) env_vars[L"JX_SESSION_ID"] = jx_session_id;
	if (has_jx_character_id) env_vars[L"JX_CHARACTER_ID"] = jx_character_id;
	if (has_jx_display_name) env_vars[L"JX_DISPLAY_NAME"] = jx_display_name;
	LPWSTR new_env = CreateEnvironmentString(env_vars);

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
	if (has_hash && !WriteStringToFile(this->rs3_exe_hash_path, hash)) {
		QSENDSTR("OK, but unable to save hash file", 200);
	}
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3App(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR("Mac binaries are not supported on this platform", 400);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsExe(CefRefPtr<CefRequest> request, std::string_view query) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	// parse query
	std::wstring hash;
	bool has_hash = false;
	std::wstring jx_session_id;
	bool has_jx_session_id = false;
	std::wstring jx_character_id;
	bool has_jx_character_id = false;
	std::wstring jx_display_name;
	bool has_jx_display_name = false;
	this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(hash)
		PQSTRING(jx_session_id)
		PQSTRING(jx_character_id)
		PQSTRING(jx_display_name)
	});

	// if there was a "hash" in the query string, we need to save the new game exe and the new hash
	if (has_hash) {
		QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);
		CefPostData::ElementVector vec;
		post_data->GetElements(vec);
		size_t exe_size = vec[0]->GetBytesCount();
		std::unique_ptr<unsigned char[]> exe(new unsigned char[exe_size]);
		vec[0]->GetBytes(exe_size, exe.get());
		if (!WriteBytesToFile(this->osrs_exe_path, exe.get(), exe_size)) {
			QSENDSTR("Failed to save executable; if the game is already running, close it and try again", 500);
		}
	}

	// create the command line for the new process
	std::wstring command_line = std::wstring(L"\"") + this->osrs_exe_path.wstring() + L"\"";

	// create the environment block for the new process
	std::map<std::wstring, std::wstring> env_vars;
	if (has_jx_session_id) env_vars[L"JX_SESSION_ID"] = jx_session_id;
	if (has_jx_character_id) env_vars[L"JX_CHARACTER_ID"] = jx_character_id;
	if (has_jx_display_name) env_vars[L"JX_DISPLAY_NAME"] = jx_display_name;
	LPWSTR new_env = CreateEnvironmentString(env_vars);

	PROCESS_INFORMATION pi;
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	if (!CreateProcessW(
		NULL,
		&command_line[0],
		NULL,
		NULL,
		false,
		CREATE_UNICODE_ENVIRONMENT,
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

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	if (has_hash && !WriteStringToFile(this->osrs_exe_hash_path, hash)) {
		QSENDSTR("OK, but unable to save hash file", 200);
	}
	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsApp(CefRefPtr<CefRequest> request, std::string_view query) {
	QSENDSTR("Mac binaries are not supported on this platform", 400);
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query, bool configure) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	const wchar_t* java_home = _wgetenv(L"JAVA_HOME");
	std::wstring java;
	if (!FindJava(java_home, java)) {
		if (java_home && *java_home) {
			QSENDSTR("Couldn't find Java: JAVA_HOME does not point to a Java binary (is it installed correctly?)", 400);
		} else {
			QSENDSTR("Couldn't find Java: please install Java and try again", 400);
		}
	}

	std::wstring jar_path, id, jx_session_id, jx_character_id, jx_display_name;
	bool 
		has_jar_path = false,
		has_id = false,
		has_jx_session_id = false,
		has_jx_character_id = false,
		has_jx_display_name = false;

	this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
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
	}
	else {
		rl_path = this->runelite_path;
		
		if (has_id) {
			QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);

			CefPostData::ElementVector vec;
			post_data->GetElements(vec);

			size_t jar_size = vec[0]->GetBytesCount();
			std::unique_ptr<unsigned char[]> jar(new unsigned char[jar_size]);
			vec[0]->GetBytes(jar_size, jar.get());

			if (!WriteBytesToFile(rl_path, jar.get(), jar_size)) {
				QSENDSTR("Failed to save JAR; if the game is already running, close it and try again", 500);
			}
		}
	}

	std::map<std::wstring, std::optional<std::wstring>> jvm_args, application_args;
	std::map<std::wstring, std::wstring> env_vars;

	if (has_jx_session_id) env_vars[L"JX_SESSION_ID"] = jx_session_id;
	if (has_jx_character_id) env_vars[L"JX_CHARACTER_ID"] = jx_character_id;
	if (has_jx_display_name) env_vars[L"JX_DISPLAY_NAME"] = jx_display_name;

	if(configure) application_args[L"--configure"] = std::nullopt;

	DWORD exit_code = LaunchJavaProcess(java, rl_path, this->data_dir, jvm_args, application_args, env_vars);
	if (exit_code != 0) {
		const std::string error_string = exit_code == ULONG_MAX ? "Failed to create environment block for new process" : "CreateProcess failed with error " + std::to_string(exit_code);
		const std::string error_message = error_string + "\n";
		return new ResourceHandler(error_message, 500, "text/plain");
	}

	// Save a file with the version number if it was provided
	if (has_id && !WriteStringToFile(this->runelite_id_path, id)) {
		QSENDSTR("OK, but unable to save version file", 200);
	}

	QSENDOK();
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchHdosJar(CefRefPtr<CefRequest> request, std::string_view query) {
	const CefRefPtr<CefPostData> post_data = request->GetPostData();

	const wchar_t* java_home = _wgetenv(L"JAVA_HOME");
	std::wstring java;
	if (!FindJava(java_home, java)) {
		if (java_home && *java_home) {
			QSENDSTR("Couldn't find Java: JAVA_HOME does not point to a Java binary (is it installed correctly?)", 400);
		} else {
			QSENDSTR("Couldn't find Java: please install Java and try again", 400);
		}
	}

	std::wstring version, jx_session_id, jx_character_id, jx_display_name;
	bool 
		has_version = false,
		has_jx_session_id = false,
		has_jx_character_id = false,
		has_jx_display_name = false;

	this->ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
		PQSTRING(version)
		PQSTRING(jx_session_id)
		PQSTRING(jx_character_id)
		PQSTRING(jx_display_name)
	});

	// If version is present in the query string, then we need to save the JAR file.
	if (has_version) {
		QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);
		
		CefPostData::ElementVector vec;
		post_data->GetElements(vec);

		size_t jar_size = vec[0]->GetBytesCount();
		std::unique_ptr<unsigned char[]> jar(new unsigned char[jar_size]);
		vec[0]->GetBytes(jar_size, jar.get());

		if(!WriteBytesToFile(this->hdos_path, jar.get(), jar_size)) {
			QSENDSTR("Failed to save JAR; if the game is already running, close it and try again", 500);
		}
	}

	std::map<std::wstring, std::optional<std::wstring>> jvm_args, application_args;
	std::map<std::wstring, std::wstring> env_vars;

	if (has_jx_session_id) env_vars[L"JX_SESSION_ID"] = jx_session_id;
	if (has_jx_character_id) env_vars[L"JX_CHARACTER_ID"] = jx_character_id;
	if (has_jx_display_name) env_vars[L"JX_DISPLAY_NAME"] =  jx_display_name;

	DWORD exit_code = LaunchJavaProcess(java, this->hdos_path, this->data_dir, jvm_args, application_args, env_vars);
	if (exit_code != 0) {
		const std::string error_string = exit_code == ULONG_MAX ? "Failed to create environment block for new process" : "CreateProcess failed with error " + std::to_string(exit_code);
		const std::string error_message = error_string + "\n";
		return new ResourceHandler(error_message, 500, "text/plain");
	}

	// Save a file with the version number if it was provided
	if (has_version && !WriteStringToFile(this->hdos_version_path, version)) {
		QSENDSTR("OK, but unable to save version file", 200);
	}

	QSENDOK();
}

void Browser::Launcher::OpenExternalUrl(char* u) const {
	const char* url = u;
	size_t size = mbsrtowcs(NULL, &url, 0, NULL);
	wchar_t* buf = new wchar_t[size + 1]();
	size = mbsrtowcs(buf, &url, size + 1, NULL);
	ShellExecuteW(0, 0, buf, 0, 0, SW_SHOW);
	delete[] buf;
}

bool Browser::Launcher::BrowseData() const {
	// MSDN: "If [ShellExecuteW] succeeds, it returns a value greater than 32."
	const uintptr_t ret = reinterpret_cast<uintptr_t>(ShellExecuteW(NULL, L"explore", this->data_dir.c_str(), NULL, NULL, SW_SHOW));
	if (ret > 32) return true;
	fmt::println(stderr, "BrowseData failed: ShellExecuteW returned {}", ret);
	return false;
}
