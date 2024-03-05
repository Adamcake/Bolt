#include <lua.h>

/*
 * The functions in this file are implemented in `plugin.c`. Their forward-declarations have been
 * separated out into this file purely to make it easier to find documentation on the plugin API.
 * 
 * There are no plugin API functions other than the ones in this file. They are listed here in no
 * particular order, other than an approximate attempt to group relevant functions together.
 * 
 * To access this API from a plugin, load the API and use its functions like so. For the purpose of
 * future-proofing, it's important to check the API version is compatible with your plugin, using
 * either `bolt.apiversion` or `bolt.checkversion`.
 * Note that `bolt.checkversion` in Lua is equivalent to `api_checkversion` declared in this file.
 * ```
 * local bolt = require("bolt")
 * bolt.checkversion(1, 0)
 * --...
 * ```
 *
 * After that, pass Lua functions to the `bolt.setcallback...` functions to set event callbacks.
 *
 * The 2D rendering pipeline is fairly simple. Images are drawn in large batches of vertices,
 * usually 6 vertices per icon (three per triangle, two triangles.) Plugins should call the
 * `verticesperimage` function instead of hard-coding the number 6. Each individual vertex has an
 * associated texture; the assumption that all six will have the same texture appears to be a safe
 * one as of right now, but who knows what might break in future engine updates?
 *
 * The 3D rendering pipeline is *far* more complicated, but is mostly tracked internally by Bolt
 * in order to provide a simple API for plugins. 3D renders are not batched, so a 3D render event
 * will contain all the vertices for one whole model. However, each vertex still has its own
 * texture, and many models do have multiple textures. Plugins usually do not need to check every
 * single vertex - a single vertex with a known texture image on it would usually be sufficient.
 *
 * Most coordinates below are specifically "world coordinates", which work on a scale of 512 per
 * tile. So if you move one tile to the east, your X in world coordinates increases by 512.
 */

/// [-0, +2, -]
/// Returns the Bolt API major version and minor version, in that order.
/// Plugins should call this function on startup and, if the major version is one it doesn't
/// recognise, it should exit by calling `error()`. The minor version however does not need to be
/// checked, as minor versions will never contain breaking changes; they may add features, though,
/// and the minor version can be used to check for the existence of those features.
///
/// For compatibility reasons, there will never be a breaking change to this function.
static int api_apiversion(lua_State*);

/// [-2, +0, v]
/// Simple alternative to `apiversion()` which calls `error()` if any of these conditions is true:
/// - the first parameter is not equal to Bolt's major version
/// - the second parameter is greater than Bolt's minor version
///
/// Due to the way `error()` is implemented in Lua, this function will never return on failure.
///
/// For compatibility reasons, there will never be a breaking change to this function.
static int api_checkversion(lua_State*);

/// [-0, +1, -]
/// Returns a monotonic time as an integer, in microseconds.
/// 
/// This function can be used for timing. The number it returns is arbitrary - that is, it's the
/// number of microseconds that have elapsed since an arbitrary point in time - therefore it's not
/// useful for anything other than to call this function multiple times and compare the results.
///
/// Note that on a 32-bit CPU this number will overflow back to 0 every ~4296 seconds, which is
/// slightly more than an hour. On a 64-bit CPU, it will overflow every ~18 trillion seconds, or
/// around 585 millennia. Playing on a 32-bit CPU is therefore not advisable, but extra precautions
/// must be taken if a plugin wishes to support 32-bit CPUs while using this function.
static int api_time(lua_State*);

/// [-2, +1, -]
/// Creates a surface with the given width and height, and returns it as a userdata object. The
/// surface will initially be fully transparent.
///
/// A surface can be drawn onto with the rendering functions and can be overlaid onto the screen
/// by calling `surface:drawtoscreen()` during a swapbuffers callback.
///
/// All of the member functions of surface objects can be found in this file, prefixed with
/// "api_surface_".
static int api_createsurface(lua_State*);

