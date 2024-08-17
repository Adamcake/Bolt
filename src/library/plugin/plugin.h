#ifndef _BOLT_LIBRARY_PLUGIN_H_
#define _BOLT_LIBRARY_PLUGIN_H_

#include <stddef.h>
#include <stdint.h>

#include "../ipc.h"
#include "../rwlock/rwlock.h"

struct RenderBatch2D;
struct Plugin;
struct lua_State;

enum PluginMouseButton {
    MBLeft = 1,
    MBRight = 2,
    MBMiddle = 3,
};

// having MouseEvent has the first member of structs allows for generalisation with pointers
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

/// Struct containing "vtable" callback information for RenderBatch2D's list of vertices.
/// Unless stated otherwise, functions will be called with three params: the index, the specified
/// userdata, and an output pointer, which must be able to index the returned number of items.
struct Vertex2DFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Returns the vertex X and Y, in screen coordinates.
    void (*xy)(size_t index, void* userdata, int32_t* out);
    
    /// Returns the X and Y of the texture image associated with this vertex, in pixel coordinates.
    void (*atlas_xy)(size_t index, void* userdata, int32_t* out);

    /// Returns the W and H of the texture image associated with this vertex, in pixel coordinates.
    void (*atlas_wh)(size_t index, void* userdata, int32_t* out);

    /// Returns the U and V of this vertex in pixel coordinates, normalised from 0.0 to 1.0 within
    /// the sub-image specified by atlas xy and wh.
    void (*uv)(size_t index, void* userdata, double* out);

    /// Returns the RGBA colour of this vertex, each one normalised from 0.0 to 1.0.
    void (*colour)(size_t index, void* userdata, double* out);
};

/// Struct containing "vtable" callback information for Render3D's list of vertices.
/// NOTE: there's an important difference here from the 2D pipeline, in the atlas_meta function.
/// The atlas_xywh does not take a vertex index, but rather a meta-ID returned from atlas_meta. The
/// purpose of this is to be able to compare meta-IDs together to check if two vertices have the
/// same texture without having to actually fetch the texture info for each one.
struct Vertex3DFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Returns the vertex X Y and Z, in model coordinates.
    void (*xyz)(size_t index, void* userdata, int32_t* out);

    /// Returns a meta-ID for the texture associated with this vertex.
    size_t (*atlas_meta)(size_t index, void* userdata);

    /// Returns the XYWH of the texture image referred to by this meta-ID, in pixel coordinates.
    void (*atlas_xywh)(size_t meta, void* userdata, int32_t* out);

    /// Returns the U and V of this vertex in pixel coordinates, normalised from 0.0 to 1.0 within
    /// the sub-image specified by atlas xy and wh.
    void (*uv)(size_t index, void* userdata, double* out);

    /// Returns the RGBA colour of this vertex, each one normalised from 0.0 to 1.0.
    void (*colour)(size_t index, void* userdata, double* out);
};

/// Struct containing "vtable" callback information for textures.
/// Note that in the context of Bolt plugins, textures are always two-dimensional.
struct TextureFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Returns the ID for the associated texture object.
    size_t (*id)(void* userdata);

    /// Returns the size of this texture atlas in pixels.
    void (*size)(void* userdata, size_t* out);

    /// Compares a section of this texture to some RGBA bytes using `memcmp`. Returns true if the
    /// section matches exactly, otherwise false.
    ///
    /// Note that changing the in-game "texture compression" setting will change the contents of
    /// the texture for some images and therefore change the result of this comparison.
    uint8_t (*compare)(void* userdata, size_t x, size_t y, size_t len, const unsigned char* data);

    /// Fetches a pointer to the texture's pixel data at coordinates x and y. Doesn't do any checks
    /// on whether x and y are in-bounds. Data is always RGBA and pixel rows are always contiguous.
    uint8_t* (*data)(void* userdata, size_t x, size_t y);
};

/// Struct containing "vtable" callback information for 3D renders' transformation matrices.
struct Render3DMatrixFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Converts an XYZ coordinate from model space to world space.
    void (*to_world_space)(int x, int y, int z, void* userdata, double* out);

    /// Converts an XYZ coordinate from model space to screen space in pixels.
    void (*to_screen_space)(int x, int y, int z, void* userdata, double* out);

    /// Gets the world-space coordinate equivalent to (0,0,0) in model space.
    void (*world_pos)(void* userdata, double* out);
};

