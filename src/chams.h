// language: C++, file: src/chams.h
#pragma once

namespace chams {
	// wraps the draw with the two-pass chams render:
	//  pass 1: depth-test off, through-wall color
	//  pass 2: depth-test on,  visible color
	//  pass 3 (optional): wireframe overlay
	// then performs the "real" draw so hitboxes land unchanged.
	void wrap_draw_elements(unsigned mode, int count, unsigned type, const void* indices);
	void wrap_draw_arrays  (unsigned mode, int first, int count);
}
