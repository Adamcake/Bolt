#include "window_launcher.hxx"
#include "resource_handler.hxx"

#include <shellapi.h>

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Deb(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "Elf binaries are not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3Exe(CefRefPtr<CefRequest> request, std::string_view query) {
	// TODO
	const char* data = ".exe is not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRs3App(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "Mac binaries are not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsExe(CefRefPtr<CefRequest> request, std::string_view query) {
	// TODO
	const char* data = ".exe is not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchOsrsApp(CefRefPtr<CefRequest> request, std::string_view query) {
	const char* data = "Mac binaries are not supported on this platform\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 400, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchRuneliteJar(CefRefPtr<CefRequest> request, std::string_view query, bool configure) {
	const char* data = "JAR files not yet supported on Windows\n";
	return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
}

CefRefPtr<CefResourceRequestHandler> Browser::Launcher::LaunchHdosJar(CefRefPtr<CefRequest> request, std::string_view query) {
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

int Browser::Launcher::BrowseData() const {
	// TODO
	return -1;
}
