#include "../browser/client.hxx"
#include "src/native/native.hxx"

#include <thread>
#include <xcb/record.h>
#include <fmt/core.h>

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
						fmt::print("[native] closing\n");
						run = false;
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
			fmt::print("[native] xcb_wait_for_event returned nullptr\n");
			break;
		}
	}

	xcb_disconnect(this->native.connection);
	this->native.record_thread.join();
	fmt::print("[native] stopped\n");
}

void Browser::Client::CloseNative() {
	xcb_client_message_event_t* event = new xcb_client_message_event_t;
	event->response_type = XCB_CLIENT_MESSAGE;
	event->window = this->native.event_window;
	event->format = 8;
	auto cookie = xcb_send_event_checked(
		this->native.connection,
		false,
		this->native.event_window,
		XCB_EVENT_MASK_STRUCTURE_NOTIFY,
		reinterpret_cast<const char*>(event)
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
