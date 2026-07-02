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
struct AptNativeHash;   // AddToDisplayList registers the placed node in the parent's property hash
struct AptRenderItem;   // instantiateCharacter returns the placed node's writable render item
struct AptCharacterInst;
struct AptCXForm;       // placeObject's supplied colour transform
class  AptValue;        // placeObject's AS class object
class  EAStringC;       // placement instance names

// The 4-byte pool-allocated head/sentinel node: its single dword is the first
// listed entry (the AptCIH chain is then linked through the CIH display-list links).
struct AptDisplayListNode
{
    AptCIH* mpFirst;   // +0x00 first listed scene node (null when empty)
};

// ---------------------------------------------------------------------------
// The .apt frame-placement command threaded into the placement entry points
// (AddToDisplayList / ReplaceDisplyListItem / mergeState / placeObject). It is a
// pair of pointers: ppPlacement[0] = the serialised PlaceObject record (its +0xC
// dword is the placed character id, read in place from the .apt timeline data), and
// ppPlacement[1] = the runtime placement-properties block below. The properties hold
// live runtime pointers, so they are NAMED members (x64 widths) -- the console
// dword offsets are documentation.
// ---------------------------------------------------------------------------
struct AptCharacter;
struct AptUint32CXForm;
struct AptFramePlacementProps
{
    AptCharacter*    mpCharacter;        // [0] +0x00  character to place (null == keep existing)
    float*           mpPositionMatrix;   // [1] +0x04  src 2D-affine (6 floats), copied when mnFlags bit2
    AptUint32CXForm* mpColorTransform;   // [2] +0x08  src packed-ARGB colour, copied when mnFlags bit3
    int32_t          mnReserved0C;       // [3] +0x0C
    int32_t          mnReserved10;       // [4] +0x10
    int32_t          mnFlags;            // [5] +0x14  bit2 = has matrix, bit3 = has colour
    int16_t          mi16CharacterId;    //     +0x18  placed-char id (mergeState gate1: == node createdOnFrame)
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

    // ---- placement surface (bodies in AptDisplayList.cpp) ------------------
    // AddToDisplayList @0x82B0B150 -- run the placed character's init actions, bind
    // its import file (for a non-animation character with no file yet), dispatch the
    // frame placement, register the placed node as a new instance, and (when it has
    // an instance name) register it in the parent object's property hash. Returns the
    // placed AptCIH. ppPlacement = {placement-record, AptCharacter* to place}.
    AptCIH* AddToDisplayList(AptNativeHash* pParentHash, void** ppPlacement, AptCIH* pParentNode);

    // _addToSetCaches @0x82AF46B8 -- when pNode is a sprite/movie-clip with registered
    // clip-event handlers, fold their event mask into the node's clip-action flags, add
    // it to the target's clip-event set cache (when it has load/unload handlers), and
    // (when bRunLoad) queue its load/init clip events. STATIC: the X360 takes the scene
    // node in r3 (no AptDisplayList `this`).
    static void _addToSetCaches(AptCIH* pNode, uint8_t bRunLoad);

    // ReplaceDisplyListItem @0x82B0B2B8 -- reconcile the node already at a depth
    // (pExisting) with a re-issued placement: replace it when the command names a new
    // character (re-running AddToDisplayList), else merge the placement's colour/
    // position onto it (unless it was ActionScript-changed). Returns the (re)placed node.
    AptCIH* ReplaceDisplyListItem(AptNativeHash* pParentHash, AptCIH* pExisting,
                                  void** ppPlacement, AptCIH* pParentNode);

    // instantiateCharacter @0x82B061D0 -- find-or-create the placed node at nDepth and
    // (re)bind it to pCharacter (force-remove / reuse-placed / placeholder-reuse / create),
    // register its instance name, enrol sprite/anim/button nodes in the new-inst table, or
    // seed a dynamic-text inst's render item. *ppOutNode = the node, *pbOutCreatedNew = the
    // not-a-reused-placed flag; returns the node's writable render item.
    AptRenderItem* instantiateCharacter(int nDepth, AptCharacter* pCharacter, const EAStringC* pName,
                                        AptCIH* pParentNode, int bForceRemove, int16_t nClipDepth,
                                        AptCIH** ppOutNode, int* pbOutCreatedNew);

