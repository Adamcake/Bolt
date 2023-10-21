#ifndef _BOLT_FILE_MANAGER_DIRECTORY_HXX_
#define _BOLT_FILE_MANAGER_DIRECTORY_HXX_
#include "../file_manager.hxx"

#include <thread>
#include <filesystem>

namespace FileManager {
	/// Serves the contents of a directory from disk, also watching the directory for changes if the
	/// current platform supports it
	class Directory: public FileManager {
		std::filesystem::path path;

#if defined(__linux__)
		std::thread inotify_thread;
		int inotify_fd;
		int inotify_wd;
#endif

		public:
			/// Initialise with the directory to serve. The given path is assumed to be a directory.
			Directory(std::filesystem::path);

			File get(std::string_view) const override;
			void free(File) const override;
			void StopFileManager() override;
	};
}

#endif
