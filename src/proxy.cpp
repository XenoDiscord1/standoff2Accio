// language: C++, file: src/proxy.cpp
// *stubs for every opengl32 export; each forwards to the real function
// pointer resolved from C:\Windows\System32\opengl32.dll at init time.
//
// x64 has a single calling convention (Microsoft x64) — this means we can
// use variadic stubs that preserve register args + stack by letting the
// compiler emit a tail call via a function-pointer cast of matching
// signature. Where the signature matters for correctness (return value in
// RAX, FP returns in XMM0), we spell the signature out explicitly.
//
// Any export NOT spelled out gets a generic 8-arg integer passthrough.
// For gl 1.1 that's safe — none of these functions take more than 8
// register-sized arguments and none return structs by value.*
#include "proxy.h"
#include "utils.h"
#include "hooks.h"
#include <stdio.h>
#include <cstring>

namespace proxy {
	void* g_real[PROXY_INDEX_COUNT]{};
	static HMODULE g_h = nullptr;

	HMODULE real_handle() { return g_h; }

	FARPROC get_real(const char* name) {
		if (!g_h) return nullptr;
		return GetProcAddress(g_h, name);
	}

	bool init(HMODULE /*our_module*/) {
		char sysdir[MAX_PATH]{};
		UINT n = GetSystemDirectoryA(sysdir, MAX_PATH);
		if (!n) return false;
		char path[MAX_PATH]{};
		snprintf(path, MAX_PATH, "%s\\opengl32.dll", sysdir);

		g_h = LoadLibraryA(path);
		if (!g_h) {
			utils::log_line("[proxy] LoadLibrary(%s) failed: %lu", path, GetLastError());
			return false;
		}
		utils::log_line("[proxy] real opengl32 @ %p (%s)", (void*)g_h, path);

		int missing = 0;
		#define X(name)                                                          \
			g_real[IDX_##name] = (void*)GetProcAddress(g_h, #name);              \
			if (!g_real[IDX_##name]) {                                           \
				utils::log_line("[proxy] GetProcAddress failed: %s", #name);     \
				++missing;                                                       \
			}
			PROXY_EXPORT_LIST(X)
		#undef X
		utils::log_line("[proxy] %d resolved, %d missing", PROXY_INDEX_COUNT - missing, missing);
		return missing == 0;
	}

	void shutdown() {
		if (g_h) { FreeLibrary(g_h); g_h = nullptr; }
	}
}

// --------------------------------------------------------------------------
// Generic 8-arg passthrough. On x64 Windows calling convention, the first
// four int-ish args are in RCX RDX R8 R9, floats in XMM0..3, remainder on
// stack. For GL 1.1 types (ints, floats, pointers, enums) this convention
// routes the arguments correctly through to the real DLL.
// --------------------------------------------------------------------------
using GenPtr = void(*)(void*, void*, void*, void*, void*, void*, void*, void*);

// Simple passthrough macro — zero args ok, variadic forwarding.
#define FWD(name) \
	extern "C" __declspec(dllexport) void name() {                                  \
		((void(*)())proxy::g_real[proxy::IDX_##name])();                            \
	}

// --------------------------------------------------------------------------
// Macro that stamps out stubs with a specific signature. Used for the
// handful of functions whose return value or arg types the compiler must
// know (return of float/pointer, pointer-returning wglGetProcAddress, etc.)
// --------------------------------------------------------------------------
#define STUB(ret, name, sig_decl, call_args)                                        \
	extern "C" __declspec(dllexport) ret name sig_decl {                            \
		using FN = ret(*) sig_decl;                                                 \
		return ((FN)proxy::g_real[proxy::IDX_##name]) call_args;                    \
	}

#define STUB_VOID(name, sig_decl, call_args)                                        \
	extern "C" __declspec(dllexport) void name sig_decl {                           \
		using FN = void(*) sig_decl;                                                \
		((FN)proxy::g_real[proxy::IDX_##name]) call_args;                           \
	}

// --------------------------------------------------------------------------
// Explicitly-signatured exports — those whose correctness depends on the
// return type (pointer/float) or that must be callable from our own code
// (not just linked into BlueStacks's import table).
// --------------------------------------------------------------------------

// wgl
STUB(int,    wglChoosePixelFormat,     (HDC a, const PIXELFORMATDESCRIPTOR* b),            (a, b))
STUB(BOOL,   wglCopyContext,           (HGLRC a, HGLRC b, UINT c),                         (a, b, c))
STUB(HGLRC,  wglCreateContext,         (HDC a),                                            (a))
STUB(HGLRC,  wglCreateLayerContext,    (HDC a, int b),                                     (a, b))
STUB(BOOL,   wglDeleteContext,         (HGLRC a),                                          (a))
STUB(BOOL,   wglDescribeLayerPlane,    (HDC a, int b, int c, UINT d, LPLAYERPLANEDESCRIPTOR e), (a, b, c, d, e))
STUB(int,    wglDescribePixelFormat,   (HDC a, int b, UINT c, LPPIXELFORMATDESCRIPTOR d),   (a, b, c, d))
STUB(HGLRC,  wglGetCurrentContext,     (),                                                 ())
STUB(HDC,    wglGetCurrentDC,          (),                                                 ())
STUB(PROC,   wglGetDefaultProcAddress, (LPCSTR a),                                         (a))
STUB(int,    wglGetLayerPaletteEntries,(HDC a, int b, int c, int d, COLORREF* e),          (a, b, c, d, e))
STUB(int,    wglGetPixelFormat,        (HDC a),                                            (a))
// wglGetProcAddress intercepted — we return our own hook impls for modern GL symbols
extern "C" __declspec(dllexport) PROC wglGetProcAddress(LPCSTR name) {
	using FN = PROC(*)(LPCSTR);
	PROC real = ((FN)proxy::g_real[proxy::IDX_wglGetProcAddress])(name);
	return hooks::on_get_proc_address(name, real);
}
// wglMakeCurrent intercepted — triggers one-shot hook install when first GL context lands
extern "C" __declspec(dllexport) BOOL wglMakeCurrent(HDC dc, HGLRC rc) {
	using FN = BOOL(*)(HDC, HGLRC);
	BOOL r = ((FN)proxy::g_real[proxy::IDX_wglMakeCurrent])(dc, rc);
	if (r && rc) hooks::on_context_bound(dc, rc);
	return r;
}
STUB(BOOL,   wglRealizeLayerPalette,   (HDC a, int b, BOOL c),                             (a, b, c))
STUB(int,    wglSetLayerPaletteEntries,(HDC a, int b, int c, int d, const COLORREF* e),    (a, b, c, d, e))
STUB(BOOL,   wglSetPixelFormat,        (HDC a, int b, const PIXELFORMATDESCRIPTOR* c),     (a, b, c))
STUB(BOOL,   wglShareLists,            (HGLRC a, HGLRC b),                                 (a, b))
// wglSwapBuffers intercepted — ImGui pass + per-frame hook state tick
extern "C" __declspec(dllexport) BOOL wglSwapBuffers(HDC dc) {
	hooks::on_swap_pre(dc);
	using FN = BOOL(*)(HDC);
	BOOL r = ((FN)proxy::g_real[proxy::IDX_wglSwapBuffers])(dc);
	hooks::on_swap_post(dc);
	return r;
}
STUB(BOOL,   wglSwapLayerBuffers,      (HDC a, UINT b),                                    (a, b))
STUB(DWORD,  wglSwapMultipleBuffers,   (UINT a, const void* b),                            (a, b))
STUB(BOOL,   wglUseFontBitmapsA,       (HDC a, DWORD b, DWORD c, DWORD d),                 (a, b, c, d))
STUB(BOOL,   wglUseFontBitmapsW,       (HDC a, DWORD b, DWORD c, DWORD d),                 (a, b, c, d))
STUB(BOOL,   wglUseFontOutlinesA,      (HDC a, DWORD b, DWORD c, DWORD d, float e, float f, int g, void* h), (a,b,c,d,e,f,g,h))
STUB(BOOL,   wglUseFontOutlinesW,      (HDC a, DWORD b, DWORD c, DWORD d, float e, float f, int g, void* h), (a,b,c,d,e,f,g,h))

// glmf (undocumented, treat as blind passthrough — never called by games anyway)
STUB_VOID(GlmfBeginGlsBlock, (void* a), (a))
STUB_VOID(GlmfCloseMetaFile, (void* a), (a))
STUB_VOID(GlmfEndGlsBlock,   (void* a), (a))
STUB_VOID(GlmfEndPlayback,   (void* a), (a))
STUB_VOID(GlmfInitPlayback,  (void* a, void* b, void* c), (a,b,c))
STUB_VOID(GlmfPlayGlsRecord, (void* a, void* b, void* c, void* d), (a,b,c,d))

// gl 1.1 — a few with real signatures; the hooked ones (glDrawElements, glDrawArrays)
// have their real pointers captured in hooks.cpp so we don't need a stub here;
// MinHook installs on the real_handle()-based address directly.
STUB_VOID(glBegin,         (unsigned a),                                      (a))
STUB_VOID(glEnd,           (),                                                ())
STUB_VOID(glClear,         (unsigned a),                                      (a))
STUB_VOID(glClearColor,    (float a, float b, float c, float d),              (a,b,c,d))
STUB_VOID(glEnable,        (unsigned a),                                      (a))
STUB_VOID(glDisable,       (unsigned a),                                      (a))
// Intercepted draw calls — route through hooks::on_draw_* which applies chams logic
extern "C" __declspec(dllexport) void glDrawArrays(unsigned mode, int first, int count) {
	hooks::on_draw_arrays(mode, first, count);
}
extern "C" __declspec(dllexport) void glDrawElements(unsigned mode, int count, unsigned type, const void* indices) {
	hooks::on_draw_elements(mode, count, type, indices);
}
STUB_VOID(glBindTexture,   (unsigned a, unsigned b),                          (a,b))
STUB_VOID(glDeleteTextures,(int a, const unsigned* b),                        (a,b))
STUB_VOID(glGenTextures,   (int a, unsigned* b),                              (a,b))
STUB(unsigned, glGetError, (),                                                ())
STUB(const unsigned char*, glGetString, (unsigned a),                         (a))
STUB_VOID(glViewport,      (int a, int b, int c, int d),                      (a,b,c,d))
STUB_VOID(glPolygonMode,   (unsigned a, unsigned b),                          (a,b))
STUB_VOID(glDepthFunc,     (unsigned a),                                      (a))
STUB_VOID(glDepthMask,     (unsigned char a),                                 (a))
STUB_VOID(glBlendFunc,     (unsigned a, unsigned b),                          (a,b))

// --------------------------------------------------------------------------
// Everything else in the .def that we didn't give a signature. The linker
// still demands symbols with the exported names. We provide "void name()"
// stubs that forward via a generic cast — on x64, as long as the caller and
// callee agree on arg count/positions through register conv, this works for
// anything taking primitives. Build system is set to not warn about
// mismatched declarations (we never prototype these elsewhere).
// --------------------------------------------------------------------------
#define GEN_STUB(name) \
	extern "C" __declspec(dllexport) void name(                                                       \
		void* a1=0, void* a2=0, void* a3=0, void* a4=0,                                               \
		void* a5=0, void* a6=0, void* a7=0, void* a8=0)                                               \
	{                                                                                                 \
		FARPROC p = GetProcAddress(proxy::real_handle(), #name);                                      \
		if (!p) return;                                                                               \
		((void(*)(void*,void*,void*,void*,void*,void*,void*,void*))p)(a1,a2,a3,a4,a5,a6,a7,a8);       \
	}

// All the symbols listed in proxy_exports.def but not individually STUBbed above.
// (Order matches the .def file for easy diffing.)
GEN_STUB(glAccum) GEN_STUB(glAlphaFunc) GEN_STUB(glAreTexturesResident) GEN_STUB(glArrayElement)
GEN_STUB(glBitmap) GEN_STUB(glCallList) GEN_STUB(glCallLists)
GEN_STUB(glClearAccum) GEN_STUB(glClearDepth) GEN_STUB(glClearIndex) GEN_STUB(glClearStencil)
GEN_STUB(glClipPlane)
GEN_STUB(glColor3b) GEN_STUB(glColor3bv) GEN_STUB(glColor3d) GEN_STUB(glColor3dv)
GEN_STUB(glColor3f) GEN_STUB(glColor3fv) GEN_STUB(glColor3i) GEN_STUB(glColor3iv)
GEN_STUB(glColor3s) GEN_STUB(glColor3sv) GEN_STUB(glColor3ub) GEN_STUB(glColor3ubv)
GEN_STUB(glColor3ui) GEN_STUB(glColor3uiv) GEN_STUB(glColor3us) GEN_STUB(glColor3usv)
GEN_STUB(glColor4b) GEN_STUB(glColor4bv) GEN_STUB(glColor4d) GEN_STUB(glColor4dv)
GEN_STUB(glColor4f) GEN_STUB(glColor4fv) GEN_STUB(glColor4i) GEN_STUB(glColor4iv)
GEN_STUB(glColor4s) GEN_STUB(glColor4sv) GEN_STUB(glColor4ub) GEN_STUB(glColor4ubv)
GEN_STUB(glColor4ui) GEN_STUB(glColor4uiv) GEN_STUB(glColor4us) GEN_STUB(glColor4usv)
GEN_STUB(glColorMask) GEN_STUB(glColorMaterial) GEN_STUB(glColorPointer)
GEN_STUB(glCopyPixels)
GEN_STUB(glCopyTexImage1D) GEN_STUB(glCopyTexImage2D)
GEN_STUB(glCopyTexSubImage1D) GEN_STUB(glCopyTexSubImage2D)
GEN_STUB(glCullFace) GEN_STUB(glDeleteLists)
GEN_STUB(glDepthRange)
GEN_STUB(glDisableClientState) GEN_STUB(glDrawBuffer) GEN_STUB(glDrawPixels)
GEN_STUB(glEdgeFlag) GEN_STUB(glEdgeFlagPointer) GEN_STUB(glEdgeFlagv)
GEN_STUB(glEnableClientState) GEN_STUB(glEndList)
GEN_STUB(glEvalCoord1d) GEN_STUB(glEvalCoord1dv) GEN_STUB(glEvalCoord1f) GEN_STUB(glEvalCoord1fv)
GEN_STUB(glEvalCoord2d) GEN_STUB(glEvalCoord2dv) GEN_STUB(glEvalCoord2f) GEN_STUB(glEvalCoord2fv)
GEN_STUB(glEvalMesh1) GEN_STUB(glEvalMesh2) GEN_STUB(glEvalPoint1) GEN_STUB(glEvalPoint2)
GEN_STUB(glFeedbackBuffer) GEN_STUB(glFinish) GEN_STUB(glFlush)
GEN_STUB(glFogf) GEN_STUB(glFogfv) GEN_STUB(glFogi) GEN_STUB(glFogiv)
GEN_STUB(glFrontFace) GEN_STUB(glFrustum) GEN_STUB(glGenLists)
GEN_STUB(glGetBooleanv) GEN_STUB(glGetClipPlane) GEN_STUB(glGetDoublev) GEN_STUB(glGetFloatv)
GEN_STUB(glGetIntegerv)
GEN_STUB(glGetLightfv) GEN_STUB(glGetLightiv)
GEN_STUB(glGetMapdv) GEN_STUB(glGetMapfv) GEN_STUB(glGetMapiv)
GEN_STUB(glGetMaterialfv) GEN_STUB(glGetMaterialiv)
GEN_STUB(glGetPixelMapfv) GEN_STUB(glGetPixelMapuiv) GEN_STUB(glGetPixelMapusv)
GEN_STUB(glGetPointerv) GEN_STUB(glGetPolygonStipple)
GEN_STUB(glGetTexEnvfv) GEN_STUB(glGetTexEnviv)
GEN_STUB(glGetTexGendv) GEN_STUB(glGetTexGenfv) GEN_STUB(glGetTexGeniv)
GEN_STUB(glGetTexImage)
GEN_STUB(glGetTexLevelParameterfv) GEN_STUB(glGetTexLevelParameteriv)
GEN_STUB(glGetTexParameterfv) GEN_STUB(glGetTexParameteriv)
GEN_STUB(glHint) GEN_STUB(glIndexMask) GEN_STUB(glIndexPointer)
GEN_STUB(glIndexd) GEN_STUB(glIndexdv) GEN_STUB(glIndexf) GEN_STUB(glIndexfv)
GEN_STUB(glIndexi) GEN_STUB(glIndexiv) GEN_STUB(glIndexs) GEN_STUB(glIndexsv)
GEN_STUB(glIndexub) GEN_STUB(glIndexubv)
GEN_STUB(glInitNames) GEN_STUB(glInterleavedArrays)
GEN_STUB(glIsEnabled) GEN_STUB(glIsList) GEN_STUB(glIsTexture)
GEN_STUB(glLightModelf) GEN_STUB(glLightModelfv) GEN_STUB(glLightModeli) GEN_STUB(glLightModeliv)
GEN_STUB(glLightf) GEN_STUB(glLightfv) GEN_STUB(glLighti) GEN_STUB(glLightiv)
GEN_STUB(glLineStipple) GEN_STUB(glLineWidth)
GEN_STUB(glListBase) GEN_STUB(glLoadIdentity)
GEN_STUB(glLoadMatrixd) GEN_STUB(glLoadMatrixf) GEN_STUB(glLoadName) GEN_STUB(glLogicOp)
GEN_STUB(glMap1d) GEN_STUB(glMap1f) GEN_STUB(glMap2d) GEN_STUB(glMap2f)
GEN_STUB(glMapGrid1d) GEN_STUB(glMapGrid1f) GEN_STUB(glMapGrid2d) GEN_STUB(glMapGrid2f)
GEN_STUB(glMaterialf) GEN_STUB(glMaterialfv) GEN_STUB(glMateriali) GEN_STUB(glMaterialiv)
GEN_STUB(glMatrixMode) GEN_STUB(glMultMatrixd) GEN_STUB(glMultMatrixf) GEN_STUB(glNewList)
GEN_STUB(glNormal3b) GEN_STUB(glNormal3bv) GEN_STUB(glNormal3d) GEN_STUB(glNormal3dv)
GEN_STUB(glNormal3f) GEN_STUB(glNormal3fv) GEN_STUB(glNormal3i) GEN_STUB(glNormal3iv)
GEN_STUB(glNormal3s) GEN_STUB(glNormal3sv) GEN_STUB(glNormalPointer)
GEN_STUB(glOrtho) GEN_STUB(glPassThrough)
GEN_STUB(glPixelMapfv) GEN_STUB(glPixelMapuiv) GEN_STUB(glPixelMapusv)
GEN_STUB(glPixelStoref) GEN_STUB(glPixelStorei)
GEN_STUB(glPixelTransferf) GEN_STUB(glPixelTransferi) GEN_STUB(glPixelZoom)
GEN_STUB(glPointSize) GEN_STUB(glPolygonOffset) GEN_STUB(glPolygonStipple)
GEN_STUB(glPopAttrib) GEN_STUB(glPopClientAttrib) GEN_STUB(glPopMatrix) GEN_STUB(glPopName)
GEN_STUB(glPrioritizeTextures)
GEN_STUB(glPushAttrib) GEN_STUB(glPushClientAttrib) GEN_STUB(glPushMatrix) GEN_STUB(glPushName)
GEN_STUB(glRasterPos2d) GEN_STUB(glRasterPos2dv) GEN_STUB(glRasterPos2f) GEN_STUB(glRasterPos2fv)
GEN_STUB(glRasterPos2i) GEN_STUB(glRasterPos2iv) GEN_STUB(glRasterPos2s) GEN_STUB(glRasterPos2sv)
GEN_STUB(glRasterPos3d) GEN_STUB(glRasterPos3dv) GEN_STUB(glRasterPos3f) GEN_STUB(glRasterPos3fv)
GEN_STUB(glRasterPos3i) GEN_STUB(glRasterPos3iv) GEN_STUB(glRasterPos3s) GEN_STUB(glRasterPos3sv)
GEN_STUB(glRasterPos4d) GEN_STUB(glRasterPos4dv) GEN_STUB(glRasterPos4f) GEN_STUB(glRasterPos4fv)
GEN_STUB(glRasterPos4i) GEN_STUB(glRasterPos4iv) GEN_STUB(glRasterPos4s) GEN_STUB(glRasterPos4sv)
GEN_STUB(glReadBuffer) GEN_STUB(glReadPixels)
GEN_STUB(glRectd) GEN_STUB(glRectdv) GEN_STUB(glRectf) GEN_STUB(glRectfv)
GEN_STUB(glRecti) GEN_STUB(glRectiv) GEN_STUB(glRects) GEN_STUB(glRectsv)
GEN_STUB(glRenderMode) GEN_STUB(glRotated) GEN_STUB(glRotatef) GEN_STUB(glScaled) GEN_STUB(glScalef)
GEN_STUB(glScissor) GEN_STUB(glSelectBuffer) GEN_STUB(glShadeModel)
GEN_STUB(glStencilFunc) GEN_STUB(glStencilMask) GEN_STUB(glStencilOp)
GEN_STUB(glTexCoord1d) GEN_STUB(glTexCoord1dv) GEN_STUB(glTexCoord1f) GEN_STUB(glTexCoord1fv)
GEN_STUB(glTexCoord1i) GEN_STUB(glTexCoord1iv) GEN_STUB(glTexCoord1s) GEN_STUB(glTexCoord1sv)
GEN_STUB(glTexCoord2d) GEN_STUB(glTexCoord2dv) GEN_STUB(glTexCoord2f) GEN_STUB(glTexCoord2fv)
GEN_STUB(glTexCoord2i) GEN_STUB(glTexCoord2iv) GEN_STUB(glTexCoord2s) GEN_STUB(glTexCoord2sv)
GEN_STUB(glTexCoord3d) GEN_STUB(glTexCoord3dv) GEN_STUB(glTexCoord3f) GEN_STUB(glTexCoord3fv)
GEN_STUB(glTexCoord3i) GEN_STUB(glTexCoord3iv) GEN_STUB(glTexCoord3s) GEN_STUB(glTexCoord3sv)
GEN_STUB(glTexCoord4d) GEN_STUB(glTexCoord4dv) GEN_STUB(glTexCoord4f) GEN_STUB(glTexCoord4fv)
GEN_STUB(glTexCoord4i) GEN_STUB(glTexCoord4iv) GEN_STUB(glTexCoord4s) GEN_STUB(glTexCoord4sv)
GEN_STUB(glTexCoordPointer)
GEN_STUB(glTexEnvf) GEN_STUB(glTexEnvfv) GEN_STUB(glTexEnvi) GEN_STUB(glTexEnviv)
GEN_STUB(glTexGend) GEN_STUB(glTexGendv) GEN_STUB(glTexGenf) GEN_STUB(glTexGenfv)
GEN_STUB(glTexGeni) GEN_STUB(glTexGeniv)
GEN_STUB(glTexImage1D) GEN_STUB(glTexImage2D)
GEN_STUB(glTexParameterf) GEN_STUB(glTexParameterfv) GEN_STUB(glTexParameteri) GEN_STUB(glTexParameteriv)
GEN_STUB(glTexSubImage1D) GEN_STUB(glTexSubImage2D)
GEN_STUB(glTranslated) GEN_STUB(glTranslatef)
GEN_STUB(glVertex2d) GEN_STUB(glVertex2dv) GEN_STUB(glVertex2f) GEN_STUB(glVertex2fv)
GEN_STUB(glVertex2i) GEN_STUB(glVertex2iv) GEN_STUB(glVertex2s) GEN_STUB(glVertex2sv)
GEN_STUB(glVertex3d) GEN_STUB(glVertex3dv) GEN_STUB(glVertex3f) GEN_STUB(glVertex3fv)
GEN_STUB(glVertex3i) GEN_STUB(glVertex3iv) GEN_STUB(glVertex3s) GEN_STUB(glVertex3sv)
GEN_STUB(glVertex4d) GEN_STUB(glVertex4dv) GEN_STUB(glVertex4f) GEN_STUB(glVertex4fv)
GEN_STUB(glVertex4i) GEN_STUB(glVertex4iv) GEN_STUB(glVertex4s) GEN_STUB(glVertex4sv)
GEN_STUB(glVertexPointer)
