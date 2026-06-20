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
//   * rw::core::stdc::StringLength(s)            -- strlen wrapper (ICEFileHandler)
//   * rw::core::stdc::StringCat(dst, src)        -- strcat wrapper (ICEFileHandler);
//                                                   returns dst, like the C library
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

    // vsnprintf wrapper: like Vsprintf, but bounded to liSize bytes of lpcDst
    // (the size-limited formatter, X360 vsnprintf wrapper). ICERender::ScrPrintfArg
    // formats the on-screen debug text into a 256-byte stack buffer through this.
    // DECLARATION-ONLY.
    s32 Vsnprintf(char* lpcDst, s32 liSize, const char* lpcFormat, va_list lvaArgs);

    // strlen wrapper: number of characters in the NUL-terminated lpcString. The
    // X360 Hex-Rays renders the argument as `_BYTE*`; it is read-only text, so it
    // is modelled as `const char*`. DECLARATION-ONLY.
    s32 StringLength(const char* lpcString);

    // strcat wrapper: append lpcSrc onto the NUL-terminated lpcDst and return
    // lpcDst (C-library strcat semantics). The X360 Hex-Rays types the buffers as
    // `unsigned __int8*`/`char*`; modelled as char* here. DECLARATION-ONLY.
    char* StringCat(char* lpcDst, const char* lpcSrc);
}
}
}
