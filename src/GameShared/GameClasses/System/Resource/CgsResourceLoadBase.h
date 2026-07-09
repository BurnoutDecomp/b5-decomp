#pragma once

#include "types.hpp"            // u32
#include "rw/rwcore_structs.h"  // rw::Resource (complete)

namespace CgsResource
{
// A loaded resource holds the base address of each of its sub-allocations in
// rw::Resource::m_baseResources[]; resource fix-up adds/subtracts segment 0's
// base as the relocation delta. The X360 build read this as the first dword of
// the rw::Resource object (*(const u32*)&resource); on the x64 PC layout that is
// the low 32 bits of m_baseResources[0]. Naming it here replaces the per-handler
// blob pokes recovered from rwcore.pdb.
inline u32 GetLoadBase(const rw::Resource& lrResource)
{
    return static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
}

// The full-width load base for handlers whose (x64-widened) offset slots relocate to
// real 64-bit pointers -- the u32 form above truncates the x64 heap base. Same segment-0
// read; the console delta arithmetic is 32-bit only because its pointers are.
inline uintptr_t GetLoadBase64(const rw::Resource& lrResource)
{
    return reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);
}
}
