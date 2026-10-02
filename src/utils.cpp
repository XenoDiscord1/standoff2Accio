// language: C++, file: src/utils.cpp
#include "utils.h"
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <unordered_map>

namespace {
	FILE* g_log = nullptr;
	std::mutex g_log_mu;
	std::unordered_map<int, bool> g_key_prev;
	std::mutex g_key_mu;

	std::string temp_dir() {
		char buf[MAX_PATH]{};
		DWORD n = GetTempPathA(MAX_PATH, buf);
		if (!n) return "C:\\Windows\\Temp\\";
		return std::string(buf, n);
	}
}

namespace utils {

	std::string log_path() {
		return temp_dir() + "s2cheat.log";
	}

	void log_init() {
		std::lock_guard<std::mutex> lk(g_log_mu);
		if (g_log) return;
		auto p = log_path();
		g_log = fopen(p.c_str(), "a");
		if (g_log) {
			SYSTEMTIME st; GetLocalTime(&st);
			fprintf(g_log, "\n=== s2cheat start %04d-%02d-%02d %02d:%02d:%02d ===\n",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			fflush(g_log);
		}
	}

	void log_line(const char* fmt, ...) {
		std::lock_guard<std::mutex> lk(g_log_mu);
		if (!g_log) return;
		va_list ap; va_start(ap, fmt);
		vfprintf(g_log, fmt, ap);
		va_end(ap);
		fputc('\n', g_log);
		fflush(g_log);
	}

	bool key_down(int vk) {
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	bool key_pressed_edge(int vk) {
		bool now = key_down(vk);
		std::lock_guard<std::mutex> lk(g_key_mu);
		bool prev = g_key_prev[vk];
		g_key_prev[vk] = now;
		return now && !prev;
	}

}
