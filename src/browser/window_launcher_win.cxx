#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include <shellapi.h>

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Deb(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = ".deb is not supported on Windows\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query) {
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
