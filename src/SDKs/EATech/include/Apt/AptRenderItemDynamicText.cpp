// ===========================================================================
// EATech Apt -- AptRenderItemDynamicText.   DECOMPILED from the X360 ARTIST.XEX.
//   ctor                          0x82AEC0D0   (factory case 2: base item + the
//                                               authored text-field defaults)
//   Clone                         0x82AEFE38   (vtable off_821458B4, copy helper
//                                               sub_82AEF678 = the clone copy-ctor)
//   Render                        0x82AEFAD0   / SetZID 0x82ADB578
//   SetTextFormat                 0x82AEC1D0
//   `vector deleting destructor'  0x82AF4E78   (compiler thunk; the base sized
//                                               operator delete frees the pool block)
//
// The dynamic-text render item owns the per-tick mutable text-field state. The
// factory ctor (Manager_CreateItem case 2) seeds it from the AptCharacterDynamicText
// authored defaults: the field margins become the layout bounds rect, the default
// font size + glyph index become mFontSize / mFontID, the two authored colour/align
// dwords seed mTextColor + the alignment field, the background colour starts white,
// the box alignment starts 3, and the default mouse-wheel flag comes from the global
// config byte. The two strings + the TextFormat object + the render-data handle
// start empty.
//
// The render-data handle (mZID) + the TextFormat object (mpTextFormat) reach the
// host through the Apt render hooks (the SetVertexMatrix-adjacent function-pointer
// slots dword_8324E864 = release / dword_8324E868 = draw); modelled here as named
// hook pointers the host text-render integration installs.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"   // authored text-field defaults
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"          // mpTextFormat (TextFormat object)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"         // mTextValue / mVarValue teardown
#include "SDKs/EATech/include/Apt/AptDefine.h"                  // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                     // DOGMA_PoolManager

#include <new>   // placement new

// ---------------------------------------------------------------------------
// Host text-render hooks + sentinels (the X360 .data function-pointer slots the
// host's text-render integration installs; the SetVertexMatrix-adjacent family).
// Declared here as named externs (homed by the host integration, like the
// custom-control hooks in AptRenderHooks.h); null until then, so the text draw /
// render-data release are inert on the current bring-up path.
//   dword_8324E868(zid, eOp, nTick)  -- draw the resolved text render data
//   dword_8324E864(zid, nOp)         -- release the text render data (nOp 2 = free)
// gAptDefaultTextMouseWheelEnabled (byte_8324E392) is the authored default for the
// mouse-wheel flag stamped into a fresh field.
// gAptEmptyTextRenderDataZID (&unk_82F72DB0) is the "unresolved/empty handle"
// sentinel -- a field whose handle equals it has nothing to draw / release.
// FLAG: on x64 the render-data handle is a 32-bit host id (the console stored a
// 32-bit pointer in this slot); kept as the header's int32_t mZID.
// ---------------------------------------------------------------------------
extern void (*gpfnAptDrawTextRenderData)(intptr_t nZId, AptMaskRenderOperation eOp, int nTick);  // dword_8324E868
extern void (*gpfnAptReleaseTextRenderData)(intptr_t nZId, int nOp);                        // dword_8324E864
extern bool gAptDefaultTextMouseWheelEnabled;                                               // byte_8324E392
extern intptr_t gAptEmptyTextRenderDataZID;                                                 // &unk_82F72DB0

// ---------------------------------------------------------------------------
// Factory ctor @0x82AEC0D0 -- base render item + the dynamic-text render-type flag,
// then stamp the authored text-field defaults read from the AptCharacterDynamicText.
// (The two EAStringC members + the AptRect default-construct; the console wrote the
// empty-string sentinel + the bounds by hand inline -- the same net result.)
// ---------------------------------------------------------------------------
AptRenderItemDynamicText::AptRenderItemDynamicText(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
{
    const AptCharacterDynamicText* pText = static_cast<const AptCharacterDynamicText*>(pCharacter);

    // Dynamic-text render-type flag: bit 19 (console __ROR4__(1,13) & 0x00FC0000).
    mFlags = (mFlags & 0xFF03FFFFu) | 0x00080000u;

    mZID         = 0;          // no render-data handle yet
    mScroll      = 1;          // console seeds the scroll offset to 1
    mpTextFormat = nullptr;    // no TextFormat object yet
    mStateFlags  = 4;          // console seeds the state flags to 4

    // Authored colour + font from the character.
    mTextColor = static_cast<uint32_t>(pText->mnAuthoredReserved1);   // char +0x28
    mFontSize  = pText->mfFontSize;                                   // char +0x2C
    mFontID    = pText->mnDefaultGlyphIndex;                          // char +0x20

    // Packed back-colour dword: background colour forced white (bits 8-31), the
    // alignment field (bits 3-6) from the character's authored align dword. The
    // console preserved the (uninitialised) low/draws-background bits here; they
    // are initialised deterministically to 0 (the portable de-opt).
    mFlagsAndBackColor = 0xFFFFFF00u;                                 // backColor = 0xFFFFFF
    SetAlignment(pText->mnAuthoredReserved0);                         // char +0x24, bits 3-6

    // Packed border-colour dword: border colour + draws-border 0, box alignment 3
    // (bits 2-5), mouse-wheel flag (bit 6) from the global authored default.
    mFlagsAndBorderColor = 0u;
    SetBoxAlignment(3);
    SetMouseWheelEnabled(gAptDefaultTextMouseWheelEnabled);

    // Layout bounds rect = the character's four field-edge margins.
    mBounds.fLeft   = pText->mfMarginLeft;     // char +0x10
    mBounds.fTop    = pText->mfMarginTop;      // char +0x14
    mBounds.fRight  = pText->mfMarginRight;    // char +0x18
    mBounds.fBottom = pText->mfMarginBottom;   // char +0x1C
}

// ---------------------------------------------------------------------------
// Clone copy-ctor (sub_82AEF678) -- base clone copy + re-stamp the dynamic-text
// render-type flag. The strings / colours / bounds / font copy from the source.
// VERIFIED vs the asm (sub_82AEF678 @0x82AEF678 IS present in .ida-exports/ARTIST):
// mStateFlags is hard-coded to 4 (li r7,4 -> stw 0x6C), NOT copied; mZID resets to 0.
// FLAG (deferred, NOT guessed): the console DEEP-COPIES mpTextFormat -- 0x82AEF80C: if
// source->mpTextFormat (0x68) != null it Allocates a 0x20 block from gpNonGCPoolManager
// and copy-constructs a fresh TextFormat from the source's, else leaves null. Kept null
// here because the TextFormat value type is still opaque (an AptValue* that SetTextFormat
// tears down as an EAStringC 0x20 block); a wrong deep-copy would double-free. Re-home
// with the TextFormat layer. Effect today: cloning a text field with active formatting
// loses its format until the next resolve -- safe (no UB), just not yet faithful.
// ---------------------------------------------------------------------------
AptRenderItemDynamicText::AptRenderItemDynamicText(const AptRenderItemDynamicText* pSource,
                                                   int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & 0xFF03FFFFu) | 0x00080000u;

    mTextValue           = pSource->mTextValue;
    mVarValue            = pSource->mVarValue;
    mTextColor           = pSource->mTextColor;
    mScroll              = pSource->mScroll;
    mFlagsAndBackColor   = pSource->mFlagsAndBackColor;
    mFlagsAndBorderColor = pSource->mFlagsAndBorderColor;
    mBounds              = pSource->mBounds;
    mFontSize            = pSource->mFontSize;
    mFontID              = pSource->mFontID;
    mStateFlags          = 4;          // console (sub_82AEF678 @0x82AEF7F8) hard-codes 4, NOT a source copy

    mZID         = 0;          // asm stores 0 to 0x3C (the render-data handle re-resolves)
    mpTextFormat = nullptr;    // FLAG (see header note above): asm deep-copies; deferred with the TextFormat layer
}

