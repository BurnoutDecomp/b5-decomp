#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_MANAGER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_MANAGER_H

#include "types.hpp"
#include <cstddef>                                                   // offsetof (_BehaviourManagerAssertLayout)
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (BehaviourHandle::Prepare)
#include "GameShared/GameClasses/Containers/CgsArray.h"              // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"         // CgsContainers::ObjectPool<T,N,TIndex>
#include "GameSource/Director/Utils/BrnAbstractPool.h"               // BrnDirector::AbstractPool<>, AbstractPoolVoidHandle
#include "GameSource/Director/Camera/Camera.h"                       // BrnDirector::Camera::Camera (mCamera, by value)

// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager.h
//
// CANONICAL HOME for BrnDirector::Camera::BehaviourManager -- the owner of every live
// director camera behaviour and the per-state behaviour-allocation book-keeping. Also
// homes the namespace-level helpers the manager hands around: BehaviourHelperIndex,
// BehaviourInterpolate (+ its Parameters), BehaviourHandle<> and BehaviourController-
// LockInterface.
//
// LAYOUT AUTHORITY: the DECFIGS DWARF for this TU (BrnBehaviourManager.cpp). The member
// list / NAMES / TYPES / ORDER below are reconstructed VERBATIM from that DWARF struct
// (BrnBehaviourManager.h:89, members at :322..:369). The X360 BrnBehaviourManager.cpp asm
// pins a handful of absolute CONSOLE byte offsets that anchor the ordering:
//   * mBehaviourHelperPool         -> console +0xFAB0 (64176)   (CheckNo/DebugDumpToTTY base)
//   * mBehaviourHelperIndexArray   -> console +0x16348 (90952), count word at +0x16458 (91064)
//   * a SpeedResponder time word   -> console +0x15870 (88176)  (UpdateAllBehaviours accumulator)
//                                     -- lands inside the responder/controller region, between
//                                        mBehaviourHelperPool and mBehaviourHelperIndexArray,
//                                        confirming the DWARF order.
// These CONSOLE offsets are 4-byte-pointer; the PC rebuild widens pointers/vptrs so the
// absolute values shift. Per the LLP64 rule, _BehaviourManagerAssertLayout below pins
// pointer-invariant RELATIVE ORDERING (offsetof comparisons), NOT the absolute console
// bytes.
//
// SAFETY: NO committed consumer embeds BehaviourManager BY VALUE -- every other use is a
// forward-declared pointer/reference (BrnMainDirector.h's "embedded" aggregate is itself a
// not-homed/declaration-only owner; BrnDirectorArbitrator.h / BrnMomentController.h thread it
// by ref). Growing the full layout here therefore changes NO consumer's object size.
//
// METHOD SET: the union of (a) the DWARF-attested manager API and (b) the call surface the
// already-committed consumers use (the arbitrator states' UnSetBehaviourUsedByHandle(u32) /
// CheckNoBehavioursAreAllocatedByState / IsBehaviourWaitingToPrepare / NewBehaviour<> family
// and the ICE movie-player's NewBehaviourInterpolate / ReleaseBehaviour /
// SetBehaviourUpdatesDuringPause). ALL methods are DECLARATION-ONLY this wave (the layout
// home); bodies land with the BehaviourManager / BehaviourInterpolate TUs.
//
// FLAG (opaque un-homed embedded sub-types): mBehaviourParameterBank,
//   mTempCameraBoostResponder, mSpeedResponder, mRotationController (Camera2DRotation-
//   Controller), mSphericalRotationController are heavy Camera-side aggregates with no
//   reconstructed home and no DWARF byte-offset authority. They are embedded BY VALUE per the
//   DWARF, so they are modelled as named opaque FLAGGED sub-objects (OpaqueSub<...>) at a
//   single placeholder size -- their interiors are NEVER fabricated. The asm pins the region
//   boundaries (helper pool .. helper-index array) but NOT each sub-type's individual byte
//   size, so the per-sub-type sizes are deliberately NOT asserted absolute; only DWARF
//   ordering is pinned. Replace each with its real layout when those Camera TUs land.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
// Forward declarations -- referenced only in pointer/reference positions in the
// declaration-only method set, so no include of their (heavy) homes is needed here.
class ArbitratorState;                 // CheckNoBehavioursAreAllocatedByState / debug owner
class Moment;                          // BehaviourHelper debug owner
class DirectorResourceManager;         // Prepare/Set/GetDirectorResourceManager
struct DebugPrinter;                   // Update/SceneQuery debug printer arg
struct BehaviourSharedInfo;            // UpdateAllBehaviours / PostCollisionUpdate arg
struct ControllerInfo;                 // UpdateAllBehaviours arg
struct CollisionPolicySharedInfo;      // GenerateSceneQueries / ProcessSceneQueryResults arg

