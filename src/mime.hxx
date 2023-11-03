#ifndef _BOLT_MIME_HXX_
#define _BOLT_MIME_HXX_
#include <filesystem>

/// Returns a mime-type string for known file types or nullptr for unknown types.
/// The file does not need to exist and will not be accessed in any way if it does. Only the extension is used.
const char* GetMimeType(std::filesystem::path& path);

#endif
