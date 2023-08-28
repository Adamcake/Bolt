#include <filesystem>
#include <fmt/core.h>
#include <gtk/gtk.h>

#include "browser.hxx"
#include "browser/app.hxx"
#include "browser/client.hxx"

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__linux__)
#include <fcntl.h>
#include <functional>
#include <sys/file.h>
#include <sys/stat.h>
#include <thread>
#endif

#if defined(CEF_X11)
#include <X11/Xlib.h>

constexpr int img_size = 24;
constexpr int img_bps = 8;

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

void GtkStart(CefMainArgs&, Browser::Client*);
int BoltRunBrowserProcess(CefMainArgs, CefRefPtr<Browser::App>);
bool LockXdgDirectories(std::filesystem::path&, std::filesystem::path&);

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
	if (!LockXdgDirectories(config_dir, data_dir)) {
		return 1;
	}

	// CefClient struct - central object for main thread, and implements lots of handlers for browser process
	CefRefPtr<Browser::Client> client = new Browser::Client(cef_app, config_dir, data_dir);

	// CEF settings - only set the ones we're interested in
	CefSettings settings = CefSettings();
	settings.log_severity = LOGSEVERITY_WARNING; // Print warnings and errors only
	settings.command_line_args_disabled = false; // Needed because we append args
	settings.uncaught_exception_stack_size = 8;  // Number of call stack frames given in unhandled exception events

	// Give CEF a place to put its cache stuff - default location is next to the exe which is not OK
	std::filesystem::path cef_cache_path = data_dir;
	cef_cache_path.append("CefCache");
	std::filesystem::create_directories(cef_cache_path);
	cef_string_from_utf8(cef_cache_path.c_str(), strlen(cef_cache_path.c_str()), &settings.cache_path);

	// Initialize CEF
	int exit_code = CefInitialize(main_args, settings, cef_app, nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting with error: CefInitialize exit_code {}\n", exit_code);
		return exit_code;
	}

	// start gtk and create a tray icon
	GtkStart(main_args, client.get());

#if defined(CEF_X11)
	// X11 error handlers - must be installed after gtk_init
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// Block on the CEF message loop until CefQuitMessageLoop() is called
	CefRunMessageLoop();
	CefShutdown();
	
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

	delete[] argv_;
	delete[] arg_;
	return ret;
}

bool LockXdgDirectories(std::filesystem::path& config_dir, std::filesystem::path& data_dir) {
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	const char* xdg_data_home = getenv("XDG_DATA_HOME");
	const char* home = getenv("HOME");

	if (xdg_data_home && *xdg_data_home) {
		data_dir.assign(xdg_data_home);
	} else if (home) {
		data_dir.assign(home);
		data_dir.append(".local");
		data_dir.append("share");
	} else {
		fmt::print("No $XDG_DATA_HOME or $HOME\n");
		return false;
	}

	if (xdg_config_home && *xdg_config_home) {
		config_dir.assign(xdg_config_home);
	} else if (home) {
		config_dir.assign(home);
		config_dir.append(".config");
	} else {
		fmt::print("No $XDG_CONFIG_HOME or $HOME\n");
		return false;
	}

	config_dir.append("bolt-launcher");
	data_dir.append("bolt-launcher");
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

	std::filesystem::path data_lock = data_dir;
	data_lock.append("lock");
	int lockfile = open(data_lock.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666);
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

// all these features exist in gtk3 and are deprecated for no reason at all, so ignore deprecation warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static void TrayPopupMenu(GtkStatusIcon* status_icon, guint button, guint32 activate_time, gpointer popup_menu) {
    gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}

static void TrayActivate(GtkStatusIcon* status_icon, gpointer popup_menu) {
	TrayPopupMenu(status_icon, gtk_get_current_event()->button.button, gtk_get_current_event_time(), popup_menu);
};

static void TrayOpenLauncher(GtkMenuItem*, Browser::Client* client) {
    client->OpenLauncher();
}

static void TrayExit(GtkMenuItem*, Browser::Client* client) {
    client->Exit();
}

void GtkStart(CefMainArgs& args, Browser::Client* client) {
	gdk_set_allowed_backends("x11");
	gtk_init(&args.argc, &args.argv);
	GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
		client->GetTrayIcon(),
		GDK_COLORSPACE_RGB,
		true,
		img_bps,
		img_size,
		img_size,
		img_size * 4,
		[](unsigned char*, void*){},
		nullptr
	);
	GtkStatusIcon* icon = gtk_status_icon_new_from_pixbuf(pixbuf);
	gtk_status_icon_set_tooltip_text(icon, "Bolt Launcher");
	gtk_status_icon_set_title(icon, "Bolt Launcher");
	GtkWidget* menu = menu = gtk_menu_new();
	GtkWidget* menu_open = gtk_menu_item_new_with_label("Open");
	GtkWidget* menu_exit = gtk_menu_item_new_with_label("Exit");
	g_signal_connect(G_OBJECT(menu_open), "activate", G_CALLBACK(TrayOpenLauncher), client);
	g_signal_connect(G_OBJECT(menu_exit), "activate", G_CALLBACK(TrayExit), client);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_open);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_exit);
	gtk_widget_show_all (menu);
	g_signal_connect(GTK_STATUS_ICON(icon), "activate", G_CALLBACK(TrayActivate), menu);
	g_signal_connect(GTK_STATUS_ICON(icon), "popup-menu", G_CALLBACK(TrayPopupMenu), menu);
	GtkWidget* menu_bar = gtk_menu_bar_new();
	GtkWidget* menu_item_top_level = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item_top_level);
	GtkWidget* main_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item_top_level), main_menu);
}
#pragma GCC diagnostic pop
