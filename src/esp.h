// language: C++, file: src/esp.h
#pragma once

namespace esp {
	// Decides whether a glDrawElements call is likely a player skinned-mesh.
	// Pure render-side heuristic — no game-memory reads.
	bool is_player_draw(unsigned mode, int count, unsigned type);
	bool is_player_draw_arrays(unsigned mode, int count);
}
