// ===========================================================================
// EATech Apt -- AptCharacterHelper bodies.   DECOMPILED from the X360 ARTIST.XEX
// (no Feb-2007 / DecFIGS source exists for this helper):
//     AptCharacterHelper::CreateTextCharacterInst @ 0x82B010C0
//     AptCharacterHelper::Shutdown                @ 0x82AE2FA0
//
// The lazy factory for the shared DEFAULT dynamic-text character template
// (spDefaultTextCharacter == off_8324E498) that backs AS-created text fields
// (MovieClip.createTextField). Shutdown frees it together with the sibling
// default movie-clip template (spDefaultMovieCharacter == off_8324E49C, built by
// AptCharacterHelper::CreateMovieCharacterInst).
//
// Both templates are 68-byte (0x44) AptCharacter subtype blocks allocated from the
// non-GC Apt pool (gpNonGCPoolManager == off_8324D808). The X360 stamps the text
// template's authored layout defaults field-by-field; reconstructed here BY NAME
// on AptCharacterDynamicText (no offset pokes). The one piece with no
// reconstructable home -- the serialised .apt font glyph-list walk that picks the
// default font + glyph index -- is routed through the declared (deferred)
// AptResolveDefaultTextFont helper rather than offset-poked.
//
// The _savegprlr_29 / _restgprlr_29 in Shutdown's asm are the compiler's
// register-save/restore prologue thunks (not real calls) -- folded away here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"  // the text template layout (by name)
#include "SDKs/EATech/include/Apt/AptCharacter.h"             // mnType / mnRefAndFlags / mpFixupLink
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCharacterInst::mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"            // AptRenderItem::mpCharacter
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH::mpCharacterInst / operator new / ctor
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpNonGCPoolManager (off_8324D808)
#include "SDKs/EATech/include/Apt/AptTarget.h"                // gpAptTarget, mpAnimationTarget
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"       // GetRootDisplayList
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // AptDisplayListState::insert(depth, item)
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"        // AptValue::setGCRoot
#include "SDKs/EATech/Apt/DogmaAllocator.h"                   // DOGMA_PoolManager::Allocate/Deallocate

#include <new>   // placement new for the level-0 root CIH

#include <cstring>   // memset

// The authored default-text constants the X360 stamps (the .rdata float literals
// flt_82006D70 / flt_82001D9C / flt_82013FB0 the builder loads).
static const float  KF_DEFAULT_TEXT_MARGIN_LOW  = -2.0f;  // flt_82006D70
static const float  KF_DEFAULT_TEXT_MARGIN_HIGH =  2.0f;  // flt_82001D9C
static const float  KF_DEFAULT_TEXT_FONT_SIZE   = 12.0f;  // flt_82013FB0
// The text-character per-type flag the builder OR-s into mnRefAndFlags' low half.
static const uint32_t KU_TEXT_CHARACTER_FLAG    = 0x8000u;
// The .apt dynamic-text character type tag.
static const int32_t  KI_CHARACTER_TYPE_TEXT    = 2;

// ---- the cached default-character template singletons (X360 .data) ------------
AptCharacterDynamicText* AptCharacterHelper::spDefaultTextCharacter  = nullptr;  // off_8324E498
AptCharacter*            AptCharacterHelper::spDefaultMovieCharacter = nullptr;  // off_8324E49C

