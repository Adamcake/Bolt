#include "window_plugin_requests.hxx"
#if defined(BOLT_PLUGINS)
#include "resource_handler.hxx"
#include "request.hxx"
#include "include/cef_task.h"

#include <format>

struct SendPluginMessageTask: public CefTask {
	SendPluginMessageTask(CefRefPtr<Browser::PluginRequestHandler> self, CefRefPtr<CefProcessMessage> message): self(self), message(message) {}
	void Execute() override {
		CefRefPtr<CefBrowser> browser = self->Browser();
		if (browser) {
			browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
		} else {
			self->AddPendingMessage(message);
		}
	}
	private:
		CefRefPtr<Browser::PluginRequestHandler> self;
		CefRefPtr<CefProcessMessage> message;
		IMPLEMENT_REFCOUNTING(SendPluginMessageTask);
		DISALLOW_COPY_AND_ASSIGN(SendPluginMessageTask);
};

struct SendCaptureMessageTask: public CefTask {
	SendCaptureMessageTask(CefRefPtr<Browser::PluginRequestHandler> self, uint64_t pid, uint64_t capture_id, int width, int height, bool needs_remap):
		self(self), pid(pid), capture_id(capture_id), width(width), height(height), needs_remap(needs_remap) {}
	void Execute() override {
		CefRefPtr<CefBrowser> browser = self->Browser();
		if (!browser) {
			// can't process this yet - inform the game process that we're done and don't do any further handling
			self->SendCaptureDone();
			return;
		}

		CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("__bolt_plugin_capture");
		CefRefPtr<CefListValue> list = message->GetArgumentList();
		const uint64_t current_capture_id = self->CurrentCaptureID();
		if (needs_remap || capture_id != current_capture_id) {
			list->SetSize(3);
#if !defined(_WIN32)
			if (capture_id != current_capture_id) {
#endif
			const CefString str = std::format("/bolt-{}-sc-{}", pid, capture_id);
			list->SetString(2, str);
#if !defined(_WIN32)
			} else {
				list->SetNull(2);
			}
#endif
		} else {
			list->SetSize(2);
		}
		list->SetInt(0, width);
		list->SetInt(1, height);
		browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
		self->SetCurrentCaptureID(capture_id);
	}
	private:
		CefRefPtr<Browser::PluginRequestHandler> self;
		uint64_t pid;
		uint64_t capture_id;
		int width;
		int height;
		bool needs_remap;
		IMPLEMENT_REFCOUNTING(SendCaptureMessageTask);
		DISALLOW_COPY_AND_ASSIGN(SendCaptureMessageTask);
};

void Browser::PluginRequestHandler::AddPendingMessage(CefRefPtr<CefProcessMessage> message) {
	this->pending_messages.push_back(message);
}

uint64_t Browser::PluginRequestHandler::CurrentCaptureID() const {
	return this->current_capture_id;
}

void Browser::PluginRequestHandler::SetCurrentCaptureID(uint64_t id) {
	this->current_capture_id = id;
}

void Browser::PluginRequestHandler::HandlePluginMessage(const uint8_t* data, size_t len) {
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("__bolt_plugin_message");
	CefRefPtr<CefListValue> list = message->GetArgumentList();
	list->SetSize(1);
	list->SetBinary(0, CefBinaryValue::Create(data, len));
	CefPostTask(TID_UI, new SendPluginMessageTask(this, message));
}

void Browser::PluginRequestHandler::HandleCaptureNotify(uint64_t pid, uint64_t capture_id, int width, int height, bool needs_remap) {
	CefPostTask(TID_UI, new SendCaptureMessageTask(this, pid, capture_id, width, height, needs_remap));
}

void Browser::PluginRequestHandler::NotifyBrowserCreated(CefRefPtr<CefBrowser> browser) {
	for (CefRefPtr<CefProcessMessage> message: this->pending_messages) {
		browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
	}
	this->pending_messages.clear();
}

