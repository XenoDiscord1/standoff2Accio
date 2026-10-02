// language: C++, file: src/hooks.h
// *public surface used by proxy.cpp. Everything the exports call during
// their lifetime passes through this. hooks.cpp holds all GL pointers
// resolved via wglGetProcAddress (modern GL that isn't in opengl32's
// static exports) and routes draw calls through chams/esp.*
#pragma once
#include <windows.h>

namespace hooks {

	// --- lifecycle ---
	void init_once();                                   // called from DllMain / first stub call
	void on_context_bound(HDC dc, HGLRC rc);            // from wglMakeCurrent after success
	void on_swap_pre(HDC dc);                           // just before real wglSwapBuffers
	void on_swap_post(HDC dc);                          // right after

	// --- wglGetProcAddress interception ---
	// returns our hook stub if `name` is one we handle, otherwise the real pointer
	FARPROC on_get_proc_address(const char* name, FARPROC real);

	// --- draw interceptors (bodies in proxy.cpp forward here) ---
	void on_draw_arrays(unsigned mode, int first, int count);
	void on_draw_elements(unsigned mode, int count, unsigned type, const void* indices);

	// --- GL fn pointers resolved from the real opengl32 ---
	// (populated once in init_once(); modern GL grabbed in on_get_proc_address() as they go by)
	namespace gl {
		// legacy / static exports (from proxy::g_real cast — see hooks.cpp)
		using PFN_glDrawArrays       = void (APIENTRY*)(unsigned, int, int);
		using PFN_glDrawElements     = void (APIENTRY*)(unsigned, int, unsigned, const void*);
		using PFN_glEnable           = void (APIENTRY*)(unsigned);
		using PFN_glDisable          = void (APIENTRY*)(unsigned);
		using PFN_glDepthFunc        = void (APIENTRY*)(unsigned);
		using PFN_glDepthMask        = void (APIENTRY*)(unsigned char);
		using PFN_glPolygonMode      = void (APIENTRY*)(unsigned, unsigned);
		using PFN_glGetIntegerv      = void (APIENTRY*)(unsigned, int*);
		using PFN_glGetError         = unsigned (APIENTRY*)();
		using PFN_glBlendFunc        = void (APIENTRY*)(unsigned, unsigned);
		using PFN_glViewport         = void (APIENTRY*)(int, int, int, int);
		// modern GL (resolved via wglGetProcAddress)
		using PFN_glUseProgram        = void (APIENTRY*)(unsigned);
		using PFN_glGetUniformLocation= int  (APIENTRY*)(unsigned, const char*);
		using PFN_glUniform4f         = void (APIENTRY*)(int, float, float, float, float);
		using PFN_glUniform4fv        = void (APIENTRY*)(int, int, const float*);
		using PFN_glUniform3f         = void (APIENTRY*)(int, float, float, float);
		using PFN_glUniform1i         = void (APIENTRY*)(int, int);
		using PFN_glGetActiveUniform  = void (APIENTRY*)(unsigned, unsigned, int, int*, int*, unsigned*, char*);
		using PFN_glGetProgramiv      = void (APIENTRY*)(unsigned, unsigned, int*);
		using PFN_glGetUniformfv      = void (APIENTRY*)(unsigned, int, float*);

		extern PFN_glDrawArrays        p_glDrawArrays;
		extern PFN_glDrawElements      p_glDrawElements;
		extern PFN_glEnable            p_glEnable;
		extern PFN_glDisable           p_glDisable;
		extern PFN_glDepthFunc         p_glDepthFunc;
		extern PFN_glDepthMask         p_glDepthMask;
		extern PFN_glPolygonMode       p_glPolygonMode;
		extern PFN_glGetIntegerv       p_glGetIntegerv;
		extern PFN_glGetError          p_glGetError;
		extern PFN_glBlendFunc         p_glBlendFunc;
		extern PFN_glViewport          p_glViewport;

		extern PFN_glUseProgram        p_glUseProgram;
		extern PFN_glGetUniformLocation p_glGetUniformLocation;
		extern PFN_glUniform4f         p_glUniform4f;
		extern PFN_glUniform4fv        p_glUniform4fv;
		extern PFN_glUniform3f         p_glUniform3f;
		extern PFN_glUniform1i         p_glUniform1i;
		extern PFN_glGetActiveUniform  p_glGetActiveUniform;
		extern PFN_glGetProgramiv      p_glGetProgramiv;
		extern PFN_glGetUniformfv      p_glGetUniformfv;
	}

	// --- shared state the chams/esp modules read ---
	struct FrameState {
		unsigned current_program = 0;
		int  color_uniform_loc   = -1;    // best-guess "material color" uniform in current program
		bool program_is_player   = false; // heuristic: has a bone-matrix uniform
		int  viewport[4]{};
	};
	extern FrameState g_frame;

}
