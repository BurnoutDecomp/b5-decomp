#pragma once

// ===========================================================================
// EATech Apt -- AptCIH (Character Instance Handle): the Apt scene-graph node.
//
// The runtime tree of a playing movie is built from AptCIHs. Each one is an
// AptValue (so ActionScript can reference movie clips), holds an instance name +
// packed state/flags, the display-list sibling/parent links, and points at its
// AptCharacterInst (which owns the renderable AptRenderItem). The spine is:
//   AptCIH (scene node) -> AptCharacterInst -> AptRenderItem (renderable).
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (6AptCIH, ~80 methods). LAYOUT
// (named, x64-width; console dwords): AptValueGC base, then
//   [2] mInstanceName          EAStringC
//   [3] mFlagsA                CIHState@29-30 / genProcDirty@24 / inCtor@27 /
//                              zombieCount@7-22 / dirty bits ...
//   [4] mFlagsB                createdOnFrame@18-31 / ...
//   [5] mpDisplayListPrevious  AptCIH*
//   [6] mpDisplayListNext      AptCIH*
//   [7] mpDisplayListParent    AptCIH*   (the parent node; ref-counted)
//   [8] mpCharacterInst        AptCharacterInst*
//   [9] mpAssetString          lazily-allocated asset-name string (freed by dtor)
//
// SCOPE: the node core -- ctor/dtor, the GC virtuals, and the state/flag/link/
// name accessors. The ~60 behavioural methods (tick / Process* / render /
// InsertChild/RemoveChild / the display-list ops / objectMember / _gotoAndX /
// the IsXInst type queries) are follow-ons.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // AptValueGC base
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // mInstanceName

struct AptCharacter;
struct AptCharacterInst;
struct AptCharacterAnimationInst;
struct AptRenderItem;
struct AptMatrix;
struct AptCXForm;
struct AptRect;
struct AptNativeHash;

// x64 8-byte fork: the embedded AptMovie/AptCharacterAnimation offset within a sprite/animation
// AptCharacter (console 0x10; native-8-format widens it to 0x20). The host sets this to match the
// loaded .apt's pointer size before ticking; AptCIH_GetClipMovie reads it. Defined in AptCIH.cpp.
extern unsigned int gAptCharMovieOffset;
// x64 8-byte fork: when set, AptCIH::tick SKIPS the timeline (doFrameControls/queueFrameActions/clip
// events) because the native-8 timeline records are un-relocated/un-widened. Defined in AptCIH.cpp.
extern unsigned int gAptSkipTimeline;

struct AptCIH : public AptValueGC
{
    EAStringC         mInstanceName;          // [2]
    uint32_t          mFlagsA;                // [3]
    uint32_t          mFlagsB;                // [4]
    AptCIH*           mpDisplayListPrevious;  // [5]
    AptCIH*           mpDisplayListNext;      // [6]
    AptCIH*           mpDisplayListParent;    // [7]
    AptCharacterInst* mpCharacterInst;        // [8]
    void*             mpAssetString;          // [9]

    AptCIH(AptCharacter* pCharacter, AptCIH* pParent);   // @0x824C7C
    virtual ~AptCIH();                                    // @0x804E68

    // ---- AptValue object-model virtual overrides --------------------------
    virtual void RegisterReferences();   // @0x7E9C78 (GC mark: parent + char props)
    virtual void DestroyGCPointers();    // @0x80553C (release parent + char inst)
    virtual void PreDestroy();           // @0x7ECBAC
    // FLAG: AptCIH also overrides GetHasClass/SetHasClass (@0x7E1E18/0x7E1DF0) +
    // objectMemberSet/Lookup; deferred (AptValue's defaults inherited for now).

    // ---- name -------------------------------------------------------------
    const EAStringC& GetInstanceName() const { return mInstanceName; }   // @0x7DF0A8
    void SetInstanceName(const EAStringC& strName) { mInstanceName = strName; }  // @0x7F56B4

