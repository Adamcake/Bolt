#include <fmt/core.h>

#include "browser.hxx"
#include "browser/app.hxx"
#include "browser/client.hxx"

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

	// CefApp struct - this implements handlers used by multiple processes
	Browser::App cef_app_;
	CefRefPtr<Browser::App> cef_app = &cef_app_;

	// CEF applications have multiple sub-processes (render, GPU, etc) that share the same executable.
	// This function checks the command-line and, if this is a sub-process, executes the appropriate logic.
	int exit_code = CefExecuteProcess(main_args, cef_app, nullptr);
	if (exit_code >= 0) {
		return exit_code;
	}

	// CefClient struct - central object for main thread, and implements lots of handlers for browser process
	Browser::Client client_(cef_app);
	CefRefPtr<Browser::Client> client = &client_;

#if defined(CEF_X11)
	// X11 error handlers
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// CEF settings - only set the ones we're interested in
	CefSettings settings = CefSettings();
	settings.log_severity = LOGSEVERITY_WARNING; // Print warnings and errors only
	settings.command_line_args_disabled = false; // Needed because we append args
	settings.uncaught_exception_stack_size = 8;  // Number of call stack frames given in unhandled exception events

	// Initialize CEF
	exit_code = CefInitialize(main_args, settings, cef_app, nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting with error: CefInitialize exit_code {}\n", exit_code);
		return exit_code;
	}

	// Block on the CEF message loop until CefQuitMessageLoop() is called
	CefRunMessageLoop();

	// Shutdown and return
	CefShutdown();
	delete[] argv_;
	delete[] arg_;
	return 0;
}
