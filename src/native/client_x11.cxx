#include "../browser/client.hxx"
#include "src/native/native.hxx"

#include <thread>
#include <xcb/record.h>
#include <fmt/core.h>
#include <xcb/xproto.h>

// Uses the xcb-record extension to report mouse button events to main event loop
void Record(xcb_connection_t*);

void Browser::Client::Run() {
	this->native.connection = xcb_connect(nullptr, nullptr);
	if (!this->native.connection) {
		fmt::print("[native] connection is null\n");
		return;
	}
	if (xcb_connection_has_error(this->native.connection)) {
		fmt::print("[native] connection has error\n");
		return;
	}
	xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(this->native.connection));
	if (screens.rem != 1) {
		fmt::print("[native] {} root screens found; expected exactly 1\n", screens.rem);
		xcb_disconnect(this->native.connection);
		return;
	}
	this->native.root_window = screens.data->root;

	auto tray_opcode_atom_cookie = xcb_intern_atom(this->native.connection, false, 23, "_NET_SYSTEM_TRAY_OPCODE");
	auto tray_atom_cookie = xcb_intern_atom(this->native.connection, false, 19, "_NET_SYSTEM_TRAY_S0");
	auto net_wm_name_cookie = xcb_intern_atom(this->native.connection, false, 12, "_NET_WM_NAME");
	auto net_wm_icon_cookie = xcb_intern_atom(this->native.connection, false, 12, "_NET_WM_ICON");
	auto net_wm_pid_cookie = xcb_intern_atom(this->native.connection, false, 11, "_NET_WM_PID");
	auto wm_client_machine_cookie = xcb_intern_atom(this->native.connection, false, 17, "WM_CLIENT_MACHINE");
	auto utf8_string_cookie = xcb_intern_atom(this->native.connection, false, 11, "UTF8_STRING");
	auto wm_delete_window_cookie = xcb_intern_atom(this->native.connection, false, 16, "WM_DELETE_WINDOW");
	auto net_wm_ping_cookie = xcb_intern_atom(this->native.connection, false, 12, "_NET_WM_PING");
	auto wm_protocols_cookie = xcb_intern_atom(this->native.connection, false, 12, "WM_PROTOCOLS");
	
	this->native.event_window = xcb_generate_id(this->native.connection);
	if (this->native.event_window == 0xFFFFFFFF) {
		fmt::print("[native] failed to create event window - xcb_generate_id failed\n");
		xcb_disconnect(this->native.connection);
		return;
	}
	auto event_mask = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	auto create_error = xcb_request_check(this->native.connection, xcb_create_window(
		this->native.connection,
		XCB_COPY_FROM_PARENT,
		this->native.event_window,
		this->native.root_window,
		0,
		0,
		1,
		1,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		XCB_COPY_FROM_PARENT,
		XCB_CW_EVENT_MASK,
		&event_mask
	));
	if (create_error) {
		fmt::print("[native] failed to create event window - error {}\n", create_error->error_code);
		free(create_error);
		xcb_disconnect(this->native.connection);
		return;
	}

	this->native.tray_window = xcb_generate_id(this->native.connection);
	if (this->native.tray_window == 0xFFFFFFFF) {
		fmt::print("[native] failed to create tray icon - xcb_generate_id failed\n");
		xcb_disconnect(this->native.connection);
		return;
	}
	create_error = xcb_request_check(this->native.connection, xcb_create_window(
		this->native.connection,
		XCB_COPY_FROM_PARENT,
		this->native.tray_window,
		this->native.root_window,
		0,
		0,
		16,
		16,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		XCB_COPY_FROM_PARENT,
		0,
		nullptr
	));
	if (create_error) {
		fmt::print("[native] failed to create tray icon - error {}\n", create_error->error_code);
		free(create_error);
		xcb_disconnect(this->native.connection);
		return;
	}

	char hostname[HOST_NAME_MAX];
	memset(hostname, 0, HOST_NAME_MAX);
	gethostname(hostname, HOST_NAME_MAX);
	auto pid = getpid();
	uint32_t icon[(16 * 16 + 2)] = {};
	memset(icon, 0xFF, (16 * 16 + 2) * sizeof(uint32_t));
	icon[0] = 16;
	icon[1] = 16;
	xcb_intern_atom_reply_t* tray_opcode_atom = xcb_intern_atom_reply(this->native.connection, tray_opcode_atom_cookie, nullptr);
	xcb_intern_atom_reply_t* tray_atom = xcb_intern_atom_reply(this->native.connection, tray_atom_cookie, nullptr);
	xcb_intern_atom_reply_t* net_wm_name = xcb_intern_atom_reply(this->native.connection, net_wm_name_cookie, nullptr);
	xcb_intern_atom_reply_t* net_wm_icon = xcb_intern_atom_reply(this->native.connection, net_wm_icon_cookie, nullptr);
	xcb_intern_atom_reply_t* net_wm_pid = xcb_intern_atom_reply(this->native.connection, net_wm_pid_cookie, nullptr);
	xcb_intern_atom_reply_t* wm_client_machine = xcb_intern_atom_reply(this->native.connection, wm_client_machine_cookie, nullptr);
	xcb_intern_atom_reply_t* utf8_string = xcb_intern_atom_reply(this->native.connection, utf8_string_cookie, nullptr);
	xcb_intern_atom_reply_t* wm_delete_window = xcb_intern_atom_reply(this->native.connection, wm_delete_window_cookie, nullptr);
	xcb_intern_atom_reply_t* net_wm_ping = xcb_intern_atom_reply(this->native.connection, net_wm_ping_cookie, nullptr);
	xcb_intern_atom_reply_t* wm_protocols = xcb_intern_atom_reply(this->native.connection, wm_protocols_cookie, nullptr);
	xcb_atom_t window_atoms[] = { wm_delete_window->atom, net_wm_ping->atom };

	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		net_wm_pid->atom,
		XCB_ATOM_CARDINAL,
		32,
		1,
		&pid
	).sequence);
	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		wm_client_machine->atom,
		XCB_ATOM_STRING,
		8,
		strlen(hostname),
		hostname
	).sequence);
	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		net_wm_name->atom,
		utf8_string->atom,
		8,
		4,
		"Bolt"
	).sequence);
	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		4,
		"Bolt"
	).sequence);
	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		XCB_ATOM_WM_CLASS,
		XCB_ATOM_STRING,
		8,
		10,
		"bolt\0bolt\0"
	).sequence);
	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		net_wm_icon->atom,
		XCB_ATOM_CARDINAL,
		32,
		(16 * 16) + 2,
		icon
	).sequence);
	xcb_discard_reply(this->native.connection, xcb_change_property(
		this->native.connection,
		XCB_PROP_MODE_REPLACE,
		this->native.tray_window,
		wm_protocols->atom,
		XCB_ATOM_ATOM,
		32,
		2,
		window_atoms
	).sequence);

	auto tray_owner_cookie = xcb_get_selection_owner(this->native.connection, tray_atom->atom);
	auto tray = xcb_get_selection_owner_reply(this->native.connection, tray_owner_cookie, nullptr)->owner;
	auto attr_cookie = xcb_change_window_attributes(this->native.connection, tray, XCB_CW_EVENT_MASK, &event_mask);
	xcb_discard_reply(this->native.connection, attr_cookie.sequence);
	xcb_client_message_event_t e;
	memset(&e, 0, sizeof e);
	e.response_type = XCB_CLIENT_MESSAGE;
	e.window = tray;
	e.type = tray_opcode_atom->atom;
	e.format = 32;
	e.data.data32[0] = XCB_CURRENT_TIME;
	e.data.data32[1] = 0; // SYSTEM_TRAY_REQUEST_DOCK
	e.data.data32[2] = this->native.tray_window;
	auto send_event_cookie = xcb_send_event_checked(
		this->native.connection,
		false,
		tray,
		XCB_EVENT_MASK_NO_EVENT,
		reinterpret_cast<const char*>(&e)
	);
	xcb_discard_reply(this->native.connection, send_event_cookie.sequence);

	this->native.record_thread = std::thread(Record, this->native.connection);

	xcb_flush(this->native.connection);

	bool run = true;
	xcb_generic_event_t* event;
	while (run) {
		event = xcb_wait_for_event(this->native.connection);
		if (event) {
			// Top bit indicates whether the event came from SendEvent
			uint8_t type = event->response_type & 0b01111111;
			switch(type) {
				case XCB_CLIENT_MESSAGE: {
					auto message = reinterpret_cast<xcb_client_message_event_t*>(event);

					if (message->window == this->native.event_window) {
						fmt::print("[native] stopping due to client message\n");
						run = false;
					} else if (message->window == this->native.tray_window && message->type == wm_protocols->atom && message->format == 32) {
						if (message->data.data32[0] == wm_delete_window->atom) {
							auto destroy = xcb_destroy_window(this->native.connection, this->native.tray_window);
							xcb_discard_reply(this->native.connection, destroy.sequence);
						} else if (message->data.data32[0] == net_wm_ping->atom) {
							message->window = this->native.root_window;
							auto cookie = xcb_send_event_checked(
								this->native.connection,
								false,
								this->native.root_window,
								XCB_EVENT_MASK_STRUCTURE_NOTIFY,
								reinterpret_cast<const char*>(&message)
							);
							xcb_discard_reply(this->native.connection, cookie.sequence);
							xcb_flush(this->native.connection);
						}
					}
					break;
				}

				// type=0 is Error, although this isn't documented anywhere nor defined with any name
				case 0: {
					xcb_generic_error_t* error = reinterpret_cast<xcb_generic_error_t*>(event);
					fmt::print(
						"[native] non-fatal error in event loop; code {} ({}.{})\n",
						static_cast<int>(error->error_code),
						static_cast<int>(error->major_code),
						static_cast<int>(error->minor_code)
					);
					break;
				}

				default: {
					fmt::print("[native] unexpected event type {}\n", type);
				}
			}

			free(event);
		} else {
			fmt::print("[native] X11 socket closed unexpectedly\n");
			break;
		}
	}

	auto destroy = xcb_destroy_window(this->native.connection, this->native.tray_window);
	xcb_discard_reply(this->native.connection, destroy.sequence);
	xcb_flush(this->native.connection);
	xcb_disconnect(this->native.connection);
	this->native.record_thread.join();
	free(tray_opcode_atom);
	free(tray_atom);
	free(net_wm_name);
	free(net_wm_icon);
	free(net_wm_pid);
	free(wm_client_machine);
	free(utf8_string);
	free(wm_delete_window);
	free(net_wm_ping);
	free(wm_protocols);
	fmt::print("[native] stopped\n");
}

