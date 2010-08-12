#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <string>
#include <exception>

void handle_exception(
	std::exception *e,
	const std::wstring &add_to_log,
	const std::wstring &window_title = std::wstring());

#endif
