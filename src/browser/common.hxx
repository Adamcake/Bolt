#ifndef _BOLT_COMMON_HXX_
#define _BOLT_COMMON_HXX_

namespace Browser {
	/// Details used to create a browser window
	struct Details {
		int min_width;
		int min_height;
		int max_width;
		int max_height;
		int preferred_width;
		int preferred_height;
		int startx;
		int starty;
		bool resizeable;
		bool frame;
		bool controls_overlay;
	};
}

#endif