    // ---- character instance / delegated visual reads ----------------------
    AptCharacterInst* GetCharacterInst() const { return mpCharacterInst; }  // @0x7DF174
    int16_t GetDepth() const;   // @0x7EA024 (through the char inst's render item)

    // ---- delegated mask / property reads (through the char inst) -----------
    AptNativeHash* GetNativeHash() const;   // @0x82AD5B28 (char inst's property hash)
    bool IsMask() const;                    // @0x82AD5BA0 (render item's mask flag)
    bool HasMask() const;                   // @0x82AD5BB8 (render item's has-mask flag)

    // ---- native-hash (per-instance AS property table) ops -----------------
    // ContainsNativeHashVirtual @0x82AD74B8 -- AptValue vtbl[3] override: does this
    // node's character instance carry a per-instance property hash?
    virtual bool ContainsNativeHashVirtual() const override;
    // HasEventMember @0x82AD74E0 -- walk this value's __proto__ chain, returning the
    // first link whose native-hash event-handler mask intersects nEventMask (the
    // matched bits, non-zero == "has an event member"), else 0.
    int HasEventMember(int nEventMask);
    // HasMouseEvent @0x82AE2268 -- true if this node responds to any mouse event:
    // either its sprite-base clip-event flags carry a mouse bit, or an AS mouse
    // handler is registered in its __proto__ chain. Body in AptCIHBehaviour.cpp.
    bool HasMouseEvent();
    // ForceCleanNativeHash @0x82AF2338 -- force-destroy the char inst's per-instance
    // property hash and null its slot (called by AptLinker::Update).
    void ForceCleanNativeHash();

    // FindAndSetEvents @0x82B02740 -- scan the built-in AS event-handler table: for each
    // (eventMask, nameIndex) descriptor whose bit is not yet set in this node's native-hash
    // event mask, look up the matching named child (AptValue::findChild against the runtime
    // AS-name table); when found, OR the event bit into the hash mask. Returns 1 if any of
    // the "needs-an-update-tick" events (mask 0x200C0) was newly set, else 0. (AptCIH::
    // AssociateInstToClass uses the return to decide whether to register a tick.)
    int FindAndSetEvents();

    // GetAnimationInst @0x82B7B358 -- mpCharacterInst narrowed to the animation
    // subtype (caller has already confirmed IsAnimationInst, type tag 9).
    AptCharacterAnimationInst* GetAnimationInst() const;

    // SetCharacterInst @0x82B00548 -- swap this node's character instance. A null
    // pCharacterInst is replaced by a freshly built "none" instance. When the new
    // instance differs from the current one it is installed; the old one (if any) has
    // its render data optionally moved into the new (bMoveRenderData), its per-frame
    // animation timer functions removed (when it was an animation, type tag 9), is
    // GC-torn-down + deleted, and the render tree is notified the item moved.
    // (AptDisplayList::instantiateCharacter / AptLinker::Update.)
    void SetCharacterInst(AptCharacterInst* pCharacterInst, bool bMoveRenderData);   // @0x82B00548

    // GetCosAngle @0x82AD7448 -- cosine of the rotation baked into an affine
    // transform's first column (1.0 when there is no rotation). Static: the X360
    // passes the AptMatrix in r3, not a CIH `this`.
    static float GetCosAngle(const AptMatrix* pMatrix);

    // ---- character-type predicates (through the char inst's type tag) -----
    // Each tests mpCharacterInst's type tag (AptCharacterInst::GetTypeTag); IsNone
    // tests this value's own AptValue vtable index (AptCIHNone == 37). Bodies in
    // AptCIH.cpp (need the full AptCharacterInst type).
    bool IsShapeInst() const;          // @0x82AD5A30  type 1
    bool IsDynamicTextInst() const;    // @0x82AD5A50  type 2
    bool IsButtonInst() const;         // @0x82AD5A10  type 4
    bool IsSpriteInst() const;         // @0x82AD59B0  type 5 or 16
    bool IsSpriteInstBase() const;     // @0x82AD59E0  type 5 or 9
    bool IsMorphInst() const;          // @0x82AD5A90  type 8
    bool IsAnimationInst() const;      // @0x82AD5AB0  type 9
    bool IsLevelInst() const;          // @0x82AD5AD0  type 15
    bool IsCustomControlInst() const;  // @0x82AD5B08  type 16
    bool IsNone() const;               // @0x82AD5AF0  AptValue vtable index 37 (AptCIHNone)

