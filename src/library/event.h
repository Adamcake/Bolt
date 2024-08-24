#ifndef _BOLT_LIBRARY_EVENT_H_
#define _BOLT_LIBRARY_EVENT_H_
#include <stdint.h>

enum PluginMouseButton {
    MBLeft = 1,
    MBRight = 2,
    MBMiddle = 3,
};

struct MouseEvent {
    int16_t x;
    int16_t y;
    uint8_t ctrl;
    uint8_t shift;
    uint8_t meta;
    uint8_t alt;
    uint8_t capslock;
    uint8_t numlock;
    uint8_t mb_left;
    uint8_t mb_right;
    uint8_t mb_middle;
};

struct ResizeEvent {
    uint16_t width;
    uint16_t height;
};

struct MouseMotionEvent {
    struct MouseEvent details;
};
struct MouseButtonEvent {
    struct MouseEvent details;
    uint8_t button; // 1 left, 2 right, 3 middle
};
struct MouseScrollEvent {
    struct MouseEvent details;
    uint8_t direction; // 0 down, 1 up
}; 

#endif