// ---------------------------------------------------------------------------
// CreateTextCharacterInst @0x82B010C0
// ---------------------------------------------------------------------------
AptCIH* AptCharacterHelper::CreateTextCharacterInst()
{
    // Allocate the shared default dynamic-text character template from the non-GC
    // pool and zero it (the X360 Allocate(68) + memset(.,0,68)).
    spDefaultTextCharacter =
        static_cast<AptCharacterDynamicText*>(gpNonGCPoolManager->Allocate(sizeof(AptCharacterDynamicText)));
    std::memset(spDefaultTextCharacter, 0, sizeof(AptCharacterDynamicText));

    AptCharacterDynamicText* pTemplate = spDefaultTextCharacter;

    // Authored layout defaults stamped into the template (in the X360 store order).
    pTemplate->mnType = KI_CHARACTER_TYPE_TEXT;                  // [c:0x00]
    pTemplate->mfMarginLeft   = KF_DEFAULT_TEXT_MARGIN_LOW;      // [c:0x10] -2
    pTemplate->mfMarginTop    = KF_DEFAULT_TEXT_MARGIN_LOW;      // [c:0x14] -2
    pTemplate->mfMarginRight  = KF_DEFAULT_TEXT_MARGIN_HIGH;     // [c:0x18]  2
    pTemplate->mfMarginBottom = KF_DEFAULT_TEXT_MARGIN_HIGH;     // [c:0x1C]  2
    pTemplate->mnAuthoredReserved0 = 0;                          // [c:0x24]
    pTemplate->mnAuthoredReserved1 = 0;                          // [c:0x28]
    pTemplate->mfFontSize = KF_DEFAULT_TEXT_FONT_SIZE;           // [c:0x2C] 12.0
    pTemplate->mnAuthoredReserved2 = 0;                          // [c:0x30]
    pTemplate->mnAuthoredReserved3 = 0;                          // [c:0x34]
    pTemplate->mnAuthoredReserved4 = 0;                          // [c:0x38]
    pTemplate->mpDefaultText = nullptr;                          // [c:0x3C]
    pTemplate->mnAuthoredReserved5 = 0;                          // [c:0x40]
    // No default glyph resolved yet (the X360 seeds the index to -1 before the walk).
    pTemplate->mnDefaultGlyphIndex = -1;                         // [c:0x20]

    // Resolve the default font from the level-0 movie. The X360 reaches the
    // font-list-bearing character through the level-0 animation node's spine, all
    // named members: node -> char-inst -> render-item -> character -> fixup-link.
    AptCIH* pLevel0 = AptGetAnimationAtLevel(0);
    AptCharacter* pFontOwner =
        pLevel0->mpCharacterInst->mpRenderItem->mpCharacter->mpFixupLink;

    // Take the default font character + its default glyph index from that owner's
    // serialised glyph list (deferred helper -- the glyph-list record has no C++
    // home). The font character is stored in the template's mpFixupLink slot (the
    // AptCharacter back-link), the glyph index in mnDefaultGlyphIndex.
    pTemplate->mpFixupLink =
        AptResolveDefaultTextFont(pFontOwner, &pTemplate->mnDefaultGlyphIndex);

    // Mark the template as a text character (per-type flag in the low half of the
    // packed ref/flags word; the X360 OR-s 0x8000 into that halfword).
    pTemplate->mnRefAndFlags |= KU_TEXT_CHARACTER_FLAG;

    // The X360 returns the level-0 animation node it walked.
    return pLevel0;
}

// ---------------------------------------------------------------------------
// CreateMovieCharacterInst @0x82B0BC.. (PS3 @0xF56FC4)
// ---------------------------------------------------------------------------
// The sibling of CreateTextCharacterInst: lazily build the shared default empty
// MOVIE-CLIP character template (spDefaultMovieCharacter, type tag 5) that backs AS
// createEmptyMovieClip. Much smaller than the text builder -- the movie template has
// no authored margins/font: just the type tag, the resolved default character, and
// the per-type flag.
//
// The PS3 body:
//   pDynamicMovie = Allocate(gpNonGCPoolManager, 0x44); memset(.,0,68);
//   *pDynamicMovie = 5;                                  // mnType (movie-clip)
//   result = _AptGetAnimationAtLevel(0);
//   *(pDynamicMovie+4) = **(*(*(*(*(result+32)+4)+4)+4) + 0x20);   // mpFixupLink
//   *(pDynamicMovie+8) |= 0x8000;                        // the per-type flag
//   return result;
//
// The walk `*(*(*(*(result+32)+4)+4)+4)` is the SAME spine the text builder walks --
// the level-0 node's char-inst -> render-item -> character (all named members) -- and
// the `**(.+0x20)` double-deref reads that character's default-character slot into the
// template's mpFixupLink (the movie template's analogue of the text builder's default-
// font resolve). The return is the level-0 node the walk started from.
// ---------------------------------------------------------------------------
AptCIH* AptCharacterHelper::CreateMovieCharacterInst()
{
    // Allocate the shared default movie-clip character template from the non-GC pool
    // and zero it (the X360/PS3 Allocate(68) + memset(.,0,68)).
    spDefaultMovieCharacter =
        static_cast<AptCharacter*>(gpNonGCPoolManager->Allocate(sizeof(AptCharacterDynamicText)));
    std::memset(spDefaultMovieCharacter, 0, sizeof(AptCharacterDynamicText));

    AptCharacter* pTemplate = spDefaultMovieCharacter;

    // Movie-clip character type tag.
    pTemplate->mnType = 5;                                       // [c:0x00] *pDynamicMovie = 5

    // Resolve the level-0 movie's default character (the same named spine the text
    // builder walks: node -> char-inst -> render-item -> character -> fixup-link).
    // PS3 @0xF5701C does FOUR loads (...+0x20,+4,+4,+4) -- the trailing ->mpFixupLink
    // was dropped here; +0x20 must be read off mpCharacter->mpFixupLink, not mpCharacter.
    AptCIH* pLevel0 = AptGetAnimationAtLevel(0);
    AptCharacter* pSpineChar =
        pLevel0->mpCharacterInst->mpRenderItem->mpCharacter->mpFixupLink;

    // mpFixupLink = **(pSpineChar + 0x20): the default character stored at the spine
    // character's +0x20 slot (a pointer to a pointer; the double-deref reads through
    // it). FLAG (x64 fork): the console reads the 32-bit slot; on x64 the slot holds a
    // host pointer, read here as AptCharacter*const* through the named base.
    AptCharacter** pDefaultSlot =
        *reinterpret_cast<AptCharacter***>(reinterpret_cast<char*>(pSpineChar) + 0x20);
    pTemplate->mpFixupLink = *pDefaultSlot;                      // [c:0x04] **(.+0x20)

    // The per-type flag the builder OR-s into the low half of the packed ref/flags word.
    pTemplate->mnRefAndFlags |= 0x8000u;                         // [c:0x08] |= 0x8000

    // The X360/PS3 return the level-0 animation node the walk started from.
    return pLevel0;
}