namespace Camera
{
    // Forward declarations of the manager + its lock interface, named in method signatures
    // (by ptr/ref) of the helper types declared just below.
    class BehaviourManager;
    class BehaviourControllerLockInterface;
    class BehaviourParameterBank;   // the per-named-behaviour parameter bank (own slice header)

    // ------------------------------------------------------------------------
    // BehaviourHelperIndex -- identifies a slot in a BehaviourManager's helper-index
    // table / the helper ObjectPool. It is an assignable value type modelled as a single
    // index word, with the int conversions the ObjectPool<...,BehaviourHelperIndex> free
    // queue and the Array<BehaviourHelperIndex,28> need (construct-from-int, to-int).
    // ------------------------------------------------------------------------
    class BehaviourHelperIndex
    {
    public:
        BehaviourHelperIndex() : miIndex(-1) {}
        BehaviourHelperIndex(s32 liIndex) : miIndex(liIndex) {}     // ObjectPool free-queue store
        operator s32() const { return miIndex; }                   // ObjectPool/Array index use
        bool operator==(const BehaviourHelperIndex& lrOther) const { return miIndex == lrOther.miIndex; }

        s32 miIndex;
    };

    // ------------------------------------------------------------------------
    // BehaviourInterpolate -- the per-behaviour interpolator that blends one camera source
    // into another. The ICE movie-player allocates two of these through the manager and
    // drives their setup. DECLARATION-ONLY (real bodies land with the BehaviourInterpolate
    // TU). FLAG: minimal slice -- no reconstructed home yet.
    // ------------------------------------------------------------------------
    class BehaviourInterpolate
    {
    public:
        // Per-take interpolation curve/easing parameters. Opaque here; the player only ever
        // holds a pointer to one.
        class Parameters {};