    // ---- display-list links -----------------------------------------------
    AptCIH* GetDisplayListPrevious() const { return mpDisplayListPrevious; }  // @0x7DF184
    AptCIH* GetDisplayListNext() const     { return mpDisplayListNext; }      // @0x7DF17C
    AptCIH* GetDisplayListParent() const   { return mpDisplayListParent; }    // @0x7DF1B4
    void SetDisplayListPrevious(AptCIH* p) { mpDisplayListPrevious = p; }     // @0x7DF1C4
    void SetDisplayListNext(AptCIH* p)     { mpDisplayListNext = p; }         // @0x7DF1BC
    void SetDisplayListParent(AptCIH* p)   { mpDisplayListParent = p; }       // @0x7DF1CC

    // GetFirstChild @0x82ADC938 -- the first placed child of a movie-clip/animation
    // node (only sprite-base instances own a child display list); null otherwise.
    AptCIH* GetFirstChild() const;

    // ReplaceZombieChild @0x82AFE098 -- swap a "zombie" child out of this container's
    // child display list for a fresh replacement (sprite-base containers only): the
    // replacement inherits the zombie's instance name + render-item visual state, the
    // zombie is queued for delayed release, and the replacement takes its slot.
    // Returns the inserted node (or `this` when this container has no child list).
    AptCIH* ReplaceZombieChild(AptCIH* pNewChild, AptCIH* pZombie);

    // ClearCIH @0x82AC... (X360-attested behavioural follow-on; body in its own
    // TU) -- tear down this node's character instance / placed state. Declared so
    // the display-list teardown (AptDisplayList::clear) can call it by name.
    void ClearCIH(bool bClearGCRoots);

    // Remove @0x82AFC0B0 -- detach this node from the live scene: cancel any
    // in-flight load + drop its queued ActionScript actions, ClearCIH its state,
    // and -- if anything outside the display list still references it (refcount > 1,
    // an AS-visible "zombie") -- unhook it from the render tree and (unless mid-
    // transition) mark its value undefined, then drop the display list's reference.
    void Remove(bool bClearGCRoots);

    // ---- movie-clip play-head (the per-frame timeline driver) -------------
    // tick @0x82B0BED8 -- advance a sprite(5)/animation(9) movie-clip node one frame
    // when its dirty bit (mFlagsA bit25) is set: optionally step the play-head
    // (auto-play, unless stopped or the recorder gate is shut), run the new frame's
    // place/remove commands (doFrameControls) and queue its frame actions
    // (queueFrameActions), fire the enterFrame / construct clip events, recurse the
    // child display list (AptDisplayList::tick), then recompute the dirty bit from
    // whether anything still wants to tick. Returns the resulting dirty bit (0/1).
    int tick();   // @0x82B0BED8

    // jumpToFrame @0x82B0BD50 -- seek a movie-clip node's play-head to nFrame. A
    // single-step forward replays just that frame's commands (doFrameControls); any
    // other jump rebuilds the intervening display-list state by re-running every
    // skipped frame into a scratch AptPseudoDisplayList and merging the result
    // (mergeState). Finishes by queueing the landed frame's actions. No-op on a
    // string-value placeholder, a negative frame, or a frame past the clip's end.
    int jumpToFrame(int nFrame);   // @0x82B0BD50

