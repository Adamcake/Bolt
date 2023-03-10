#include "client.hxx"

#include <fmt/core.h>

void CEF_CALLBACK add_ref_client(cef_base_ref_counted_t*);
int CEF_CALLBACK release_client(cef_base_ref_counted_t*);
int CEF_CALLBACK has_one_ref_client(cef_base_ref_counted_t*);
int CEF_CALLBACK has_any_refs_client(cef_base_ref_counted_t*);
cef_audio_handler_t* CEF_CALLBACK get_audio_handler(cef_client_t*);
cef_command_handler_t* CEF_CALLBACK get_command_handler(cef_client_t*);
cef_context_menu_handler_t* CEF_CALLBACK get_context_menu_handler(cef_client_t*);
cef_dialog_handler_t* CEF_CALLBACK get_dialog_handler(cef_client_t*);
cef_display_handler_t* CEF_CALLBACK get_display_handler(cef_client_t*);
cef_download_handler_t* CEF_CALLBACK get_download_handler(cef_client_t*);
cef_drag_handler_t* CEF_CALLBACK get_drag_handler(cef_client_t*);
cef_find_handler_t* CEF_CALLBACK get_find_handler(cef_client_t*);
cef_frame_handler_t* CEF_CALLBACK get_frame_handler(cef_client_t*);
cef_focus_handler_t* CEF_CALLBACK get_focus_handler(cef_client_t*);
cef_jsdialog_handler_t* CEF_CALLBACK get_jsdialog_handler(cef_client_t*);
cef_keyboard_handler_t* CEF_CALLBACK get_keyboard_handler(cef_client_t*);
cef_life_span_handler_t* CEF_CALLBACK get_life_span_handler(cef_client_t*);
cef_load_handler_t* CEF_CALLBACK get_load_handler(cef_client_t*);
cef_permission_handler_t* CEF_CALLBACK get_permission_handler(cef_client_t*);
cef_print_handler_t* CEF_CALLBACK get_print_handler(cef_client_t*);
cef_render_handler_t* CEF_CALLBACK get_render_handler(cef_client_t*);
cef_request_handler_t* CEF_CALLBACK get_request_handler(cef_client_t*);
int CEF_CALLBACK on_process_message_received(cef_client_t*, cef_browser_t*, cef_frame_t*, cef_process_id_t, cef_process_message_t*);


Browser::Client* resolve_client(cef_client_t* client) {
	return reinterpret_cast<Browser::Client*>(reinterpret_cast<uintptr_t>(client) - offsetof(Browser::Client, cef_client));
}

Browser::Client* resolve_client_base(cef_base_ref_counted_t* base) {
	return reinterpret_cast<Browser::Client*>(reinterpret_cast<uintptr_t>(base) - (offsetof(Browser::Client, cef_client) + offsetof(cef_client_t, base)));
}

Browser::Client::Client(LifeSpanHandler* life_span_handler) {
    life_span_handler->add_ref();
    this->life_span_handler = life_span_handler;
	this->cef_client.base.size = sizeof(cef_base_ref_counted_t);
	this->cef_client.base.add_ref = ::add_ref_client;
    this->cef_client.base.release = ::release_client;
    this->cef_client.base.has_one_ref = ::has_one_ref_client;
    this->cef_client.base.has_at_least_one_ref = ::has_any_refs_client;
    this->cef_client.get_audio_handler = ::get_audio_handler;
    this->cef_client.get_command_handler = ::get_command_handler;
    this->cef_client.get_context_menu_handler = ::get_context_menu_handler;
    this->cef_client.get_dialog_handler = ::get_dialog_handler;
    this->cef_client.get_display_handler = ::get_display_handler;
    this->cef_client.get_download_handler = ::get_download_handler;
    this->cef_client.get_drag_handler = ::get_drag_handler;
    this->cef_client.get_find_handler = ::get_find_handler;
    this->cef_client.get_frame_handler = ::get_frame_handler;
    this->cef_client.get_focus_handler = ::get_focus_handler;
    this->cef_client.get_jsdialog_handler = ::get_jsdialog_handler;
    this->cef_client.get_keyboard_handler = ::get_keyboard_handler;
    this->cef_client.get_life_span_handler = ::get_life_span_handler;
    this->cef_client.get_load_handler = ::get_load_handler;
    this->cef_client.get_permission_handler = ::get_permission_handler;
    this->cef_client.get_print_handler = ::get_print_handler;
    this->cef_client.get_render_handler = ::get_render_handler;
    this->cef_client.get_request_handler = ::get_request_handler;
	this->refcount = 1;
}

