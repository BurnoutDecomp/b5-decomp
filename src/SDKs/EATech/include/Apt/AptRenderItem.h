#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItem: the base of the render tree (the renderable node).
//
// Every on-screen Apt character instance owns an AptRenderItem subtype
// (Shape/Sprite/Button/StaticText/DynamicText/Morph/Animation/Level/...). The
// base holds the visual state -- the position + colour transforms, depth/clip
// depth, mask, visibility -- plus the AptRenderTreeManager double-buffering links
// (next-revision / next-sibling / first-child) and a reference count. The
// concrete draw is the virtual Render()/Push/PopRenderData implemented by each
// subtype.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (cross-checked vs X360 ARTIST). LAYOUT
// pinned against the x64 XB1 export (sizeof 0x58; ctor sub_1408267A0):
//   +0x00 vptr                  (the compiler vptr -- this class IS polymorphic)
//   +0x08 mpCharacter           the AptCharacter this renders (ref-counted)
//   +0x10 mpPositionMatrix      AptMatrix* (lazily allocated; null -> identity)
//   +0x18 mpColorMatrix         AptCXForm* (lazily allocated; null -> identity)
//   +0x20 mpMaskPositionMatrix  AptMatrix*
//   +0x28 mDepth:i16 +0x2A mClipDepth:i16   (both -1 at ctor)
//   +0x2C mFlags                x64 bits: 0 isVisible / 1 isMask / 2 hasMask /
//                               3 deletionMark / 4 clone-copied / 5 tree-vis state /
//                               6 writable-revision / 7 tree-vis companion /
//                               8-13 render-type (shape=1 sprite=5 morph=8 anim=9
//                               statictext=0xA level=0xF custom=0x10)
//   +0x30 mpMask                AptRenderItem* (the mask item; ref-counted)
//   +0x38 mCreatedOnTick        int
//   +0x3C mRefCount             int (Add/ReleaseReference mutate it atomically)
//   +0x40 mpManagerNextRevision AptRenderItem*  (the render-tree-manager links --
//   +0x48 mpManagerNextSibling  AptRenderItem*   the double-buffered revision tree;
//   +0x50 mpManagerFirstChild   AptRenderItem*   populated by AptRenderTreeManager)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // offsetof (_AssertLayout)
#include <cstdint>

struct AptCharacter;
struct AptMatrix;
struct AptCXForm;
class AptRenderingContext;
enum AptMaskRenderOperation : int;   // values in CgsAptCallbackRender.h (Subtract=-1 / Normal=0 / Add=1)

// The shared identity transforms returned when an item has no own matrix (DEFINED
// in AptGlobals.cpp) and the global render-item teardown latch (DEFINED in
// AptRenderItem.cpp, X360 byte_8324E56C).
extern AptMatrix gIdentityMatrix;
extern AptCXForm gIdentityCXForm;
extern bool      gbRenderItemShuttingDown;

struct AptRenderItem
{
    // NB: NO explicit vtable member -- this class declares real virtuals, so the
    // compiler vptr occupies +0x00. (The old `mpVTable_unused` member alongside the
    // virtuals produced a DOUBLE vptr, shifting every member +8 vs the binary.)
    AptCharacter*  mpCharacter;       // +0x08
    AptMatrix*     mpPositionMatrix;  // +0x10
    AptCXForm*     mpColorMatrix;     // +0x18
    AptMatrix*     mpMaskPositionMatrix; // +0x20
    int16_t        mDepth;            // +0x28
    int16_t        mClipDepth;        // +0x2A
    uint32_t       mFlags;            // +0x2C (x64 bit layout -- see header comment)
    AptRenderItem* mpMask;            // +0x30
    int32_t        mCreatedOnTick;    // +0x38
    volatile int32_t mRefCount;       // +0x3C
    AptRenderItem* mpManagerNextRevision; // +0x40
    AptRenderItem* mpManagerNextSibling;  // +0x48
    AptRenderItem* mpManagerFirstChild;   // +0x50