        // 0 == cut / no pause updates, 2 == updates-during-pause.
        void SetInterpolationMode(s32 liMode);
        void SetParameters(const Parameters* lpParams);
        void SetupDuration(f32 lfDuration);
        bool HasFinished() const;
        void SetupCameraAFromCamera(const Camera& lrCamera);
        void SetupCameraAFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager);
        void SetupCameraBFromCamera(const Camera& lrCamera);
        void SetupCameraBFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager);
        void Setup();
        // The camera this interpolator is producing this frame.
        const Camera& GetCamera() const;
    };

    // ------------------------------------------------------------------------
    // BehaviourHandle<TBehaviour> -- a typed handle to a behaviour owned by a
    // BehaviourManager. Five-word layout pinned from the X360 BehaviourHandle::Prepare
    // @0x8224AFF0 / ::Release @0x8222DD00 asm: mbAllocated(+0x00), muAllocationKey(+0x04),
    // muHelperIndex(+0x08), mpManager(+0x0C), mpBehaviour(+0x10). The arbitrator/moment
    // states inline-duplicate this same five-word layout as their own nested types
    // (BrnArbStateRankUp.h etc.); those nested copies are distinct types, left untouched.
    // FLAG: the +0x08 helper word's exact role is not fully recovered (opaque lookup helper).
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    class BehaviourHandle
    {
    public:
        BehaviourHandle()
            : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
              mpManager(0), mpBehaviour(0) {}

        bool IsAllocated() const { return mbAllocated; }
        BehaviourManager* GetManager() const { return mpManager; }
        TBehaviour* GetBehaviour() const { return mpBehaviour; }

        // Allocate this handle onto a freshly-reserved behaviour. X360-attested
        // (BehaviourHandle::Prepare @0x8224AFF0). Always returns true. Body out-of-line below
        // (needs BehaviourManager complete).
        bool Prepare(u32 luAllocationKey, u32 luHelperIndex, BehaviourManager* lpManager);

        // Drop the manager-side hold and clear the handle. X360-attested
        // (BehaviourHandle::Release @0x8222DD00). Always returns true. Body out-of-line below.
        bool Release();

        // Is the behaviour this handle owns still queued for its first Prepare. X360-attested:
        // every BehaviourHandle<TBehaviour>::IsWaitingToPrepare() instantiation is byte-identical
        //   BrnDirector::Camera::BehaviourGyroCam   @0x82212510
        //   BrnDirector::Camera::BehaviourIceAnim   @0x822128A0
        //   BrnDirector::Camera::BehaviourInterpo   @0x82212150
        //   BrnDirector::Camera::BehaviourLooseAt   @0x82212A68
        //   BrnDirector::Camera::BehaviourRoadRun   @0x82212AC8
        //   BrnDirector::Camera::BehaviourRotateA   @0x82212B98
        // (the truncated symbols collapse the per-behaviour-type instantiations of this one
        // template member). Asserts the handle is allocated, then forwards the handle's
        // allocation key to the manager. Body out-of-line below (needs BehaviourManager complete).
        bool IsWaitingToPrepare() const;

        // The boolean negation of IsWaitingToPrepare() -- "has the behaviour finished waiting and
        // is it ready to Prepare". X360-attested BrnDirector::Camera::BehaviourAfterto @0x8222D0E8
        // (the `IsBehaviourWaitingToPrepare(...) == 0` tail). Body out-of-line below.
        bool IsReadyToPrepare() const;

        // ADDITIVE GROW (BrnArbStateTakedown.cpp): the camera this handle's live behaviour
        // produced this frame. X360-attested: every one of the several de-inlined
        // `sub_821FCxxx`/`sub_821FDxxx` accessors this project's arbitrator states call
        // (e.g. @0x821FCD40, @0x821FCEE0, @0x821FD450, @0x821FD780) is byte-identical --
        // assert IsAllocated(), then resolve the manager pool slot through
        // GetBehaviourSlotFromHandle<TBehaviour>(muHelperIndex, muAllocationKey) and return the
        // Camera embedded 16 bytes (console) into that slot (BehaviourHelper::mCamera, the
        // member right after the type-erased pool handle -- see BehaviourHelper below). Modelled
        // here by reusing BehaviourHelper's own accessor via the resolved slot pointer, so the
        // 16-byte offset is never poked directly. Body out-of-line below (needs BehaviourManager
        // complete).
        const Camera& GetProducedCamera() const;

        void Clear()
        {
            mbAllocated     = false;
            muAllocationKey = 0;
            muHelperIndex   = 0;
            mpManager       = 0;
            mpBehaviour     = 0;
        }

        // ADDITIVE GROW (SharedCameraContainer::GetGameplayCameraHelperIndex @0x82219718):
        // the manager-facing BehaviourHelperIndex this handle passes to the manager's
        // BehaviourHelperIndex-taking APIs. X360-attested: the de-inlined per-type accessors
        // (sub_822122F0 / sub_822124A0) assert the handle is allocated then return its +0x04
        // word -- the same word the committed call sites hand to SetBehaviourUpdatesDuringPause
        // / IsBehaviourWaitingToPrepare (see the key-vs-index naming FLAG on those manager
        // members: the DWARF types this word as a BehaviourHelperIndex).
        BehaviourHelperIndex GetBehaviourHelperIndex() const
        {
            CGS_ASSERT(mbAllocated, "IsAllocated()");
            return BehaviourHelperIndex(static_cast<s32>(muAllocationKey));
        }

        // ADDITIVE GROW (SharedCameraContainer::Prepare @0x82263D50): whether the owned
        // behaviour keeps updating while the game is paused. X360-attested handle-level
        // wrapper (BrnBehaviourManager.h:676): assert the handle is allocated ("IsAllocated()",
        // line 676), then forward the handle's +0x04 word to the manager's
        // SetBehaviourUpdatesDuringPause. Body out-of-line below (needs BehaviourManager
        // complete).
        void SetUpdatesDuringPause(bool lbUpdatesDuringPause);

    private:
        bool              mbAllocated;     // +0x00  owns a behaviour
        u32               muAllocationKey; // +0x04  manager-side allocation key
        u32               muHelperIndex;   // +0x08  behaviour-lookup helper (FLAG: role not recovered)
        BehaviourManager* mpManager;       // +0x0C  owning manager
        TBehaviour*       mpBehaviour;     // +0x10  resolved behaviour
    };

    // ------------------------------------------------------------------------
    // BehaviourManager -- the owner of all live camera behaviours and the per-state
    // allocation book-keeping. Full layout from the DWARF (members reconstructed verbatim;
    // see file header). All methods DECLARATION-ONLY.
    // ------------------------------------------------------------------------
    class BehaviourManager
    {
    public:
        // --- nested helper structs (DWARF BrnBehaviourManager.h:241 / :353) -----------

        // One live-behaviour slot held in the manager's ObjectPool. DWARF member shape
        // (BrnBehaviourManager.h:309..:313): the type-erased pool handle, the slot's camera
        // (by value), and the two debug owners.
        struct BehaviourHelper
        {
            // -- methods (DWARF BrnBehaviourManager.h:254..:304) -- DECLARATION-ONLY --
            bool                          Prepare(AbstractPoolVoidHandle lHandle);
            bool                          Update(const BehaviourSharedInfo& lrSharedInfo);
            bool                          PostCollisionUpdate(const BehaviourSharedInfo& lrSharedInfo);
            bool                          Release();
            void                          GenerateSceneQueries(const CollisionPolicySharedInfo& lrSharedInfo);
            void                          ProcessSceneQueryResults(const CollisionPolicySharedInfo& lrSharedInfo);
            void                          SetDebugArbitratorStateOwner(const ArbitratorState* lpOwner);
            void                          SetMomentOwner(const Moment* lpOwner);
            const ArbitratorState*        GetDebugArbitratorStateOwner() const { return mpDebugArbitratorStateOwner; }
            const Moment*                 GetDebugMomentOwner() const { return mpDebugMomentOwner; }
            const Camera&                 GetCamera() const { return mCamera; }
            s32                           GetBehaviourSize() const;
            const char*                   GetDebugFullName(char lacFullNameOut[64]) const;

            // -- layout (DWARF BrnBehaviourManager.h:309..:313) --
            AbstractPoolVoidHandle mBehaviourPoolHandle;          // +0x00  type-erased pool handle (0x10)
            Camera                 mCamera;                       // the slot's camera (by value)
            const ArbitratorState* mpDebugArbitratorStateOwner;   // debug: owning arbitrator state
            const Moment*          mpDebugMomentOwner;            // debug: owning moment
        };

        // The single attached camera-tweaker slot (DWARF BrnBehaviourManager.h:353..:356).
        // FLAG: mTweaker is the committed BrnDirector::Camera::Utils::Tweaker (BrnCameraTweaker.h),
        //   but pulling that home in here collides with the minimal Tweaker fork that
        //   BrnBehaviourIceAnim.h still defines (a separate pre-existing ODR fork, out of scope
        //   for this layout wave). To keep the manager home self-contained and not drag that
        //   collision into every consumer, the embedded tweaker is modelled here as an opaque
        //   FLAGGED sized sub-object (size un-pinned placeholder; interior NEVER fabricated).
        //   Replace with the committed Tweaker by value once the IceAnim Tweaker fork is
        //   reconciled to the committed home.
        struct TweakerHelper
        {
            bool                 mbAttached;            // +0x00  slot occupied
            BehaviourHelperIndex mBehaviourHelperIndex; // the behaviour being tweaked
            u8                   maTweakerOpaque[4];    // FLAG opaque: the live Tweaker (size un-pinned)
        };

        // --- manager API (DWARF BrnBehaviourManager.h:96..:378) -- DECLARATION-ONLY -----

        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

        void PrepareBehaviours(const DirectorResourceManager* lpResourceManager);
        void UpdateAllBehaviours(bool lbPaused, BehaviourSharedInfo& lrSharedInfo,
                                 const ControllerInfo& lrControllerInfo, bool lbArg,
                                 DebugPrinter& lrDebugPrinter);
        void GenerateSceneQueries(bool lbPaused, const CollisionPolicySharedInfo& lrSharedInfo,
                                  DebugPrinter& lrDebugPrinter);
        void ProcessSceneQueryResults(bool lbPaused, const CollisionPolicySharedInfo& lrSharedInfo,
                                      DebugPrinter& lrDebugPrinter);
        void PostCollisionUpdateAllBehaviours(bool lbPaused, BehaviourSharedInfo& lrSharedInfo,
                                              const ControllerInfo& lrControllerInfo, bool lbArg,
                                              DebugPrinter& lrDebugPrinter);
        void ReleaseBehaviours();

        const DirectorResourceManager* GetDirectorResourceManager() const { return mpDirectorResourceManager; }
        void SetDirectorResourceManager(const DirectorResourceManager* lpResourceManager) { mpDirectorResourceManager = lpResourceManager; }

        // X360-attested debug check that the given state holds no behaviours at release time.
        // The committed arbitrator states call this with `this` (an ArbitratorState*).
        void CheckNoBehavioursAreAllocatedByState(ArbitratorState* lpArbitratorState);
        // Overload used by the ICE / arbitrator slice call sites that pass an opaque owner
        // (the manager only uses it as an owner key). Kept so existing `CheckNo...(this)`
        // call sites that pass a non-ArbitratorState `this` still resolve.
        void CheckNoBehavioursAreAllocatedByState(const void* lpState);

        const BrnDirector::AbstractPool<250u, 8u, rw::math::vpu::Vector4>* DebugGetLargePool() const { return &mLargeBehaviourPool; }

        void DetachAllTweakers();
        const Camera& GetCameraFromBehaviour(BehaviourHelperIndex lHelper) const;

        // The per-handle manager-side hold book-keeping. X360-attested:
        //   SetBehaviourUsedByHandle   @0x8224B054 (from BehaviourHandle::Prepare)
        //   UnSetBehaviourUsedByHandle @0x822194B0 (from the arbitrator states' Release)
        // The committed call sites pass the handle's allocation key (a u32); the DWARF names
        // the arg as a BehaviourHelperIndex (which converts from/to s32). Declared with the
        // u32 key signature the consumers use.
        void SetBehaviourUsedByHandle(u32 luAllocationKey);
        void UnSetBehaviourUsedByHandle(u32 luAllocationKey);

        // The per-named-behaviour parameter bank (the DWARF :325 mBehaviourParameterBank
        // sub-object, X360 manager +0x12530). The bank's own layout is un-homed (the member
        // below is a FLAGGED opaque slot), so this accessor is DECLARATION-ONLY -- the body
        // lands when the bank sub-object is homed. X360-attested consumer:
        // SharedCameraContainer::Prepare @0x82263D50 (addis/addi manager+0x12530 then fixed
        // offsets into the bank).
        const BehaviourParameterBank& GetBehaviourParameterBank() const;

        void LockBehaviourForInterpolation(BehaviourHelperIndex lFrom, BehaviourHelperIndex lTo);
        void UnlockBehaviourForInterpolation(BehaviourHelperIndex lFrom, BehaviourHelperIndex lTo);
        void SetBehaviourUpdatesDuringPause(BehaviourHelperIndex lHelper, bool lbUpdatesDuringPause);
        void SetupBehaviourControllerLockInterface(BehaviourControllerLockInterface& lrInterface,
                                                   const BehaviourHandle<BehaviourInterpolate>& lrHandle);

        // X360-attested (BrnBehaviourManager.h:517): is the behaviour a handle owns (identified
        // by its allocation key) still queued for its first Prepare. The committed arbitrator
        // states call this with the handle's u32 allocation key.
        bool IsBehaviourWaitingToPrepare(u32 luAllocationKey) const;

        void DebugDumpToTTY() const;

        // --- consumer-driven convenience API (existing committed call sites) ------------
        // These wrap the manager's allocation entry points in the named forms the committed
        // ICE movie-player and arbitrator states already invoke. DECLARATION-ONLY.

        // Allocate a fresh BehaviourInterpolate into lrHandle (ICE movie-player blend in/out).
        void NewBehaviourInterpolate(BehaviourHandle<BehaviourInterpolate>& lrHandle,
                                     s32 liArgA, s32 liArgB, s32 liArgC);
        // Release the behaviour a handle owns (leaves the handle ready to Clear()).
        void ReleaseBehaviour(BehaviourHandle<BehaviourInterpolate>& lrHandle);
        // Whether a behaviour keeps updating while the game is paused (handle overload).
        void SetBehaviourUpdatesDuringPause(BehaviourHandle<BehaviourInterpolate>& lrHandle,
                                            bool lbUpdatesDuringPause);

        // Resolve the manager-pool slot a handle's (helper word, allocation key) names. Static
        // lookup (the X360 passes the helper word as the first arg, NOT a manager `this`).
        // DECLARATION-ONLY. FLAG: the helper word's role is not fully recovered.
        template <typename TBehaviour>
        static TBehaviour** GetBehaviourSlotFromHandle(u32 luHelperIndex, u32 luAllocationKey);

        // Allocate a fresh TBehaviour into lrHandle, owned by lpOwningState. The
        // BrnDirector::Camera::BehaviourManager::NewBehaviour<TBehaviour> family the arbitrator
        // states drive; generic over the handle type so the states' own 5-word BehaviourHandle<>
        // binds as well as this header's interpolator handle. DECLARATION-ONLY.
        // (3rd param: the arbitrator call sites pass 0 there; MomentHitTraffic::Update
        // @0x82271EA0 passes the owning MOMENT pointer in that register (r6) -- typed
        // const void* so both shapes bind to the one X360 symbol family.)
        template <typename TBehaviour, typename THandle>
        void NewBehaviour(THandle& lrHandle, void* lpOwningState, const void* lpOwner, s32 liArgB);

        // ADDITIVE GROW (BrnArbStateDriveThru::Prepare @0x8226E938): the ATTRIBUTE-TAKING
        // overload of NewBehaviour<TBehaviour>. X360-attested distinct calling convention from
        // the 4-explicit-arg family above (r3=manager, r4=lpAttributeData, r5=&lrHandle,
        // r6=lpOwningState, r7=liArgA, r8=liArgB -- the attribute-data pointer is inserted right
        // after `this`, shifting the rest one register over). Used where TBehaviour is the
        // generic BrnDirector::Camera::Behaviour base (not a concrete behaviour type already
        // pinned at the call site), so the manager needs the attrib block to pick which concrete
        // behaviour to allocate. DECLARATION-ONLY (body lands with the BehaviourManager TU).
        template <typename TBehaviour, typename THandle>
        void NewBehaviour(THandle& lrHandle, const void* lpAttributeData, void* lpOwningState,
                          s32 liArgA, s32 liArgB);

    private:
        void RefCountLogDump(const BehaviourHelperIndex& lrHelper) const;
        void AttachTweaker(BehaviourHelperIndex lHelper);
        void DetachTweaker(BehaviourHelperIndex lHelper);

    private:
        // ===== LAYOUT (DWARF BrnBehaviourManager.h:322..:369, reconstructed verbatim) =====
        // Reused-by-name committed containers (auto-sized on PC). FLAGGED opaque sub-objects
        // for the un-homed embedded Camera-side aggregates (see file header).

        // FLAG: opaque placeholder for an un-homed, un-byte-pinned embedded sub-type. The
        // interior is NEVER fabricated; only the named slot + DWARF ordering are modelled.
        template <int tiTag>
        struct OpaqueSub { u8 maOpaque[4]; /* FLAG: size un-pinned (placeholder) */ };

        // +console 0x...  the two behaviour-storage pools (DWARF :322 / :323).
        BrnDirector::AbstractPool<250u, 8u, rw::math::vpu::Vector4> mLargeBehaviourPool;   // :322
        BrnDirector::AbstractPool<100u, 20u, rw::math::vpu::Vector4> mSmallBehaviourPool;  // :323

        // +console 0xFAB0 (64176)  the live-behaviour helper pool (DWARF :324).
        CgsContainers::ObjectPool<BehaviourHelper, 28, BehaviourHelperIndex> mBehaviourHelperPool; // :324

        // FLAG opaque: the per-named-behaviour parameter bank (DWARF :325). Un-homed heavy
        // cascade; modelled as a named opaque sub-object.
        OpaqueSub<0> mBehaviourParameterBank;                                              // :325

        // The four 28-bit book-keeping sets (DWARF :327..:331). The X360 indexes these as
        // 64-bit fields (CgsBitArray.h:203 assert) -- one u64 field per BitArray<28>.
        CgsContainers::BitArray<28u> mBehaviourNeedsPreparingFlags;                        // :327
        CgsContainers::BitArray<28u> mBehaviourNeedsReleasingFlags;                        // :328
        CgsContainers::BitArray<28u> mBehaviourUpdateDuringPauseFlags;                     // :329
        CgsContainers::BitArray<28u> mBehaviourUsedByHandleFlags;                          // :331

        // Per-behaviour reference counts + the debug ref-count audit log (DWARF :333..:338).
        Array<s32, 28u> mBehaviourRefCounts;                                // :333
        Array<Array<BehaviourHelperIndex, 28u>, 28u>
                                       mDebugBehaviourRefCountIndexLog;                     // :337
        Array<s32, 28u> mDebugBehaviourRefCountLimits;                       // :338

        // FLAG opaque: the camera responders + rotation controllers (DWARF :346..:350). All
        // un-homed heavy Camera aggregates; named opaque sub-objects. The X360 keeps a float
        // time accumulator in this region (console +0x15870, UpdateAllBehaviours), confirming
        // it sits between the helper pool and the helper-index array.
        OpaqueSub<1> mTempCameraBoostResponder;                                            // :346
        OpaqueSub<2> mSpeedResponder;                                                      // :347
        OpaqueSub<3> mRotationController;          // Camera2DRotationController             // :349
        OpaqueSub<4> mSphericalRotationController; // CameraSphericalRotationController      // :350

        // The single attached-tweaker slot (DWARF :359).
        TweakerHelper mTweakerHelper;                                                      // :359

        // +console 0x16348 (90952), count word +0x16458 (91064)  the active helper-index table
        // (DWARF :362). This array drives every iterate-all-behaviours loop in the asm.
        Array<BehaviourHelperIndex, 28u> mBehaviourHelperIndexArray;        // :362

        const DirectorResourceManager* mpDirectorResourceManager;                          // :364
        f32                            mfLastHandbrakeTime;                                // :366
        bool                           mbDebugDisplayAllCameras;                           // :369

        // Out-of-line template body access.
        template <typename> friend class BehaviourHandle;
        friend void _BehaviourManagerAssertLayout();
    };

    // ------------------------------------------------------------------------
    // BehaviourControllerLockInterface -- the per-behaviour lock interface the manager hands
    // to a behaviour so it can lock/unlock an interpolation pair (DWARF BrnBehaviourManager.h:460).
    // ------------------------------------------------------------------------
    class BehaviourControllerLockInterface
    {
    public:
        void LockBehaviourForInterpolation(BehaviourHelperIndex lHelper) const;
        void UnlockBehaviourForInterpolation(BehaviourHelperIndex lHelper) const;

    private:
        void Construct(BehaviourManager* lpManager);
        void SetBehaviourHelperIndex(BehaviourHelperIndex lHelper);

        BehaviourHelperIndex mCurrentBehaviourHelper;   // +0x00  the helper being locked
        BehaviourManager*    mpBehaviourManager;        // the owning manager

        friend class BehaviourManager;
    };

    // ------------------------------------------------------------------------
    // BehaviourHandle::Prepare @0x8224AFF0 -- bind this handle onto a freshly-reserved
    // behaviour. Defined out-of-line here, where BehaviourManager is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline bool BehaviourHandle<TBehaviour>::Prepare(u32 luAllocationKey, u32 luHelperIndex,
                                                     BehaviourManager* lpManager)
    {
        if (mbAllocated)
        {
            mpManager->UnSetBehaviourUsedByHandle(muAllocationKey);
            muHelperIndex = 0;
            mpManager     = 0;
            mpBehaviour   = 0;
            mbAllocated   = false;
        }

        muAllocationKey = luAllocationKey;
        muHelperIndex   = luHelperIndex;
        mpManager       = lpManager;
        mbAllocated     = true;

        mpManager->SetBehaviourUsedByHandle(muAllocationKey);

        CGS_ASSERT(mbAllocated, "IsAllocated()");

        mpBehaviour =
            *BehaviourManager::GetBehaviourSlotFromHandle<TBehaviour>(muHelperIndex, muAllocationKey);
        return true;
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::Release @0x8222DD00 -- drop the manager-side hold and clear the handle.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline bool BehaviourHandle<TBehaviour>::Release()
    {
        if (mbAllocated)
        {
            mpManager->UnSetBehaviourUsedByHandle(muAllocationKey);
            muHelperIndex = 0;
            mpManager     = 0;
            mpBehaviour   = 0;
            mbAllocated   = false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::SetUpdatesDuringPause (SharedCameraContainer::Prepare @0x82263D50,
    // inlined) -- assert the handle is allocated (BrnBehaviourManager.h:676), then forward
    // the handle's +0x04 word and the flag to the owning manager's
    // SetBehaviourUpdatesDuringPause (X360: lwz r3,0xC(handle); lwz r4,4(handle); li r5,flag).
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline void BehaviourHandle<TBehaviour>::SetUpdatesDuringPause(bool lbUpdatesDuringPause)
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        mpManager->SetBehaviourUpdatesDuringPause(BehaviourHelperIndex(static_cast<s32>(muAllocationKey)),
                                                  lbUpdatesDuringPause);
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::IsWaitingToPrepare @0x82212510 (and its byte-identical siblings) --
    // is the owned behaviour still queued for its first Prepare. The X360 asm asserts the
    // handle is allocated (assert text "mbIsAllocated", BrnBehaviourManager.h:517), then loads
    // muAllocationKey (+0x04) and mpManager (+0x0C) and tail-calls the manager's
    // IsBehaviourWaitingToPrepare(u32). The dynamic gpcMessageBuffer assert is folded to
    // CGS_ASSERT per the project rule; the manager query side effect is preserved exactly.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline bool BehaviourHandle<TBehaviour>::IsWaitingToPrepare() const
    {
        CGS_ASSERT(mbAllocated, "mbIsAllocated");
        return mpManager->IsBehaviourWaitingToPrepare(muAllocationKey);
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::IsReadyToPrepare @0x8222D0E8 (BrnDirector::Camera::BehaviourAfterto) --
    // the boolean negation of IsWaitingToPrepare(): same allocated assert, then returns
    // `IsBehaviourWaitingToPrepare(...) == 0` (the asm's clrlwi/cntlzw/extrwi boolean tail).
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline bool BehaviourHandle<TBehaviour>::IsReadyToPrepare() const
    {
        CGS_ASSERT(mbAllocated, "mbIsAllocated");
        return mpManager->IsBehaviourWaitingToPrepare(muAllocationKey) == false;
    }

    // BehaviourHandle::GetProducedCamera declaration (see the class body above): DECLARATION-
    // ONLY, no out-of-line definition here. X360-attested (e.g. @0x821FCD40 / @0x821FCEE0 /
    // @0x821FD450 / @0x821FD780) to assert IsAllocated() then resolve the manager pool slot this
    // handle names and return the Camera embedded 16 bytes (console) into it
    // (BehaviourHelper::mCamera, right after the type-erased pool handle). Reconstructing the
    // body faithfully needs GetBehaviourSlotFromHandle<TBehaviour>'s own pointer arithmetic back
    // to its owning BehaviourHelper slot, and GetBehaviourSlotFromHandle is ITSELF declaration-
    // only (its real body -- how the (helper index, allocation key) pair resolves into the pool
    // -- lands with the BehaviourManager TU); deriving GetProducedCamera's body without it would
    // mean fabricating that resolution rather than reusing a named one. Left declaration-only;
    // BrnArbStateTakedown.cpp (the first consumer) only needs the declaration to compile against
    // under the per-TU cl /c gate.

    // ------------------------------------------------------------------------
    // _BehaviourManagerAssertLayout -- NEVER CALLED. Pins the DWARF member ordering. The DWARF
    // offsets are CONSOLE (4-byte-pointer); the PC rebuild widens pointers, so we assert
    // pointer-invariant RELATIVE ORDERING (offsetof comparisons in DWARF order), NOT the
    // absolute console bytes. Any future edit that reorders a member trips a static_assert.
    // ------------------------------------------------------------------------
    inline void _BehaviourManagerAssertLayout()
    {
        typedef BehaviourManager BM;

        // DWARF member order (BrnBehaviourManager.h:322 -> :369), pinned as strictly-increasing
        // offsets. Relative ordering is pointer-invariant under LLP64 widening.
        static_assert(offsetof(BM, mLargeBehaviourPool)            <  offsetof(BM, mSmallBehaviourPool),            "BM order: large<small pool");
        static_assert(offsetof(BM, mSmallBehaviourPool)            <  offsetof(BM, mBehaviourHelperPool),          "BM order: small pool<helper pool");
        static_assert(offsetof(BM, mBehaviourHelperPool)          <  offsetof(BM, mBehaviourParameterBank),       "BM order: helper pool<param bank");
        static_assert(offsetof(BM, mBehaviourParameterBank)       <  offsetof(BM, mBehaviourNeedsPreparingFlags), "BM order: param bank<prep flags");
        static_assert(offsetof(BM, mBehaviourNeedsPreparingFlags) <  offsetof(BM, mBehaviourNeedsReleasingFlags), "BM order: prep<release flags");
        static_assert(offsetof(BM, mBehaviourNeedsReleasingFlags) <  offsetof(BM, mBehaviourUpdateDuringPauseFlags), "BM order: release<pause flags");
        static_assert(offsetof(BM, mBehaviourUpdateDuringPauseFlags) < offsetof(BM, mBehaviourUsedByHandleFlags), "BM order: pause<used flags");
        static_assert(offsetof(BM, mBehaviourUsedByHandleFlags)   <  offsetof(BM, mBehaviourRefCounts),           "BM order: used flags<ref counts");
        static_assert(offsetof(BM, mBehaviourRefCounts)           <  offsetof(BM, mDebugBehaviourRefCountIndexLog), "BM order: ref counts<index log");
        static_assert(offsetof(BM, mDebugBehaviourRefCountIndexLog) < offsetof(BM, mDebugBehaviourRefCountLimits), "BM order: index log<ref limits");
        static_assert(offsetof(BM, mDebugBehaviourRefCountLimits) <  offsetof(BM, mTempCameraBoostResponder),     "BM order: ref limits<boost responder");
        static_assert(offsetof(BM, mTempCameraBoostResponder)     <  offsetof(BM, mSpeedResponder),               "BM order: boost<speed responder");
        static_assert(offsetof(BM, mSpeedResponder)               <  offsetof(BM, mRotationController),           "BM order: speed responder<2D rot");
        static_assert(offsetof(BM, mRotationController)           <  offsetof(BM, mSphericalRotationController),  "BM order: 2D rot<spherical rot");
        static_assert(offsetof(BM, mSphericalRotationController)  <  offsetof(BM, mTweakerHelper),                "BM order: spherical rot<tweaker helper");
        static_assert(offsetof(BM, mTweakerHelper)                <  offsetof(BM, mBehaviourHelperIndexArray),    "BM order: tweaker helper<helper index array");
        static_assert(offsetof(BM, mBehaviourHelperIndexArray)    <  offsetof(BM, mpDirectorResourceManager),     "BM order: helper index array<resource mgr");
        static_assert(offsetof(BM, mpDirectorResourceManager)     <  offsetof(BM, mfLastHandbrakeTime),           "BM order: resource mgr<handbrake time");
        static_assert(offsetof(BM, mfLastHandbrakeTime)           <  offsetof(BM, mbDebugDisplayAllCameras),      "BM order: handbrake time<display-all flag");

        // The reused-by-name helper pool stores BehaviourHelper by value: its element type must
        // begin with the type-erased pool handle (DWARF :309), the anchor every pool body reads.
        static_assert(offsetof(BM::BehaviourHelper, mBehaviourPoolHandle) == 0, "BehaviourHelper: pool handle at +0");
        static_assert(offsetof(BM::BehaviourHelper, mpDebugArbitratorStateOwner) < offsetof(BM::BehaviourHelper, mpDebugMomentOwner),
                      "BehaviourHelper: arb-state owner before moment owner");
    }
}
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_MANAGER_H
