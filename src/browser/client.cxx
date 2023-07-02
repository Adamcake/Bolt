#include "client.hxx"

#include <algorithm>
#include <fmt/core.h>
#include <fstream>
#include <regex>

/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_resource_request_handler.h
/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_resource_handler.h
struct ResourceHandler: public CefResourceRequestHandler, CefResourceHandler {
	ResourceHandler(const unsigned char* data, size_t len, int status, CefString mime):
		data(data), data_len(len), status(status), mime(mime), has_location(false), cursor(0) { }
	ResourceHandler(const unsigned char* data, size_t len, int status, CefString mime, CefString location):
		data(data), data_len(len), status(status), mime(mime), location(location), has_location(true), cursor(0) { }

	bool Open(CefRefPtr<CefRequest>, bool& handle_request, CefRefPtr<CefCallback>) override {
		handle_request = true;
		return true;
	}

	void GetResponseHeaders(CefRefPtr<CefResponse> response, int64& response_length, CefString& redirectUrl) override {
		response->SetStatus(this->status);
		response->SetMimeType(this->mime);
		if (this->has_location) {
			response->SetHeaderByName("Location", this->location, false);
		}
		response_length = this->data_len;
	}

	bool Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback>) override {
		if (this->cursor == this->data_len) {
			// "To indicate response completion set |bytes_read| to 0 and return false."
			bytes_read = 0;
			return false;
		}
		if (this->cursor + bytes_to_read <= this->data_len) {
			// requested read is entirely in bounds
			bytes_read = bytes_to_read;
			memcpy(data_out, this->data + this->cursor, bytes_read);
			this->cursor += bytes_to_read;
		} else {
			// read only to end of string
			bytes_read = this->data_len - this->cursor;
			memcpy(data_out, this->data + this->cursor, bytes_read);
			this->cursor = this->data_len;
		}
		return true;
	}

	bool Skip(int64 bytes_to_skip, int64& bytes_skipped, CefRefPtr<CefResourceSkipCallback>) override {
		if (this->cursor + bytes_to_skip <= this->data_len) {
			// skip in bounds
			bytes_skipped = bytes_to_skip;
			this->cursor += bytes_to_skip;
		} else {
			// skip to end of string
			bytes_skipped = this->data_len - this->cursor;
			this->cursor = this->data_len;
		}
		return true;
	}

	void Cancel() override {
		this->cursor = this->data_len;
	}

	CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>) override {
		return this;
	}

	private:
		const unsigned char* data;
		size_t data_len;
		int status;
		CefString mime;
		CefString location;
		bool has_location;
		size_t cursor;
		IMPLEMENT_REFCOUNTING(ResourceHandler);
		DISALLOW_COPY_AND_ASSIGN(ResourceHandler);
};

_InternalFile allocate_file(const char* filename, CefString mime_type) {
	// little helper function for Client constructor
	std::ifstream file(filename, std::ios::binary);
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();

	if (size < 0) {
		return _InternalFile { .success = false };
	}

	file.seekg(0, std::ios::beg);
	std::vector<unsigned char> buf(size);
	if (file.read(reinterpret_cast<char*>(buf.data()), size)) {
		return _InternalFile {
			.success = true,
			.data = buf,
			.mime_type = mime_type,
		};
	} else {
		return _InternalFile { .success = false };
	}
}

Browser::Client::Client(CefRefPtr<Browser::App> app) {
	CefString mime_type_html = "text/html";
	CefString mime_type_js = "application/javascript";
	app->SetBrowserProcessHandler(this);
	this->internal_pages["index.html"] = allocate_file("html/index.html", mime_type_html);
	this->internal_pages["oauth.html"] = allocate_file("html/oauth.html", mime_type_html);
	this->internal_pages["game_auth.html"] = allocate_file("html/game_auth.html", mime_type_html);
	this->internal_pages["frame.html"] = allocate_file("html/frame.html", mime_type_html);
}

CefRefPtr<CefLifeSpanHandler> Browser::Client::GetLifeSpanHandler() {
	return this;
}

CefRefPtr<CefRequestHandler> Browser::Client::GetRequestHandler() {
	return this;
}

