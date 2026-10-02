// language: C++, file: src/chams.cpp
// *two-pass chams: xray color through walls, visible color on top,
// optional wireframe overlay. Falls back to a plain pass if the color
// uniform wasn't located (prog info not probed yet or shader is unusual).*
#include "chams.h"
#include "hooks.h"
#include "config.h"

namespace {
	// GL constants we need (verbatim from the OpenGL headers to avoid pulling in gl3.h)
	constexpr unsigned GL_DEPTH_TEST     = 0x0B71;
	constexpr unsigned GL_FRONT_AND_BACK = 0x0408;
	constexpr unsigned GL_LINE           = 0x1B01;
	constexpr unsigned GL_FILL           = 0x1B02;
	constexpr unsigned GL_LESS           = 0x0201;
	constexpr unsigned GL_ALWAYS         = 0x0207;
	constexpr unsigned GL_BLEND          = 0x0BE2;

	inline bool have_pointers() {
		using namespace hooks::gl;
		return p_glEnable && p_glDisable && p_glDepthFunc && p_glDepthMask &&
			   p_glPolygonMode && p_glUniform4f;
	}

	inline void set_color(float r, float g, float b, float a) {
		int loc = hooks::g_frame.color_uniform_loc;
		if (loc >= 0 && hooks::gl::p_glUniform4f) {
			hooks::gl::p_glUniform4f(loc, r, g, b, a);
		}
	}

	void chams_pass_before(bool through_wall) {
		using namespace hooks::gl;
		// Through-wall: depth off → always draws
		// Visible: depth on → only draws visible fragments
		if (through_wall) {
			p_glDisable(GL_DEPTH_TEST);
			if (p_glDepthMask) p_glDepthMask(0);
		} else {
			p_glEnable(GL_DEPTH_TEST);
			if (p_glDepthFunc) p_glDepthFunc(GL_LESS);
			if (p_glDepthMask) p_glDepthMask(1);
		}
	}

	void chams_pass_after(bool /*through_wall*/) {
		using namespace hooks::gl;
		// restore defaults (game will reconfigure anyway for the next draw)
		p_glEnable(GL_DEPTH_TEST);
		if (p_glDepthFunc) p_glDepthFunc(GL_LESS);
		if (p_glDepthMask) p_glDepthMask(1);
	}
}

namespace chams {

	void wrap_draw_elements(unsigned mode, int count, unsigned type, const void* indices) {
		using namespace hooks::gl;
		if (!have_pointers() || !p_glDrawElements) {
			if (p_glDrawElements) p_glDrawElements(mode, count, type, indices);
			return;
		}

		bool have_color = (hooks::g_frame.color_uniform_loc >= 0);
		bool do_chams = g_cfg.chams_enabled.load() && have_color;
		bool do_wire  = g_cfg.chams_wireframe.load();

		if (do_chams) {
			// Pass 1 — through walls
			chams_pass_before(true);
			set_color(g_cfg.chams_xray[0], g_cfg.chams_xray[1], g_cfg.chams_xray[2], g_cfg.chams_xray[3]);
			if (do_wire) p_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			p_glDrawElements(mode, count, type, indices);
			if (do_wire) p_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			// Pass 2 — visible
			chams_pass_before(false);
			set_color(g_cfg.chams_vis[0], g_cfg.chams_vis[1], g_cfg.chams_vis[2], g_cfg.chams_vis[3]);
			p_glDrawElements(mode, count, type, indices);

			chams_pass_after(false);
			// Pass 3 — original (so animations/hitboxes still render for anti-cheat timing parity)
			// NOTE: color uniform is left at vis color here; the game's next shader bind will reset it.
		}

		// Always do the real draw at the end — this is what the game expected
		p_glDrawElements(mode, count, type, indices);
	}

	void wrap_draw_arrays(unsigned mode, int first, int count) {
		using namespace hooks::gl;
		if (!have_pointers() || !p_glDrawArrays) {
			if (p_glDrawArrays) p_glDrawArrays(mode, first, count);
			return;
		}

		bool have_color = (hooks::g_frame.color_uniform_loc >= 0);
		bool do_chams = g_cfg.chams_enabled.load() && have_color;
		bool do_wire  = g_cfg.chams_wireframe.load();

		if (do_chams) {
			chams_pass_before(true);
			set_color(g_cfg.chams_xray[0], g_cfg.chams_xray[1], g_cfg.chams_xray[2], g_cfg.chams_xray[3]);
			if (do_wire) p_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			p_glDrawArrays(mode, first, count);
			if (do_wire) p_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			chams_pass_before(false);
			set_color(g_cfg.chams_vis[0], g_cfg.chams_vis[1], g_cfg.chams_vis[2], g_cfg.chams_vis[3]);
			p_glDrawArrays(mode, first, count);

			chams_pass_after(false);
		}

		p_glDrawArrays(mode, first, count);
	}

}
