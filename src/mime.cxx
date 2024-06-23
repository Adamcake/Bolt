#include "mime.hxx"

#include <map>
#include <string>

// Map of mime types for files loaded from disk
const std::map<std::string, const char*> map({
	{".txt", "text/plain"},
	{".html", "text/html"},
	{".js", "text/javascript"},
	{".css", "text/css"},

	{".avif", "image/avif"},
	{".avifs", "image/avif"},
	{".bmp", "image/bmp"},
	{".gif", "image/gif"},
	{".jpg", "image/jpg"},
	{".jpeg", "image/jpeg"},
	{".png", "image/png"},
	{".svg", "image/svg+xml"},

	{".aac", "audio/aac"},
	{".mp4", "audio/aac"},
	{".m4a", "audio/aac"},
	{".flac", "audio/flac"},
	{".mp4a", "audio/mp4"},
	{".oga", "audio/ogg"},
	{".ogg", "audio/ogg"},
	{".opus", "audio/opus"},
	{".wav", "audio/wav"},

	{".otf", "font/otf"},
	{".woff", "font/woff"},
	{".woff2", "font/woff2"},
});

const char* GetMimeType(std::filesystem::path& path) {
	auto i = map.find(path.extension().string());
	return (i != map.end()) ? i->second : nullptr;
}