// ---------------------------------------------------------------------------
// Shutdown @0x82AE2FA0 -- free both cached default templates + null the statics.
// ---------------------------------------------------------------------------
void AptCharacterHelper::Shutdown()
{
    if (spDefaultTextCharacter)
    {
        gpNonGCPoolManager->Deallocate(spDefaultTextCharacter, sizeof(AptCharacterDynamicText));
    }
    if (spDefaultMovieCharacter)
    {
        // The movie template is the same 68-byte block size as the text template.
        gpNonGCPoolManager->Deallocate(spDefaultMovieCharacter, sizeof(AptCharacterDynamicText));
    }

    spDefaultTextCharacter  = nullptr;
    spDefaultMovieCharacter = nullptr;
}

// ---------------------------------------------------------------------------
// AptGetAnimationAtLevel -- the root AptCIH-at-display-level resolver (X360
// _AptGetAnimationAtLevel @0x82B00788 / PS3 _Z23_AptGetAnimationAtLeveli @0xF4E540).
//
// Walk the root display list of the active target's animation director for the node
// whose placed depth equals nLevel; if none exists, lazily create an empty AptCIH,
// stamp its render-item depth to nLevel, pin it as a GC root, and insert it into the
// root display list at that depth. Returns the (found or created) node, or null when
// no movie is mounted.
//
// CANONICAL signature reconciled to AptCIH* (the AptCharacterHelper.h / AptLinker.h
// form); the AptAnimationTarget/AptCharacterAnimation/AptMovie/AptScriptFunctionBase
// TUs that declared a `void*`-returning extern are corrected to AptCIH* (a void* sink
// still accepts the AptCIH*, and the reinterpret_cast<AptValue*> sites are unaffected).
//
// X360 walks children by *(result+24) and PS3 by result[6] -- both the named
// mpDisplayListNext sibling link; the depth match is
// cih->mpCharacterInst->mpRenderItem->GetDepth() == nLevel. The PS3 inserts directly
// via AptDisplayListState::insert(root, depth, node) (the X360's sub_82AEE788 is the
// same findInst+insert+stamp-depth, folded here to the one insert).
// ---------------------------------------------------------------------------
// DEFENSIVE PROBE SINK (implemented by the host, BrnAptRuntimeBringUp.cpp): logs a step tag + a
// pointer so a single run names the exact line AptGetAnimationAtLevel reaches before any AV. A weak
// no-op default is provided so other callers / TUs that do not define it still link.
#if defined(_MSC_VER)
extern "C" void CgsApt_GalProbe(const char* pcStep, const void* p);
#pragma comment(linker, "/alternatename:CgsApt_GalProbe=CgsApt_GalProbeDefault")
extern "C" void CgsApt_GalProbeDefault(const char*, const void*) {}
#else
extern "C" void CgsApt_GalProbe(const char* pcStep, const void* p);
#endif

