#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterAnimationInst: the per-instance render node for an
// ANIMATION / imported-movie character. It is the animation sibling of
// AptCharacterSpriteInst: both derive from the shared movie-clip base
// AptCharacterSpriteInstBase (AptCIH -> AptCharacterInst -> AptRenderItem, plus
// the movie-clip frame/clip-event state + the embedded child display list). Where
// AptCharacterSpriteInst is the plain sprite leaf (adds nothing of its own), this
// instance carries the imported movie's own state: a reference to the source .apt
// file the animation was imported from, and it drives that movie's character-table
// teardown (AptCharacterAnimation::ClearCharacterList / ResetInitIndicators) when
// the instance is destroyed.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class (X360-only, like the
// sibling AptCharacterSpriteInst / AptCharacterMorphInst). SHAPE + BODIES are
// reconstructed STRICTLY from the X360 ARTIST.XEX (the authoritative spine). The
// X360 ledger attests exactly TWO real functions for the class:
//     AptCharacterAnimationInst::PreDestroy                 @ 0x82AFE148
//     AptCharacterAnimationInst::~AptCharacterAnimationInst @ 0x82AFFEC0
//     AptCharacterAnimationInst::`scalar deleting destructor' (thunk -- dropped;
//                                                 the ~ above is the real body)
// There is no ctor in the X360 ledger (construction is the inlined base ctor folded
// into the AptCharacterInst::CreateCharacterInst factory, as for the sibling sprite
// leaves), so only PreDestroy + the destructor are hand-written.
//
// BASE: AptCharacterSpriteInstBase (36 bytes / 0x24 -- AptCharacterInst 16 +
// mnGotoFrame/mnClipActionFlags/mnCurrentFrame/mDisplayList/mnLastActionFrame).
//
// LAYOUT (console; reconstructed with NAMED members so x64 widths stay correct):
//   AptCharacterSpriteInstBase base ......... 0x24 (36 bytes)
//   +0x24  mnAccumulatedUpdateMs    u32   the banked update milliseconds the
//                                          AptUpdate frame pacer accumulates
//                                          (AptUpdate @0x82B0DB68 / its per-target
//                                          driver @0x82B0D608 read it, add the
//                                          elapsed ms, subtract one authored frame
//                                          per tick, store the remainder back).
//                                          ATTESTED -- renamed from the old
//                                          mAnimationState_unknown placeholder when
//                                          the AptUpdate pacer landed.
//   +0x28  mAnimationFilePtr  AptFilePtr   the ref-counted source .apt file the
//                                          animation was imported from (an
//                                          AptSharedPtr<AptFile>, one pointer). The
//                                          destructor nulls it and drops one
//                                          reference (deleting the shared AptFile at
//                                          zero), the exact AptSharedPtr<AptFile>
//                                          teardown idiom (decref + AptSharedPtrDelete
//                                          at zero). The asm reads/writes this slot at
//                                          0x28(this) (a1[10]).
//
// sizeof is the MINIMUM consistent with the asm: the destructor's last member access
// is at +0x28 (a one-pointer AptFilePtr), giving 0x2C / 44. The class's exact size
// would be pinned by its scalar-deleting-destructor's sized pool free, but that
// compiler thunk is a separate function (not in this TU) -- so this models the
// smallest layout the two reconstructed bodies require.
//
// NON-virtual destructor: it must match the committed AptCharacterSpriteInstBase /
// AptCharacterInst convention, which models the console vtable as a manual
// `void* mpVTable_unused` member at offset 0 (non-polymorphic). A C++ `virtual` here
// would inject a second, compiler-synthesised vptr and fork the layout -> FAIL.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"   // base (36 bytes)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                  // AptFilePtr (the source-file ref)

struct AptCharacter;
struct AptCharacterAnimation;

// ---------------------------------------------------------------------------
// AptGetMovieCharacterAnimation (HOMED in AptCharacterAnimationInst.cpp): an
// animation/movie AptCharacter embeds its AptCharacterAnimation movie root by value
// immediately after the AptCharacter base (AptMovie.h documents the same embedded
// timeline). The destructor + ctor drive that embedded movie's table teardown/
// registration. The serialized record has no owning character-subtype header (the
// blob IS the runtime layout), so the embedded movie is reached through this single
// named accessor instead of a raw cast at the call sites.
//
// x64 fork (VERIFIED): the console reaches it as `addi r3, mpCharacter, 0x10` (the
// console sizeof(AptCharacter)); on the native-8 gate the serialised header widens to
// 0x20, so the embedded body is at char + KU_AptEmbeddedMovieOff (AptCIH.h; the same
// def-base offset AptGetClipMovie uses, verified vs TITLE_SCREEN02.bundle). Null-safe.
// ---------------------------------------------------------------------------
AptCharacterAnimation* AptGetMovieCharacterAnimation(AptCharacter* pCharacter);

struct AptCharacterAnimationInst : public AptCharacterSpriteInstBase
{
    // +0x24 -- the banked update milliseconds the AptUpdate frame pacer accumulates
    // (AptUpdate @0x82B0DB68 / its per-target driver @0x82B0D608 read v7[9], add the
    // elapsed ms, subtract one authored frame per tick, and store the remainder
    // back). Zeroed by the ctor. ATTESTED (was an unattested placeholder dword).
    uint32_t   mnAccumulatedUpdateMs;     // +0x24

    // +0x28 -- the ref-counted source .apt file the animation was imported from.
    AptFilePtr mAnimationFilePtr;         // +0x28

    // @0x82AFE148 -- pre-teardown: clear the inherited child display list's entries
    // (release the placed children) WITHOUT freeing its head node -- i.e.
    // mDisplayList.clear(false). This differs from the base
    // AptCharacterSpriteInstBase::PreDestroy (which calls mDisplayList.PreDestroy(),
    // freeing + nulling the head), so the animation inst defines its own. Non-virtual
    // for the same manual-vtable reason as the destructor.
    void PreDestroy();

    // @0x82AFFEC0 -- destroy the animation instance:
    //   * tear down the imported movie's character list
    //     (AptCharacterAnimation::ClearCharacterList on the movie root embedded in
    //     the current render item's character),
    //   * if the writable render item still has a character, reset that movie's
    //     init indicators (AptCharacterAnimation::ResetInitIndicators),
    //   * drop one reference on mAnimationFilePtr (deleting the shared AptFile at
    //     zero), then chain ~AptCharacterSpriteInstBase (the embedded display list +
    //     the AptCharacterInst base teardown, emitted by the compiler).
    // NON-virtual: matches the committed AptCharacterSpriteInstBase manual-vtable
    // convention (a virtual dtor here injects a second vptr -> layout fork -> FAIL).
    ~AptCharacterAnimationInst();
};

// ---------------------------------------------------------------------------
// MakeCharacterAnimationInst -- the AptCharacterAnimationInst::AptCharacterAnimationInst
// ctor @0x82AFFDE8 wrapped as the AptLinker::Update factory: pool-allocate the 44-byte
// instance, then construct it over the pending file's loaded movie root (the ctor's
// AptCharacter a2 = pFile->mpData) with an incref'd hold of pFile (the ctor's a3).
// Body in AptCharacterAnimationInst.cpp (the X360 ctor is folded -- no own export
// symbol -- disassembled from the decrypted ARTIST.XEX, cross-checked vs the PS3 lift).
// ---------------------------------------------------------------------------
struct AptFile;
AptCharacterAnimationInst* MakeCharacterAnimationInst(AptFile* pFile);
