// stuff lifted from xcb.h/xproto.h

#include <stdint.h>

#define XCB_MAP_NOTIFY 19
#define XCB_RESIZE_REQUEST 25

typedef struct xcb_connection_t xcb_connection_t;
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_visualid_t;
typedef uint32_t xcb_colormap_t;
typedef uint32_t xcb_drawable_t;

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

typedef struct {
    unsigned int sequence;
} xcb_get_geometry_cookie_t;

typedef struct xcb_get_geometry_reply_t {
    uint8_t      response_type;
    uint8_t      depth;
    uint16_t     sequence;
    uint32_t     length;
    xcb_window_t root;
    int16_t      x;
    int16_t      y;
    uint16_t     width;
    uint16_t     height;
    uint16_t     border_width;
    uint8_t      pad0[2];
} xcb_get_geometry_reply_t;

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
