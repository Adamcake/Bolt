#include <fmt/core.h>

#include "browser.hxx"
#include "browser/app.hxx"
#include "browser/client.hxx"

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__linux__)
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#if defined(CEF_X11)
#include <X11/Xlib.h>

int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
	fmt::print(
		"X error received: type {}, serial {}, error_code {}, request_code {}, minor_code {}\n",
		event->type,
		event->serial,
		event->error_code,
		event->request_code,
		event->minor_code
	);
	return 0;
}

int XIOErrorHandlerImpl(Display* display) {
	return 0;
}
#endif

int BoltRunBrowserProcess(CefMainArgs, CefRefPtr<Browser::App>);
bool LockConfigDirectory(std::filesystem::path&);

int BoltRunAnyProcess(CefMainArgs main_args) {
	// CefApp struct - this implements handlers used by multiple processes
	Browser::App cef_app_;
	CefRefPtr<Browser::App> cef_app = &cef_app_;

	// CEF applications have multiple sub-processes (render, GPU, etc) that share the same executable.
	// This function checks the command-line and, if this is a sub-process, executes the appropriate logic.
	int exit_code = CefExecuteProcess(main_args, cef_app, nullptr);
	if (exit_code >= 0) {
		return exit_code;
	}

	return BoltRunBrowserProcess(main_args, cef_app);
}

int BoltRunBrowserProcess(CefMainArgs main_args, CefRefPtr<Browser::App> cef_app) {
#if defined(CEF_X11)
	// X11 error handlers
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// find home directory
	std::filesystem::path config_dir;
	if (!LockConfigDirectory(config_dir)) {
		return 1;
	}

	// CefClient struct - central object for main thread, and implements lots of handlers for browser process
	Browser::Client client_(cef_app, config_dir);
	CefRefPtr<Browser::Client> client = &client_;

	// CEF settings - only set the ones we're interested in
	CefSettings settings = CefSettings();
	settings.log_severity = LOGSEVERITY_WARNING; // Print warnings and errors only
	settings.command_line_args_disabled = false; // Needed because we append args
	settings.uncaught_exception_stack_size = 8;  // Number of call stack frames given in unhandled exception events

	// Initialize CEF
	int exit_code = CefInitialize(main_args, settings, cef_app, nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting with error: CefInitialize exit_code {}\n", exit_code);
		return exit_code;
	}

	// Block on the CEF message loop until CefQuitMessageLoop() is called
	CefRunMessageLoop();
	
	return 0;
}

#if defined(__linux__)
int main(int argc, char* argv[]) {
	// Provide CEF with command-line arguments
	// Add a flag to disable web security, because a certain company's API endpoints don't have correct
	// access control settings; remove this setting if they ever get their stuff together
	const char* arg = "--disable-web-security";
	const size_t arg_len = strlen(arg);
	char* arg_ = new char[arg_len + 1];
	memcpy(arg_, arg, arg_len);
	arg_[arg_len] = '\0';
	char** argv_ = new char*[argc + 1];
	memcpy(argv_, argv, argc * sizeof(char*));
	argv_[argc] = arg_;

	CefMainArgs main_args(argc + 1, argv_);
	int ret = BoltRunAnyProcess(main_args);

	CefShutdown();
	delete[] argv_;
	delete[] arg_;
	return ret;
}

bool LockConfigDirectory(std::filesystem::path& path) {
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	const char* home = getenv("HOME");
	if (xdg_config_home) {
		path.assign(xdg_config_home);
	} else if (home) {
		path.assign(home);
		path.append(".config");
	} else {
		fmt::print("No $XDG_CONFIG_HOME or $HOME\n");
		return false;
	}
	path.append("bolt-launcher");
	std::error_code err;
	std::filesystem::create_directories(path, err);
	if (err.value() != 0) {
		fmt::print("Could not create directories (error {}) {}\n", err.value(), path.c_str());
		return false;
	}

	std::filesystem::path fpath = path;
	fpath.append("lock");
	int lockfile = open(fpath.c_str(), O_WRONLY | O_CREAT, 0666);
	if (flock(lockfile, LOCK_EX | LOCK_NB)) {
		fmt::print("Failed to obtain lockfile; is the program already running?\n");
		return false;
	} else {
		return true;
	}
}
#endif

#if defined(_WIN32)
int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nCmdShow) {
	// Provide CEF with command-line arguments
	// Re-run with --disable-web-security if it's not already there, because a certain company's API endpoints
	// don't have correct access control settings; remove this setting if they ever get their stuff together
	int argc;
	LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
	if (argv == nullptr) {
		fmt::print("Exiting with error: CommandLineToArgvW failed\n");
		return 1;
	}
	const wchar_t* const arg = L"--disable-web-security";
	const wchar_t* subp_arg = L"--type=";
	bool has_arg = false;
	bool is_subprocess = false;
	for(size_t i = 0; i < argc; i += 1) {
		// check if arg is --disable-web-security
		if (wcsstr(argv[i], arg)) {
			has_arg = true;
		}
		// check if arg begins with --type=
		if (wcsncmp(argv[i], subp_arg, wcslen(subp_arg)) == 0) {
			is_subprocess = true;
			break;
		}
	}
	LocalFree(argv);

	if (is_subprocess) {
		int ret = BoltRunAnyProcess(CefMainArgs(hInstance));
		CefShutdown();
		return ret;
	} else if (has_arg) {
		// CefApp struct - this implements handlers used by multiple processes
		Browser::App cef_app_;
		CefRefPtr<Browser::App> cef_app = &cef_app_;
		int ret = BoltRunBrowserProcess(CefMainArgs(hInstance), cef_app);
		CefShutdown();
		return ret;
	} else {
		wchar_t module_name[1024];
		GetModuleFileNameW(NULL, module_name, 1024);
		std::wstring command_line(lpCmdLine);
		command_line += std::wstring(L" ");
		command_line += std::wstring(arg);
		PROCESS_INFORMATION process_info;
		STARTUPINFOW startup_info;
		GetStartupInfoW(&startup_info);
		CreateProcessW(module_name, command_line.data(), NULL, NULL, 0, 0, NULL, NULL, &startup_info, &process_info);
		return 0;
	}
}
#endif