    // _gotoAndX @0x82B0D2F0 -- the AS gotoAndPlay/gotoAndStop core. Reads the goto
    // target off the interpreter operand stack: a numeric value is the 1-based frame
    // number; a string/label value resolves through the clip's label hash. Seeks via
    // jumpToFrame, then sets/clears the clip's auto-play flag from bPlay and re-dirties
    // the node when it stopped. Returns the shared `undefined` value. nArgCount gates
    // the whole op (must be >= 1). Static: the X360 passes the CIH in r3, no `this`.
    void* _gotoAndX(int nArgCount, unsigned int bPlay);   // @0x82B0D2F0

    // ---- packed state / flags (mFlagsA bit-fields) -----------------------
    uint32_t GetCIHState() const;       void SetCIHState(uint32_t eState);    // @0x7DF12C/0x7DF110
    int16_t  GetZombieCount() const;    void IncZombieCount();  void DecZombieCount();  // @0x7DF160/0x7DF138/0x7FB648
    bool     IsInCtor() const;          void SetInCtor(uint32_t b);           // @0x7DF0E8/0x7DF0F4
    int      GetCreatedOnFrame() const; void SetCreatedOnFrame(int nFrame);   // @0x7DF1D4/0x7DF1E4

    // ActionScript-changed flag (bit 31). @0x82AD50E8/0x82AD50C8
    bool GetASChanged() const  { return (mFlagsA >> 31) != 0; }
    void SetASChanged(bool b)  { mFlagsA = (mFlagsA & 0x7FFFFFFFu) | (b ? 0x80000000u : 0u); }
    // Dirty-state flag (bit 25). @0x82AD5C10 / @0x82AD76B8
    bool GetDirtyState() const { return ((mFlagsA >> 25) & 1u) != 0; }
    // SetDirtyState @0x82AD76B8 -- set/clear the dirty bit; container/leaf types
    // (shape/dyn-text/static-text) and the empty placeholder never carry it. When
    // dirtying with bPropagate, mark up the parent chain to the first dirty ancestor.
    void SetDirtyState(bool bDirty, bool bPropagate);
    // "In remove list" flag (bit 26). @0x82AD5188/0x82AD5170
    bool GetInRemList() const  { return ((mFlagsA >> 26) & 1u) != 0; }
    void SetInRemList(bool b)  { mFlagsA = (mFlagsA & 0xFBFFFFFFu) | (b ? 0x04000000u : 0u); }

    // ---- AptValue object-model overrides (mFlagsA bit 28) -----------------
    // @0x82AD7438/0x82AD7418 -- whether this CIH has an attached AS class.
    virtual bool GetHasClass() const override { return ((mFlagsA >> 28) & 1u) != 0; }
    virtual void SetHasClass(int b) override  { mFlagsA = (mFlagsA & 0xEFFFFFFFu) | (b ? 0x10000000u : 0u); }

    // ---- behavioural batch 2: delegated transform / flag / native-hash ops -
    // All forward through the char inst's (writable) render item or its property
    // hash; bodies in AptCIHBehaviour.cpp, verified against the X360 asm.
    const AptMatrix* GetPositionMatrixConst() const;   // @0x82ADC2F0
    const AptCXForm* GetColorMatrixConst() const;      // @0x82ADC318
    AptMatrix*       GetPositionMatrixWritable();       // @0x82AE6730 (lazy-alloc)
    AptCXForm*       GetColorMatrixWritable();          // @0x82AE6758 (lazy-alloc)
    AptRenderItem*   SetDepth(int16_t nDepth);          // @0x82AE2300
    bool             GetIsPlaying() const;              // @0x82AD5C00 (movie-clip play-head bit)
    void             SetEventHandler(int nEventMask);   // @0x82AD5B48 (OR into the hash event mask)
    void             RemoveEventHandler(int32_t nMask); // @0x82AD5B70 (clear hash event bits)
    void             SetHasMask(bool bHasMask, AptRenderItem* pMask);  // @0x82AE22B8
    AptCIH*          SetIsInserted();                   // @0x82AECE40 (render-tree insert notify)

