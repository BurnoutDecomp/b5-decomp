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
// (console 52 bytes / 13 dwords; reconstructed with NAMED members):
//   [0]  vtable
//   [1]  mpCharacter            the AptCharacter this renders (ref-counted)
//   [2]  mpPositionMatrix       AptMatrix* (lazily allocated; null -> identity)
//   [3]  mpColorMatrix          AptCXForm* (lazily allocated; null -> identity)
//   [4]  mpMaskPositionMatrix   AptMatrix*
//   [5]  mDepth:i16 + mClipDepth:i16   (packed; both -1 at ctor)
//   [6]  mFlags                 bit31 isVisible, bit30 isMask, bit29 hasMask
//   [7]  mpMask                 AptRenderItem* (the mask item; ref-counted)
//   [8]  mCreatedOnTick         int
//   [9]  mRefCount              int (Add/ReleaseReference mutate it atomically)
//   [10] mpManagerNextRevision  AptRenderItem*  (the render-tree-manager links --
//   [11] mpManagerNextSibling   AptRenderItem*   the double-buffered revision tree;
//   [12] mpManagerFirstChild    AptRenderItem*   populated by AptRenderTreeManager)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptCharacter;
struct AptMatrix;
struct AptCXForm;
struct AptRenderingContext;
enum AptMaskRenderOperation : int;   // values reconstructed with the render path (FLAG)

// FLAG (homed elsewhere): the shared identity transforms returned when an item
// has no own matrix, and the global render-item teardown latch + alloc counter.
extern AptMatrix gIdentityMatrix;
extern AptCXForm gIdentityCXForm;
extern bool      gbRenderItemShuttingDown;

struct AptRenderItem
{
    void*          mpVTable_unused;   // [0] (the real vtable; named for layout clarity)
    AptCharacter*  mpCharacter;       // [1]
    AptMatrix*     mpPositionMatrix;  // [2]
    AptCXForm*     mpColorMatrix;     // [3]
    AptMatrix*     mpMaskPositionMatrix; // [4]
    int16_t        mDepth;            // [5] low half
    int16_t        mClipDepth;        // [5] high half
    uint32_t       mFlags;            // [6]
    AptRenderItem* mpMask;            // [7]
    int32_t        mCreatedOnTick;    // [8]
    volatile int32_t mRefCount;       // [9] (byte 36)
    AptRenderItem* mpManagerNextRevision; // [10]
    AptRenderItem* mpManagerNextSibling;  // [11]
    AptRenderItem* mpManagerFirstChild;   // [12]

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

    // SetIsVisible @0x82AE0708 -- set the is-visible flag (bit31) + recompute the
    // subtree's mask-driven visibility (PropagateTreeIsVisible) when the bit changes.
    AptRenderItem* SetIsVisible(bool bVisible);

    bool GetIsVisible() const;      // @0x7DEE68
    bool GetIsMask() const;         // @0x7DEE20
    bool GetHasMask() const;        // @0x7DEE34
    AptRenderItem* GetMask() const; // @0x7DEE2C

    // SetHasMask @0x82ADB388 -- set the has-mask flag (bit29) + (un)bind the mask
    // render item (ref-counted; propagates visibility). Returns the previous mask
    // (X360 r3 = this on the no-op path / the released old mask otherwise).
    AptRenderItem* SetHasMask(bool bHasMask, AptRenderItem* pMask);
    // SetIsMask @0x82AEBF28 -- set the is-mask flag (bit30) + the mask matrix.
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

    // FLAG (bodies are [todo] in their own render-item revision TU; declared here
    // decl-only so the AptRenderTreeManager Render_*/Update_* facade compiles
    // against them by name). These are the per-item revision/link mutators the
    // render-tree manager drives -- the writable-revision chase + the
    // first-child / next-sibling / mask link writes for the double-buffered tree.
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
    // state (mFlags bits 24..26) down this item's first-child + next-sibling subtree.
    // nVisibleMode: 1 = becoming hidden by a mask, 0 = becoming shown. Recursive.
    AptRenderItem* PropagateTreeIsVisible(int nVisibleMode);

    // True when this item is already the writable revision for nTick (its
    // creation tick matches, or it is flagged the current/highest revision). @0x7DEF54
    bool IsWritableForThisTick(int nTick) const;
    bool Manager_IsDeletionMark() const;            // @0x7DEEBC (mFlags bit 28)
    void Manager_SetNextRevision(AptRenderItem* pNext);   // @0x7DEF00
    void Manager_SetDeletionMark(bool bMark);             // @0x7E48DC (simplified)

    // Manager_GetMask @0x82AD4F48 -- the mask render item (manager-side accessor).
    AptRenderItem* Manager_GetMask() const { return mpMask; }
    // IsHighestRevisionItem @0x82AD4FF0 -- this item is the latest revision in the
    // manager's double-buffered chain (no newer revision linked).
    bool IsHighestRevisionItem() const { return mpManagerNextRevision == nullptr; }
    // IsRenderableForThisTick @0x82AD4FC8 -- drawable on nTick: aged in (created on or
    // before nTick) AND the committed (non-writable) revision (mFlags bit 25 clear).
    bool IsRenderableForThisTick(int nTick) const;
};

// The render-traversal helpers the character subtypes call from Render -- push
// the item's matrices onto the context's transform/colour stacks, draw, then pop.
//   PushMatrices @0x7F21E4 / PopMatrices @0x7ECA68  (homed in AptRenderItem.cpp,
//   against the real AptRenderingContext).
//   AptCharacter_render -> AptCharacter::render @0x810E74 -- FLAG: the geometry
//   draw; still homed by the geometry/render-backend layer (not yet built).
void PushMatrices(AptRenderingContext* pCtx, const AptRenderItem* pItem);
void PopMatrices(AptRenderingContext* pCtx, const AptRenderItem* pItem);
void AptCharacter_render(AptCharacter* pCharacter, AptRenderingContext* pCtx,
                         AptMaskRenderOperation eOp, int nTick);
// PushMatricesAbsolute @0x7F28B8 (PS3 External) -- the absolute (world-space) push
// variant used by PushRenderDataAbsolute (resets the vertex matrix to identity, then
// appends the item's mask position matrix). Homed in AptRenderItem.cpp.
void PushMatricesAbsolute(AptRenderingContext* pCtx, const AptRenderItem* pItem);

// Free-function render-item accessors (the AptLinker zombie-swap path). The X360
// inlines these as raw field reads/writes + a vtbl-slot copy dispatch; homed in
// AptRenderItem.cpp against the named members (GetDepth/SetDepth/CopyRenderDataFrom).
int16_t AptRenderItem_GetDepth(const AptRenderItem* pItem);                          // console *(item+0x14)
void    AptRenderItem_SetDepth(AptRenderItem* pItem, int16_t nDepth);               // console *(item+0x14)=
void    AptRenderItem_CopyVisualFrom(AptRenderItem* pDst, const AptRenderItem* pSrc); // console (*item->vtbl[+0x14])(dst, src)