// ---------------------------------------------------------------------------
// dtor -- the two EAStringC members destruct automatically (the compiler emits the
// refcount drop) and the base ~AptRenderItem runs.
// FLAG: the real ~AptRenderItemDynamicText is OUT OF SCOPE for this TU -- the
// dossier marks it [external/unknown] (only the vector-deleting-destructor
// @0x82AF4E78 is in scope, and it just calls this dtor then the sized base delete).
// Its body is NOT in the asm, so the owned-resource teardown (the mpTextFormat
// 0x20-byte pool block + the render-data handle release) is DEFERRED rather than
// guessed -- a fabricated free here could double-free or release out of order vs
// the real console dtor. Bodied as an empty stub so the class is destroyable and
// the vector-deleting-destructor synthesizes; the resource teardown is the
// out-of-scope follow-on (see SetTextFormat/SetZID for the per-resource teardown
// the real dtor is expected to mirror).
// ---------------------------------------------------------------------------
AptRenderItemDynamicText::~AptRenderItemDynamicText()
{
}

// ---------------------------------------------------------------------------
// Clone @0x82AEFE38 -- pool-allocate a fresh dynamic-text render item (112 bytes
// on the console; sizeof here for the x64 build) copy-initialised from this one
// (null on pool exhaustion).
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderItemDynamicText::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemDynamicText));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemDynamicText(this, nCreatedOnTick, bCopyExtended);
}

// ---------------------------------------------------------------------------
// Render @0x82AEFAD0 -- when the field has a resolved render-data handle (set, and
// not the empty sentinel), bracket the host text-draw hook with this item's
// transforms.
// ---------------------------------------------------------------------------
void AptRenderItemDynamicText::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    if (mZID && mZID != gAptEmptyTextRenderDataZID)
    {
        PushMatrices(pCtx, this);
        if (gpfnAptDrawTextRenderData)
            gpfnAptDrawTextRenderData(mZID, eOp, nTick);
        PopMatrices(pCtx, this);
    }
}

// ---------------------------------------------------------------------------
// SetZID @0x82ADB578 -- swap the field's host render-data handle: release the old
// one through the host hook (unless empty), then store the new one.
// ---------------------------------------------------------------------------
void AptRenderItemDynamicText::SetZID(intptr_t nZID)
{
    if (mZID && mZID != gAptEmptyTextRenderDataZID && gpfnAptReleaseTextRenderData)
        gpfnAptReleaseTextRenderData(mZID, 2);
    mZID = nZID;
}

// ---------------------------------------------------------------------------
// SetTextFormat @0x82AEC1D0 -- swap the field's TextFormat object: release + free
// the old one (drop its embedded string's refcount, return the 0x20-byte block to
// the non-GC pool), then store the new one.
// ---------------------------------------------------------------------------
void AptRenderItemDynamicText::SetTextFormat(AptValue* pTextFormat)
{
    if (mpTextFormat)
    {
        // The TextFormat object embeds an EAStringC at offset 0; its dtor drops that
        // shared string's internal refcount (console: EAStringC::DecreaseInternalRefCount
        // on *mpTextFormat, inlined), then the 0x20-byte block returns to the non-GC pool.
        reinterpret_cast<EAStringC*>(mpTextFormat)->~EAStringC();
        gpNonGCPoolManager->Deallocate(mpTextFormat, 0x20u);
    }
    mpTextFormat = pTextFormat;
}
