// language: C++, file: src/proxy.h
// *loads real C:\Windows\System32\opengl32.dll, resolves every export
// listed in proxy_exports.def, exposes them to proxy.cpp stubs*
#pragma once
#include <windows.h>

namespace proxy {

	// X-macro table — every entry here must have:
	//   1. a line in src/proxy_exports.def
	//   2. a stub body in proxy.cpp
	// Keep in sync. Order does not matter.
	#define PROXY_EXPORT_LIST(X) \
		/* wgl */ \
		X(wglChoosePixelFormat) X(wglCopyContext) X(wglCreateContext) X(wglCreateLayerContext) \
		X(wglDeleteContext) X(wglDescribeLayerPlane) X(wglDescribePixelFormat) \
		X(wglGetCurrentContext) X(wglGetCurrentDC) X(wglGetDefaultProcAddress) \
		X(wglGetLayerPaletteEntries) X(wglGetPixelFormat) X(wglGetProcAddress) \
		X(wglMakeCurrent) X(wglRealizeLayerPalette) X(wglSetLayerPaletteEntries) \
		X(wglSetPixelFormat) X(wglShareLists) X(wglSwapBuffers) X(wglSwapLayerBuffers) \
		X(wglSwapMultipleBuffers) X(wglUseFontBitmapsA) X(wglUseFontBitmapsW) \
		X(wglUseFontOutlinesA) X(wglUseFontOutlinesW) \
		/* glmf */ \
		X(GlmfBeginGlsBlock) X(GlmfCloseMetaFile) X(GlmfEndGlsBlock) \
		X(GlmfEndPlayback) X(GlmfInitPlayback) X(GlmfPlayGlsRecord) \
		/* gl 1.1 — abbreviated here, full set wired up in proxy.cpp */ \
		X(glBegin) X(glEnd) X(glClear) X(glClearColor) X(glEnable) X(glDisable) \
		X(glDrawArrays) X(glDrawElements) X(glBindTexture) X(glDeleteTextures) \
		X(glGenTextures) X(glGetError) X(glGetString) X(glViewport) \
		X(glPolygonMode) X(glDepthFunc) X(glDepthMask) X(glBlendFunc)

	// Pointer table — one void* per real export.
	// Populated at init(); stubs call through it.
	extern void* g_real[]; // indexed by the ProxyIndex enum below

	enum ProxyIndex : int {
		#define X(name) IDX_##name,
			PROXY_EXPORT_LIST(X)
		#undef X
		PROXY_INDEX_COUNT
	};

	bool init(HMODULE our_module);
	void shutdown();
	FARPROC get_real(const char* name);  // generic lookup for wglGetProcAddress passthrough
	HMODULE real_handle();                // for the hook installer to see base address

}
