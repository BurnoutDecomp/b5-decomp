#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterInst: the per-instance character data in the render
// spine. It sits between the scene-graph node (AptCIH) and the renderable
// (AptRenderItem): AptCIH -> AptCharacterInst -> AptRenderItem. It owns the
// instance's current render item (double-buffered by AptRenderTreeManager) and an
// optional per-instance property hash.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF. LAYOUT (console 16 bytes / 4 dwords):
//   [0] vtable
//   [1] mpRenderItem   the current AptRenderItem (ref-counted)
//   [2] mTypeFlags     packed; high 6 bits (>>26) = the AptCharacter type tag
//   [3] mpProperties   optional per-instance AptNativeHash* (20 bytes; lazy)
//
// The const accessors read straight through mpRenderItem; the ctor + the
// *Writable paths go through the render-tree manager (Update_CreateItem /
// Update_GetTickItemWritable) which double-buffers the render items per tick.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptRenderItem.h"   // mpRenderItem + delegated accessors

struct AptCharacter;
struct AptNativeHash;
struct AptMatrix;
struct AptCXForm;
struct AptRenderTreeManager;

// ---------------------------------------------------------------------------
// FLAG (homed by the AptRenderTreeManager / AptTargetSim TUs, not yet built):
// the render items are owned + double-buffered by the current target sim's
// render-tree manager. Routed through helpers (rather than the console literal
// gpCurrentTargetSim+0x2C offset) so the x64 layout stays correct. Null until a
// target sim is active.
//   AptCurrentRenderTreeManager() -> gpCurrentTargetSim's manager, or null.
//   AptRTM_CreateItem            -> AptRenderTreeManager::Update_CreateItem @0x814254
//   AptRTM_GetTickItemWritable   -> AptRenderTreeManager::Update_GetTickItemWritable
// ---------------------------------------------------------------------------
AptRenderTreeManager* AptCurrentRenderTreeManager();
AptRenderItem*        AptRTM_CreateItem(AptRenderTreeManager* pMgr, AptCharacter* pCharacter, int nTick);
AptRenderItem*        AptRTM_GetTickItemWritable(AptRenderTreeManager* pMgr, const AptRenderItem* pItem, int nTick);
extern int            gnCurrUpdateTick;

struct AptCharacterInst
{
    void*          mpVTable_unused;   // [0]
    AptRenderItem* mpRenderItem;      // [1]
    uint32_t       mTypeFlags;        // [2] (char type tag in bits 26-31)
    AptNativeHash* mpProperties;      // [3]

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
    AptRenderItem*       SetRenderItem(AptRenderItem* pItem);   // @0x7E20E8

    // The character type tag (mTypeFlags bits 26..31): 1 shape / 2 dynamic-text /
    // 4 button / 5 sprite(movie-clip) / 8 morph / 9 animation / 15 level /
    // 16 custom-control. Drives the AptCIH::IsXxxInst predicates.
    uint32_t GetTypeTag() const { return mTypeFlags >> 26; }

    AptCharacter*        SetCharacter(AptCharacter* pCharacter); // @0x80F324

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
