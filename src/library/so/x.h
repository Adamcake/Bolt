#ifndef _BOLT_LIBRARY_SO_X_H_
#define _BOLT_LIBRARY_SO_X_H_
// stuff lifted from xcb.h/xproto.h/xinput.h

#include <stdint.h>

// Note: I *think* this is a hard-coded value, but I spent over an hour trying to find where it's
// actually defined in X11 or XCB and came up without even a single lead. It seems to be hard-coded
// into the game client, and since I don't have the option of calling xcb_query_extension out-of-band
// (causes a client crash), I have no choice but to do the same thing.
#define XINPUTEXTENSION 131

#define XCB_NONE 0L
#define XCB_PROP_MODE_REPLACE 0
#define XCB_ATOM_WM_HINTS 35

#define XCB_KEY_PRESS 2
#define XCB_KEY_RELEASE 3
#define XCB_BUTTON_PRESS 4
#define XCB_BUTTON_RELEASE 5
#define XCB_MOTION_NOTIFY 6
#define XCB_ENTER_NOTIFY 7
#define XCB_LEAVE_NOTIFY 8
#define XCB_FOCUS_IN 9
#define XCB_FOCUS_OUT 10
#define XCB_KEYMAP_NOTIFY 11
#define XCB_EXPOSE 12
#define XCB_VISIBILITY_NOTIFY 15
#define XCB_DESTROY_NOTIFY 17
#define XCB_UNMAP_NOTIFY 18
#define XCB_MAP_NOTIFY 19
#define XCB_REPARENT_NOTIFY 21
#define XCB_CONFIGURE_NOTIFY 22
#define XCB_PROPERTY_NOTIFY 28
#define XCB_CLIENT_MESSAGE 33
#define XCB_GE_GENERIC 35

#define XCB_INPUT_BUTTON_PRESS 4
#define XCB_INPUT_BUTTON_RELEASE 5
#define XCB_INPUT_MOTION 6
#define XCB_INPUT_ENTER 7
#define XCB_INPUT_LEAVE 8
#define XCB_INPUT_RAW_BUTTON_PRESS 15
#define XCB_INPUT_RAW_BUTTON_RELEASE 16
#define XCB_INPUT_RAW_MOTION 17

typedef struct xcb_connection_t xcb_connection_t;
typedef uint32_t xcb_atom_t;
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_pixmap_t;
typedef uint32_t xcb_visualid_t;
typedef uint32_t xcb_colormap_t;
typedef uint32_t xcb_drawable_t;
typedef uint32_t xcb_timestamp_t;
typedef uint8_t xcb_button_t;

typedef uint16_t xcb_input_device_id_t;
typedef int32_t xcb_input_fp1616_t;

typedef unsigned long XWindow;
typedef unsigned long XCursor;

typedef struct {
	int x, y;
	int width, height;
	int border_width;
	int depth;
	void* visual;
	XWindow root;
	int class;
	int bit_gravity;
	int win_gravity;
	int backing_store;
	unsigned long backing_planes;
	unsigned long backing_pixel;
	int save_under;
	unsigned long colormap;
	int map_installed;
	int map_state;
	long all_event_masks;
	long your_event_mask;
	long do_not_propagate_mask;
	int override_redirect;
	void* screen;
} XWindowAttributes;

typedef struct {
	uint32_t flags;
	uint32_t input;
	uint32_t initial_state;
	xcb_pixmap_t icon_pixmap;
	xcb_window_t icon_window;
	int32_t icon_x;
    int32_t icon_y;
	xcb_pixmap_t icon_mask;
    uint32_t _; // ICCCM only specifies 8 fields but xlib and everyone else seem to use structs with 9 fields.
} XWMHints;

typedef struct {
    unsigned int sequence;
} xcb_void_cookie_t;

