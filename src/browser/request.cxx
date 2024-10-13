#include "request.hxx"

void Browser::ParseQuery(std::string_view query, std::function<void(const std::string_view&, const std::string_view&)> callback, char delim) {
	size_t pos = 0;
	while (true) {
		const size_t next_and = query.find(delim, pos);
		const size_t next_eq = query.find('=', pos);
		if (next_eq == std::string_view::npos) break;
		else if (next_and != std::string_view::npos && next_eq > next_and) {
			pos = next_and + 1;
			continue;
		}
		const bool is_last = next_and == std::string_view::npos;
		const auto end = is_last ? query.end() : query.begin() + next_and;
		const std::string_view key(query.begin() + pos, query.begin() + next_eq);
		const std::string_view val(query.begin() + next_eq + 1, end);
		callback(key, val);
		if (is_last) break;
		pos = next_and + 1;
	}
}