/// Struct containing "vtable" callback information for surfaces.
struct SurfaceFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Equivalent to glClearColor(r, g, b, a) & glClear().
    void (*clear)(void* userdata, double r, double g, double b, double a);

    /// Draws a rectangle from the surface, indicated by sx,sy,sw,sh, to a rectangle on the backbuffer,
    /// indicated by dx,dy,dw,dh. All values are in pixels.
    void (*draw_to_screen)(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

    /// Draws a rectangle from the surface, indicated by sx,sy,sw,sh, to a rectangle on the target surface,
    /// indicated by dx,dy,dw,dh. All values are in pixels.
    void (*draw_to_surface)(void* userdata, void* target, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
};

/// Struct containing functions initiated by plugin code, which must be set on startup, as opposed to
/// being set when a callback object is created like with other vtable structs.
struct PluginManagedFunctions {
    void (*surface_init)(struct SurfaceFunctions*, unsigned int, unsigned int, const void*);
    void (*surface_destroy)(void*);
    void (*surface_resize_and_clear)(void*, unsigned int, unsigned int);
};

struct WindowPendingInput {
    /* bools are listed at the top to make the structure smaller by having less padding in it */
    uint8_t mouse_motion;
    uint8_t mouse_left;
    uint8_t mouse_right;
    uint8_t mouse_middle;
    uint8_t mouse_left_up;
    uint8_t mouse_right_up;
    uint8_t mouse_middle_up;
    uint8_t mouse_scroll_down;
    uint8_t mouse_scroll_up;
    struct MouseEvent mouse_motion_event;
    struct MouseEvent mouse_left_event;
    struct MouseEvent mouse_right_event;
    struct MouseEvent mouse_middle_event;
    struct MouseEvent mouse_left_up_event;
    struct MouseEvent mouse_right_up_event;
    struct MouseEvent mouse_middle_up_event;
    struct MouseEvent mouse_scroll_down_event;
    struct MouseEvent mouse_scroll_up_event;
};

struct EmbeddedWindowMetadata {
    int x;
    int y;
    int width;
    int height;
};

struct EmbeddedWindow {
    uint64_t id;
    struct SurfaceFunctions surface_functions;
    struct lua_State* plugin;
    RWLock lock; // applies to the metadata
    struct EmbeddedWindowMetadata metadata;
    RWLock input_lock; // applies to the pending inputs
    struct WindowPendingInput input;
};

struct WindowInfo {
    RWLock lock; // applies to the map
    struct hashmap* map;
    RWLock input_lock; // applies to the pending inputs
    struct WindowPendingInput input;
};

struct RenderBatch2D {
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t index_count;
    uint32_t vertices_per_icon;
    uint8_t is_minimap;
    struct Vertex2DFunctions vertex_functions;
    struct TextureFunctions texture_functions;
};

struct Render3D {
    uint32_t vertex_count;
    struct Vertex3DFunctions vertex_functions;
    struct TextureFunctions texture_functions;
    struct Render3DMatrixFunctions matrix_functions;
};

struct RenderMinimapEvent {
    double angle;
    double scale;
    double x;
    double y;
};

struct SwapBuffersEvent {
#if defined(_MSC_VER)
    // MSVC doesn't allow empty structs
    void* _;
#endif
};

/// Setup the plugin library. Must be called (and return) before using any other plugin library functions,
/// including init and is_inited. This function does not have a "close" reciprocal, as it's expected
/// that this will be called immediately at startup and remain for the entire duration of the process.
void _bolt_plugin_on_startup();

/// Init the plugin library. Call _bolt_plugin_close at the end of execution, and don't double-init.
/// Must be provided with a fully-populated PluginManagedFunctions struct for backend-specific functions
/// from plugin code. The contents are copied so they do not need to remain in scope after this function ends.
void _bolt_plugin_init(const struct PluginManagedFunctions*);

/// Returns true if the plugin library is initialised (i.e. init has been called more recently than
/// close), otherwise false. This function is not thread-safe, since in a multi-threaded context,
/// the returned value could be invalidated immediately by another thread calling plugin_init.
uint8_t _bolt_plugin_is_inited();

/// Processes events for all plugin windows/browsers and renders them. Needs the width and height
/// of the game view.
void _bolt_plugin_process_windows(uint32_t, uint32_t);

/// Close the plugin library.
void _bolt_plugin_close();

/// Opens the IPC channel. (OS-specific)
void _bolt_plugin_ipc_init(BoltSocketType*);

/// Closes the IPC channel. (OS-specific)
void _bolt_plugin_ipc_close(BoltSocketType);

/// Gets a reference to the global WindowInfo struct
struct WindowInfo* _bolt_plugin_windowinfo();

/// Handle all incoming IPC messages.
void _bolt_plugin_handle_messages();

/// Creates a new instance of a plugin with its own Lua environment (lua_setfenv).
/// The `lua` param will be loaded and executed in a fresh environment, then event callbacks will be
/// sent to it until it is destroyed by the plugin being stopped.
///
/// Returns 0 on success or 1 on failure.
uint8_t _bolt_plugin_add(const char* path, struct Plugin* plugin);

/// Stops a plugin via its unique ID, as passed to `_bolt_plugin_add`.
void _bolt_plugin_stop(char* id, uint32_t id_length);

/// Sends a SwapBuffers event to all plugins.
void _bolt_plugin_handle_swapbuffers(struct SwapBuffersEvent*);

/// Sends a RenderBatch2D to all plugins.
void _bolt_plugin_handle_render2d(struct RenderBatch2D*);

/// Sends a Render3D to all plugins.
void _bolt_plugin_handle_render3d(struct Render3D*);

/// Sends a RenderMinimap to all plugins.
void _bolt_plugin_handle_minimap(struct RenderMinimapEvent*);

#endif
