#ifndef _BOLT_REQUEST_H_
#define _BOLT_REQUEST_H_
#include <string_view>
#include <functional>
#include "include/cef_parser.h" // note: unused-includes warning is incorrect, this is used by the PQ macros

namespace Browser {
    /// Goes through all the key-value pairs in the given query string and calls the callback for each one.
    void ParseQuery(std::string_view query, std::function<void(const std::string_view& key, const std::string_view& val)> callback, char delim = '&');
}

/* the following macros are intended for use in and around ParseQuery callbacks */

#if defined(_WIN32)
typedef std::wstring QSTRING;
#define PQTOSTRING ToWString
#else
typedef std::string QSTRING;
#define PQTOSTRING ToString
#endif

#define PQCEFSTRING(KEY) \
if (key == #KEY) { \
	has_##KEY = true; \
	KEY = CefURIDecode(std::string(val), true, (cef_uri_unescape_rule_t)(UU_SPACES | UU_PATH_SEPARATORS | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS | UU_REPLACE_PLUS_WITH_SPACE)); \
	return; \
}

#define PQSTRING(KEY) \
if (key == #KEY) { \
	has_##KEY = true; \
	KEY = CefURIDecode(std::string(val), true, (cef_uri_unescape_rule_t)(UU_SPACES | UU_PATH_SEPARATORS | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS | UU_REPLACE_PLUS_WITH_SPACE)).PQTOSTRING(); \
	return; \
}

#define PQINT(KEY) \
if (key == #KEY) { \
	bool _pq_negative = false; \
	has_##KEY = true; \
	KEY##_valid = true; \
	KEY = 0; \
	for (auto it = val.begin(); it != val.end(); it += 1) { \
		if (*it == '-' && it == val.begin()) { \
			_pq_negative = true; \
			continue; \
		} \
		if (*it < '0' || *it > '9') { \
			KEY##_valid = false; \
			return; \
		} \
		KEY = (KEY * 10) + (*it - '0'); \
	} \
	if (_pq_negative) KEY = -KEY; \
	return; \
}

#define PQBOOL(KEY) \
if (key == #KEY) { \
	KEY = (val.size() > 0 && val != "0"); \
	return; \
}

#define QSENDSTR(STR, CODE) return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(STR "\n"), sizeof(STR "\n") - sizeof(*STR), CODE, "text/plain")
#define QSENDMOVED(LOC) return new Browser::ResourceHandler(nullptr, 0, 302, "text/plain", LOC)
#define QSENDOK() QSENDSTR("OK", 200)
#define QSENDNOTFOUND() QSENDSTR("Not found", 404)
#define QSENDBADREQUESTIF(COND) if (COND) QSENDSTR("Bad response", 400)
#define QSENDSYSTEMERRORIF(COND) if (COND) QSENDSTR("System error", 500)
#define QSENDNOTSUPPORTED() QSENDSTR("Not supported", 400)
#define QREQPARAM(NAME) if (!has_##NAME) QSENDSTR("Missing required param " #NAME, 400)
#define QREQPARAMINT(NAME) QREQPARAM(NAME); if (!NAME##_valid) QSENDSTR("Invalid value for required param " #NAME, 400)

#endif