CefRefPtr<CefResourceRequestHandler> Browser::PluginRequestHandler::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling
) {
	// Parse URL
	const std::string request_url = request->GetURL().ToString();
	const std::string::size_type colon = request_url.find_first_of(':');
	if (colon == std::string::npos) return nullptr;
	const std::string_view schema(request_url.begin(), request_url.begin() + colon);
	if (request_url.begin() + colon + 1 == request_url.end()) return nullptr;
	if (request_url.at(colon + 1) != '/') return nullptr;

	if (schema == "file") {
		disable_default_handling = true;
		const std::string::size_type question_mark = request_url.find_first_of('?');
		std::string_view req_path(request_url.begin() + colon + 1, (question_mark == std::string::npos) ? request_url.end() : request_url.begin() + question_mark);
		CefRefPtr<FileManager::FileManager> fm = this->FileManager();
		FileManager::File file = fm->get(req_path);
		if (file.contents) {
			return new ResourceHandler(file, fm);
		} else {
			fm->free(file);
			QSENDNOTFOUND();
		}
	}

	if (schema == "http" || schema == "https") {
		// figure out if this is a request to the "bolt-api" hostname - if not, return nullptr
		// (i.e. allow the request to be handled normally)
		const std::string::size_type host_name_start = request_url.find_first_not_of('/', colon + 1);
		if (host_name_start == std::string::npos) return nullptr;
		const std::string::size_type next_slash = request_url.find_first_of('/', host_name_start);
		if (next_slash == std::string::npos) return nullptr;
		const std::string_view hostname(request_url.begin() + host_name_start, request_url.begin() + next_slash);
		if (hostname != "bolt-api") return nullptr;

		// indicate that we will definitely be handling this request, and parse the rest of it
		disable_default_handling = true;
		const std::string::size_type api_name_start = next_slash + 1;
		const std::string::size_type next_questionmark = request_url.find_first_of('?', api_name_start);
		const bool has_query = next_questionmark != std::string::npos;
		const std::string_view api_name = std::string_view(request_url.begin() + api_name_start, has_query ? request_url.begin() + next_questionmark : request_url.end());
		std::string_view query;
		if (has_query) query = std::string_view(request_url.begin() + next_questionmark + 1, request_url.end());

		// match API endpoint
		if (api_name == "send-message") {
			const CefRefPtr<CefPostData> post_data = request->GetPostData();
			QSENDBADREQUESTIF(post_data == nullptr || post_data->GetElementCount() != 1);
			CefPostData::ElementVector vec;
			post_data->GetElements(vec);
			size_t message_size = vec[0]->GetBytesCount();
			uint8_t* message = new uint8_t[message_size];
			vec[0]->GetBytes(message_size, message);

			const BoltIPCBrowserMessageHeader header = { .window_id = this->WindowID(), .plugin_id = this->PluginID(), .message_size = message_size };
			this->send_lock->lock();
			const uint8_t ret = _bolt_ipc_send(this->ClientFD(), &this->message_type, sizeof(this->message_type))
				|| _bolt_ipc_send(this->ClientFD(), &header, sizeof(header))
				|| _bolt_ipc_send(this->ClientFD(), message, message_size);
			this->send_lock->unlock();
			delete[] message;
			QSENDSYSTEMERRORIF(ret);
			QSENDOK();
		}

		if (api_name == "close-request") {
			this->HandlePluginCloseRequest();
			QSENDOK();
		}

		if (api_name == "start-reposition") {
			const BoltIPCMessageTypeToClient msg_type = IPC_MSG_OSRSTARTREPOSITION;
			BoltIPCOsrStartRepositionHeader header;
			
			bool has_h = false;
			bool has_v = false;
			bool h_valid, v_valid;
			int64_t h, v;
			ParseQuery(query, [&](const std::string_view& key, const std::string_view& val) {
				PQINT(h)
				PQINT(v)
			});
			QREQPARAMINT(h);
			QREQPARAMINT(v);

			header.window_id = this->WindowID();
			header.horizontal = (h == 0) ? 0 : ((h > 0) ? 1 : -1);
			header.vertical = (v == 0) ? 0 : ((v > 0) ? 1 : -1);
			this->send_lock->lock();
			const uint8_t ret = _bolt_ipc_send(this->ClientFD(), &msg_type, sizeof(msg_type))
				|| _bolt_ipc_send(this->ClientFD(), &header, sizeof(header));
			this->send_lock->unlock();
			QSENDSYSTEMERRORIF(ret);
			QSENDOK();
		}

		if (api_name == "cancel-reposition") {
			const BoltIPCMessageTypeToClient msg_type = IPC_MSG_OSRCANCELREPOSITION;
			const BoltIPCOsrCancelRepositionHeader header = { .window_id = this->WindowID() };
			this->send_lock->lock();
			const uint8_t ret = _bolt_ipc_send(this->ClientFD(), &msg_type, sizeof(msg_type))
				|| _bolt_ipc_send(this->ClientFD(), &header, sizeof(header));
			this->send_lock->unlock();
			QSENDSYSTEMERRORIF(ret);
			QSENDOK();
		}

		if (api_name == "show-devtools") {
			this->HandleShowDevtools();
			QSENDOK();
		}

		// no API endpoint matched, so respond 404
		QSENDNOTFOUND();
	}

	return nullptr;
}

#endif
