#include "resource_handler.hxx"
#include "include/cef_urlrequest.h"
#include "include/internal/cef_types.h"

// returns content-length or -1 if it's malformed or nonexistent
// CEF interprets -1 as "unknown length"
static int64_t GetContentLength(CefRefPtr<CefResponse> response) {
	std::string content_length = response->GetHeaderByName("Content-Length").ToString();
	if (content_length.size() == 0) {
		return -1;
	}
	int64_t length = 0;
	for (auto it = content_length.begin(); it != content_length.end(); it++) {
		if (*it < '0' || *it > '9') return -1;
		length = (length * 10) + ((int64_t)(*it) - '0');
	}
	return length;
}

bool Browser::ResourceHandler::Open(CefRefPtr<CefRequest>, bool& handle_request, CefRefPtr<CefCallback>) {
	handle_request = true;
	return true;
}

void Browser::ResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl) {
	response->SetStatus(this->status);
	response->SetMimeType(this->mime);
	if (this->has_location) {
		response->SetHeaderByName("Location", this->location, false);
	}
	response_length = this->data_len;
}

bool Browser::ResourceHandler::Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback>) {
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
	if (this->cursor == this->data_len) this->finish();
	return true;
}

bool Browser::ResourceHandler::Skip(int64_t bytes_to_skip, int64_t& bytes_skipped, CefRefPtr<CefResourceSkipCallback>) {
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

void Browser::ResourceHandler::Cancel() {
	if (this->cursor != this->data_len) {
		this->cursor = this->data_len;
		this->finish();
	}
}

CefRefPtr<CefResourceHandler> Browser::ResourceHandler::GetResourceHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>) {
	return this;
}

void Browser::ResourceHandler::finish() {
	if (this->file_manager) {
		this->file_manager->free(FileManager::File { .contents = this->data, .size = this->data_len, .mime_type = this->mime });
		this->file_manager = nullptr;
	}
}

Browser::DefaultURLHandler::DefaultURLHandler(CefRefPtr<CefRequest> request): urlrequest_complete(false), headers_checked(false), urlrequest_callback(nullptr), cursor(0) {
	this->url_request = CefURLRequest::Create(request, this, nullptr);
}

CefResourceRequestHandler::ReturnValue Browser::DefaultURLHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) {
	// use CefResourceRequestHandler::OnBeforeResourceLoad to stall asynchronously on the IO thread
	// until the request completes, if it hasn't already
	if (urlrequest_complete) {
		return RV_CONTINUE;
	} else {
		this->urlrequest_callback = callback;
		return RV_CONTINUE_ASYNC;
	}
}

CefRefPtr<CefResourceHandler> Browser::DefaultURLHandler::GetResourceHandler(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request) {
	return this;
}

bool Browser::DefaultURLHandler::Open(CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) {
	handle_request = true;
	return true;
}

void Browser::DefaultURLHandler::GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl) {
	CefRefPtr<CefResponse> url_response = this->url_request->GetResponse();
	CefResponse::HeaderMap headers;
	url_response->GetHeaderMap(headers);
	response->SetHeaderMap(headers);
	response->SetStatus(url_response->GetStatus());
	response->SetStatusText(url_response->GetStatusText());
	response->SetMimeType(url_response->GetMimeType());
	response->SetCharset(url_response->GetCharset());
	response->SetHeaderByName("Access-Control-Allow-Origin", "*", true);
	response_length = GetContentLength(url_response);
}

bool Browser::DefaultURLHandler::Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback) {
	if (this->cursor == this->data.size()) {
		// "To indicate response completion set |bytes_read| to 0 and return false."
		bytes_read = 0;
		return false;
	}
	if (this->cursor + bytes_to_read <= this->data.size()) {
		// requested read is entirely in bounds
		bytes_read = bytes_to_read;
		memcpy(data_out, &this->data[this->cursor], bytes_read);
		this->cursor += bytes_to_read;
	} else {
		// read only to end of string
		bytes_read = this->data.size() - this->cursor;
		memcpy(data_out, &this->data[this->cursor], bytes_read);
		this->cursor = this->data.size();
	}
	return true;
}

bool Browser::DefaultURLHandler::Skip(int64_t bytes_to_skip, int64_t& bytes_skipped, CefRefPtr<CefResourceSkipCallback> callback) {
	if (this->cursor + bytes_to_skip <= this->data.size()) {
		// skip in bounds
		bytes_skipped = bytes_to_skip;
		this->cursor += bytes_to_skip;
	} else {
		// skip to end of string
		bytes_skipped = this->data.size() - this->cursor;
		this->cursor = this->data.size();
	}
	return true;
}

void Browser::DefaultURLHandler::Cancel() {
	this->cursor = this->data.size();
}

void Browser::DefaultURLHandler::OnRequestComplete(CefRefPtr<CefURLRequest> request) {
	this->urlrequest_complete = true;
	if (this->urlrequest_callback) {
		this->urlrequest_callback->Continue();
	}
}

void Browser::DefaultURLHandler::OnUploadProgress(CefRefPtr<CefURLRequest> request, int64_t current, int64_t total) {}
void Browser::DefaultURLHandler::OnDownloadProgress(CefRefPtr<CefURLRequest> request, int64_t current, int64_t total) {}

void Browser::DefaultURLHandler::OnDownloadData(CefRefPtr<CefURLRequest> request, const void* data, size_t data_length) {
	CefRefPtr<CefResponse> response = request->GetResponse();
	if (!this->headers_checked && response) {
		const int64_t content_length = GetContentLength(response);
		if (content_length != -1) this->data.reserve(content_length);
		this->headers_checked = true;
	}
	size_t write_offset = this->data.size();
	this->data.resize(write_offset + data_length);
	memcpy(this->data.data() + write_offset, data, data_length);
}

bool Browser::DefaultURLHandler::GetAuthCredentials(bool isProxy, const CefString& host, int port, const CefString& realm, const CefString& scheme, CefRefPtr<CefAuthCallback> callback) {
	return false;
}
