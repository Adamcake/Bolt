#ifndef _BOLT_RESOURCE_HANDLER_HXX_
#define _BOLT_RESOURCE_HANDLER_HXX_

#include "../file_manager.hxx"

#include "include/cef_base.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_urlrequest.h"
#include <string>

namespace Browser {
	/// Struct for sending some bytes from memory as an HTTP response. Store individual instances on the heap.
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_resource_request_handler.h
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_resource_handler.h
	struct ResourceHandler: public CefResourceRequestHandler, CefResourceHandler {
		ResourceHandler(const unsigned char* data, size_t len, int status, const char* mime):
			data(data), data_len(len), status(status), mime(mime), has_location(false), cursor(0), file_manager(nullptr) { }
		ResourceHandler(const unsigned char* data, size_t len, int status, const char* mime, CefString location):
			data(data), data_len(len), status(status), mime(mime), location(location), has_location(true), cursor(0), file_manager(nullptr) { }

		ResourceHandler(std::string data, int status, const char* mime):
			internal_string(data), data_len(data.length()), status(status), mime(mime), has_location(false), cursor(0), file_manager(nullptr) {
			this->data = reinterpret_cast<const unsigned char*>(this->internal_string.c_str());
		}
		ResourceHandler(std::string data, int status, const char* mime, CefString location):
			internal_string(data), data_len(data.length()), status(status), mime(mime), location(location), has_location(true), cursor(0), file_manager(nullptr) {
			this->data = reinterpret_cast<const unsigned char*>(this->internal_string.c_str());
		}
		
		/// This constructor assumes the file does exist i.e. all params are initialised, and status will be 200
		ResourceHandler(FileManager::File file, CefRefPtr<FileManager::FileManager> file_manager):
			ResourceHandler(file.contents, file.size, 200, file.mime_type) { this->file_manager = file_manager; }

		bool Open(CefRefPtr<CefRequest>, bool&, CefRefPtr<CefCallback>) override;
		void GetResponseHeaders(CefRefPtr<CefResponse>, int64_t&, CefString&) override;
		bool Read(void*, int, int&, CefRefPtr<CefResourceReadCallback>) override;
		bool Skip(int64_t, int64_t&, CefRefPtr<CefResourceSkipCallback>) override;
		void Cancel() override;
		CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>) override;

		protected:
			ResourceHandler(const char* mime): cursor(0), has_location(false), mime(mime) { }

			void finish();

			const unsigned char* data;
			size_t data_len;
			int status;
			const char* mime;
			CefString location;
			bool has_location;
			size_t cursor;
			const std::string internal_string;
			CefRefPtr<FileManager::FileManager> file_manager;
			IMPLEMENT_REFCOUNTING(ResourceHandler);
			DISALLOW_COPY_AND_ASSIGN(ResourceHandler);
	};

	/// Struct for bridging a CefURLRequestClient to a CefResourceHandler
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_resource_request_handler.h
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_resource_handler.h
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_urlrequest.h#L137
	struct DefaultURLHandler: public CefResourceRequestHandler, CefResourceHandler, CefURLRequestClient {
		DefaultURLHandler(CefRefPtr<CefRequest>);

		CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override;
		CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request) override;

		bool Open(CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) override;
		void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl) override;
		bool Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback) override;
		bool Skip(int64_t bytes_to_skip, int64_t& bytes_skipped, CefRefPtr<CefResourceSkipCallback> callback) override;
		void Cancel() override;

		void OnRequestComplete(CefRefPtr<CefURLRequest> request) override;
		void OnUploadProgress(CefRefPtr<CefURLRequest> request, int64_t current, int64_t total) override;
		void OnDownloadProgress(CefRefPtr<CefURLRequest> request, int64_t current, int64_t total) override;
		void OnDownloadData(CefRefPtr<CefURLRequest> request, const void* data, size_t data_length) override;
		bool GetAuthCredentials(bool isProxy, const CefString& host, int port, const CefString& realm, const CefString& scheme, CefRefPtr<CefAuthCallback> callback) override;

		private:
			CefRefPtr<CefURLRequest> url_request;
			CefRefPtr<CefCallback> urlrequest_callback;
			bool urlrequest_complete;
			bool headers_checked;
			std::vector<uint8_t> data;
			size_t cursor;
			IMPLEMENT_REFCOUNTING(DefaultURLHandler);
			DISALLOW_COPY_AND_ASSIGN(DefaultURLHandler);
	};

	#endif
}
