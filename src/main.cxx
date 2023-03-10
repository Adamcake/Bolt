#include <fmt/core.h>

#include "browser/app.hxx"
#include "browser/client.hxx"
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

	cef_settings_t settings = {0};
	settings.size = sizeof settings;
	settings.log_severity = LOGSEVERITY_WARNING;
	//settings.external_message_pump = true;
	//settings.command_line_args_disabled = true;
	settings.uncaught_exception_stack_size = 16;

	// Initialize CEF, then release our ownership of the app
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
	const char* url = "https://adamcake.com/";
	cef_string_t url_cef = {};
	cef_string_from_utf8(url, strlen(url), &url_cef);

	// Window info
	cef_window_info_t window_info;
	window_info.bounds.x = 100;
	window_info.bounds.y = 100;
	window_info.bounds.width = 800;
	window_info.bounds.height = 608;
	window_info.window_name = std::move(window_title_cef);
	window_info.windowless_rendering_enabled = false;
	window_info.shared_texture_enabled = false;
	window_info.external_begin_frame_enabled = false;
#if defined(OS_WIN)
	window_info.style = 0;
	window_info.ex_style = 0;
	window_info.menu = -1;
#endif
#if defined(OS_MAC)
	window_info.parent_view = 0;
	window_info.view = 0;
#else
	window_info.parent_window = 0;
	window_info.window = 0;
#endif

	// Browser settings
	cef_browser_settings_t browser_settings = {};
	browser_settings.size = sizeof browser_settings;
	
	// Our CEF client and the various things it needs pointers to
	Browser::LifeSpanHandler life_span_handler;
	Browser::Client client(&life_span_handler);

	cef_browser_host_create_browser(&window_info, client.client(), &url_cef, &browser_settings, nullptr, nullptr);

	// Run the CEF message loop
	cef_run_message_loop();

	// Shut down CEF
	life_span_handler.release();
	cef_app.release();
	cef_shutdown();

	return 0;
}