/// [-1, +0, -]
/// Sets a callback function for SwapBuffers events, overwriting the previous callback, if any.
/// Passing a non-function (ideally `nil`) will restore the default setting, which is to have no
/// handler for SwapBuffers events.
///
/// In simple terms, SwapBuffers represents the end of a frame's rendering, as well as the start of
/// the next one. The callback will be called with one param, a SwapBuffers userdata object, which
/// currently has no purpose.
///
/// The callback will be called every frame - that's anywhere from 5 to 150+ times per second - so
/// avoid using it for anything time-consuming.
static int api_setcallbackswapbuffers(lua_State*);

/// [-1, +0, -]
/// Sets a callback function for rendering of 2D images, overwriting the previous callback, if any.
/// Passing a non-function (ideally `nil`) will restore the default setting, which is to have no
/// handler for 2D rendering.
///
/// Each time a batch of 2D images is rendered, the callback will be called with one param, that
/// being a 2D batch object. All of the member functions of 2D batch objects can be found in this
/// file, prefixed with with "api_batch2d_". The batch object and everything contained by it will
/// become invalid as soon as the callback ends, so do not retain them.
///
/// The callback will be called an extremely high amount of times per second, so great care should
/// be taken to determine as quickly as possible whether any image is of interest or not, such as
/// by checking each image's width and height.
static int api_setcallback2d(lua_State*);

/// [-1, +0, -]
/// Sets a callback function for rendering of 3D models, overwriting the previous callback, if any.
/// Passing a non-function (ideally `nil`) will restore the default setting, which is to have no
/// handler for 2D rendering.
///
/// Each time a 3D model is rendered, the callback will be called with one param, that being a 3D
/// render object. All of the member functions of 3D render objects can be found in this file,
/// prefixed with "api_render3d". The object and everything contained by it will become invalid as
/// soon as the callback ends, so do not retain them.
///
/// The callback will be called an extremely high amount of times per second, so great care should
/// be taken to determine as quickly as possible whether any image is of interest or not, such as
/// by checking the model's vertex count.
static int api_setcallback3d(lua_State*);

/// [-1, +0, -]
/// Sets a callback function for rendering of a minimap background image, overwriting the previous
/// callback, if any. Passing a non-function (ideally `nil`) will restore the default setting,
/// which is to have no handler for minimap rendering.
///
/// The game renders chunks of 3D land to a large 2048x2048 texture and caches it until the player
/// moves far enough away that it needs to be remade. A scaled and rotated section of this image is
/// drawn to a smaller texture once per frame while the minimap is on screen. As such, plugins can
/// expect to get a maximum of one minimap event per frame (i.e. between each SwapBuffers event.)
///
/// The callback will be called with one param, that being a minimap render object. All of the
/// member functions of that object can be found in this file, prefixed with "api_minimap_".
///
/// The pixel contents cannot be examined directly, however it's possible to query the image angle,
/// image scale (zoom level), and a rough estimate of the tile position it's centered on.
static int api_setcallbackminimap(lua_State*);

/// [-1, +1, -]
/// Returns the number of vertices in a 2D batch object.
static int api_batch2d_vertexcount(lua_State*);

/// [-1, +1, -]
/// Returns the number of vertices per individual image in this batch. At time of writing, this
/// will always return 6 (enough to draw two separate triangles.)
///
/// If the game engine is ever improved to be able to draw an image with only 4 vertices (enough
/// to draw a solid rectangle, e.g. using GL_TRIANGLE_STRIP), then that will be indicated here, so
/// it's recommended to use this function instead of hard-coding the number 6.
static int api_batch2d_verticesperimage(lua_State*);

/// [-1, +1, -]
/// Returns true if this render targets the minimap texture. There will usually be a maximum of one
/// batch per frame targeting the minimap texture.
static int api_batch2d_isminimap(lua_State*);