void Browser::Client::add_ref() {
	this->refcount += 1;
}

int Browser::Client::release() {
	// Since this->refcount is atomic, the operation of decrementing it and checking if the result
	// is 0 must only access the value once. Accessing it twice would cause a race condition.
	if (--this->refcount == 0) {
		this->destroy();
		return 1;
	}
	return 0;
}

void Browser::Client::destroy() {
	this->life_span_handler->release();
}

cef_client_t* Browser::Client::client() {
	this->add_ref();
	return &this->cef_client;
}

void CEF_CALLBACK add_ref_client(cef_base_ref_counted_t* client) {
	resolve_client_base(client)->add_ref();
}

int CEF_CALLBACK release_client(cef_base_ref_counted_t* client) {
	return resolve_client_base(client)->release();
}

int CEF_CALLBACK has_one_ref_client(cef_base_ref_counted_t* client) {
	return (resolve_client_base(client)->refcount == 1) ? 1 : 0;
}

int CEF_CALLBACK has_any_refs_client(cef_base_ref_counted_t* client) {
	return (resolve_client_base(client)->refcount >= 1) ? 1 : 0;
}

cef_audio_handler_t* CEF_CALLBACK get_audio_handler(cef_client_t* self) {
    fmt::print("get_audio_handler\n");
    return nullptr;
}

cef_command_handler_t* CEF_CALLBACK get_command_handler(cef_client_t* self) {
    fmt::print("get_command_handler\n");
    return nullptr;
}

cef_context_menu_handler_t* CEF_CALLBACK get_context_menu_handler(cef_client_t* self) {
    fmt::print("get_context_menu_handler\n");
    return nullptr;
}

cef_dialog_handler_t* CEF_CALLBACK get_dialog_handler(cef_client_t* self) {
    fmt::print("get_dialog_handler\n");
    return nullptr;
}

cef_display_handler_t* CEF_CALLBACK get_display_handler(cef_client_t* self) {
    fmt::print("get_display_handler\n");
    return nullptr;
}

cef_download_handler_t* CEF_CALLBACK get_download_handler(cef_client_t* self) {
    fmt::print("get_download_handler\n");
    return nullptr;
}

cef_drag_handler_t* CEF_CALLBACK get_drag_handler(cef_client_t* self) {
    fmt::print("get_drag_handler\n");
    return nullptr;
}

cef_find_handler_t* CEF_CALLBACK get_find_handler(cef_client_t* self) {
    fmt::print("get_find_handler\n");
    return nullptr;
}

cef_frame_handler_t* CEF_CALLBACK get_frame_handler(cef_client_t* self) {
    fmt::print("get_frame_handler\n");
    return nullptr;
}

cef_focus_handler_t* CEF_CALLBACK get_focus_handler(cef_client_t* self) {
    fmt::print("get_focus_handler\n");
    return nullptr;
}

cef_jsdialog_handler_t* CEF_CALLBACK get_jsdialog_handler(cef_client_t* self) {
    fmt::print("get_jsdialog_handler\n");
    return nullptr;
}

cef_keyboard_handler_t* CEF_CALLBACK get_keyboard_handler(cef_client_t* self) {
    fmt::print("get_keyboard_handler\n");
    return nullptr;
}

cef_life_span_handler_t* CEF_CALLBACK get_life_span_handler(cef_client_t* self) {
    fmt::print("get_life_span_handler\n");
    // TODO: this needs to be implemented
    return nullptr;
}

cef_load_handler_t* CEF_CALLBACK get_load_handler(cef_client_t* self) {
    fmt::print("get_load_handler\n");
    return nullptr;
}

cef_permission_handler_t* CEF_CALLBACK get_permission_handler(cef_client_t* self) {
    fmt::print("get_permission_handler\n");
    return nullptr;
}

cef_print_handler_t* CEF_CALLBACK get_print_handler(cef_client_t* self) {
    fmt::print("get_render_handler\n");
    return nullptr;
}

cef_render_handler_t* CEF_CALLBACK get_render_handler(cef_client_t* self) {
    fmt::print("get_render_handler\n");
    return nullptr;
}

cef_request_handler_t* CEF_CALLBACK get_request_handler(cef_client_t* self) {
    fmt::print("get_request_handler\n");
    return nullptr;
}

int CEF_CALLBACK on_process_message_received(cef_client_t* self, cef_browser_t* browser, cef_frame_t* frame, cef_process_id_t source_process, cef_process_message_t* message) {
    fmt::print("on_process_message_received\n");
    return 0;
}