    // ---- GC-value pool allocation (AptCIH is an AptValueGC) ---------------
    // operator new @0x82AE5B90 / delete @0x82AE72E8 -- allocate/free from the GC
    // value pool (gpGCPoolManager) + flip the AptValueGC_MemItem allocated flag.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Release @0x82AE7390 -- AptValue vtbl[1] override: a CIHState-1, singly-referenced
    // node (only the display list owns it) pins itself -- Release is a no-op; every
    // other state/refcount delegates to AptValue::Release.
    virtual void Release() override;

    // SetMask @0x82AF6650 -- make pMaskSlave the mask for this node (the MASTER), or
    // tear the relationship down: wires the two render items' isMask/hasMask flags +
    // the "#!MASKSLAVE!#"/"#!MASKMASTER!#" native-hash cross-links, builds the slave's
    // world mask matrix from this node's parent chain, and notifies the render tree.
    // (AS .setMask, via AptCIHNativeFunctionHelper::sMethod_setMask.)
    void SetMask(AptCIH* pMaskSlave);

    // GetMask @0x82AE7B48 -- the current mask SLAVE, resolved from this master's
    // "#!MASKMASTER!#" native-hash key (or null when this node carries no mask / has no
    // property hash). Body in AptCIHBehaviour.cpp. (X360 const; r3 = the master CIH.)
    AptCIH* GetMask() const;                                    // @0x82AE7B48

    // HasClipEvent @0x82B027C0 -- does this node's sprite-base clip-event flag set
    // (mnClipActionFlags high bits) carry any handler in nEventMask? Body in
    // AptCIHBehaviour.cpp.
    bool HasClipEvent(int nEventMask);                          // @0x82B027C0
    // HasEvent @0x82B02838 -- HasClipEvent(nEventMask) OR HasEventMember(nEventMask):
    // the node responds to nEventMask via either a packed clip-event flag or an AS
    // __proto__ handler.
    bool HasEvent(int nEventMask);                              // @0x82B02838

    // queueClipEvents @0x82B0... -- queue this node's handlers for nEventMask (the AS
    // clip-event scan + dispatch). For each matching packed clip-event the handler is
    // enqueued on the deferred-action queue stamped with nFrameId; when bDeferred and an
    // AS __proto__ event member matches, the named child handler is enqueued instead.
    // Returns the shared 0/`undefined`-as-int the callers ignore. Body in
    // AptCIHBehaviour.cpp (FLAG: the AS bytecode-execution sub-path is deferred).
    AptValue* queueClipEvents(int nEventMask, unsigned int nFrameId, int bDeferred);   // @0x82B0...

    // GeneralisedProcess @0x82AE01B0(per-node) -- the per-node generalised-process pass
    // (the deferred dirty-state / mask-matrix / AS-process callback flush). Runs the
    // registered process callbacks, recurses sprite/animation child lists, and folds the
    // results into this node's subtree-dirty bit (mFlagsA bit23). Returns that bit (0/1).
    // Body in AptCIHBehaviour.cpp.
    unsigned int GeneralisedProcess(AptCIH* pRoot, void* pContext);   // @0x82B...

    // SetGeneralizedProcessDirtyState @0x82ADCA50 -- mark this node's generalized-
    // process-dirty state. When dirtying: set the self bit (mFlagsA bit24) and
    // propagate the subtree bit (bit23) up the parent chain to the first marked
    // ancestor. When clearing: clear the self bit, then (if the subtree bit is clear)
    // scan for a still-dirty child. Returns the offending child, else `this`.
    AptCIH* SetGeneralizedProcessDirtyState(bool bDirty);      // @0x82ADCA50

