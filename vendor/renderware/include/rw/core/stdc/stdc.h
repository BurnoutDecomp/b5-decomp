#pragma once

#include "types.hpp"

#include <cstdarg>   // va_list (Vsprintf)

// Canonical RenderWare SDK home for the rwcore standard-C wrappers
// (EARenderWare rwcore, rw/core/stdc/stdc.h). RenderWare routes its raw memory
// and string operations through this thin namespace so the engine can swap in
// platform/SIMD-optimised implementations; the ICE camera-take code uses
// MemClear/MemCopy (ICETakeData) and Vsprintf (ICEMath::StringPrintf) here.
//
// MINIMAL SLICE -- declares exactly the entry points the ICE bodies call:
//   * rw::core::stdc::MemClear(dst, size)        -- zero-fill (X360 memset wrapper)
//   * rw::core::stdc::MemCopy(dst, src, size)    -- copy (X360 memcpy wrapper)
//   * rw::core::stdc::Vsprintf(dst, fmt, args)   -- vararg formatter (X360 vsprintf
//                                                   wrapper; the StringPrintf sink)
// DECLARATION-ONLY: the real bodies live in the rwcore stdc TU and the per-TU
// `cl /c` gate does not link. When the full rwcore stdc surface is reconstructed,
// GROW this header in place (MemSet/MemMove/MemCmp/...) -- do NOT fork it.
//
// Size args are typed u32 to match the call sites (the lengths passed are small
// take-buffer sizes); the rwcore signatures use RwUInt32, an unsigned 32-bit int.

namespace rw
{
namespace core
{
namespace stdc
{
    void MemClear(void* lpDst, u32 luSize);
    void MemCopy(void* lpDst, const void* lpSrc, u32 luSize);

    // vsprintf wrapper: format lpcFormat with the variadic args in lvaArgs into
    // lpcDst, returning the number of characters written. The X360 build passes a
    // pointer into the spilled register-save area as the third argument; on PC
    // that is the standard `va_list`. DECLARATION-ONLY.
    s32 Vsprintf(char* lpcDst, const char* lpcFormat, va_list lvaArgs);
}
}
}
