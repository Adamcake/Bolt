#ifndef _BOLT_RESOURCE_HANDLER_HXX_
#define _BOLT_RESOURCE_HANDLER_HXX_

#include "../file_manager.hxx"

#include "include/cef_base.h"
#include "include/cef_request_handler.h"

namespace Browser {
	/// Struct for sending some bytes from memory as an HTTP response. Store individual instances on the heap.
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_resource_request_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_resource_handler.h
	struct ResourceHandler: public CefResourceRequestHandler, CefResourceHandler {
		ResourceHandler(const unsigned char* data, size_t len, int status, const char* mime):
			data(data), data_len(len), status(status), mime(mime), has_location(false), cursor(0), file_manager(nullptr) { }
		ResourceHandler(const unsigned char* data, size_t len, int status, const char* mime, CefString location):
			data(data), data_len(len), status(status), mime(mime), location(location), has_location(true), cursor(0), file_manager(nullptr) { }
		
		/// This constructor assumes the file does exist i.e. all params are initialised, and status will be 200
		ResourceHandler(FileManager::File file, CefRefPtr<FileManager::FileManager> file_manager):
			ResourceHandler(file.contents, file.size, 200, file.mime_type) { this->file_manager = file_manager; }

		bool Open(CefRefPtr<CefRequest>, bool&, CefRefPtr<CefCallback>) override;
		void GetResponseHeaders(CefRefPtr<CefResponse>, int64&, CefString&) override;
		bool Read(void*, int, int&, CefRefPtr<CefResourceReadCallback>) override;
		bool Skip(int64, int64&, CefRefPtr<CefResourceSkipCallback>) override;
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
			CefRefPtr<FileManager::FileManager> file_manager;
			IMPLEMENT_REFCOUNTING(ResourceHandler);
			DISALLOW_COPY_AND_ASSIGN(ResourceHandler);
	};

	#endif
}
