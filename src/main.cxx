#if defined(_WIN32)
#if defined(BOLT_PLUGINS)
#include <winsock2.h>
#include <afunix.h>
#define close closesocket
#define SHUT_RDWR SD_BOTH
#endif
#include <windows.h>
#elif defined(BOLT_PLUGINS)
#include <sys/un.h>
#include <sys/socket.h>
#include "library/ipc.h"
#endif

#include <filesystem>
#include <fmt/core.h>

#include "browser.hxx"
#include "browser/app.hxx"
#include "browser/client.hxx"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
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

constexpr int img_size = 24;
constexpr int img_bps = 8;

#define PROGRAM_DIRNAME "bolt-launcher"

int BoltRunBrowserProcess(CefMainArgs, CefRefPtr<Browser::App>);
bool LockXdgDirectories(std::filesystem::path&, std::filesystem::path&, std::filesystem::path&);
void BoltFailToObtainLockfile(std::filesystem::path tempdir);

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
	// find and lock the xdg base directories (or an approximation of them on Windows)
	std::filesystem::path config_dir;
	std::filesystem::path data_dir;
	std::filesystem::path runtime_dir;
	if (!LockXdgDirectories(config_dir, data_dir, runtime_dir)) {
		return 1;
	}

	// CefClient struct - central object for main thread, and implements lots of handlers for browser process
	CefRefPtr<Browser::Client> client = new Browser::Client(cef_app, config_dir, data_dir, runtime_dir);

	// CEF settings - only set the ones we're interested in
	CefSettings settings = CefSettings();
	settings.log_severity = LOGSEVERITY_WARNING; // Print warnings and errors only
	settings.command_line_args_disabled = false; // Needed because we append args
	settings.uncaught_exception_stack_size = 8;  // Number of call stack frames given in unhandled exception events
#if defined(BOLT_PLUGINS)
	settings.windowless_rendering_enabled = true; // Needed for embedded plugin browsers
#endif

	// Give CEF a place to put its cache stuff - default location is next to the exe which is not OK
	std::filesystem::path cef_cache_path = data_dir;
	cef_cache_path.append("CefCache");
	std::filesystem::create_directories(cef_cache_path);
#if defined(_WIN32)
	const wchar_t* cache_path = cef_cache_path.c_str();
	cef_string_from_wide(cache_path, wcslen(cache_path), &settings.cache_path);
#else
	const char* cache_path = cef_cache_path.c_str();
	cef_string_from_utf8(cache_path, strlen(cache_path), &settings.cache_path);
#endif

	// Initialize CEF
	int exit_code = CefInitialize(main_args, settings, cef_app, nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting with error: CefInitialize exit_code {}\n", exit_code);
		return exit_code;
	}

#if defined(CEF_X11)
	// X11 error handlers
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// Block on the CEF message loop until CefQuitMessageLoop() is called
	CefRunMessageLoop();
	CefShutdown();
	
	return 0;
}

// called after we fail to obtain the lockfile, likely meaning an instance of Bolt is already running
void BoltFailToObtainLockfile(std::filesystem::path tempdir) {
	tempdir.append("ipc-0");
#if defined(BOLT_PLUGINS)
#if defined(_WIN32)
	WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
	std::string ipc_path_str = tempdir.string();
	const char* ipc_path = ipc_path_str.c_str();
#else
	const char* ipc_path = tempdir.c_str();
#endif
	struct sockaddr_un addr = {.sun_family = AF_UNIX};
    int fd = socket(addr.sun_family, SOCK_STREAM, 0);
	strncpy(addr.sun_path, ipc_path, sizeof(addr.sun_path));
    if (connect(fd, (const struct sockaddr*)&addr, sizeof(addr)) != -1) {
		struct BoltIPCMessageToHost message {.message_type = IPC_MSG_DUPLICATEPROCESS};
        bool success = !_bolt_ipc_send(fd, &message, sizeof(message));
		shutdown(fd, SHUT_RDWR);
		close(fd);
		if (success) return;
    }
#endif
	fmt::print("Failed to obtain lockfile; is the program already running?\n");
}

#if defined(__linux__)
int main(int argc, char* argv[]) {
	CefMainArgs main_args(argc, argv);
	int ret = BoltRunAnyProcess(main_args);
	return ret;
}