void Browser::Client::CloseNative() {
	xcb_client_message_event_t event;
	memset(&event, 0, sizeof event);
	event.response_type = XCB_CLIENT_MESSAGE;
	event.window = this->native.event_window;
	event.format = 8;
	auto cookie = xcb_send_event_checked(
		this->native.connection,
		false,
		this->native.event_window,
		XCB_EVENT_MASK_STRUCTURE_NOTIFY,
		reinterpret_cast<const char*>(&event)
	);
	xcb_discard_reply(this->native.connection, cookie.sequence);
	xcb_flush(this->native.connection);
}

void Record(xcb_connection_t* connection) {
	const xcb_query_extension_reply_t* record = xcb_get_extension_data(connection, &xcb_record_id);
	if (!record) {
		fmt::print("[native] failed to setup record extension; some features will not work\n");
		return;
	}

	xcb_record_context_t id = xcb_generate_id(connection);
	xcb_record_range_t range;
	memset(&range, 0, sizeof range);
	range.device_events.first = XCB_BUTTON_PRESS;
	range.device_events.last = XCB_BUTTON_PRESS;
	xcb_record_client_spec_t client_spec = XCB_RECORD_CS_CURRENT_CLIENTS;
	xcb_void_cookie_t context_cookie = xcb_record_create_context_checked(
		connection,
		id,
		0,
		1,
		1,
		&client_spec,
		&range
	);
	xcb_generic_error_t* error = xcb_request_check(connection, context_cookie);
	if (error) {
		fmt::print("[native] failed to create record context; some features will not work\n");
		free(error);
		return;
	}

	xcb_connection_t* record_connection = xcb_connect(nullptr, nullptr);
	if (!record_connection) {
		fmt::print("[native] failed to create record connection; some features will not work\n");
		xcb_record_disable_context (connection, id);
		xcb_record_free_context (connection, id);
		return;
	}
	if (xcb_connection_has_error(record_connection)) {
		fmt::print("[native] record connection has error; some features will not work\n");
		xcb_record_disable_context (connection, id);
		xcb_record_free_context (connection, id);
		return;
	}

	xcb_record_enable_context_cookie_t c = xcb_record_enable_context(record_connection, id);
	bool run = true;
	while(run) {
		xcb_record_enable_context_reply_t* reply = xcb_record_enable_context_reply(record_connection, c, nullptr);
		if (!reply) {
			fmt::print("[native] error in xcb_record_enable_context_reply\n");
			continue;
		}
		if (reply->client_swapped) {
			fmt::print("[native] unsupported mode: client_swapped\n");
			break;
		}

		switch (reply->category) {
			case 0: { // XRecordFromServer
				uint8_t* data = xcb_record_enable_context_data(reply);
				int data_len = xcb_record_enable_context_data_length(reply);
				if (data_len == sizeof(xcb_button_press_event_t)) {
					xcb_generic_event_t* ev = reinterpret_cast<xcb_generic_event_t*>(data);
					switch (ev->response_type) {
						case XCB_BUTTON_PRESS: {
							xcb_button_press_event_t* event = reinterpret_cast<xcb_button_press_event_t*>(ev);
							fmt::print("[native] record {}\n", event->detail);
						}
					}
				}
				break;
			}
			case 4: { // XRecordStartOfData
				fmt::print("[native] record loop started\n");
				break;
			}
			case 5: { // XRecordEndOfData
				fmt::print("[native] record loop ended\n");
				run = false;
				break;
			}
		}

		free(reply);
	}

	xcb_disconnect(record_connection);
}