void Browser::Client::OnContextInitialized() {
	std::lock_guard<std::mutex> _(this->apps_lock);
	fmt::print("[B] OnContextInitialized\n");
	// After main() enters its event loop, this function will be called on the main thread when CEF
	// context is ready to go, so, as suggested by CEF examples, Bolt treats this as an entry point.
	Browser::Details details = {
		.preferred_width = 800,
		.preferred_height = 608,
		.startx = 100,
		.starty = 100,
		.resizeable = true,
		.frame = true,
		.controls_overlay = false,
	};
	Browser::Window* w = new Browser::Window(this, details, this->internal_url);
	this->apps.push_back(w);
}

void Browser::Client::OnScheduleMessagePumpWork(int64 delay_ms) {
	// This function will be called from many different threads, because we enabled `external_message_pump`
	// in CefSettings in main(). The docs state that the given delay may be either positive or negative.
	// A negative number indicates we should call CefDoMessageLoopWork() right now on the main thread.
	// A positive number indicates we should do that after at least the given number of milliseconds.
}

bool Browser::Client::DoClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] DoClose for browser {}\n", browser->GetIdentifier());
	return false;
}

void Browser::Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	std::lock_guard<std::mutex> _(this->apps_lock);
	fmt::print("[B] OnBeforeClose for browser {}\n", browser->GetIdentifier());
}

bool Browser::Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	std::lock_guard<std::mutex> _(this->apps_lock);
	CefString name = message->GetName();

	if (name == "__bolt_app_settings") {
		fmt::print("[B] bolt_app_settings received for browser {}\n", browser->GetIdentifier());
		CefWindowInfo window_info; // ignored, because this is a BrowserView
		CefBrowserSettings browser_settings;
		browser->GetHost()->ShowDevTools(window_info, this, browser_settings, CefPoint());
		return true;
	}

	if (name == "__bolt_app_minify") {
		fmt::print("[B] bolt_app_minify received for browser {}\n", browser->GetIdentifier());
		return true;
	}

	if (name == "__bolt_app_begin_drag") {
		fmt::print("[B] bolt_app_begin_drag received for browser {}\n", browser->GetIdentifier());
		return true;
	}

	return false;
}

CefRefPtr<CefResourceRequestHandler> Browser::Client::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling
) {
	const std::string request_url = request->GetURL().ToString();

	// custom schema thing
	const std::regex regex("^.....:code=([^,]+),state=([^,]+),intent=social_auth$");
	std::smatch match;
	if (std::regex_match(request_url, match, regex)) {
		if (match.size() == 3) {
			disable_default_handling = true;
			const char* data = "Moved\n";
			CefString location = CefString(this->internal_url + "oauth.html?code=" + match[1].str() + "&state=" + match[2].str());
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 302, "text/plain", location);
		}
	}

	// another custom schema type of thing, but this one uses localhost, for whatever reason
	const std::regex regex2("^http:\\/\\/localhost\\/#(code=.+)$");
	std::smatch match2;
	if (std::regex_match(request_url, match2, regex2)) {
		if (match2.size() == 2) {
			disable_default_handling = true;
			const char* data = "Moved\n";
			CefString location = CefString(this->internal_url + "game_auth.html?" + match2[1].str());
			return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 302, "text/plain", location);
		}
	}

	// internal pages
	const char* url_cstr = request_url.c_str();
	size_t url_len = request_url.find_first_of("?#");
	if (url_len == std::string::npos) url_len = request_url.size();
	size_t internal_url_size = this->internal_url.size();
	if (url_len >= internal_url_size) {
		if (memcmp(url_cstr, this->internal_url.c_str(), internal_url_size) == 0) {
			disable_default_handling = true;

			if (url_len == internal_url_size) {
				const char* data = "Moved\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 302, "text/plain", "/index.html");
			}

			auto it = std::find_if(
				this->internal_pages.begin(),
				this->internal_pages.end(),
				[url_cstr, url_len, internal_url_size](const auto& e) {
					if (e.first.size() != url_len - internal_url_size) return false;
					return !memcmp(e.first.c_str(), url_cstr + internal_url_size, url_len - internal_url_size);
				}
			);
			if (it != this->internal_pages.end()) {
				if (it->second.success) {
					return new ResourceHandler(
						it->second.data.data(),
						it->second.data.size(),
						200,
						it->second.mime_type
					);
				} else {
					// this file failed to load during startup for some reason - 500
					const char* data = "Internal Server Error\n";
					return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 500, "text/plain");
				}
			} else {
				// no matching internal page - 404
				const char* data = "Not Found\n";
				return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 404, "text/plain");
			}
		}
	}

	// route the request normally, to a website or whatever
	return nullptr;
}
