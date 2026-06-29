#pragma once

// ===========================================================================
// EATech Apt -- AptDisplayList: a movie-clip's child display list.
//
// The depth-ordered list of AptCIH scene nodes a sprite/movie-clip instance has
// placed (placeObject / instantiateCharacter / AddToDisplayList add to it; tick /
// GeneralisedProcess / GetBoundingRect walk it). Unlike the lightweight
// AptDisplayListState (which IS the head pointer, embedded directly), AptDisplayList
// owns a single pointer to a small pool-allocated HEAD NODE; the actual entries
// (AptCIH*) chain off that node. It is embedded BY VALUE inside the sprite character
// instances (AptCharacterSpriteInstBase at +0x1C), so this header pins its 1-pointer
// layout for those owners; the large behavioural surface (tick/placeObject/...) is
// reconstructed by its own follow-on TUs and extends this header.
//
// SHAPE + the ctor/dtor/PreDestroy/clear BODIES from the X360 ARTIST.XEX (the
// authoritative spine):
//     AptDisplayList::AptDisplayList   @ 0x82AE4850  (allocate + zero the 4-byte head node)
//     AptDisplayList::~AptDisplayList  @ 0x82AFE9F0  (clear, then free the head node)
//     AptDisplayList::PreDestroy       @ 0x82AFD2B8  (clear + free + null the head)
//     AptDisplayList::clear            @ 0x82AFD1F8  (walk the entries, release each)
//
// LAYOUT (4 bytes / 1 dword; the ctor's `*this = Allocate(pool, 4)` pins it):
//   +0x00  mpHead   the pool-allocated head node (its +0 dword points at the first
//                    listed AptCIH; null until the pool alloc, or after PreDestroy).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptCIH;
struct AptDisplayListState;

// The 4-byte pool-allocated head/sentinel node: its single dword is the first
// listed entry (the AptCIH chain is then linked through the CIH display-list links).
struct AptDisplayListNode
{
    AptCIH* mpFirst;   // +0x00 first listed scene node (null when empty)
};

struct AptDisplayList
{
    AptDisplayListNode* mpHead;   // +0x00 pool-allocated head node (4 bytes)

    // ctor @0x82AE4850 -- pool-allocate a zeroed 4-byte head node (or null on
    // allocation failure) and store it. Body in AptDisplayList.cpp (its own TU).
    AptDisplayList();

    // dtor @0x82AFE9F0 -- clear() the listed entries, then return the head node to
    // the pool (4 bytes). Body in AptDisplayList.cpp (its own TU).
    ~AptDisplayList();

    // PreDestroy @0x82AFD2B8 -- when a head node exists: clear() the entries, free
    // the head node (4 bytes), then null mpHead. Body in AptDisplayList.cpp.
    void PreDestroy();

    // clear @0x82AFD1F8 -- walk the listed AptCIH entries releasing each (optionally
    // dropping each one's GC-root + ClearCIH when bClearGCRoots). Body in
    // AptDisplayList.cpp (its own TU).
    void clear(bool bClearGCRoots);

    // ---- removal (teardown core; bodies in AptDisplayList.cpp) -------------
    // The head node's single AptCIH* head IS layout-identical to an
    // AptDisplayListState (both are a lone AptCIH* head + the CIH-resident list
    // links), so the list-mutation ops delegate to AptDisplayListState by
    // reinterpreting mpHead. AsState() centralises that typed reinterpretation
    // (NOT a raw-offset hack -- both are 1-pointer SDK head structures).
    AptDisplayListState* AsState() const;

    // removeObject @0x82AFD0B0 -- if pItem is a real placed node, drop its entry
    // from the owning AS object's property hash (when it has a non-empty instance
    // name registered there) then hand it to the list state's delayed-release
    // list. Body in AptDisplayList.cpp.
    void removeObject(AptCIH* pItem);

    // removeClonedObject @0x82AFD198 -- find the node at pSource's depth in this
    // list and removeObject it (used to drop a script-removed clone). Body in
    // AptDisplayList.cpp.
    void removeClonedObject(AptCIH* pSource);

    // GetBoundingRect @0x82AD9B38 -- accumulate the world-space bounds of every
    // placed (defined, non-level, unclipped) node in this list into pAccumulator
    // under pTransform, by recursing into each node's AptCIH::GetBoundingRect.
    // Returns pAccumulator. Body in AptDisplayList.cpp.
    struct AptRect* GetBoundingRect(int nMode, const struct AptMatrix* pTransform, struct AptRect* pAccumulator);

    // ---- per-frame walks (bodies in AptDisplayList.cpp) --------------------
    // tick @0x82AD9BB8 -- advance every eligible placed node one frame: a node
    // ticks when its render-item depth layer is selected (when bUseDepthLayerMask)
    // or it is not a dead node (CIHState != 3), AND it is a sprite(5)/animation(9)/
    // button(4) character instance. Returns the OR of each ticked node's result.
    int tick(int nDepthLayerMask, uint8_t bUseDepthLayerMask);

    // GeneralisedProcess @0x82AE01B0 -- run the generalised-process (deferred AS
    // action / dirty-state) pass over every eligible placed node (gated by the
    // render-item depth layer when bUseDepthLayerMask). Returns the OR of each
    // node's AptCIH::GeneralisedProcess result.
    int GeneralisedProcess(int nFlags, int nDepthLayerMask, uint8_t bUseDepthLayerMask);
};

// ===========================================================================
// DONE (faithful X360 decompiles, bodies in AptDisplayList.cpp): tick @0x82AD9BB8
// and GeneralisedProcess @0x82AE01B0 -- the per-frame node walks. They gate each
// listed node on the render-item depth layer / CIHState / character type tag (all
// now named) and OR-accumulate AptCIH::tick / AptCIH::GeneralisedProcess (declared
// as the same AptCIH_* free-function shims AptLinker uses; un-homed cluster).
//
// BLOCKED behavioural surface (NOT reconstructed here -- their own follow-on TUs):
//   AddToDisplayList @0x82B0B150 / ReplaceDisplyListItem @0x82B0B2B8 /
//   mergeState @0x82B0B438 / instantiateCharacter @0x82B061D0 /
//   placeObject @0x82B097D8 / placeObjectNCXForm @0x82B0AD28 /
//   _addToSetCaches @0x82AF46B8
//     -- need the un-homed AptCharacter frame/clip-event-mask layout, the module
//        static dispatch arrays (off_8324E544 / dword_8324E548 the "to be ticked"
//        list, off_8324E574, dword_8324E514), the unnamed local subs
//        (sub_82B0B080 / sub_82B008B0 / sub_82AEE788), AptCharacterAnimation::
//        ExecuteInitActions, AptActionInterpreter::setVariable,
//        Burnout_X360_Artist_0040_0, and AptCXForm::AptUint32CXFormCopy's
//        recovered signature. Reconstructed when those types/callees land.
// ===========================================================================
