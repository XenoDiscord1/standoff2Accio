// language: C++, file: src/gui.h
#pragma once
#include <windows.h>

namespace gui {
	void on_context_bound(HDC dc, HGLRC rc);  // one-shot init
	void render_frame(HDC dc);                // called from on_swap_pre
	void shutdown();
}
