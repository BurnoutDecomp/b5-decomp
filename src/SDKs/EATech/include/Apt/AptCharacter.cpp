// ===========================================================================
// EATech Apt -- AptCharacter base methods.   DECOMPILED from the PS3 EXTERNAL ELF
// (cross-checked vs X360 ARTIST).
//
// The reference count lives in the HIGH 16 bits of mnRefAndFlags; the console
// mutates it with the interrupt-masked lwarx/stwcx. +/-0x10000 idiom. Operating
// on the full 32-bit word is endian-independent, so the host interlocked add on
// the whole word is the faithful equivalent (FLAG: console atomic).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptFile.h"        // (mpAnimationFile)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptSharedPtrDecRef / AptSharedPtrDelete
#include "SDKs/EATech/include/Apt/AptRenderHooks.h" // AptHook_DrawShape (the RW boundary)

#include <intrin.h>   // _InterlockedExchangeAdd (MSVC)

// GetRefCount @0x7DEDDC -- the high-16 of the packed word.
uint32_t AptCharacter::GetRefCount() const
{
    return mnRefAndFlags >> 16;
}

// AddCharacterReference @0x7E39D8 -- atomic +1 to the high-16 reference count.
// (The console re-stores 0xFFFF before the +0x10000 when the count is already
// saturated; that guard is a no-op -- it writes back the same value -- so the
// faithful behaviour is simply the +0x10000 add.)
void AptCharacter::AddCharacterReference() const
{
    _InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(&const_cast<AptCharacter*>(this)->mnRefAndFlags), 0x10000);
}

// ReleaseCharacterReference @0x80F2B0 -- atomic -1 to the high-16 reference count;
// when it reaches zero, release the owning animation file.
void AptCharacter::ReleaseCharacterReference() const
{
    const long prev = _InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(&const_cast<AptCharacter*>(this)->mnRefAndFlags), -0x10000);
    const long now  = prev - 0x10000;
    if ((now & 0xFFFF0000) == 0)
        ReleaseAnimationFile();
}

// ReleaseAnimationFile @0x80F204 -- drop the AptFilePtr at mpAnimationFile.
void AptCharacter::ReleaseAnimationFile() const
{
    AptFile* p = mpAnimationFile;
    const_cast<AptCharacter*>(this)->mpAnimationFile = nullptr;
    if (p)
    {
        if (AptSharedPtrDecRef(p) == 0)
            AptSharedPtrDelete(p);
    }
}

// SetupCharacter @0x80E894 -- run after Fixup for each character: release the
// animation-file reference, clear the high-16 count, and set the low-16 flags by
// type. (The flag twiddling is the console's halfword-rotate idiom expressed on
// the full word; behaviour preserved.)
void AptCharacter::SetupCharacter()
{
    ReleaseAnimationFile();

    // Clear the reference count (high 16 bits); flags (low 16) stay.
    mnRefAndFlags &= 0x0000FFFFu;

    if (mnType == 1)
    {
        // Flag bit 15 = "low 15 flag-bits are non-zero".
        if (mnRefAndFlags & 0x7FFFu)
            mnRefAndFlags |= 0x8000u;
        else
            mnRefAndFlags &= ~0x8000u;
    }
    else if (mnType == 7)
    {
        // Clear flag bits 14-15.
        mnRefAndFlags &= ~0xC000u;
    }
    // else: just the count clear above.
}

// render @0x810E74 -- draw a shape character's geometry.
//
// The console handles only shape characters (mnType == 1) here; other character
// types render through their own paths (the display-list walk / the render-item
// subtypes), so this returns for them. For a shape:
//   * the SIMPLE case (no imported sub-characters -- flag bit 0x8000 clear) hands
//     the shape straight to the host shape-draw rasteriser hook;
//   * the IMPORT case walks the owning movie's character table, resolves each
//     imported sub-character against its source .apt (IsImport/GetIDFromImport-
//     File + the resolve/draw hooks) and draws the resolved glyphs.
//
// FLAG: the import-resolution branch couples to the full loaded-.apt data layout
// (built by the .apt parse) + the import draw/resolve hooks; it is deferred. The
// base shape geometry is still drawn through the hook in both cases, so a plain
// (non-importing) shape -- the common boot-UI case -- renders correctly now. The
// host shape-draw hook (AptHook_DrawShape) IS the RW 2D rasteriser boundary.
void AptCharacter::render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick)
{
    (void)pCtx;   // the transform was already set on the context (PushMatrices ->
                  // pfnSetVertexMatrix); the rasteriser uses the current state.

    if (mnType != 1)
        return;   // only shape characters draw via this entry point

    // (FLAG: when (mnRefAndFlags & 0x8000) is set the console resolves + draws the
    //  shape's imported sub-character glyphs first -- deferred with the .apt parse.)
    AptHook_DrawShape(this, eOp, nTick);   // console: dword_1059C6B8(char[8], op, tick)
}