    // SetProceduralProperty @0x82AE73C0 -- write a built-in AS "procedural" property
    // (the AS setProperty writer, mirror of GetProceduralProperty) by selector. The
    // selector order is the X360 jump table (word_82145258): 0 _x / 1 _y / 2 _xscale /
    // 3 _yscale / 4 _width / 5 _height / 6 _rotation / 7 _alpha / 8 colour-translate R /
    // 9 G / 10 B / 11 _visible. bASChanged stamps the AS-changed flag (mFlagsA bit31).
    // Writes through the char inst's writable position/colour matrix (or SetIsVisible),
    // then re-marks the generalized-process dirty state for _visible.
    void SetProceduralProperty(uint32_t nSelector, float fValue, bool bASChanged);   // @0x82AE73C0

    // GetProceduralProperty @0x82AE2D10 -- read a built-in AS "procedural" property
    // (scale/rotation/x/y/alpha/colour/visible/width/height) by selector and return
    // it as a float; out-of-range -> -1. Derives from the node's position + colour
    // transforms (the AS getProperty reader). FLAG: the case-label integers are
    // inferred from an unexported remap table (only index 11 == _visible is
    // confirmed, via IsVisible); _width/_height defer to the un-homed GetBoundingRect.
    float GetProceduralProperty(uint32_t nPropertyIndex) const;   // @0x82AE2D10

    // IsVisible @0x82AE2F30 -- true iff this node AND every display-list ancestor is
    // visible (walks mpDisplayListParent, testing GetProceduralProperty(_visible)).
    bool IsVisible() const;

    // SetIsMask @0x82AECE58 -- set/clear this node's render-item mask flag (with an
    // optional mask position matrix) via the writable render item, then propagate the
    // generalized-process dirty state. Returns SetGeneralizedProcessDirtyState's result.
    AptCIH* SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix);   // @0x82AECE58

    // GetBoundingRect @0x82AE2B30 -- accumulate this node's world-space bounds into
    // pAccumulator under pParentTransform: concat this node's position matrix, then
    // (sprite-base) recurse into its child display list, or (leaf shape/static-text/
    // dynamic-text) expand by the character/render-item bounds. Returns pAccumulator.
    AptRect* GetBoundingRect(int nMode, const AptMatrix* pParentTransform, AptRect* pAccumulator);   // @0x82AE2B30

    // ProcessMaskMatricies @0x82AEDAE0 -- when this node's render item is flagged as a
    // mask: clamp its colour-scale alpha up to the minimum mask alpha (51), then rebuild
    // the mask's world position matrix by concatenating every display-list ancestor's
    // position transform top-down, install it on the writable render item (SetIsMask),
    // and (re)mark the generalized-process dirty state. Returns true when a mask was
    // (re)processed, false when the node carries no mask.
    bool ProcessMaskMatricies();   // @0x82AEDAE0

    // CleanNativeFunctions @0x82AD6FB8 -- shutdown teardown: Release + null each of the
    // process-wide ActionScript native-function singletons (the built-in AS functions
    // registered at startup). Static: the X360 takes no `this`. (Called by AptUpdateShutdown.)
    static void CleanNativeFunctions();   // @0x82AD6FB8

    // ProcessTextInst @0x82B076F0 -- per-frame dynamic-text refresh. Only acts on a
    // dynamic-text node (char type tag 2). When the node is visible and its render-data
    // handle is unresolved (0 / the empty-text sentinel) or its text is dirty (state
    // bit0 clear), it re-resolves the text (EnsureStringAllocated, fed the parent node
    // so inherited TextFormat resolves). Returns true if the node is dynamic text.
    bool ProcessTextInst();   // @0x82B076F0

    // EnsureStringAllocated @0x82B06F08 -- FLAG (declared-only): the deep dynamic-text
    // build/layout path (AptCharacterTextInst::UpdateText + the glyph/TextFormat layout
    // engine that bakes the field's render data). Its body belongs with the Apt text/font
    // engine TU; declared here so ProcessTextInst / the AS createTextField path can name
    // it. pParent is the display-list parent (inherited TextFormat source).
    void EnsureStringAllocated(AptCIH* pParent);   // @0x82B06F08
};