typedef enum xcb_cw_t {
    XCB_CW_BACK_PIXMAP = 1,
    XCB_CW_BACK_PIXEL = 2,
    XCB_CW_BORDER_PIXMAP = 4,
    XCB_CW_BORDER_PIXEL = 8,
    XCB_CW_BIT_GRAVITY = 16,
    XCB_CW_WIN_GRAVITY = 32,
    XCB_CW_BACKING_STORE = 64,
    XCB_CW_BACKING_PLANES = 128,
    XCB_CW_BACKING_PIXEL = 256,
    XCB_CW_OVERRIDE_REDIRECT = 512,
    XCB_CW_SAVE_UNDER = 1024,
    XCB_CW_EVENT_MASK = 2048,
    XCB_CW_DONT_PROPAGATE = 4096,
    XCB_CW_COLORMAP = 8192,
    XCB_CW_CURSOR = 16384
} xcb_cw_t;

typedef struct {
    uint8_t  response_type;
    uint8_t  pad0;
    uint16_t sequence;
    uint32_t pad[7];
    uint32_t full_sequence;
} xcb_generic_event_t;

typedef struct {
    uint8_t  response_type;
    uint8_t  error_code;
    uint16_t sequence;
    uint32_t resource_id;
    uint16_t minor_code;
    uint8_t  major_code;
    uint8_t  pad0;
    uint32_t pad[5];
    uint32_t full_sequence;
} xcb_generic_error_t;

typedef struct xcb_query_extension_cookie_t {
    unsigned int sequence;
} xcb_query_extension_cookie_t;

typedef struct xcb_button_press_event_t {
    uint8_t         response_type;
    xcb_button_t    detail;
    uint16_t        sequence;
    xcb_timestamp_t time;
    xcb_window_t    root;
    xcb_window_t    event;
    xcb_window_t    child;
    int16_t         root_x;
    int16_t         root_y;
    int16_t         event_x;
    int16_t         event_y;
    uint16_t        state;
    uint8_t         same_screen;
    uint8_t         pad0;
} xcb_button_press_event_t;
typedef xcb_button_press_event_t xcb_button_release_event_t;

typedef struct xcb_map_notify_event_t {
    uint8_t      response_type;
    uint8_t      pad0;
    uint16_t     sequence;
    xcb_window_t event;
    xcb_window_t window;
    uint8_t      override_redirect;
    uint8_t      pad1[3];
} xcb_map_notify_event_t;

typedef struct xcb_resize_request_event_t {
    uint8_t      response_type;
    uint8_t      pad0;
    uint16_t     sequence;
    xcb_window_t window;
    uint16_t     width;
    uint16_t     height;
} xcb_resize_request_event_t;

typedef struct xcb_ge_generic_event_t {
    uint8_t  response_type;
    uint8_t  extension;
    uint16_t sequence;
    uint32_t length;
    uint16_t event_type;
    uint8_t  pad0[22];
    uint32_t full_sequence;
} xcb_ge_generic_event_t;

typedef struct xcb_query_extension_reply_t {
    uint8_t  response_type;
    uint8_t  pad0;
    uint16_t sequence;
    uint32_t length;
    uint8_t  present;
    uint8_t  major_opcode;
    uint8_t  first_event;
    uint8_t  first_error;
} xcb_query_extension_reply_t;

typedef struct xcb_get_property_reply_t {
    uint8_t    response_type;
    uint8_t    format;
    uint16_t   sequence;
    uint32_t   length;
    xcb_atom_t type;
    uint32_t   bytes_after;
    uint32_t   value_len;
    uint8_t    pad0[12];
} xcb_get_property_reply_t;

typedef struct xcb_motion_notify_event_t {
    uint8_t         response_type;
    uint8_t         detail;
    uint16_t        sequence;
    xcb_timestamp_t time;
    xcb_window_t    root;
    xcb_window_t    event;
    xcb_window_t    child;
    int16_t         root_x;
    int16_t         root_y;
    int16_t         event_x;
    int16_t         event_y;
    uint16_t        state;
    uint8_t         same_screen;
    uint8_t         pad0;
} xcb_motion_notify_event_t;

