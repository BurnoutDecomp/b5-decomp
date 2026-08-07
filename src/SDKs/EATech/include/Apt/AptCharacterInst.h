#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterInst: the per-instance character data in the render
// spine. It sits between the scene-graph node (AptCIH) and the renderable
// (AptRenderItem): AptCIH -> AptCharacterInst -> AptRenderItem. It owns the
// instance's current render item (double-buffered by AptRenderTreeManager) and an
// optional per-instance property hash.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF; layout pinned against the x64 XB1
// export (sizeof 0x20; console was 16 bytes / 4 dwords):
//   +0x00 vptr           (x64: real per-class vtables, off_140C17D88..DE0)
//   +0x08 mpRenderItem   the current AptRenderItem (ref-counted)
//   +0x10 mTypeFlags     packed; LOW 6 bits (& 0x3F) = the AptCharacter type tag
//   +0x18 mpProperties   optional per-instance AptNativeHash* (0x28 bytes; lazy)
//
// The const accessors read straight through mpRenderItem; the ctor + the
// *Writable paths go through the render-tree manager (Update_CreateItem /
// Update_GetTickItemWritable) which double-buffers the render items per tick.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // offsetof (_AssertLayout)
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptRenderItem.h"   // mpRenderItem + delegated accessors

struct AptCharacter;
struct AptNativeHash;
struct AptMatrix;
struct AptCXForm;
struct AptRenderTreeManager;
struct AptRenderItem;
struct AptCIH;

// ---------------------------------------------------------------------------
// FLAG (homed by the AptRenderTreeManager TU; not declared on the manager facade
// yet): the two render-tree-manager UPDATE entry points the static helpers below
// route through. Decompiled faithfully from the X360 ARTIST.XEX:
//   CloneItem -> AptRenderTreeManager::Update_CloneItem(mgr, pItem, a2, nTick) @0x82AE1C98
//   ItemMoved -> AptRenderTreeManager::Update_ItemMoved (mgr, pNode, nTick)    @0x82AECD50
// Declared as free functions taking the manager (rather than members) so the
// home file can reference them without editing the manager header; the live
// double-buffer bodies are deferred (single-buffer bring-up).
// ---------------------------------------------------------------------------
struct AptCIH* AptCloneManagedItem(AptRenderTreeManager* pMgr, struct AptCIH* pNode,
                                int nSourceArg, int nTick);
struct AptCIH* AptManagedItemMoved(AptRenderTreeManager* pMgr, struct AptCIH* pNode, int nTick);

// ---------------------------------------------------------------------------
// FLAG (homed by the AptRenderTreeManager / AptTargetSim TUs, not yet built):
// the render items are owned + double-buffered by the current target sim's
// render-tree manager. Routed through helpers (rather than the console literal
// gpCurrentTargetSim+0x2C offset) so the x64 layout stays correct. Null until a
// target sim is active.
//   AptCurrentRenderTreeManager() -> gpCurrentTargetSim's manager, or null.
//   AptGetTickItemWritable   -> AptRenderTreeManager::Update_GetTickItemWritable
// (AptRTM_CreateItem removed -- unused; the manager path calls Update_CreateItem directly.)
// ---------------------------------------------------------------------------
AptRenderTreeManager* AptCurrentRenderTreeManager();
AptRenderItem*        AptGetTickItemWritable(AptRenderTreeManager* pMgr, const AptRenderItem* pItem, int nTick);
extern int            gnCurrUpdateTick;

