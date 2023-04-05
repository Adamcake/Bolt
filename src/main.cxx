#include <fmt/core.h>

#include "browser.hxx"
#include "browser/app.hxx"
#include "browser/client.hxx"

#if defined(OS_WIN)
#include <windows.h>
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

int main(int argc, char* argv[]) {
	// Provide CEF with command-line arguments
	CefMainArgs main_args(argc, argv);

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
	client->SetupNative();

#if defined(CEF_X11)
	// X11 error handlers
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// Parse command-line arguments
	CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
#if defined(OS_WIN)
	command_line->InitFromString(::GetCommandLineW());
#else
	command_line->InitFromArgv(argc, argv);
#endif

	// CEF settings - only set the ones we're interested in
	CefSettings settings = CefSettings();
	settings.log_severity = LOGSEVERITY_WARNING; // Print warnings and errors only
	//settings.external_message_pump = true;     // Allows us to integrate CEF's event loop into our own
	settings.command_line_args_disabled = true;  // We don't want our command-line to configure CEF's windows
	settings.uncaught_exception_stack_size = 8;  // Number of call stack frames given in unhandled exception events

	// Initialize CEF
	exit_code = CefInitialize(main_args, settings, cef_app, nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting with error: cef_initialize exit_code {}\n", exit_code);
		return exit_code;
	}

	// Run the CEF message loop
	// TODO: later this will be replaced with an OS-specific event loop capable of calling
	// cef_do_message_loop_work() in response to CEF's "message pump"
	CefRunMessageLoop();

	// Shutdown and return
	CefShutdown();
	client->CloseNative();
	return 0;
}
