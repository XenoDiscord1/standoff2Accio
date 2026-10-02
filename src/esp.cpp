// language: C++, file: src/esp.cpp
// *detection = (shader program has bone-matrix uniform) AND (index count
// inside configured range) AND (mode is GL_TRIANGLES). This matches the
// Oifox pattern: skinned-mesh shaders are only used by characters.*
#include "esp.h"
#include "hooks.h"
#include "config.h"

namespace {
	constexpr unsigned GL_TRIANGLES = 0x0004;
}

namespace esp {

	bool is_player_draw(unsigned mode, int count, unsigned /*type*/) {
		if (mode != GL_TRIANGLES) return false;
		int lo = g_cfg.min_index_count.load();
		int hi = g_cfg.max_index_count.load();
		if (count < lo || count > hi) return false;
		if (g_cfg.require_bone_shader.load() && !hooks::g_frame.program_is_player) return false;
		// if the user disabled the bone-shader requirement, we fall through on count alone
		return true;
	}

	bool is_player_draw_arrays(unsigned mode, int count) {
		if (mode != GL_TRIANGLES) return false;
		int lo = g_cfg.min_index_count.load();
		int hi = g_cfg.max_index_count.load();
		if (count < lo || count > hi) return false;
		if (g_cfg.require_bone_shader.load() && !hooks::g_frame.program_is_player) return false;
		return true;
	}

}