/// [-1, +2, -]
/// Returns the width and height of the target area of this render, in pixels.
///
/// If `isminimap()` is true, this will be the size of the minimap texture - usually 256x256.
///
/// If `isminimap()` is false, this will be proprortional to the size of the inner area of the game
/// window - that is, if the user has an interface scaling other than 100%, it will be larger or
/// smaller than that area, proportionally.
static int api_batch2d_targetsize(lua_State*);

/// [-2, +2, -]
/// Given an index of a vertex in a batch, returns its X and Y in screen coordinates.
static int api_batch2d_vertexxy(lua_State*);

/// [-2, +2, -]
/// Given an index of a vertex in a batch, returns the X and Y of its associated image in the
/// batch's texture atlas, in pixel coordinates.
static int api_batch2d_vertexatlasxy(lua_State*);

/// [-2, +2, -]
/// Given an index of a vertex in a batch, returns the width and height of its associated image in
/// the batch's texture atlas, in pixel coordinates.
static int api_batch2d_vertexatlaswh(lua_State*);

/// [-2, +2, -]
/// Given an index of a vertex in a batch, returns the vertex's associated "UV" coordinates.
///
/// The values will be floating-point numbers in the range 0.0 - 1.0. They are relative to the
/// position of the overall image in the texture atlas, queried by vertexatlasxy and vertexatlaswh.
static int api_batch2d_vertexuv(lua_State*);

/// [-2, +4, -]
/// Given an index of a vertex in a batch, returns the red, green, blue and alpha values for that
/// vertex, in that order. All four values will be floating-point numbers in the range 0.0 - 1.0.
///
/// Also aliased as "vertexcolor" to keep the Americans happy.
static int api_batch2d_vertexcolour(lua_State*);

/// [-1, +1, -]
/// Returns the unique ID of the texture associated with this render. There will always be one (and
/// only one) texture associated with a 2D render batch. These textures are "atlased", meaning they
/// will contain a large amount of small images, and each set of vertices in the batch may relate
/// to different images in the same texture.
///
/// The plugin API does not have a way to get a texture by its ID; this is intentional. The purpose
/// of this function is to be able to compare texture IDs together to check if the current texture
/// atlas is the same one that was used in a previous render.
static int api_batch2d_textureid(lua_State*);

/// [-1, +2, -]
/// Returns the size of the overall texture atlas associated with this render, in pixels.
static int api_batch2d_texturesize(lua_State*);

/// [-4, +1, -]
/// Compares a section of the texture atlas for this batch to some RGBA data. For example:
/// 
/// `batch:comparetexture(64, 128, {0xFF, 0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF})`
///
/// This would check if the pixels at 64,128 and 65,128 are red. The bytes must match exactly
/// for the function to return true, otherwise it will return false.
///
/// Normally the X and Y coordinates should be calculated from `vertexatlasxy()` and
/// `vertexatlaswh()`. Comparing a whole block of pixels at once by this method is relatively fast,
/// but can only be done one row at a time.
static int api_batch2d_texturecompare(lua_State*);

/// [-1, +1, -]
/// Returns the angle at which the minimap background image is being rendered, in radians.
/// 
/// The angle is 0 when upright (facing directly north), and increases counter-clockwise (note that
/// turning the camera clockwise rotates the minimap counter-clockwise and vice versa.)
static int api_minimap_angle(lua_State*);

/// [-1, +1, -]
/// Returns the scale at which the minimap background image is being rendered.
///
/// This indicates how far in or out the player has zoomed their minimap. It appears to be capped
/// between roughly 0.5 and 3.5.
static int api_minimap_scale(lua_State*);

/// [-1, +2, -]
/// Returns an estimate of the X and Y position the minimap is centered on, in world coordinates.
/// 
/// This is only a rough estimate and can move around a lot even while standing still. It usually
/// doesn't vary by more than half a tile.
static int api_minimap_position(lua_State*);

