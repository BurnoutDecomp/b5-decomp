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
//   * rw::core::stdc::MemClear(dst, size)        -- zero-fill (memset wrapper)
//   * rw::core::stdc::MemCopy(dst, src, size)    -- copy (memcpy wrapper)
//   * rw::core::stdc::MemCompare(a, b, size)     -- compare (memcmp wrapper; the
//                                                   ICETake undo DataChanged check)
//   * rw::core::stdc::Vsprintf(dst, fmt, args)   -- vararg formatter (vsprintf
//                                                   wrapper; the StringPrintf sink)
//   * rw::core::stdc::StringLength(s)            -- strlen wrapper (ICEFileHandler)
//   * rw::core::stdc::StringCat(dst, src)        -- strcat wrapper (ICEFileHandler);
//                                                   returns dst, like the C library
//   * rw::core::stdc::StringCopy(dst, src)       -- strcpy wrapper (ICEWidgetMenuItem);
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

    // memset-style fills. MemFill32 is the workhorse the others trampoline into:
    // it splats the low byte of luValue across luSize bytes of lpDst, in 64/16/4-byte
    // unrolled blocks after head-aligning the destination (the X360 build replicates
    // the byte into a 32-bit pattern and stores it whole-word). MemClear forwards as
    // MemFill32(dst, 0, size); MemFill8 forwards as MemFill32(dst, byte*0x01010101, size)
    // after broadcasting the byte. luValue's low byte is the fill byte (the high 24
    // bits are ignored by the byte-granular tails).
    void MemFill32(void* lpDst, u32 luValue, u32 luSize);
    void MemFill8(void* lpDst, u8 luValue, u32 luSize);

    // memcmp wrapper: compare luSize bytes of lpA against lpB, returning a value
    // <0 / 0 / >0 (C-library memcmp semantics). ICETake::DataChanged uses it to
    // detect whether the live take differs from its newest undo snapshot. The
    // buffers are read-only, modelled as const void*. DECLARATION-ONLY.
    s32 MemCompare(const void* lpA, const void* lpB, u32 luSize);

    // vsprintf wrapper: format lpcFormat with the variadic args in lvaArgs into
    // lpcDst, returning the number of characters written. The third argument is the
    // standard `va_list`. DECLARATION-ONLY.
    s32 Vsprintf(char* lpcDst, const char* lpcFormat, va_list lvaArgs);

    // vsnprintf wrapper: like Vsprintf, but bounded to liSize bytes of lpcDst
    // (the size-limited formatter). ICERender::ScrPrintfArg formats the on-screen
    // debug text into a 256-byte stack buffer through this. DECLARATION-ONLY.
    s32 Vsnprintf(char* lpcDst, s32 liSize, const char* lpcFormat, va_list lvaArgs);

    // strlen wrapper: number of characters in the NUL-terminated lpcString. The
    // argument is read-only text, modelled as `const char*`. DECLARATION-ONLY.
    s32 StringLength(const char* lpcString);

    // strcat wrapper: append lpcSrc onto the NUL-terminated lpcDst and return
    // lpcDst (C-library strcat semantics). The buffers are NUL-terminated text,
    // modelled as char* here. DECLARATION-ONLY.
    char* StringCat(char* lpcDst, const char* lpcSrc);

    // strcpy wrapper: copy the NUL-terminated lpcSrc into lpcDst and return lpcDst
    // (C-library strcpy semantics). The buffers are read-only source / writable
    // destination text, modelled as const char*/char* here. DECLARATION-ONLY.
    char* StringCopy(char* lpcDst, const char* lpcSrc);

    // strncpy wrapper: copy at most luCount characters of the NUL-terminated lpcSrc
    // into lpcDst and return lpcDst (C-library strncpy semantics). ICEAuthor::
    // CreateNewTake uses it to copy a new take name (capped at the 32-byte take-name
    // field) into the freshly allocated take. The destination is writable, the source
    // read-only text. DECLARATION-ONLY.
    char* StringnCopy(char* lpcDst, const char* lpcSrc, u32 luCount);

    // snprintf wrapper: format lpcFormat with the trailing variadic args into the
    // first liSize bytes of lpcDst, returning the number of characters written (the
    // size-bounded sibling of the C-library sprintf). ICEAuthor::SaveTake formats the
    // take guid as a decimal string through it before emitting the take XML.
    // DECLARATION-ONLY.
    s32 Snprintf(char* lpcDst, s32 liSize, const char* lpcFormat, ...);

    // atoi wrapper: parse the leading decimal integer of the NUL-terminated
    // lpcString (@0x82BC7650). BrnDirector::DirectorDevTools::GameTalkMsgHandler
    // parses the ICE take GUIDs / post-FX hook value from the GameTalk message
    // content through it. DECLARATION-ONLY.
    s32 ConvertAToI(const char* lpcString);

    // strcmp wrapper: compare the NUL-terminated lpcA against lpcB, returning a
    // value <0 / 0 / >0 (C-library strcmp semantics; 0 means equal). The ICE menu
    // widget uses it to detect empty cells (cell text == the empty sentinel) and
    // the "<ICON>" cell marker. Both buffers are read-only text, modelled as
    // const char*. The bytes are compared as signed char (the X360 build sign-
    // extends each byte before the difference).
    s32 StringCompare(const char* lpcA, const char* lpcB);

    // stricmp wrapper: case-insensitive compare of the NUL-terminated lpcA against
    // lpcB, returning <0 / 0 / >0 (0 means equal ignoring ASCII case). Each step
    // normalises lpcA's character toward lpcB's case before comparing, so the result
    // is keyed on the (case-folded) lpcA byte vs the raw lpcB byte. The device/bundle
    // lookups use it for case-insensitive name matching.
    s32 StringNoCaseCompare(const char* lpcA, const char* lpcB);

    // strupr-style copy: copy the NUL-terminated lpcSrc into lpcDst, upper-casing the
    // ASCII a-z range as it goes, and return lpcDst. The bundle loader upper-cases
    // resource names through it. Source read-only, destination writable.
    char* StringToUpperCase(char* lpcDst, const char* lpcSrc);

    // -----------------------------------------------------------------------------
    // Integer/hex -> ASCII formatters (the building blocks the Vsprintf/Vsnprintf
    // formatter dispatches into for %d/%i/%u/%x/%X/%p). Each writes the decimal/hex
    // digits of the value into lpcDst as a NUL-terminated string and returns lpcDst.
    //
    // DECLARATION-ONLY: the X360 bodies emit the digits via a recursive high-order
    // digit helper (a private rwcore routine, not part of this class' function set)
    // and the printf family that consumes them is itself declaration-only here, so
    // there is no grounded body to reconstruct without fabricating that helper. The
    // signatures are pinned from the call sites in Vsprintf/Vsnprintf:
    //   * liRadix selects base 10 vs 8 (ConvertIToA) / 10 (ConvertI64ToA);
    //   * ConvertXToA's liUpperCase picks 'a'-'f' (0) vs 'A'-'F' (>0) hex digits.
    char* ConvertIToA(s32 liValue, char* lpcDst, u32 luRadix);
    char* ConvertI64ToA(s64 llValue, char* lpcDst, s32 liRadix);
    char* ConvertXToA(s32 liValue, char* lpcDst, s32 liUpperCase);
}
}
}