struct AptCharacterInst
{
    void*          mpVTable_unused;   // +0x00 (x64: a REAL per-class vptr -- the ctors/dtors
                                      //  stamp off_140C17D88..DE0; modelled manually)
    AptRenderItem* mpRenderItem;      // +0x08
    uint32_t       mTypeFlags;        // +0x10 (x64: char type tag in the LOW 6 bits)
    AptNativeHash* mpProperties;      // +0x18
    // (the base ends here -- console word[4]+ belong to the SPRITE subclass:
    // AptCharacterSpriteInstBase::mnGotoFrame [4] / mnClipActionFlags [5] /
    // mpClipEventHandlers [6] ... mnLastActionFrame [8]. The per-frame drains'
    // charInst+0x10/+0x20 reads are those sprite fields, gated on type tag 5/16.)

    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class offsetof context).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptCharacterInst, mpVTable_unused) == 0x00, "x64: vptr slot (ctors stamp off_140C17D88 here)");
        static_assert(offsetof(AptCharacterInst, mpRenderItem)    == 0x08, "x64: GetRenderItem 'mov rax,[rcx+8]' @0x140839860");
        static_assert(offsetof(AptCharacterInst, mTypeFlags)      == 0x10, "x64: AptCIH::IsSpriteInst reads [charInst+10h] & 0x3F @0x14083B370");
        static_assert(offsetof(AptCharacterInst, mpProperties)    == 0x18, "x64: DestroyGCPointers frees [this+18h] @0x1408360C0");
        static_assert(sizeof(AptCharacterInst) == 0x20, "x64: factory Allocate(0x20) for leaf insts @0x140835410");
    }

    explicit AptCharacterInst(AptCharacter* pCharacter);   // @0x81431C
    ~AptCharacterInst();                                    // @0x7F8414

    // @0x817D40 -- factory: build the right AptCharacterInst for a character.
    static AptCharacterInst* CreateCharacterInst(AptCharacter* pCharacter);

    // ItemInserted @0x82AECD70 -- render-tree "item (re)inserted" notification.
    // Static helper: it takes the scene NODE (re-reads mpCharacterInst from it),
    // clears the item's deletion mark, and notifies the render-tree manager.
    // FLAG: behavioural body in its own TU; declared so AptCIH::SetIsInserted
    // compiles. (struct AptCIH is forward-declared below.)
    static struct AptCIH* ItemInserted(struct AptCIH* pNode);

    AptRenderItem*       GetRenderItem() const;          // @0x7DF008
    AptRenderItem*       GetRenderItemWritable();        // @0x7EC910 (via the manager)

    // ---- static render-tree-manager notifications (operate on a scene NODE) -
    // Each re-derives a render-tree link from the changed AptCIH and notifies the
    // current target sim's render-tree manager. They take the scene node directly
    // (the X360 passes it in r3, no `this`) and route through AptCurrentRenderTreeManager().
    static AptCIH* CloneItem(AptCIH* pNode, int nArg);            // @0x82AE1C98
    static AptCIH* ItemFirstChildChanged(AptCIH* pNode);          // @0x82AE1C78
    static AptCIH* ItemMoved(AptCIH* pNode);                      // @0x82AECD50
    static AptCIH* SetMaskedItem(AptCIH* pNode, bool bIsMask);    // @0x82AE1C50
    static AptCIH* SetRootItem(AptCIH* pNode);                    // @0x82AE66A0
    // GetRenderItemDynamicTextWritable @0x82AE1BF8 -- ICF-folded thunk: identical body
    // to GetRenderItemWritable (the X360 linker folded the duplicate).
    AptRenderItem*       GetRenderItemDynamicTextWritable() { return GetRenderItemWritable(); }
    AptRenderItem*       SetRenderItem(AptRenderItem* pItem);   // @0x7E20E8

    // The character type tag (x64: mTypeFlags LOW 6 bits -- every IsXxxInst does
    // `and ecx,3Fh` on [charInst+0x10]; the old >>26 was the X360 big-endian
    // reversal): 1 shape / 2 dynamic-text / 4 button / 5 sprite(movie-clip) /
    // 8 morph / 9 animation / 10 static-text / 15 level / 16 custom-control.
    // Drives the AptCIH::IsXxxInst predicates.
    uint32_t GetTypeTag() const { return mTypeFlags & 0x3Fu; }

    AptCharacter*        SetCharacter(AptCharacter* pCharacter); // @0x80F324

    // DestroyGCPointers @0x82AF5490 -- mark the owned render item deleted and
    // tear down the per-instance property hash (the GC drop path).
    void DestroyGCPointers();

    // The writable character / position matrix (through the tick-writable render item).
    AptCharacter* GetCharacterWritable();                // @0x82AE1CC0
    AptMatrix*    GetPositionMatrixWritable();           // @0x82AE6650

    // MoveRenderDataFrom @0x82AF4630 -- transfer pSource's render data (visual
    // state via the render item + the clip depth) and steal its property hash.
    void MoveRenderDataFrom(AptCharacterInst* pSource);

    // SetHasMask @0x82AE1D50 / SetIsMask @0x82AECDC0 -- delegate to the writable
    // render item's mask flags (a3 is the mask item / mask matrix respectively).
    void SetHasMask(bool bHasMask, AptRenderItem* pMask);
    void SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix);

    // ---- const reads (straight through the render item) -------------------
    const AptCharacter* GetCharacterConst() const;       // @0x7E8784
    int16_t  GetDepth() const;                           // @0x7E8868
    int16_t  GetClipDepth() const;                       // @0x7E8774
    bool     GetIsVisible() const;                       // @0x7E83C4
    bool     GetIsMask() const;                          // @0x7E8790
    bool     GetHasMask() const;                         // @0x7EABA0
    const AptMatrix* GetPositionMatrixConst() const;     // @0x7E7360
    const AptCXForm* GetColorMatrixConst() const;        // @0x7E7368

    // ---- writes (through the writable render item) ------------------------
    void SetDepth(int nDepth);                           // @0x7F071C
    void SetClipDepth(int nClipDepth);                   // @0x7F074C
    void SetIsVisible(bool bVisible);                    // @0x7ED7F4
};