bool LockXdgDirectories(std::filesystem::path& config_dir, std::filesystem::path& data_dir, std::filesystem::path& runtime_dir) {
	// As well as trying to obtain a lockfile, this function will also explicitly set the XDG_*_HOME
	// env vars to their defaults if they're not set. This avoids issues later down the line with
	// needing to spoof $HOME for certain games.
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	const char* xdg_data_home = getenv("XDG_DATA_HOME");
	const char* xdg_cache_home = getenv("XDG_CACHE_HOME");
	const char* xdg_state_home = getenv("XDG_STATE_HOME");
	const char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
	const char* home = getenv("HOME");

	if (!home || !*home) {
		fmt::print("No $HOME set, please set one\n");
		return false;
	}

	if (xdg_data_home && *xdg_data_home) {
		data_dir.assign(xdg_data_home);
	} else {
		data_dir.assign(home);
		data_dir.append(".local");
		data_dir.append("share");
		setenv("XDG_DATA_HOME", data_dir.c_str(), true);
	}

	if (xdg_config_home && *xdg_config_home) {
		config_dir.assign(xdg_config_home);
	} else {
		config_dir.assign(home);
		config_dir.append(".config");
		setenv("XDG_CONFIG_HOME", config_dir.c_str(), true);
	}

	if (!xdg_cache_home || !*xdg_cache_home) {
		std::filesystem::path dir = home;
		dir.append(".cache");
		setenv("XDG_CACHE_HOME", dir.c_str(), true);
	}

	if (!xdg_state_home || !*xdg_state_home) {
		std::filesystem::path dir = home;
		dir.append(".local");
		dir.append("state");
		setenv("XDG_STATE_HOME", dir.c_str(), true);
	}

	if (xdg_runtime_dir && *xdg_runtime_dir) {
		runtime_dir.assign(xdg_runtime_dir);
	} else {
		runtime_dir.assign("/tmp");
	}

	config_dir.append(PROGRAM_DIRNAME);
	data_dir.append(PROGRAM_DIRNAME);
	runtime_dir.append(PROGRAM_DIRNAME);
	std::error_code err;
	std::filesystem::create_directories(config_dir, err);
	if (err.value() != 0) {
		fmt::print("Could not create config directories (error {}) {}\n", err.value(), data_dir.c_str());
		return false;
	}
	std::filesystem::create_directories(data_dir, err);
	if (err.value() != 0) {
		fmt::print("Could not create data directories (error {}) {}\n", err.value(), data_dir.c_str());
		return false;
	}
	std::filesystem::create_directories(runtime_dir, err);
	if (err.value() != 0) {
		fmt::print("Could not create runtime directory (error {}) {}\n", err.value(), runtime_dir.c_str());
		return false;
	}

	std::filesystem::path data_lock = runtime_dir;
	data_lock.append("lock");
	int lockfile = open(data_lock.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (flock(lockfile, LOCK_EX | LOCK_NB)) {
		BoltFailToObtainLockfile(runtime_dir);
		return false;
	} else {
		return true;
	}
}
#endif

#if defined(_WIN32)
int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nCmdShow) {
	setlocale(LC_CTYPE, "");
	
	// Provide CEF with command-line arguments
	// Re-run with --disable-web-security if it's not already there, because a certain company's API endpoints
	// don't have correct access control settings; remove this setting if they ever get their stuff together
	int argc;
	LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
	if (argv == nullptr) {
		fmt::print("Exiting with error: CommandLineToArgvW failed\n");
		return 1;
	}
	const wchar_t* subp_arg = L"--type=";
	bool is_subprocess = false;
	for(size_t i = 0; i < argc; i += 1) {
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
	} else {
		// CefApp struct - this implements handlers used by multiple processes
		Browser::App cef_app_;
		CefRefPtr<Browser::App> cef_app = &cef_app_;
		int ret = BoltRunBrowserProcess(CefMainArgs(hInstance), cef_app);
		CefShutdown();
		return ret;
	}
}

bool LockXdgDirectories(std::filesystem::path& config_dir, std::filesystem::path& data_dir, std::filesystem::path& runtime_dir) {
	const char* appdata = getenv("appdata");
	std::filesystem::path appdata_dir;
	if (appdata && *appdata) {
		appdata_dir.assign(appdata);
		appdata_dir.append(PROGRAM_DIRNAME);
	} else {
		fmt::print("No appdata in environment\n");
		return false;
	}

	config_dir = appdata_dir;
	config_dir.append("config");
	data_dir = appdata_dir;
	data_dir.append("data");
	runtime_dir = appdata_dir;
	runtime_dir.append("run");

	std::error_code err;
	std::filesystem::create_directories(config_dir, err);
	if (err.value() != 0) {
		fmt::print("Could not create config directories (error {}) {}/{}\n", err.value(), appdata, PROGRAM_DIRNAME);
		return false;
	}
	std::filesystem::create_directories(data_dir, err);
	if (err.value() != 0) {
		fmt::print("Could not create data directories (error {}) {}/{}\n", err.value(), appdata, PROGRAM_DIRNAME);
		return false;
	}
	std::filesystem::create_directories(runtime_dir, err);
	if (err.value() != 0) {
		fmt::print("Could not create runtime directory (error {}) {}/{}\n", err.value(), appdata, PROGRAM_DIRNAME);
		return false;
	}

	std::filesystem::path data_lock = appdata_dir;
	data_lock.append("lock");
	HANDLE lockfile = CreateFileW(data_lock.c_str(), GENERIC_READ, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (lockfile == INVALID_HANDLE_VALUE) {
		BoltFailToObtainLockfile(runtime_dir);
		return false;
	} else {
		return true;
	}
}
#endif
