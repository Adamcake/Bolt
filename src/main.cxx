#include <fmt/core.h>

#include "include/capi/views/cef_window_capi.h"

#include "browser/app.hxx"
#include "browser/client.hxx"
#include "browser/details.hxx"
#include "browser/window_delegate.hxx"
#include "browser/browser_view_delegate.hxx"
#include "browser/handler/life_span_handler.hxx"

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
	cef_main_args_t main_args;
	main_args.argc = argc;
	main_args.argv = argv;

	// Set up our app struct
	Browser::App cef_app;

	// CEF applications have multiple sub-processes (render, GPU, etc) that share the same executable.
	// This function checks the command-line and, if this is a sub-process, executes the appropriate logic.
	int exit_code = cef_execute_process(&main_args, cef_app.app(), nullptr);
	if (exit_code >= 0) {
		cef_app.release();
		return exit_code;
	}

#if defined(CEF_X11)
	// X11 error handlers
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// Parse command-line arguments
	cef_command_line_t* command_line = cef_command_line_create();
#if defined(OS_WIN)
	(command_line->init_from_string)(command_line, ::GetCommandLineW());
#else
	(command_line->init_from_argv)(command_line, argc, argv);
#endif

	// CEF settings - only set the ones we're interested in
	cef_settings_t settings = {0};
	settings.size = sizeof settings;
	settings.log_severity = LOGSEVERITY_WARNING; // Print warnings and errors only
	//settings.external_message_pump = true;     // Allows us to integrate CEF's event loop into our own
	settings.command_line_args_disabled = true;  // We don't want our command-line to configure CEF's windows
	settings.uncaught_exception_stack_size = 8;  // Number of call stack frames given in unhandled exception events

	// Initialize CEF
	exit_code = cef_initialize(&main_args, &settings, cef_app.app(), nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting with error: cef_initialize exit_code {}\n", exit_code);
		cef_app.release();
		return exit_code;
	}

	// Window title
	const char* window_title = "Bolt";
	cef_string_t window_title_cef = {};
	cef_string_from_utf8(window_title, strlen(window_title), &window_title_cef);

	// Initial URL
	const char* url = "https://adamcake.com";
	cef_string_t url_cef = {};
	cef_string_from_utf8(url, strlen(url), &url_cef);

	// Browser settings
	cef_browser_settings_t browser_settings = {};
	browser_settings.size = sizeof browser_settings;
	
	// Our CEF client and the various things it needs pointers to
	Browser::LifeSpanHandler life_span_handler;
	Browser::Client client(&life_span_handler);

	// Spawn a window using the "views" pipeline
	Browser::Details details = {
		.min_width = 800,
		.min_height = 608,
		.max_width = 800,
		.max_height = 608,
		.preferred_width = 800,
		.preferred_height = 608,
		.startx = 100,
		.starty = 100,
		.resizeable = false,
		.frame = true,
	};
	Browser::BrowserViewDelegate bvd(details);
	cef_browser_view_t* browser_view = cef_browser_view_create(client.client(), &url_cef, &browser_settings, nullptr, nullptr, bvd.delegate());
	Browser::WindowDelegate window_delegate(browser_view, details);
	cef_window_create_top_level(window_delegate.window_delegate());

	// Run the CEF message loop
	// TODO: later this will be replaced with an OS-specific event loop capable of calling
	// cef_do_message_loop_work() in response to CEF's "message pump"
	cef_run_message_loop();

	// Release things we still own, then shut down CEF
	bvd.release();
	client.release();
	window_delegate.release();
	life_span_handler.release();
	cef_app.release();
	cef_shutdown();

	return 0;
}
