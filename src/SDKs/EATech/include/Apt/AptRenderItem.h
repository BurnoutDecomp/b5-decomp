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
    virtual ~AptRenderItem();                                       // @0x80F860

    // The base render hooks are empty (@0x7E49A8/0x7E499C/0x7E49A4/0x7E49A0); each
    // character subtype (Shape/Sprite/Button/Text/...) overrides Render to draw.
    // (So the base is concrete -- it just renders nothing.)
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

    int16_t GetDepth() const;       AptRenderItem* SetDepth(int nDepth);          // @0x7DEE14/0x7DEEA0
    int16_t GetClipDepth() const;   AptRenderItem* SetClipDepth(int nClipDepth);  // @0x7DEDF8/0x7DEEA8

    bool GetIsVisible() const;      // @0x7DEE68
    bool GetIsMask() const;         // @0x7DEE20
    bool GetHasMask() const;        // @0x7DEE34
    AptRenderItem* GetMask() const; // @0x7DEE2C

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

    // True when this item is already the writable revision for nTick (its
    // creation tick matches, or it is flagged the current/highest revision). @0x7DEF54
    bool IsWritableForThisTick(int nTick) const;
    bool Manager_IsDeletionMark() const;            // @0x7DEEBC (mFlags bit 28)
    void Manager_SetNextRevision(AptRenderItem* pNext);   // @0x7DEF00
    void Manager_SetDeletionMark(bool bMark);             // @0x7E48DC (simplified)
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
// PushMatricesAbsolute @0x7F28B8 -- the absolute (world-space) push variant used
// by PushRenderDataAbsolute. FLAG: homed by the render-context layer.
void PushMatricesAbsolute(AptRenderingContext* pCtx, const AptRenderItem* pItem);