    // placeObject @0x82B097D8 -- place a character at a depth: instantiate when no node is
    // supplied, mark the parent's generalised-process dirty state, copy the supplied colour
    // transform / position matrix / placement field, stamp a morph blend value, and bind a
    // freshly-instantiated node's AS class. Returns the placed node.
    AptCIH* placeObject(AptCIH* pExistingNode, int nDepth, AptCharacter* pCharacter,
                        const EAStringC* pName, AptCIH* pParentNode, int bForceRemove,
                        int16_t nClipDepth, double fFrameValue, const AptCXForm* pColorXForm,
                        const float* pPositionMatrix, const void* pPlacementClipActions /*console u32 field18*/,
                        AptValue* pClassObject);

    // placeObjectNCXForm @0x82B0AD28 -- placeObject with the colour supplied as a packed-ARGB
    // AptUint32CXForm* (expanded into a scratch AptCXForm), the clip-event hash forced null.
    AptCIH* placeObjectNCXForm(AptCIH* pExistingNode, int nDepth, AptCharacter* pCharacter,
                               const EAStringC* pName, AptCIH* pParentNode, int bForceRemove,
                               int16_t nClipDepth, double fFrameValue, const float* pPositionMatrix,
                               const void* pPlacementClipActions /*console u32 field18*/, const AptUint32CXForm* pPackedColor);

    // mergeState @0x82B0B438 -- reconcile this list against a source frame's placement chain
    // (ppMergeInfo[0] = source AptPseudoDisplayList, ppMergeInfo[1] = parent node), walking
    // both depth-ordered lists in lockstep (match -> merge/replace, lower source -> add,
    // lower existing -> remove unless bKeepRemoved). Returns the last (re)placed node.
    AptCIH* mergeState(void** ppMergeInfo, AptNativeHash* pParentHash, char bKeepRemoved);
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
//     -- still un-faithfully-bodyable (Phase B re-audit). Some prerequisites have
//        landed since the original note: the module-static "new instance / to-be-
//        ticked" table (off_8324E544 / dword_8324E548) is now reachable through
//        AptAnimationTarget::GetNewInsts() / GetNewInstSize() / DecNewInstSize();
//        off_8324E574 == gpAptTarget (AptTarget.h) and dword_8324E514 ==
//        gnAptActionFrameId (AptMovie.cpp). The TRUE remaining blockers are TYPE-
//        and SUB-level, and bodying around them would require guessing offsets on
//        un-homed types (forbidden) or extending the SHARED value/char headers
//        (out of scope this phase):
//          * the opaque local sub_82B0B080 -- the frame-placement dispatcher whose
//            return is the placed CIH (AddToDisplayList / ReplaceDisplyListItem hinge
//            on it). IDA exported NO standalone body for it, so its placement logic +
//            any struct layout it touches are unrecoverable -> these two stay missing.
//          * the un-homed AptCharacter SPRITE/ANIMATION-subclass + AS frame/placement
//            record layout: the deep chains (e.g. *(*(*(*(a4+32)+4)+4)+4)+16 reaching
//            AptCharacterAnimation::ExecuteInitActions' `this`, and the AS depth->name
//            lookup table at renderItem+0x30/+0x34 walked by AddToDisplayList /
//            ReplaceDisplyListItem / instantiateCharacter) index fields these headers
//            do NOT model. AptCharacterInst is modelled only to +0x10, but placeObject
//            / instantiateCharacter read charInst+0x10 (create-depth) and +0x18
//            (un-named) -- un-homed.
//          * sub_82B008B0 / sub_82AEE788 ARE recoverable (create/re-insert a CIH at a
//            depth via AptDisplayListState::findInst+insert), but their callers
//            (instantiateCharacter) remain blocked on the above, so they are not
//            broken out yet.
//        Also pending: AptFile::operator= on the un-homed AS placement record, the
//        AptCXForm::AptUint32CXFormCopy 1-arg form (IDA flags its local alloc failed),
//        and AptActionInterpreter::setVariable's placeObject use of the un-homed
//        per-instance __proto__ walk. Reconstructed when those types/callees land.
// ===========================================================================
