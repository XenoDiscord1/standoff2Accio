// language: C++, file: src/utils.h
#pragma once
#include <windows.h>
#include <string>

namespace utils {
	void log_init();
	void log_line(const char* fmt, ...);
	bool key_down(int vk);           // instant
	bool key_pressed_edge(int vk);   // rising-edge, debounced per key

	// %TEMP%\s2cheat.log
	std::string log_path();
}
