#pragma once
#include "types.hpp"
namespace renderengine {
// Default-off draw-owner observation. The material's resident name is copied;
// no renderer setting or particle data is changed.
bool LionDrawDiag_EnabledPC();
void LionDrawDiag_SetMaterialPC(const char* name,u32 flags,u32 shader,u32 blend,
    u32 textureHash,u32 mapIndex,u64 resourceId,const void* sourceEntry,
    const void* expectedNative);
}
