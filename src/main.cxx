#include <fmt/core.h>

#include "browser/app.hxx"

#ifdef WIN32
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

	// CEF applications have multiple sub-processes (render, GPU, etc) that share the same executable.
	// This function checks the command-line and, if this is a sub-process, executes the appropriate logic.
	int exit_code = cef_execute_process(&main_args, nullptr, nullptr);
	if (exit_code >= 0) {
		return exit_code;
	}

#if defined(CEF_X11)
	// X11 error handlers
	XSetErrorHandler(XErrorHandlerImpl);
	XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

	// Parse command-line arguments
	cef_command_line_t* command_line = cef_command_line_create();
#ifdef WIN32
	(command_line->init_from_string)(command_line, ::GetCommandLineW());
#else
	(command_line->init_from_argv)(command_line, argc, argv);
#endif

	cef_settings_t settings = {0};
	settings.size = sizeof settings;
	//settings.external_message_pump = true;
	settings.command_line_args_disabled = true;
	settings.uncaught_exception_stack_size = 16;

	Browser::App cef_app;

	// Initialize CEF
	exit_code = cef_initialize(&main_args, &settings, cef_app.app(), nullptr);
	if (exit_code == 0) {
		fmt::print("Exiting: cef_initialize exit_code {}\n", exit_code);
		return exit_code;
	}

	// Run the CEF message loop
	cef_run_message_loop();

	// Shut down CEF
	cef_shutdown();

	return 0;
}