typedef struct xcb_enter_notify_event_t {
    uint8_t         response_type;
    uint8_t         detail;
    uint16_t        sequence;
    xcb_timestamp_t time;
    xcb_window_t    root;
    xcb_window_t    event;
    xcb_window_t    child;
    int16_t         root_x;
    int16_t         root_y;
    int16_t         event_x;
    int16_t         event_y;
    uint16_t        state;
    uint8_t         mode;
    uint8_t         same_screen_focus;
} xcb_enter_notify_event_t;
typedef xcb_enter_notify_event_t xcb_leave_notify_event_t;

typedef struct xcb_focus_in_event_t {
    uint8_t      response_type;
    uint8_t      detail;
    uint16_t     sequence;
    xcb_window_t event;
    uint8_t      mode;
    uint8_t      pad0[3];
} xcb_focus_in_event_t;
typedef xcb_focus_in_event_t xcb_focus_out_event_t;

typedef struct xcb_configure_notify_event_t {
    uint8_t      response_type;
    uint8_t      pad0;
    uint16_t     sequence;
    xcb_window_t event;
    xcb_window_t window;
    xcb_window_t above_sibling;
    int16_t      x;
    int16_t      y;
    uint16_t     width;
    uint16_t     height;
    uint16_t     border_width;
    uint8_t      override_redirect;
    uint8_t      pad1;
} xcb_configure_notify_event_t;

typedef struct xcb_input_modifier_info_t {
    uint32_t base;
    uint32_t latched;
    uint32_t locked;
    uint32_t effective;
} xcb_input_modifier_info_t;

typedef struct xcb_input_group_info_t {
    uint8_t base;
    uint8_t latched;
    uint8_t locked;
    uint8_t effective;
} xcb_input_group_info_t;

typedef struct xcb_input_button_press_event_t {
    uint8_t                   response_type;
    uint8_t                   extension;
    uint16_t                  sequence;
    uint32_t                  length;
    uint16_t                  event_type;
    xcb_input_device_id_t     deviceid;
    xcb_timestamp_t           time;
    uint32_t                  detail;
    xcb_window_t              root;
    xcb_window_t              event;
    xcb_window_t              child;
    uint32_t                  full_sequence;
    xcb_input_fp1616_t        root_x;
    xcb_input_fp1616_t        root_y;
    xcb_input_fp1616_t        event_x;
    xcb_input_fp1616_t        event_y;
    uint16_t                  buttons_len;
    uint16_t                  valuators_len;
    xcb_input_device_id_t     sourceid;
    uint8_t                   pad0[2];
    uint32_t                  flags;
    xcb_input_modifier_info_t mods;
    xcb_input_group_info_t    group;
} xcb_input_button_press_event_t;
typedef xcb_input_button_press_event_t xcb_input_motion_event_t;
typedef xcb_input_button_press_event_t xcb_input_button_release_event_t;

typedef struct xcb_input_enter_event_t {
    uint8_t                   response_type;
    uint8_t                   extension;
    uint16_t                  sequence;
    uint32_t                  length;
    uint16_t                  event_type;
    xcb_input_device_id_t     deviceid;
    xcb_timestamp_t           time;
    xcb_input_device_id_t     sourceid;
    uint8_t                   mode;
    uint8_t                   detail;
    xcb_window_t              root;
    xcb_window_t              event;
    xcb_window_t              child;
    uint32_t                  full_sequence;
    xcb_input_fp1616_t        root_x;
    xcb_input_fp1616_t        root_y;
    xcb_input_fp1616_t        event_x;
    xcb_input_fp1616_t        event_y;
    uint8_t                   same_screen;
    uint8_t                   focus;
    uint16_t                  buttons_len;
    xcb_input_modifier_info_t mods;
    xcb_input_group_info_t    group;
} xcb_input_enter_event_t;
typedef xcb_input_enter_event_t xcb_input_leave_event_t;

#endif
