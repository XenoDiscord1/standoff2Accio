# Standoff2 BlueStacks Cheat (opengl32.dll proxy)

ESP + Chams for Standoff 2 running inside **BlueStacks 5 / nxt** emulator on Windows x64.
Hooks the **host-side** `opengl32.dll` that BlueStacks uses to translate the guest Android
`libGLESv2.so` draw calls. All model draws from the emulated game pass through our hooks,
so chams and visibility ESP are purely render-side (no game-memory reads, no offsets, works
across game updates).

Based on techniques from:
- [Oifox/Standoff2-Chams](https://github.com/Oifox/Standoff2-Chams) — glDrawElements + glGetUniformLocation chams, color + wireframe
- [SlyrithDevelopment/Android-OpenGL-ES-Chams](https://github.com/SlyrithDevelopment/Android-OpenGL-ES-Chams) — generic ES chams
- [hntr111/0.39.2-standoff-2-source-cheat-src](https://github.com/hntr111/0.39.2-standoff-2-source-cheat-src), [source-0.39.1-src-standoff-2-ONLY-ESP](https://github.com/hntr111/source-0.39.1-src-standoff-2-ONLY-ESP) — ESP structure
- [andrd3v/Excalibur_v.2.0](https://github.com/andrd3v/Excalibur_v.2.0) — ESP overlay reference

## Features

- **Color Chams** — flat solid color over every detected player model
- **Wireframe Chams** — line draw mode so enemies glow through walls
- **Visibility ESP** — two-pass draw: colored through walls, different color when visible, acts as walls-off highlighting
- **ImGui menu** — toggle each feature at runtime, pick colors, insert key = `INSERT`
- **Zero offsets** — nothing in this project is tied to a specific Standoff 2 build

## Install

1. Download `Standoff2Cheat.zip` from the GitHub Actions **Artifacts** on the latest `main` build.
2. Extract. You get `opengl32.dll`.
3. Close BlueStacks completely (check tray).
4. Drop `opengl32.dll` into `D:\Program Files\BlueStacks_nxt\`
   (same folder as `HD-Player.exe` / `BlueStacks.exe`).
5. Set BlueStacks graphics renderer to **OpenGL** (Settings → Graphics → Graphics engine → OpenGL).
6. Launch BlueStacks → Standoff 2.
7. Press **INSERT** in-game to open menu.

If the menu does not appear, see Troubleshooting below.

## Build from source (GitHub Actions)

1. Fork or push this repo to GitHub.
2. Push to `main` or trigger the workflow manually.
3. Download `Standoff2Cheat` artifact from the completed run.

Local build (optional): open `Standoff2Cheat.sln` in **Visual Studio 2022** with "Desktop development with C++"
workload installed. Submodules required — clone with `git clone --recursive`, or run:

```
git submodule update --init --recursive
```

Build config: **x64 / Release**. Output: `x64\Release\opengl32.dll`.

## How it works

### Proxy
Windows DLL loader resolves `opengl32.dll` by search path — BlueStacks's own folder is searched
before `System32`. Our DLL exports the same symbols BlueStacks imports; each export is a thin
stub that calls the real function in `C:\Windows\System32\opengl32.dll` which we load manually
at `DllMain`.

### Hooks
At first `wglMakeCurrent` we lazily bind GL function pointers. Because our DLL IS the
`opengl32.dll` BlueStacks links against, we don't need an inline hooker — every call
BlueStacks makes to `glDrawElements`, `wglSwapBuffers`, `wglGetProcAddress` lands directly
in our exported stubs. We intercept:
- `glDrawElements` / `glDrawArrays` — chams passes before/after the real draw
- `wglGetProcAddress` — swap in our own stubs for `glUseProgram` + `glGetUniformLocation`
  so we can track the active shader and remember where the color uniform lives
- `wglSwapBuffers` — ImGui pass + per-frame state tick (hotkey handling)
- `wglMakeCurrent` — one-shot initialization of hooks + GUI once a GL context is live

### Player detection heuristic
Standoff 2 player skinned-mesh draws have distinctive index counts (several thousand indices,
GL_TRIANGLES, uses a skinning shader with a bone-matrix uniform). We flag a draw as "player"
when a shader program with `boneMatrices` / `_Bones` uniform is bound. This mirrors Oifox's
approach and works without any game offsets.

### Chams
Before the flagged draw:
1. Save `GL_DEPTH_TEST` / blend / polygon-mode state.
2. First pass: `glDisable(GL_DEPTH_TEST)`, override color uniform to **through-wall color**, draw.
3. Second pass: `glEnable(GL_DEPTH_TEST)`, override color uniform to **visible color**, draw.
4. Restore state. Let the original draw happen (so hitboxes/animations still land cleanly).

Wireframe variant adds `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` on pass 1 and restores.

### ESP overlay
ImGui is drawn in `wglSwapBuffers` just before the real swap. Status strings (FPS, hooks installed,
feature toggles) plus the menu itself.

## Config / keys

- `INSERT` — toggle menu
- `END` — panic (unload hooks, stay as pass-through proxy)
- Colors / toggles — menu

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| BlueStacks won't start, instant crash | wrong renderer (DirectX) — switch to OpenGL in BlueStacks settings |
| BlueStacks starts but no menu | our DLL loaded but `wglMakeCurrent` was not hooked — check `%TEMP%\s2cheat.log` |
| Chams don't show on players | shader detection missed — enable "debug: log shader uniforms" in menu, send the log |
| Game displays, menu displays, but no chams and no log entry for draws | BlueStacks is using Vulkan or ANGLE — force OpenGL in settings |
| "The code execution cannot proceed because opengl32.dll is missing" | our proxy failed to load real opengl32 — ensure `C:\Windows\System32\opengl32.dll` exists (always does on real Windows) |
| Access violation at startup | 32-bit vs 64-bit mismatch — this build is x64 only. BlueStacks 5 / nxt is x64. BlueStacks 4 (32-bit) will not load this. |

## Legal

For personal research and reverse-engineering education. You are responsible for how you use it.
Running cheats on live servers violates Standoff 2's TOS and will get your account banned —
use a throwaway account.

## Layout

```
standoff2-cheat/
├── .github/workflows/build.yml       github actions: msbuild + zip artifact
├── Standoff2Cheat.sln
├── Standoff2Cheat.vcxproj
├── Standoff2Cheat.vcxproj.filters
├── src/
│   ├── dllmain.cpp                   entry
│   ├── proxy.h / proxy.cpp           real opengl32 loader + fnptr table
│   ├── proxy_exports.def             export list (fed to linker)
│   ├── hooks.h / hooks.cpp           minhook setup + hook impls
│   ├── chams.h / chams.cpp           chams passes
│   ├── esp.h / esp.cpp               player-draw detection, highlight pass
│   ├── gui.h / gui.cpp               imgui menu
│   ├── config.h                      global state
│   ├── utils.h / utils.cpp           log, keypress helpers
├── vendor/                           imgui (git submodule)
└── .gitmodules
```
