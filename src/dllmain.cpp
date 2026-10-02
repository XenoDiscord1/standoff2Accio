// language: C++, file: src/dllmain.cpp
// *entry point. On DLL_PROCESS_ATTACH we spin up logging and load the
// real opengl32.dll so our proxy export stubs have real pointers to
// forward to. Everything else (hooks, GUI) is lazy — initialized on
// the first wglMakeCurrent that gets a live context.*
#include <windows.h>
#include "proxy.h"
#include "hooks.h"
#include "gui.h"
#include "utils.h"
#include "config.h"

CheatConfig g_cfg;

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID /*reserved*/) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(h);
		utils::log_init();
		utils::log_line("[dllmain] attach pid=%lu", GetCurrentProcessId());
		if (!proxy::init(h)) {
			utils::log_line("[dllmain] proxy::init FAILED — host will crash on first GL call");
		}
		break;
	case DLL_PROCESS_DETACH:
		utils::log_line("[dllmain] detach");
		gui::shutdown();
		proxy::shutdown();
		break;
	}
	return TRUE;
}
