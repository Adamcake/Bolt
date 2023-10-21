#ifndef _BOLT_FILE_MANAGER_HXX_
#define _BOLT_FILE_MANAGER_HXX_
#include "include/cef_base.h"
#include "include/internal/cef_string.h"

#include <string_view>

namespace FileManager {
	struct File {
		/// File contents in bytes, or nullptr for a file that doesn't exist (equivalent of 404)
		const unsigned char* contents;

		/// Number of bytes in contents
		size_t size;

		/// MIME type of this file - not initialised if contents is nullptr
		CefString mime_type;
	};

	class FileManager: public CefBaseRefCounted {
		public:
			/// Fetches a file, as if from a web server, by its abolsute path.
			/// For example, in the url "https://adamcake.com/index.html", the path would be "/index.html".
			virtual File get(std::string_view path) const = 0;

			/// Any File obtained from get() must be passed to free() exactly once and must never be
			/// accessed after passing it to free().
			virtual void free(File) const = 0;
	};
}

#endif
