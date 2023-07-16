#include "resource_handler.hxx"

bool Browser::ResourceHandler::Open(CefRefPtr<CefRequest>, bool& handle_request, CefRefPtr<CefCallback>) {
	handle_request = true;
	return true;
}

void Browser::ResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response, int64& response_length, CefString& redirectUrl) {
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
	return true;
}

bool Browser::ResourceHandler::Skip(int64 bytes_to_skip, int64& bytes_skipped, CefRefPtr<CefResourceSkipCallback>) {
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
	this->cursor = this->data_len;
}

CefRefPtr<CefResourceHandler> Browser::ResourceHandler::GetResourceHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>) {
	return this;
}
