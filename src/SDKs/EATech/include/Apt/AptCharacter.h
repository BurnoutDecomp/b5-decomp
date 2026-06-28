#pragma once

// ===========================================================================
// EATech Apt -- AptCharacter: the base of the whole .apt character hierarchy.
//
// Every entry in an AptCharacterAnimation's character table is an AptCharacter
// (or a type-tagged subclass). The runtime instances (AptCIH / the AptRenderItem
// family) all hang off it. SHAPE + BODIES from the PS3 EXTERNAL ELF:
//   AptCharacter::GetRefCount               @0x7DEDDC
//   AptCharacter::AddCharacterReference     @0x7E39D8
//   AptCharacter::ReleaseCharacterReference @0x80F2B0
//   AptCharacter::ReleaseAnimationFile      @0x80F204
//   AptCharacter::SetupCharacter            @0x80E894 (post-Fixup per-character init)
//
// BASE LAYOUT (console offsets; reconstructed with NAMED members so x64 widths
// are correct -- the engine is 32-bit-pointer + hard-coded-offset code):
//   +0  mnType          the character tag (the AptCharacterAnimation::Fixup switch:
//                       1, 2, 3, 5, 7, 9, 0xA -> shape/sprite/text/movie/font/...);
//                       subclasses add type-specific fields after the base.
//   +4  mpFixupLink     wired during Fixup to a character-table pointer (a
//                       back-link); pointer-typed so the x64 width is correct.
//   +8  mnRefAndFlags   PACKED: high 16 bits = reference count (Add/Release mutate
//                       it with the lwarx/stwcx. +/-0x10000 idiom), low 16 bits =
//                       per-type flags (SetupCharacter twiddles these). The packed
//                       arithmetic is endian-independent on the full 32-bit word;
//                       only halfword *addressing* would be endian-specific, so we
//                       operate on the whole word. (FLAG: console atomic -> host
//                       interlocked / single-threaded.)
//   +12 mpAnimationFile the owning .apt's AptFilePtr (a counted AptFile*); released
//                       by ReleaseAnimationFile/SetupCharacter.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptFile;   // SDKs/EATech/include/Apt/AptFile.h (mpAnimationFile is an AptFile*)
struct AptRenderingContext;
enum AptMaskRenderOperation : int;

struct AptCharacter
{
    int32_t           mnType;
    AptCharacter*     mpFixupLink;
    volatile uint32_t mnRefAndFlags;
    AptFile*          mpAnimationFile;

    // Reference count = the high 16 bits of mnRefAndFlags. @0x7DEDDC
    uint32_t GetRefCount() const;

    // Atomically bump / drop the character reference count (high 16 bits). When
    // ReleaseCharacterReference drops it to zero it releases the animation file.
    void AddCharacterReference() const;        // @0x7E39D8
    void ReleaseCharacterReference() const;    // @0x80F2B0

    // Release mpAnimationFile (AptSharedPtr DecRef/Delete) and null it. @0x80F204
    void ReleaseAnimationFile() const;

    // Post-Fixup per-character init: drop the animation-file ref, clear the count,
    // and set the type-specific low-16 flags. @0x80E894
    void SetupCharacter();

    // Draw a shape character's geometry. @0x810E74 -- for a plain shape (mnType
    // == 1, no imported sub-characters) it hands the shape to the host rasterizer
    // hook; the imported-sub-character resolution path is deferred (see the .cpp).
    void render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick);
};