    // Layout pinned against the x64 XB1 accessors (never called; member body gives
    // complete-class offsetof context).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptRenderItem, mpCharacter)           == 0x08, "x64 ?GetCharacterConst: mov rax,[rcx+8]");
        static_assert(offsetof(AptRenderItem, mpPositionMatrix)      == 0x10, "x64 ctor 0x1408267A0 zeroes [rcx+10h]");
        static_assert(offsetof(AptRenderItem, mpColorMatrix)         == 0x18, "x64 ctor zeroes [rcx+18h]");
        static_assert(offsetof(AptRenderItem, mpMaskPositionMatrix)  == 0x20, "x64 ?GetMaskPositionMatrixConst: [rcx+20h]");
        static_assert(offsetof(AptRenderItem, mDepth)                == 0x28, "x64 ?GetDepth: movsx eax, word [rcx+28h]");
        static_assert(offsetof(AptRenderItem, mClipDepth)            == 0x2A, "x64 ?GetClipDepth: [rcx+2Ah]");
        static_assert(offsetof(AptRenderItem, mFlags)                == 0x2C, "x64 ?GetIsVisible: dword [rcx+2Ch]");
        static_assert(offsetof(AptRenderItem, mpMask)                == 0x30, "x64 ?GetMask: [rcx+30h]");
        static_assert(offsetof(AptRenderItem, mCreatedOnTick)        == 0x38, "x64 ?GetCreatedOnTick: [rcx+38h]");
        static_assert(offsetof(AptRenderItem, mRefCount)             == 0x3C, "x64 ?GetRefCount: [rcx+3Ch]");
        static_assert(offsetof(AptRenderItem, mpManagerNextRevision) == 0x40, "x64 ?Manager_GetNextRevision: [rcx+40h]");
        static_assert(offsetof(AptRenderItem, mpManagerNextSibling)  == 0x48, "x64 ?Manager_GetNextSibling: [rcx+48h]");
        static_assert(offsetof(AptRenderItem, mpManagerFirstChild)   == 0x50, "x64 ?Manager_GetFirstChild: [rcx+50h]");
        static_assert(sizeof(AptRenderItem) == 0x58, "x64 factory 0x14083C0D0 pool-allocs 88 for base-only subtypes");
    }

    static int sItemsAllocated;

    // Pool-backed sized delete (render items live in gpNonGCPoolManager); the
    // virtual deleting destructor supplies the dynamic subtype's size.
    static void operator delete(void* p, size_t sz);

    AptRenderItem(AptCharacter* pCharacter, int nCreatedOnTick);   // @0x7E47A0

    // Clone copy-ctor (the shared base copy helper @0x82AEB9A0): deep-copy pSource
    // into this fresh pool block for a new render-tree-manager revision. Always
    // copies the base visual state + transforms; with bCopyExtended it also brings
    // over the mask, depth and manager links (each chased to its latest revision and
    // reference-counted). Each subtype Clone allocates the sized block, runs this,
    // then stamps its own render-type flag + vtable.
    AptRenderItem(const AptRenderItem* pSource, int nCreatedOnTick, bool bCopyExtended);

    // Clone (vtable slot 0) -- pool-allocate a fresh copy of this item for a new
    // revision (null when the pool is exhausted). PURE: the base is abstract; every
    // concrete subtype supplies its sized allocation + render-type flag + vtable.
    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) = 0;

    virtual ~AptRenderItem();                                       // @0x80F860

    // The base render hooks are empty (@0x7E49A8/0x7E499C/0x7E49A4/0x7E49A0); each
    // character subtype (Shape/Sprite/Button/Text/...) overrides Render to draw.
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;
    virtual void PushRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;
    virtual void PopRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;
    virtual void PushRenderDataAbsolute(AptRenderingContext* pCtx) const;

    // ---- visual state -----------------------------------------------------
    const AptMatrix* GetPositionMatrixConst() const;      // @0x7DFB60
    const AptCXForm* GetColorMatrixConst() const;         // @0x7DFB80
    const AptMatrix* GetMaskPositionMatrixConst() const;  // @0x7DEE0C
    AptMatrix* GetPositionMatrixWritable();               // @0x7F1578
    AptCXForm* GetColorMatrixWritable();                  // @0x7FDF68

    // SetMaskMatrix @0x82AE52F8 -- lazily (de)allocate + copy the mask position
    // matrix (null clears it). Used by the clone copy-ctor's extended path.
    void SetMaskMatrix(const AptMatrix* pMatrix);

    // CopyRenderDataFrom @0x82AE5400 -- transfer a source item's visual state onto
    // this one: the position + colour transforms (lazily (re)allocated, or reset to
    // identity when the source has none), the clip depth, and the isVisible flag.
    // (Used by AptCIH::ReplaceZombieChild to carry a swapped node's look over.)
    void CopyRenderDataFrom(const AptRenderItem* pSource);

    int16_t GetDepth() const;       AptRenderItem* SetDepth(int nDepth);          // @0x7DEE14/0x7DEEA0
    int16_t GetClipDepth() const;   AptRenderItem* SetClipDepth(int nClipDepth);  // @0x7DEDF8/0x7DEEA8

    // SetIsVisible @0x82AE0708 -- set the is-visible flag (x64 bit 0) + recompute the
    // subtree's mask-driven visibility (PropagateTreeIsVisible) when the bit changes.
    AptRenderItem* SetIsVisible(bool bVisible);

    bool GetIsVisible() const;      // @0x7DEE68
    bool GetIsMask() const;         // @0x7DEE20
    bool GetHasMask() const;        // @0x7DEE34
    AptRenderItem* GetMask() const; // @0x7DEE2C

    // SetHasMask @0x82ADB388 -- set the has-mask flag (x64 bit 2) + (un)bind the mask
    // render item (ref-counted; propagates visibility). Returns the previous mask
    // (X360 r3 = this on the no-op path / the released old mask otherwise).
    AptRenderItem* SetHasMask(bool bHasMask, AptRenderItem* pMask);
    // SetIsMask @0x82AEBF28 -- set the is-mask flag (x64 bit 1) + the mask matrix.
    // Returns this (X360 r3).
    AptRenderItem* SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix);

    const AptCharacter* GetCharacterConst() const;        // @0x7DEE04
    AptCharacter*       GetCharacterWritable() const;     // @0x7DEE90
    AptCharacter*       SetCharacter(AptCharacter* pCharacter);  // @0x80F2DC

    int GetCreatedOnTick() const;   // @0x7DEE84
    int GetRefCount() const;        // @0x7DEE78

    // ---- reference count (atomic) ----------------------------------------
    void AddReference() const;      // @0x7DFA7C
    void ReleaseReference() const;  // @0x7DFA98 (deletes at zero)

    // ---- render-tree-manager links (getters) -----------------------------
    AptRenderItem* Manager_GetFirstChild() const;   // @0x7DEED0
    AptRenderItem* Manager_GetNextSibling() const;  // @0x7DEEC8
    AptRenderItem* Manager_GetNextRevision() const; // @0x7DEED8

    // ---- render-tree-manager double-buffering (the facade uses these) -----
    // Factory: allocate the right render-item subtype for a character. @0x814094
    // (defined in AptRenderTreeManager.cpp, where the subtype headers are visible).
    static AptRenderItem* Manager_CreateItem(AptCharacter* pCharacter, int nTick);

    // The per-item revision/link mutators the render-tree manager drives -- the
    // writable-revision chase + the first-child / next-sibling / mask link writes
    // for the double-buffered tree. ALL BODIES HOMED in AptRenderItem.cpp
    // (@0x82ADAAE8 / @0x82ADABA0 / @0x82ADAC58 / @0x82ADACA8 / @0x82ADB1B8 /
    // @0x82ADAF58 / @0x82ADAD90, plus the symmetry-reconstructed
    // Manager_UpdateFirstChild).
    //   Manager_GetRenderRevision  @ (called from Render_GetChildInvisible/...) --
    //     resolve THIS item's render revision for nTick (chases the revision chain).
    AptRenderItem* Manager_GetRenderRevision(int nTick);
    //   Manager_UpdateFirstChild/NextSibling/Mask -- commit pRevision into the
    //     corresponding manager link of THIS item for the current render walk.
    void Manager_UpdateFirstChild(AptRenderItem* pRevision);
    void Manager_UpdateNextSibling(AptRenderItem* pRevision);
    void Manager_UpdateMask(AptRenderItem* pRevision);
    //   Manager_CloneNewItem -- pool-allocate a fresh revision of THIS item for nTick.
    AptRenderItem* Manager_CloneNewItem(int nTick);
    //   Manager_CreateNewRevision @0x82ADABA0 -- like CloneNewItem but copies the
    //     extended (mask/depth/manager-link) state into the new revision.
    AptRenderItem* Manager_CreateNewRevision(int nTick);
    //   Manager_SetFirstChild/NextSibling -- set THIS item's manager first-child /
    //     next-sibling link (writable-revision side).
    AptRenderItem* Manager_SetFirstChild(AptRenderItem* pChild);
    AptRenderItem* Manager_SetNextSibling(AptRenderItem* pSibling);

    // PropagateTreeIsVisible @0x82ADA8B8 -- recompute the mask-driven "tree visible"
    // state (x64 mFlags bits 5/7) down this item's first-child + next-sibling subtree.
    // nVisibleMode: 1 = becoming hidden by a mask, 0 = becoming shown. Recursive.
    AptRenderItem* PropagateTreeIsVisible(int nVisibleMode);

    // True when this item is already the writable revision for nTick (its
    // creation tick matches, or it is flagged the current/highest revision). @0x7DEF54
    bool IsWritableForThisTick(int nTick) const;
    bool Manager_IsDeletionMark() const;            // @0x7DEEBC (x64 mFlags bit 3)
    void Manager_SetNextRevision(AptRenderItem* pNext);   // @0x7DEF00
    void Manager_SetDeletionMark(bool bMark);             // @0x82ADB138 (full link-release form)

    // Manager_GetMask @0x82AD4F48 -- the mask render item (manager-side accessor).
    AptRenderItem* Manager_GetMask() const { return mpMask; }
    // IsHighestRevisionItem @0x82AD4FF0 -- this item is the latest revision in the
    // manager's double-buffered chain (no newer revision linked).
    bool IsHighestRevisionItem() const { return mpManagerNextRevision == nullptr; }
    // IsRenderableForThisTick @0x82AD4FC8 -- drawable on nTick: aged in (created on or
    // before nTick) AND the committed (non-writable) revision (x64 mFlags bit 6 clear).
    bool IsRenderableForThisTick(int nTick) const;
};