AptCIH* AptGetAnimationAtLevel(int nLevel)
{
    // No active target / director / root display list -> nothing mounted.
    if (gpAptTarget == nullptr || gpAptTarget->mpAnimationTarget == nullptr)
    {
        CgsApt_GalProbe("no target/director", gpAptTarget);
        return nullptr;
    }
    CgsApt_GalProbe("director", gpAptTarget->mpAnimationTarget);

    AptDisplayList* pRoot = gpAptTarget->mpAnimationTarget->GetRootDisplayList();   // *(target+0x18)+0x20
    CgsApt_GalProbe("rootDisplayList", pRoot);
    AptDisplayListState* pState = (pRoot != nullptr) ? pRoot->AsState() : nullptr;
    // The root display list's head node (mpHead, which AsState() returns) is nullable -- the ctor may
    // fail to allocate it. Bail rather than deref pState->mpFirst.
    if (pState == nullptr)
    {
        CgsApt_GalProbe("pState NULL -> bail", nullptr);
        return nullptr;
    }
    CgsApt_GalProbe("pState", pState);

    // Search the placed children for the node at depth nLevel. GUARDED: skip any node whose
    // charInst / renderItem is null (a partially-built node would AV on the GetDepth() chain).
    CgsApt_GalProbe("search mpFirst", pState->mpFirst);
    for (AptCIH* pNode = pState->mpFirst; pNode != nullptr; pNode = pNode->mpDisplayListNext)
    {
        if (pNode->mpCharacterInst == nullptr || pNode->mpCharacterInst->mpRenderItem == nullptr)
            continue;
        if (pNode->mpCharacterInst->mpRenderItem->GetDepth() == nLevel)
            return pNode;
    }

    // Not present: lazily create the level node. AptCIH::operator new -> the GC pool; the AptCIH ctor
    // runs AptCharacterInst::CreateCharacterInst -> the render-tree manager (AptRTM_CreateItem). If the
    // render-tree manager is not initialised (AptRenderInitialize is un-homed in our bring-up) this is
    // where it can AV -- the probes bracket each sub-step.
    // FLAG (x64 size): the console allocates 40 bytes (10 console dwords) for an AptCIH; on x64
    // the node is sizeof(AptCIH) (8-byte pointers widen it well past 40). The hardcoded 40 here
    // under-allocated the ROOT level node, so its later members (mpDisplayListParent [7] /
    // mpCharacterInst [8]) fell OUTSIDE the allocation and overlapped adjacent pool objects --
    // the child-placement AddRef/insert on the root then corrupted the root's parent link (an AV
    // in SetGeneralizedProcessDirtyState during the first frame-0 place). Use sizeof(AptCIH), the
    // same size AptDLState_CreateInstAtDepth already uses for the placed child nodes.
    CgsApt_GalProbe("operator new(sizeof AptCIH)", nullptr);
    void* pMem = AptCIH::operator new(sizeof(AptCIH));  // AptValueGC pool (x64 size)
    CgsApt_GalProbe("operator new ->", pMem);
    if (pMem == nullptr)
        return nullptr;                                  // GC carve failed -> bail (no AV)

    CgsApt_GalProbe("AptCIH ctor (CreateCharacterInst)", nullptr);
    AptCIH* pNew = ::new (pMem) AptCIH(nullptr, nullptr);
    CgsApt_GalProbe("AptCIH ctor done; charInst", pNew ? pNew->mpCharacterInst : nullptr);

    // GUARDED post-create derefs: a partial render-tree manager can leave mpCharacterInst /
    // mpRenderItem null. Bail (the node exists but is not fully wired) rather than AV.
    if (pNew == nullptr || pNew->mpCharacterInst == nullptr)
    {
        CgsApt_GalProbe("charInst NULL -> bail", pNew);
        return nullptr;
    }

    CgsApt_GalProbe("GetRenderItemWritable/SetDepth", nullptr);
    AptRenderItem* pRI = pNew->mpCharacterInst->GetRenderItemWritable();
    // FLAG (x64 bring-up): the render-tree manager is a null stub (AptCurrentRenderTreeManager==0),
    // so no render item is created and pRI is null. The console always has one (SetDepth stamps the
    // render item's depth). We still create + GC-root + INSERT the root node so the display tree
    // exists + the instantiation chain proceeds; only the render-item depth stamp is skipped (the
    // node's depth defaults). When the render-tree manager lands, SetDepth runs faithfully.
    if (pRI != nullptr)
        pRI->SetDepth(nLevel);
    else
        CgsApt_GalProbe("renderItem NULL -> SetDepth skipped (no RTM)", nullptr);
    CgsApt_GalProbe("setGCRoot", nullptr);
    pNew->setGCRoot(1);
    CgsApt_GalProbe("insert", nullptr);
    pState->insert(nLevel, pNew);
    CgsApt_GalProbe("RETURN root CIH", pNew);
    return pNew;
}

