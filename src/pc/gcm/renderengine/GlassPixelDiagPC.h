#pragma once
#include "types.hpp"
namespace renderengine {
// Default-off readback around the original glass draw. No state/geometry change.
bool GlassPixelDiag_BeginPC();
void GlassPixelDiag_EndPC(u32 batches,u64 acceptedDraws);
}
