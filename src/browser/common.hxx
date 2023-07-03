#ifndef _BOLT_COMMON_HXX_
#define _BOLT_COMMON_HXX_

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
	};
}

#endif