// ---------------------------------------------------------------------------
// AptResolveDefaultTextFont -- the default-font resolve folded into the X360's
// CreateTextCharacterInst @0x82B010C0 (cross-checked vs the PS3 DecFIGS DWARF
// AptCharacterHelper::CreateTextCharacterInst @0xF570B4, which decompiles the same
// walk with cleaner control flow):
//
//   v2 = pFontOwner + 16;                  // the embedded movie root
//   *pTemplate->mpFixupLink = **(v2 + 0x10);   // first char-table entry's first member
//   count = *(v2 + 12);                    // character-table count
//   if (count > 0) {
//     table = *(v2 + 16);                  // character-table base (AptCharacter*[])
//     for (idx = 0; idx < count; ++idx)
//       if (**table[idx] == 3) {           // char mnType == 3 (font)
//         *pnDefaultGlyphIndex = idx; break;
//       }
//   }
//   return the resolved font character (mpFixupLink result).
//
// FLAG (deferred serialised-.apt layer -- raw-offset access on an un-homed type):
// pFontOwner+16 is the serialised movie-root record embedded in the AptCharacter; its
// layout (count@+12 / table@+16 / fixup-source@+0x10) is the .apt serialised form, NOT
// the transcoded AptCharacterAnimation (whose named members live at +0/+4) -- so there
// is no C++ home to name it by (no header in b5-decomp/src, no Feb-2007 source; the PS3
// DWARF names the surrounding function but still walks these raw offsets). Reconstructed
// faithfully BYTE-FOR-BYTE from the X360 + PS3 disasm with the offsets documented in
// [c:0xNN] form; promote to named members when the serialised movie-root type is homed.
// The font character is returned (stored into the template's mpFixupLink slot by the
// caller), and the default glyph index is written through pnDefaultGlyphIndex (the X360
// seeds it to -1 first; this routine only sets it when a type-3 font character is found).
// ---------------------------------------------------------------------------
AptCharacter* AptResolveDefaultTextFont(AptCharacter* pFontOwner, int32_t* pnDefaultGlyphIndex)
{
    if (pFontOwner == nullptr)
        return nullptr;

    // v2 = the serialised movie root embedded at pFontOwner+0x10 [c:0x10].
    char* const pMovieRoot = reinterpret_cast<char*>(pFontOwner) + 16;

    // mpFixupLink result = **(v2 + 0x10): the character-table base held at
    // [c:movieRoot+0x10], dereferenced twice to its first AptCharacter* entry.
    AptCharacter* const* const ppDefaultTable =
        *reinterpret_cast<AptCharacter* const* const*>(pMovieRoot + 0x10);   // *(v2 + 0x10)
    AptCharacter* const pFontCharacter = *ppDefaultTable;                    // **(v2 + 0x10)

    // Walk the character table for the first font character (mnType == 3) and record
    // its index as the default glyph index.
    const int32_t nCount = *reinterpret_cast<const int32_t*>(pMovieRoot + 12);   // *(v2 + 12)
    if (nCount > 0)
    {
        AptCharacter* const* const pTable =
            *reinterpret_cast<AptCharacter* const* const*>(pMovieRoot + 16);     // *(v2 + 16)
        for (int32_t i = 0; i < nCount; ++i)
        {
            if (pTable[i]->mnType == 3)   // **(4*idx + table) == 3 (font character type)
            {
                *pnDefaultGlyphIndex = i;
                break;
            }
        }
    }

    return pFontCharacter;
}
