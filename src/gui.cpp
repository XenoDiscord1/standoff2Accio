// language: C++, file: src/gui.cpp
// *ImGui over the game's own GL context — initialized on the first
// wglMakeCurrent that gives us a valid HGLRC. The host window is
// discovered via WindowFromDC. We subclass its WndProc so ImGui gets
// input, and we gate input to BlueStacks when the menu is open.*
#include "gui.h"
#include "hooks.h"
#include "config.h"
#include "utils.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include <atomic>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
	std::atomic<bool> g_inited{ false };
	HWND     g_hwnd    = nullptr;
	WNDPROC  g_wndproc_old = nullptr;

	LRESULT CALLBACK wndproc_hook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		if (g_inited.load()) {
			ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
			if (g_cfg.menu_open.load() && !g_cfg.panic.load()) {
				// swallow mouse/kb (but let BlueStacks keep rendering) when menu is open
				switch (msg) {
				case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
				case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_MBUTTONDOWN: case WM_MBUTTONUP:
				case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
				case WM_KEYDOWN:  case WM_KEYUP:
				case WM_SYSKEYDOWN: case WM_SYSKEYUP:
				case WM_CHAR: case WM_SYSCHAR:
				case WM_SETCURSOR:
					return 1;
				default: break;
				}
			}
		}
		return CallWindowProcW(g_wndproc_old, hwnd, msg, wp, lp);
	}

	void color_edit(const char* label, float* rgba) {
		ImGui::ColorEdit4(label, rgba, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
	}
}

namespace gui {

	void on_context_bound(HDC dc, HGLRC /*rc*/) {
		if (g_inited.load()) return;

		g_hwnd = WindowFromDC(dc);
		if (!g_hwnd) { utils::log_line("[gui] WindowFromDC returned null — gui disabled"); return; }

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = nullptr;
		ImGui::StyleColorsDark();

		if (!ImGui_ImplWin32_Init(g_hwnd))         { utils::log_line("[gui] win32 init failed"); return; }
		if (!ImGui_ImplOpenGL3_Init("#version 130")){ utils::log_line("[gui] gl3 init failed"); return; }

		g_wndproc_old = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc_hook);

		g_inited = true;
		utils::log_line("[gui] initialized against hwnd=%p", (void*)g_hwnd);
	}

	void render_frame(HDC /*dc*/) {
		if (!g_inited.load()) return;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Status strip (always visible)
		{
			ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.35f);
			ImGui::Begin("##status", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
			ImGui::Text("Standoff2Cheat  %.0f fps", ImGui::GetIO().Framerate);
			ImGui::Text("chams:%s  wire:%s  esp:%s  panic:%s",
				g_cfg.chams_enabled.load() ? "on" : "off",
				g_cfg.chams_wireframe.load() ? "on" : "off",
				g_cfg.esp_enabled.load() ? "on" : "off",
				g_cfg.panic.load() ? "ON" : "off");
			ImGui::Text("INSERT = menu   END = panic");
			ImGui::End();
		}

		if (g_cfg.menu_open.load()) {
			ImGui::SetNextWindowSize(ImVec2(380.0f, 420.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Standoff2Cheat");

			if (ImGui::CollapsingHeader("Chams", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool v;
				v = g_cfg.chams_enabled.load();   if (ImGui::Checkbox("Enable",    &v)) g_cfg.chams_enabled   = v;
				v = g_cfg.chams_wireframe.load(); if (ImGui::Checkbox("Wireframe", &v)) g_cfg.chams_wireframe = v;
				color_edit("Through-wall (XRAY)", g_cfg.chams_xray);
				color_edit("Visible",             g_cfg.chams_vis);
			}
			if (ImGui::CollapsingHeader("ESP / Detection", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool v;
				v = g_cfg.esp_enabled.load();        if (ImGui::Checkbox("ESP highlight", &v)) g_cfg.esp_enabled        = v;
				v = g_cfg.require_bone_shader.load();if (ImGui::Checkbox("Require bone-matrix shader", &v)) g_cfg.require_bone_shader = v;
				int lo = g_cfg.min_index_count.load();
				int hi = g_cfg.max_index_count.load();
				if (ImGui::SliderInt("Min index count", &lo, 100, 10000)) g_cfg.min_index_count = lo;
				if (ImGui::SliderInt("Max index count", &hi, 10000, 120000)) g_cfg.max_index_count = hi;
			}
			if (ImGui::CollapsingHeader("Debug")) {
				bool v = g_cfg.debug_log_shaders.load();
				if (ImGui::Checkbox("Log shader uniforms (noisy)", &v)) g_cfg.debug_log_shaders = v;
				ImGui::TextWrapped("Log: %s", utils::log_path().c_str());
				ImGui::Separator();
				ImGui::Text("Current program: %u", hooks::g_frame.current_program);
				ImGui::Text("Color uniform loc: %d", hooks::g_frame.color_uniform_loc);
				ImGui::Text("Is player shader: %s", hooks::g_frame.program_is_player ? "yes" : "no");
			}
			if (ImGui::CollapsingHeader("Panic")) {
				bool v = g_cfg.panic.load();
				if (ImGui::Checkbox("Pass-through only (unhook)", &v)) g_cfg.panic = v;
				ImGui::TextWrapped("With panic on, we skip all chams/ESP work and act as a plain opengl32 proxy.");
			}
			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void shutdown() {
		if (!g_inited.exchange(false)) return;
		if (g_hwnd && g_wndproc_old) {
			SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_wndproc_old);
			g_wndproc_old = nullptr;
		}
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

}
