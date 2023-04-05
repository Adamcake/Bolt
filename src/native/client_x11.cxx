#include "../browser/client.hxx"
#include "src/native/native.hxx"

#include <thread>
#include <xcb/record.h>
#include <fmt/core.h>

// Uses the xcb-record extension to report mouse button events to main event loop
void Record(xcb_connection_t*);

bool Browser::Client::SetupNative() {
	this->native.connection = xcb_connect(nullptr, nullptr);
	if (!this->native.connection) {
		fmt::print("[native] connection is null\n");
		return false;
	}
	if (xcb_connection_has_error(this->native.connection)) {
		fmt::print("[native] connection has error\n");
		return false;
	}
	xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(this->native.connection));
	if (screens.rem != 1) {
		fmt::print("[native] {} root screens found; expected exactly 1\n", screens.rem);
		xcb_disconnect(this->native.connection);
		return false;
	}
	this->native.root_window = screens.data->root;

	this->native.record_thread = std::thread(Record, this->native.connection);

	return true;
}

void Browser::Client::Run() {
	xcb_generic_event_t* event;
	while (true) {
		event = xcb_wait_for_event(this->native.connection);
		if (event) {
			// Top bit indicates whether the event came from SendEvent
			uint8_t type = event->response_type & 0b01111111;
			switch(type) {
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
}

void Browser::Client::CloseNative() {
	xcb_disconnect(this->native.connection);
	this->native.record_thread.join();
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
				fmt::print("[native] record\n");
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

	xcb_record_disable_context (connection, id);
	xcb_record_free_context (connection, id);
	xcb_disconnect (record_connection);
}
