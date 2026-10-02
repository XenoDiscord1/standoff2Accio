// language: C++, file: src/hooks.cpp
// *resolves real GL pointers, routes draw calls through chams logic,
// intercepts wglGetProcAddress so modern GL symbols (glUseProgram,
// glGetUniformLocation, glUniform4f, …) go through our hook stubs
// instead of the real DLL. Those hooks capture per-program state we need
// to decide if a draw is a player skinned-mesh.*
#include "hooks.h"
#include "proxy.h"
#include "chams.h"
#include "esp.h"
#include "gui.h"
#include "utils.h"
#include "config.h"
#include <cstring>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace hooks {

	namespace gl {
		PFN_glDrawArrays         p_glDrawArrays        = nullptr;
		PFN_glDrawElements       p_glDrawElements      = nullptr;
		PFN_glEnable             p_glEnable            = nullptr;
		PFN_glDisable            p_glDisable           = nullptr;
		PFN_glDepthFunc          p_glDepthFunc         = nullptr;
		PFN_glDepthMask          p_glDepthMask         = nullptr;
		PFN_glPolygonMode        p_glPolygonMode       = nullptr;
		PFN_glGetIntegerv        p_glGetIntegerv       = nullptr;
		PFN_glGetError           p_glGetError          = nullptr;
		PFN_glBlendFunc          p_glBlendFunc         = nullptr;
		PFN_glViewport           p_glViewport          = nullptr;

		PFN_glUseProgram         p_glUseProgram        = nullptr;
		PFN_glGetUniformLocation p_glGetUniformLocation = nullptr;
		PFN_glUniform4f          p_glUniform4f         = nullptr;
		PFN_glUniform4fv         p_glUniform4fv        = nullptr;
		PFN_glUniform3f          p_glUniform3f         = nullptr;
		PFN_glUniform1i          p_glUniform1i         = nullptr;
		PFN_glGetActiveUniform   p_glGetActiveUniform  = nullptr;
		PFN_glGetProgramiv       p_glGetProgramiv      = nullptr;
		PFN_glGetUniformfv       p_glGetUniformfv      = nullptr;
	}

	FrameState g_frame;

	namespace {
		std::atomic<bool> g_booted{ false };
		std::once_flag    g_boot_once;

		// shader program -> cached info (whether it is a player shader + color uniform loc)
		struct ProgInfo {
			bool  is_player   = false;
			int   color_loc   = -1;
			int   wireframe_tex_loc = -1;
			bool  probed      = false;
		};
		std::unordered_map<unsigned, ProgInfo> g_programs;
		std::mutex g_prog_mu;

		// GL constants we need directly (don't want to include glew/gl3 headers)
		static constexpr unsigned GL_ACTIVE_UNIFORMS         = 0x8B86;
		static constexpr unsigned GL_ACTIVE_UNIFORM_MAX_LEN  = 0x8B87;
		static constexpr unsigned GL_FRONT_AND_BACK          = 0x0408;
		static constexpr unsigned GL_LINE                    = 0x1B01;
		static constexpr unsigned GL_FILL                    = 0x1B02;
		static constexpr unsigned GL_DEPTH_TEST              = 0x0B71;
		static constexpr unsigned GL_LESS                    = 0x0201;
		static constexpr unsigned GL_ALWAYS                  = 0x0207;
		static constexpr unsigned GL_VIEWPORT                = 0x0BA2;

		void probe_program(unsigned program) {
			if (!gl::p_glGetProgramiv || !gl::p_glGetActiveUniform) return;
			std::lock_guard<std::mutex> lk(g_prog_mu);
			auto& info = g_programs[program];
			if (info.probed) return;
			info.probed = true;

			int n_uniforms = 0;
			gl::p_glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &n_uniforms);
			int max_name = 0;
			gl::p_glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LEN, &max_name);
			if (max_name <= 0 || max_name > 256) max_name = 128;
			char* name = (char*)alloca((size_t)max_name);

			for (int i = 0; i < n_uniforms; ++i) {
				int sz = 0; unsigned type = 0; int len = 0;
				name[0] = 0;
				gl::p_glGetActiveUniform(program, (unsigned)i, max_name, &len, &sz, &type, name);
				if (!name[0]) continue;

				// skinned-mesh signature — standoff2 player shaders have one of these
				if (strstr(name, "boneMatrices") || strstr(name, "_Bones") ||
					strstr(name, "u_Bones")     || strstr(name, "_BoneMatrices") ||
					strstr(name, "unity_Bones") || strstr(name, "u_SkinMatrix")) {
					info.is_player = true;
				}
				// color-tint-ish uniform. We'll override this during chams passes.
				if (info.color_loc < 0) {
					if (strstr(name, "u_color")      || strstr(name, "_Color") ||
						strstr(name, "u_MainColor") || strstr(name, "u_Tint") ||
						strstr(name, "u_BaseColor")|| strstr(name, "_MainColor")) {
						info.color_loc = gl::p_glGetUniformLocation(program, name);
					}
				}
				if (g_cfg.debug_log_shaders.load()) {
					utils::log_line("[prog %u] %s (type=0x%04x, size=%d)", program, name, type, sz);
				}
			}
			utils::log_line("[prog %u] probed: is_player=%d color_loc=%d uniforms=%d",
				program, info.is_player ? 1 : 0, info.color_loc, n_uniforms);
		}
	} // anon

	// ---------------- init ----------------
	void init_once() {
		std::call_once(g_boot_once, [] {
			utils::log_init();
			utils::log_line("[hooks] init_once");

			// bind the legacy GL fn pointers that come from the static exports table
			gl::p_glDrawArrays   = (gl::PFN_glDrawArrays)  proxy::g_real[proxy::IDX_glDrawArrays];
			gl::p_glDrawElements = (gl::PFN_glDrawElements)proxy::g_real[proxy::IDX_glDrawElements];
			gl::p_glEnable       = (gl::PFN_glEnable)      proxy::g_real[proxy::IDX_glEnable];
			gl::p_glDisable      = (gl::PFN_glDisable)     proxy::g_real[proxy::IDX_glDisable];
			gl::p_glDepthFunc    = (gl::PFN_glDepthFunc)   proxy::g_real[proxy::IDX_glDepthFunc];
			gl::p_glDepthMask    = (gl::PFN_glDepthMask)   proxy::g_real[proxy::IDX_glDepthMask];
			gl::p_glPolygonMode  = (gl::PFN_glPolygonMode) proxy::g_real[proxy::IDX_glPolygonMode];
			gl::p_glBlendFunc    = (gl::PFN_glBlendFunc)   proxy::g_real[proxy::IDX_glBlendFunc];
			gl::p_glGetError     = (gl::PFN_glGetError)    proxy::g_real[proxy::IDX_glGetError];
			gl::p_glViewport     = (gl::PFN_glViewport)    proxy::g_real[proxy::IDX_glViewport];
			// glGetIntegerv not in our abbreviated X-macro list — resolve by name
			gl::p_glGetIntegerv  = (gl::PFN_glGetIntegerv) GetProcAddress(proxy::real_handle(), "glGetIntegerv");

			g_booted = true;
			utils::log_line("[hooks] legacy GL pointers bound");
		});
	}

	// ---------------- wglMakeCurrent ----------------
	void on_context_bound(HDC dc, HGLRC rc) {
		init_once();
		utils::log_line("[hooks] context bound: dc=%p rc=%p", (void*)dc, (void*)rc);
		gui::on_context_bound(dc, rc);
	}

	// ---------------- wglGetProcAddress ----------------
	// If BlueStacks's GLES translator requests one of the modern GL entry points
	// we care about, grab a copy of the real pointer, then return the real one
	// (we don't need to rewrite the pointer BlueStacks sees — we only need our
	// own copy to call manually). The exception is glUseProgram and
	// glGetUniformLocation which we DO want to intercept, so we return our stubs.
	static void glUseProgram_hook(unsigned program);
	static int  glGetUniformLocation_hook(unsigned program, const char* name);

	FARPROC on_get_proc_address(const char* name, FARPROC real) {
		if (!name || !real) return real;

		#define GRAB(sym) if (!gl::p_##sym && std::strcmp(name, #sym) == 0) { \
			gl::p_##sym = (gl::PFN_##sym)real; }
		GRAB(glUseProgram);
		GRAB(glGetUniformLocation);
		GRAB(glUniform4f);
		GRAB(glUniform4fv);
		GRAB(glUniform3f);
		GRAB(glUniform1i);
		GRAB(glGetActiveUniform);
		GRAB(glGetProgramiv);
		GRAB(glGetUniformfv);
		#undef GRAB

		// intercept:
		if (std::strcmp(name, "glUseProgram") == 0)         return (FARPROC)&glUseProgram_hook;
		if (std::strcmp(name, "glGetUniformLocation") == 0) return (FARPROC)&glGetUniformLocation_hook;
		return real;
	}

	static void glUseProgram_hook(unsigned program) {
		if (gl::p_glUseProgram) gl::p_glUseProgram(program);
		g_frame.current_program = program;
		if (program == 0) {
			g_frame.color_uniform_loc = -1;
			g_frame.program_is_player = false;
			return;
		}
		probe_program(program);
		std::lock_guard<std::mutex> lk(g_prog_mu);
		auto it = g_programs.find(program);
		if (it != g_programs.end()) {
			g_frame.color_uniform_loc = it->second.color_loc;
			g_frame.program_is_player = it->second.is_player;
		}
	}

	static int glGetUniformLocation_hook(unsigned program, const char* name) {
		int loc = gl::p_glGetUniformLocation ? gl::p_glGetUniformLocation(program, name) : -1;
		// Opportunistic: capture color-ish locs here too in case probe missed them.
		if (name && loc >= 0) {
			if (strstr(name, "u_color") || strstr(name, "_Color") ||
				strstr(name, "u_MainColor") || strstr(name, "u_Tint") ||
				strstr(name, "u_BaseColor") || strstr(name, "_MainColor")) {
				std::lock_guard<std::mutex> lk(g_prog_mu);
				auto& info = g_programs[program];
				if (info.color_loc < 0) info.color_loc = loc;
			}
		}
		return loc;
	}

	// ---------------- draw interceptors ----------------
	void on_draw_elements(unsigned mode, int count, unsigned type, const void* indices) {
		init_once();

		if (!g_cfg.panic.load() &&
			(g_cfg.chams_enabled.load() || g_cfg.esp_enabled.load()) &&
			esp::is_player_draw(mode, count, type))
		{
			chams::wrap_draw_elements(mode, count, type, indices);
			return;
		}
		if (gl::p_glDrawElements) gl::p_glDrawElements(mode, count, type, indices);
	}

	void on_draw_arrays(unsigned mode, int first, int count) {
		init_once();
		if (!g_cfg.panic.load() &&
			(g_cfg.chams_enabled.load() || g_cfg.esp_enabled.load()) &&
			esp::is_player_draw_arrays(mode, count))
		{
			chams::wrap_draw_arrays(mode, first, count);
			return;
		}
		if (gl::p_glDrawArrays) gl::p_glDrawArrays(mode, first, count);
	}

	// ---------------- swap ----------------
	void on_swap_pre(HDC dc) {
		init_once();
		// refresh viewport (used by GUI positioning)
		if (gl::p_glGetIntegerv) gl::p_glGetIntegerv(GL_VIEWPORT, g_frame.viewport);

		// hotkeys
		if (utils::key_pressed_edge(g_cfg.key_menu))  g_cfg.menu_open = !g_cfg.menu_open;
		if (utils::key_pressed_edge(g_cfg.key_panic)) g_cfg.panic     = !g_cfg.panic;

		if (!g_cfg.panic.load()) gui::render_frame(dc);
	}
	void on_swap_post(HDC /*dc*/) { /* nothing — swap already happened */ }

}
