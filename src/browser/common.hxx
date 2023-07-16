#ifndef _BOLT_COMMON_HXX_
#define _BOLT_COMMON_HXX_

#include "include/cef_base.h"

#include <fstream>
#include <string>
#include <vector>

namespace Browser {
	/// Details used to create a browser window
	struct Details {
		int preferred_width;
		int preferred_height;
		int startx;
		int starty;
		bool center_on_open; // startx and starty will be ignored if center_on_open is true
		bool resizeable;
		bool frame;
		bool controls_overlay;
		bool is_devtools = false;
	};

	struct InternalFile {
		bool success;
		std::vector<unsigned char> data;
		CefString mime_type;

		InternalFile(const char* filename, CefString mime_type): mime_type(mime_type) {
			std::ifstream file(filename, std::ios::binary);
			file.seekg(0, std::ios::end);
			std::streamsize size = file.tellg();
			if (size < 0) {
				this->success = false;
				file.close();
				return;
			}
			file.seekg(0, std::ios::beg);
			this->data.resize(size);
			if (file.read(reinterpret_cast<char*>(this->data.data()), size)) {
				this->success = true;
			} else {
				this->success = false;
			}
			file.close();
		}
	};
}

#endif