// The render-traversal helpers the character subtypes call from Render -- push
// the item's matrices onto the context's transform/colour stacks, draw, then pop.
//   PushMatrices @0x7F21E4 / PopMatrices @0x7ECA68  (homed in AptRenderItem.cpp,
//   against the real AptRenderingContext).
// The shape/morph subtypes draw their geometry with pCharacter->render() directly
// (AptCharacter::render @0x810E74 -> AptHook_DrawShape) -- the former
// AptCharacter_render free-function shim is retired.
void PushMatrices(AptRenderingContext* pCtx, const AptRenderItem* pItem);
void PopMatrices(AptRenderingContext* pCtx, const AptRenderItem* pItem);
// PushMatricesAbsolute @0x7F28B8 (PS3 External) -- the absolute (world-space) push
// variant used by PushRenderDataAbsolute (resets the vertex matrix to identity, then
// appends the item's mask position matrix). Homed in AptRenderItem.cpp.
void PushMatricesAbsolute(AptRenderingContext* pCtx, const AptRenderItem* pItem);

// The AptLinker zombie-swap depth/visual accessors are the named render-item members
// (GetDepth()/SetDepth()/CopyRenderDataFrom()) called directly -- the X360 inlines
// them as raw field reads/writes + a vtbl-slot copy dispatch; the free-function shims
// (AptRenderItem_GetDepth/SetDepth/CopyVisualFrom) are retired.