/// [-(1|4|5), +0, -]
/// Deletes any previous contents of the surface and sets it to contain a single colour and alpha.
///
/// If four params are provided, they must be RGBA values, in that order, in the range 0.0-1.0.
///
/// If three params are provided, they must be RGB values, in that order, in the range 0.0-1.0. The
/// alpha value will be inferred to be 1.0.
///
/// If no params are provided, the alpha values will be inferred to be 0.0 (fully transparent),
/// with the red, green and blue values undefined.
static int api_surface_clear(lua_State*);

/// [-9, +0, -]
/// Draws a section of the surface directly onto a section of the screen's backbuffer. If used
/// outside a swapbuffers event, this is unlikely to have any visible effect.
///
/// Paramaters are source X,Y,W,H followed by destination X,Y,W,H, all in pixels.
static int api_surface_drawtoscreen(lua_State*);

/// [-1, +1, -]
/// Returns the number of vertices in a 3D render object (i.e. a model).
static int api_render3d_vertexcount(lua_State*);

/// [-2, +3, -]
/// Given an index of a vertex in a model, returns its X Y and Z in model coordinates.
static int api_render3d_vertexxyz(lua_State*);

/// [-2, +1, -]
/// Given an index of a vertex in a model, returns a meta-ID relating to its associated image.
///
/// Much like 2D batches, 3D renders always have exactly one texture atlas associated with them,
/// but each vertex can still be associated with a different image from the atlas. To allow for
/// finding if two vertices share the same image without having to fetch and compare the whole
/// image data for each one, an extra step was added to the API: plugins must query the vertex's
/// image meta-ID, then use that ID to fetch texture details (if desired). Meta-IDs should not be
/// retained and used outside the current callback, as the game may invalidate them.
static int api_render3d_vertexmeta(lua_State*);

/// Given an image meta-ID from this render, fetches the X Y W and H of its associated image in the
/// texture atlas, in pixel coordinates.
static int api_render3d_atlasxywh(lua_State*);

/// [-2, +2, -]
/// Given an index of a vertex in a model, returns the vertex's associated "UV" coordinates.
///
/// The values will be floating-point numbers in the range 0.0 - 1.0. They are relative to the
/// position of the overall image in the texture atlas, queried by atlasxywh.
static int api_render3d_vertexuv(lua_State*);

/// [-2, +4, -]
/// Given an index of a vertex in a model, returns the red, green, blue and alpha values for that
/// vertex, in that order. All four values will be floating-point numbers in the range 0.0 - 1.0.
///
/// Also aliased as "vertexcolor" to keep the Americans happy.
static int api_render3d_vertexcolour(lua_State*);

/// [-1, +1, -]
/// Returns the unique ID of the texture associated with this render. There will always be one (and
/// only one) texture associated with a 3D model render. These textures are "atlased", meaning they
/// will contain a large amount of small images, and each vertex in the model may relate to
/// different images in the same texture.
///
/// The plugin API does not have a way to get a texture by its ID; this is intentional. The purpose
/// of this function is to be able to compare texture IDs together to check if the current texture
/// atlas is the same one that was used in a previous render.
static int api_render3d_textureid(lua_State*);

/// [-1, +2, -]
/// Returns the size of the overall texture atlas associated with this render, in pixels.
static int api_render3d_texturesize(lua_State*);

/// [-4, +1, -]
/// Compares a section of the texture atlas for this render to some RGBA data. For example:
/// 
/// `render:comparetexture(64, 128, {0xFF, 0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF})`
///
/// This would check if the pixels at 64,128 and 65,128 are red. The bytes must match exactly
/// for the function to return true, otherwise it will return false.
///
/// Normally the X and Y coordinates should be calculated from `atlasxywh()`. Comparing a whole
/// block of pixels at once by this method is relatively fast, but can only be done one row at a
/// time.
static int api_render3d_texturecompare(lua_State*);
