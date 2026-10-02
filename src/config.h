// language: C++, file: src/config.h
// *global toggles + colors, read by chams/esp/gui*
#pragma once
#include <atomic>

struct CheatConfig {
	// master
	std::atomic<bool> menu_open{ true };
	std::atomic<bool> panic{ false };

	// chams
	std::atomic<bool> chams_enabled{ true };
	std::atomic<bool> chams_wireframe{ false };
	// through-wall color (XRAY), rgb a
	float chams_xray[4]{ 1.0f, 0.0f, 0.0f, 1.0f };
	// visible color (ESP highlight)
	float chams_vis[4]{ 0.0f, 1.0f, 0.0f, 1.0f };

	// esp (render-side highlighting, same mechanism, split flag for clarity)
	std::atomic<bool> esp_enabled{ true };

	// detection heuristics (tunable via menu — "debug" category)
	std::atomic<int>  min_index_count{ 1500 };
	std::atomic<int>  max_index_count{ 60000 };
	std::atomic<bool> require_bone_shader{ true };
	std::atomic<bool> debug_log_shaders{ false };

	// hotkeys
	int key_menu  = 0x2D; // VK_INSERT
	int key_panic = 0x23; // VK_END
};

extern CheatConfig g_cfg;
