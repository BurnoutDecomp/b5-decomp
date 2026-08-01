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
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"     // ValidityAccount (per-frame mask reset)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"         // Camera::Behaviour -- the pooled objects'
                                                                     //   BASE: every helper dispatch goes
                                                                     //   through its vtable

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
// (BehaviourSharedInfo / BehaviourSharedPrepareReleaseInfo now come from the canonical
//  Camera/Behaviours/Behaviour.h included above -- they live in BrnDirector::Camera, not
//  BrnDirector, so the old BrnDirector-scope forward declaration here was a DIFFERENT
//  type that could never have bound to the real one.)
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
    // ⛔ RETIRED 2026-08-01 -- THE BehaviourInterpolate SLICE THAT USED TO LIVE HERE IS GONE.
    //
    // It was a second, mutually-exclusive definition of BrnDirector::Camera::BehaviourInterpolate
    // (no base, no members, sizeof == 1, 11 declaration-only methods) and -- because this
    // header is the one every arbitrator state includes -- it was the definition the whole
    // tree actually saw, while the REAL home (Behaviours/BrnBehaviourInterpolate.h) sat
    // unreachable behind a C2011.
    //
    // The slice could not be bodied at all: with no members there was nothing for
    // SetParameters / SetupDuration / Setup / SetupCameraA|BFromHelper / HasFinished to write,
    // so six symbols stayed permanently unresolved. It was also the reason
    // BrnBehaviourManager.cpp's `AllocateBehaviour<BehaviourInterpolate>` booked a 1600-byte
    // pool bucket and placement-new'd a ONE-BYTE object into it.
    //
    // Consumers that need the complete type now include the real home directly. A forward
    // declaration is enough here: this header only ever names the class through the
    // pointer member of BehaviourHandle<T>.
    // ------------------------------------------------------------------------
    class BehaviourInterpolate;

    // BehaviourHandle<TBehaviour> is DEFINED below BehaviourManager (it names the manager's
    // nested HelperPool / BehaviourHelper by value). Forward-declared here so the manager's
    // own signatures can take it by reference.
    template <typename TBehaviour> class BehaviourHandle;

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
            Camera&                       GetCamera()       { return mCamera; }

            // The pooled behaviour this slot owns. X360: every dispatch site loads the
            // helper's FIRST word (`lwz r3, 0(helper)`) -- that word is
            // mBehaviourPoolHandle.mpObject, the AbstractPool slot the behaviour was
            // constructed into. Named so no consumer reads the helper head as a pointer.
            Behaviour*                    GetBehaviour()       { return static_cast<Behaviour*>(mBehaviourPoolHandle.Get()); }
            const Behaviour*              GetBehaviour() const { return static_cast<const Behaviour*>(mBehaviourPoolHandle.Get()); }

            // The type-erased pool handle itself (ReleaseBehaviours hands the slot back
            // through it).
            AbstractPoolVoidHandle&       GetPoolHandle()      { return mBehaviourPoolHandle; }

            s32                           GetBehaviourSize() const;
            const char*                   GetDebugFullName(char lacFullNameOut[64]) const;

            // -- layout (DWARF BrnBehaviourManager.h:309..:313) --
            AbstractPoolVoidHandle mBehaviourPoolHandle;          // +0x00  type-erased pool handle (0x10)
            Camera                 mCamera;                       // the slot's camera (by value)
            const ArbitratorState* mpDebugArbitratorStateOwner;   // debug: owning arbitrator state
            const Moment*          mpDebugMomentOwner;            // debug: owning moment
        };

        // The 28-slot live-behaviour pool type. Named because a BehaviourHandle stores a
        // POINTER to its owning manager's pool (see BehaviourHandle::mpHelperPool).
        typedef CgsContainers::ObjectPool<BehaviourHelper, 28, BehaviourHelperIndex> HelperPool;

        // The number of behaviour slots (the bound every BitArray<28> / Array<...,28> and
        // every CgsBitArray.h:203 index assert in this class uses).
        enum { KI_MAX_BEHAVIOURS = 28 };

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

        // RETIRED (2026-07-29): `GetBehaviourSlotFromHandle<T>(u32, u32) -> T**` used to sit
        // here as a declaration-only stand-in for a resolution nobody could write. It is now
        // BehaviourHandle::GetHelper() + BehaviourHelper::GetBehaviour(): the console's
        // `BrnDirec(handle[+8], handle[+4])` is an ObjectPool::operator[] on the manager's
        // helper pool, and the `T**` was the helper's FIRST WORD taken by address -- i.e.
        // mBehaviourPoolHandle.mpObject. No pointer-to-pointer fiction is needed.

        // Reserve a slot for a fresh TBehaviour from the size-appropriate behaviour pool and
        // hand back the four-word type-erased pool handle. X360-attested template family
        // (BehaviourManager::AllocateBehaviour<TBehaviour> @0x82263370 and its 19 siblings): the
        // compiler bakes the pool choice per instantiation from sizeof(TBehaviour) -- a behaviour
        // that fits the small pool's bucket (<=100 Vector4 == 1600 bytes) shares the many-slot
        // small pool (mSmallBehaviourPool), otherwise it takes one of the few large slots
        // (mLargeBehaviourPool). Asserts the chosen pool has a free slot (dumping the manager's
        // behaviour table first when exhausted), then returns pool.AllocateVoid<TBehaviour>().
        // Out-of-line template body below (needs the pool members complete). The concrete
        // instantiations are emitted by BrnBehaviourManager.cpp (one per behaviour type).
        template <typename TBehaviour>
        AbstractPoolVoidHandle AllocateBehaviour();

        // Allocate a fresh TBehaviour into lrHandle, owned by lpOwningState. The
        // BrnDirector::Camera::BehaviourManager::NewBehaviour<TBehaviour> family the arbitrator
        // states drive; generic over the handle type so the states' own 5-word BehaviourHandle<>
        // binds as well as this header's interpolator handle. DECLARATION-ONLY.
        // (3rd param: the arbitrator call sites pass 0 there; MomentHitTraffic::Update
        // @0x82271EA0 passes the owning MOMENT pointer in that register (r6) -- typed
        // const void* so both shapes bind to the one X360 symbol family.)
        template <typename TBehaviour, typename THandle>
        void NewBehaviour(THandle& lrHandle, void* lpOwningState, const void* lpOwner, s32 liArgB);

        // ⭐ THE BODIED OVERLOAD (X360 BehaviourManager::NewBehaviour<TBehaviour> --
        // @0x822580F8 for BehaviourRoadRunner and its 15 byte-identical siblings). Selected by
        // overload resolution whenever the handle is the SHARED BehaviourHandle<TBehaviour>
        // above; the generic THandle form (declaration-only, unchanged) still binds the
        // arbitrator states' own nested five-word handle copies, so nothing regresses.
        // Body out-of-line below, where BehaviourHelper / the pools are complete.
        template <typename TBehaviour>
        void NewBehaviour(BehaviourHandle<TBehaviour>& lrHandle, void* lpOwningState,
                          const void* lpOwner, s32 liRefLimit);

        // ADDITIVE GROW (BrnArbStateDriveThru::Prepare @0x8226E938): the ATTRIBUTE-TAKING
        // overload of NewBehaviour<TBehaviour>. X360-attested distinct calling convention from
        // the 4-explicit-arg family above (r3=manager, r4=lpAttributeData, r5=&lrHandle,
        // r6=lpOwningState, r7=liArgA, r8=liArgB -- the attribute-data pointer is inserted right
        // after `this`, shifting the rest one register over). Used where TBehaviour is the
        // generic BrnDirector::Camera::Behaviour base (not a concrete behaviour type already
        // pinned at the call site), so the manager needs the attrib block to pick which concrete
        // behaviour to allocate. DECLARATION-ONLY (body lands with the BehaviourManager TU).
        // (RETYPED 2026-07: the r7 arg was modelled `s32 liArgA`, but
        // MomentHardStop::Update @0x82271438 passes the owning MOMENT pointer there --
        // the same owner slot as the 4-arg family's `const void* lpOwner`.)
        template <typename TBehaviour, typename THandle>
        void NewBehaviour(THandle& lrHandle, const void* lpAttributeData, void* lpOwningState,
                          const void* lpOwner, s32 liArgB);

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
        HelperPool mBehaviourHelperPool;                                                   // :324

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
    // BehaviourHandle<TBehaviour> -- a typed handle to a behaviour owned by a
    // BehaviourManager. Five-word layout pinned from the X360 BehaviourHandle::Prepare
    // @0x8224AFF0 / ::Release @0x8222DD00 asm: mbAllocated(+0x00), muAllocationKey(+0x04),
    // mpHelperPool(+0x08), mpManager(+0x0C), mpBehaviour(+0x10). The arbitrator/moment
    // states inline-duplicate this same five-word layout as their own nested types
    // (BrnArbStateRankUp.h etc.); those nested copies are distinct types, left untouched.
    //
    // ⭐ THE +0x08 WORD IS NOW IDENTIFIED, and it was BUG CLASS (a) -- a 4-byte serialized
    // slot holding a POINTER. NewBehaviour<BehaviourRoadRunner> @0x822580F8 calls the
    // handle's Prepare as
    //     Prepare(r4 = lHelperID, r5 = manager + 64176, r6 = manager)
    // where `manager + 64176` is &mBehaviourHelperPool, and every later resolution is
    //     BrnDirec( handle[+0x08], handle[+0x04] )   == mBehaviourHelperPool[lHelperID]
    // i.e. an ObjectPool::operator[] with the pool taken from the handle. So +0x04 is the
    // BehaviourHelperIndex (the manager APIs' "allocation key" IS the helper index) and
    // +0x08 is the owning pool POINTER. Modelled as a real typed pointer here -- on x64 a
    // u32 would have truncated it.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    class BehaviourHandle
    {
    public:
        BehaviourHandle()
            : mbAllocated(false), muAllocationKey(0), mpHelperPool(0),
              mpManager(0), mpBehaviour(0) {}

        bool IsAllocated() const { return mbAllocated; }
        BehaviourManager* GetManager() const { return mpManager; }
        TBehaviour* GetBehaviour() const { return mpBehaviour; }

        // Allocate this handle onto a freshly-reserved behaviour. X360-attested
        // (BehaviourHandle::Prepare @0x8224AFF0). Always returns true. Body out-of-line below
        // (needs BehaviourManager complete).
        bool Prepare(BehaviourHelperIndex lHelperIndex,
                     BehaviourManager::HelperPool* lpHelperPool,
                     BehaviourManager* lpManager);

        // The pool slot this handle names. X360 `BrnDirec(handle[+8], handle[+4])`.
        BehaviourManager::BehaviourHelper& GetHelper() const;

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
            mpHelperPool    = 0;
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

        // ADDITIVE GROW (the BehaviourHandle<BehaviourRig> instantiation TU): attach /
        // detach the manager's authoring tweaker to the behaviour this handle owns.
        // X360-attested handle-level wrappers (BrnBehaviourManager.h:623 / :633 --
        // BehaviourHandle<BehaviourRig>::AttachTweaker @0x82212608, DetachTweaker
        // @0x82212668): assert allocated, then forward the +0x04 word to the manager.
        // The X360 calls the manager's AttachTweaker out-of-line but INLINES
        // DetachTweaker's internals (the mTweakerHelper current-index compare @+0x158E4
        // + lock-byte clear @+0x158E0 + helper-pool (+0xFAB0) slot flag-byte clear at
        // slot +0xA) -- expressed here as the declared manager call. Bodies out-of-line
        // below (need BehaviourManager complete).
        void AttachTweaker();
        void DetachTweaker();

    private:
        bool                          mbAllocated;     // +0x00  owns a behaviour
        u32                           muAllocationKey; // +0x04  the BehaviourHelperIndex
        BehaviourManager::HelperPool* mpHelperPool;    // +0x08  the owning manager's helper pool
        BehaviourManager*             mpManager;       // +0x0C  owning manager
        TBehaviour*                   mpBehaviour;     // +0x10  resolved behaviour

        friend class BehaviourManager;
    };

    // ------------------------------------------------------------------------
    // BehaviourManager::AllocateBehaviour<TBehaviour> @0x82263370 (and its 19 per-behaviour-type
    // siblings) -- reserve a pool slot for a fresh TBehaviour and return the four-word type-erased
    // handle. ONE shared body; the compiler folds the sizeof compare to a single pool at each
    // instantiation, exactly reproducing each sibling's asm (one pool access, one assert message):
    //   * fits the small bucket  (sizeof(TBehaviour) <= 1600) -> mSmallBehaviourPool, "small
    //     behaviour" out-of-slots message (BrnBehaviourManager.h:1137 on the X360);
    //   * otherwise               (sizeof up to the large bucket 4000)  -> mLargeBehaviourPool,
    //     "large behaviour" message (:1148).
    // The out-of-slots pre-check reads the pool's free counter, DebugDumpToTTY()s the manager, then
    // trips the non-gating assert (the X360 falls through into AllocateVoid regardless, matching
    // CGS_ASSERT). AbstractPool::AllocateVoid<TBehaviour> pops a slot, constructs the behaviour in
    // it, and returns the (object, free-interface, slot, size) handle.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour()
    {
        // Compile-time pool selection: a behaviour that fits the small pool's bucket shares the
        // many-slot small pool; anything larger takes one of the few large slots. The X360 bakes
        // this to a single pool per instantiation (the dead branch is folded away).
        if (sizeof(TBehaviour) <= sizeof(decltype(mSmallBehaviourPool)::Bucket))
        {
            const s32 liNumFree = mSmallBehaviourPool.GetNumFreeObjects();
            if (liNumFree <= 0)
            {
                DebugDumpToTTY();
                CGS_ASSERT(liNumFree > 0, "Ran out of slots when trying to allocate a small behaviour");
            }
            return mSmallBehaviourPool.AllocateVoid<TBehaviour>();
        }
        else
        {
            const s32 liNumFree = mLargeBehaviourPool.GetNumFreeObjects();
            if (liNumFree <= 0)
            {
                DebugDumpToTTY();
                CGS_ASSERT(liNumFree > 0, "Ran out of slots when trying to allocate a large behaviour");
            }
            return mLargeBehaviourPool.AllocateVoid<TBehaviour>();
        }
    }

    // ------------------------------------------------------------------------
    // BehaviourControllerLockInterface -- the per-behaviour lock interface the manager hands
    // to a behaviour so it can lock/unlock an interpolation pair (DWARF BrnBehaviourManager.h:460).
    //
    // ⭐ THE TWO PUBLIC ACCESSORS ARE BODIED HERE, AS HEADER INLINES (2026-08-01). Neither has
    // a standalone symbol in BURNOUT_X360_ARTIST.XEX and neither carries an assert of its own;
    // every call site expands them in place, so the expansion IS the definition. The one at
    // CameraReference::Prepare @0x82252484 is the whole function:
    //     lwz r5, 0x164(reference)  ; the CameraReference's mBehaviourHelperIndex  -> arg 2
    //     lwz r4, 0(interface)      ; interface +0x00 == mCurrentBehaviourHelper   -> arg 1
    //     lwz r3, 4(interface)      ; interface +0x04 == mpBehaviourManager        -> this
    //     bl  BehaviourManager::LockBehaviourForInterpolation
    // and CameraReference::Release @0x8225252C is the identical shape into Unlock.
    //
    // That expansion pins THREE things at once, none of which were previously attested:
    //   * the two member offsets modelled below (+0x00 helper index, +0x04 manager),
    //   * the ARGUMENT ORDER of the manager's two-argument pair -- the interface's OWN helper
    //     (the behaviour doing the locking) is lFrom, the passed-in helper is lTo; the
    //     manager's assert messages (" is trying to lock ") confirm it from the other side,
    //   * that the manager's pair is what these forward to (there is no third function).
    // ------------------------------------------------------------------------
    class BehaviourControllerLockInterface
    {
    public:
        // The interpolating behaviour (mCurrentBehaviourHelper, stamped by the manager before
        // each dispatch) takes / drops a lock on lHelper's behaviour so the pool cannot recycle
        // it while it is being interpolated from or to.
        void LockBehaviourForInterpolation(BehaviourHelperIndex lHelper) const
        {
            mpBehaviourManager->LockBehaviourForInterpolation(mCurrentBehaviourHelper, lHelper);
        }

        void UnlockBehaviourForInterpolation(BehaviourHelperIndex lHelper) const
        {
            mpBehaviourManager->UnlockBehaviourForInterpolation(mCurrentBehaviourHelper, lHelper);
        }

    private:
        void Construct(BehaviourManager* lpManager);
        void SetBehaviourHelperIndex(BehaviourHelperIndex lHelper);

        BehaviourHelperIndex mCurrentBehaviourHelper;   // +0x00  the helper being locked
        BehaviourManager*    mpBehaviourManager;        // the owning manager

        friend class BehaviourManager;
    };

    // ------------------------------------------------------------------------
    // ⭐ BehaviourManager::NewBehaviour<TBehaviour> @0x822580F8 -- allocate a fresh behaviour
    // into lrHandle and hand it to the pool helper that will drive it.
    //
    // This is the function the whole director camera stack was waiting on: it is the ONLY
    // path by which a Camera::Behaviour ever comes into existence, and it could not be
    // written until the Behaviour base was homed, because it dispatches that base's vtable
    // (through BehaviourHelper::Prepare) and stores the two owners into the helper interior.
    //
    // X360 shape (line numbers are this header's own, quoted by the console's asserts):
    //     lHelperID = mBehaviourHelperPool.AllocateObject();
    //     assert(lHelperID >= 0);                                              // :782
    //     assert(liRefLimit >= 0);                                             // :784
    //     assert(!mBehaviourNeedsPreparingFlags.IsBitSet(lHelperID));          // :786
    //     assert(!mBehaviourUsedByHandleFlags.IsBitSet(lHelperID));            // :788
    //     assert(mBehaviourRefCounts[lHelperID] == 0);                         // :789
    //     mBehaviourNeedsPreparingFlags.SetBit(lHelperID);                     // manager +84656
    //     mBehaviourUpdateDuringPauseFlags.UnSetBit(lHelperID);                // manager +84672
    //     mBehaviourHelperIndexArray.Append(lHelperID);                        // manager +90952
    //     mDebugBehaviourRefCountLimits[lHelperID] = liRefLimit;               // manager +88056
    //     mDebugBehaviourRefCountIndexLog[lHelperID].Clear();                  // count word +112
    //     lrHelper = mBehaviourHelperPool[lHelperID];
    //     assert(lrHelper.Prepare(AllocateBehaviour<BehaviourClass>()));       // :810
    //     lrHandle.Prepare(lHelperID, &mBehaviourHelperPool, this);
    //     assert(lrHandle.IsAllocated());                                      // :654
    //     lrHandle.GetHelper().mpDebugArbitratorStateOwner = lpOwningState;    // helper +0x170
    //     assert(lrHandle.IsAllocated());                                      // :665
    //     lrHandle.GetHelper().mpDebugMomentOwner          = lpOwner;          // helper +0x174
    //
    // The two trailing stores go through a RE-RESOLUTION of the helper from the handle in the
    // console (`BrnDirec(*(a2+8), *(a2+4))`), which is what identified the handle's +0x08 word
    // as the pool pointer. Reproduced with the same shape (GetHelper()) rather than reusing
    // the local reference, so the dependency stays visible.
    //
    // NOTE: the third argument is typed `void*` by the declaration above because the console
    // symbol is shared between the arbitrator states (which pass `this`, an ArbitratorState*)
    // and the moments (which pass 0 there and the Moment* in the fourth slot).
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline void BehaviourManager::NewBehaviour(BehaviourHandle<TBehaviour>& lrHandle,
                                               void* lpOwningState, const void* lpOwner,
                                               s32 liRefLimit)
    {
        const BehaviourHelperIndex lHelperID = mBehaviourHelperPool.AllocateObject();

        CGS_ASSERT(static_cast<s32>(lHelperID) >= 0, "lHelperID >= 0");                 // :782
        CGS_ASSERT(liRefLimit >= 0, "liRefLimit >= 0");                                 // :784
        CGS_ASSERT(!mBehaviourNeedsPreparingFlags.IsBitSet(static_cast<u32>(lHelperID)),
                   "!mBehaviourNeedsPreparingFlags.IsBitSet(lHelperID)");               // :786
        CGS_ASSERT(!mBehaviourUsedByHandleFlags.IsBitSet(static_cast<u32>(lHelperID)),
                   "!mBehaviourUsedByHandleFlags.IsBitSet(lHelperID)");                 // :788
        CGS_ASSERT(mBehaviourRefCounts[static_cast<u32>(lHelperID)] == 0,
                   "mBehaviourRefCounts[lHelperID] == 0");                              // :789

        mBehaviourNeedsPreparingFlags.SetBit(static_cast<u32>(lHelperID));
        mBehaviourUpdateDuringPauseFlags.UnSetBit(static_cast<u32>(lHelperID));

        mBehaviourHelperIndexArray.Append(lHelperID);

        mDebugBehaviourRefCountLimits[static_cast<u32>(lHelperID)]   = liRefLimit;
        mDebugBehaviourRefCountIndexLog[static_cast<u32>(lHelperID)].Clear();

        BehaviourHelper& lrHelper = mBehaviourHelperPool[lHelperID];
        const bool lbHelperPrepared = lrHelper.Prepare(AllocateBehaviour<TBehaviour>());
        CGS_ASSERT(lbHelperPrepared,
                   "lrHelper.Prepare(AllocateBehaviour<BehaviourClass>())");            // :810
        (void)lbHelperPrepared;

        lrHandle.Prepare(lHelperID, &mBehaviourHelperPool, this);

        CGS_ASSERT(lrHandle.IsAllocated(), "IsAllocated()");                            // :654
        lrHandle.GetHelper().SetDebugArbitratorStateOwner(
            static_cast<const ArbitratorState*>(lpOwningState));

        CGS_ASSERT(lrHandle.IsAllocated(), "IsAllocated()");                            // :665
        lrHandle.GetHelper().SetMomentOwner(static_cast<const Moment*>(lpOwner));
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::GetHelper -- the pool slot this handle names. X360
    // `BrnDirec(handle[+0x08], handle[+0x04])` == ObjectPool::operator[] on the manager's
    // helper pool with the handle's helper index.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline BehaviourManager::BehaviourHelper& BehaviourHandle<TBehaviour>::GetHelper() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return (*mpHelperPool)[BehaviourHelperIndex(static_cast<s32>(muAllocationKey))];
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::Prepare @0x8224AFF0 -- bind this handle onto a freshly-reserved
    // behaviour. Defined out-of-line here, where BehaviourManager is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline bool BehaviourHandle<TBehaviour>::Prepare(BehaviourHelperIndex lHelperIndex,
                                                     BehaviourManager::HelperPool* lpHelperPool,
                                                     BehaviourManager* lpManager)
    {
        if (mbAllocated)
        {
            mpManager->UnSetBehaviourUsedByHandle(muAllocationKey);
            mpHelperPool  = 0;
            mpManager     = 0;
            mpBehaviour   = 0;
            mbAllocated   = false;
        }

        muAllocationKey = static_cast<u32>(static_cast<s32>(lHelperIndex));
        mpHelperPool    = lpHelperPool;
        mpManager       = lpManager;
        mbAllocated     = true;

        mpManager->SetBehaviourUsedByHandle(muAllocationKey);

        CGS_ASSERT(mbAllocated, "IsAllocated()");

        // The console's `mpBehaviour = *GetBehaviourSlotFromHandle<T>(pool, index)` -- the
        // dereference is the helper's FIRST word, i.e. the pooled object. Taken from the
        // type-erased pool handle (a plain `void*` on both sides) rather than through the
        // helper's Behaviour* accessor, because most behaviour classes have not been re-based
        // onto the canonical Camera::Behaviour yet (see the NewBehaviour<> instantiation note
        // in the .cpp) and a Behaviour* -> TBehaviour* downcast would not compile for them.
        mpBehaviour = static_cast<TBehaviour*>(GetHelper().GetPoolHandle().Get());
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
            mpHelperPool  = 0;
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
    // BehaviourHandle::AttachTweaker @0x82212608 / DetachTweaker @0x82212668 (the
    // BehaviourHandle<BehaviourRig> instantiation; BrnBehaviourManager.h:623/:633) --
    // assert allocated, then forward the handle's allocation key to the manager's
    // tweaker attach/detach. (The X360 inlines DetachTweaker's manager internals into
    // the handle body; the declared manager call is the named-helper de-opt.)
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline void BehaviourHandle<TBehaviour>::AttachTweaker()
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");   // :623
        mpManager->AttachTweaker(BehaviourHelperIndex(static_cast<s32>(muAllocationKey)));
    }

    template <typename TBehaviour>
    inline void BehaviourHandle<TBehaviour>::DetachTweaker()
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");   // :633
        mpManager->DetachTweaker(BehaviourHelperIndex(static_cast<s32>(muAllocationKey)));
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

    // ------------------------------------------------------------------------
    // GetProducedCamera -- BODIED (was declaration-only). X360-attested
    // (@0x821FCD40 / @0x821FCEE0 / @0x821FD450 / @0x821FD780, all byte-identical): assert
    // IsAllocated(), resolve the manager pool slot the handle names, and return the Camera
    // embedded 16 bytes (console) into it -- BehaviourHelper::mCamera, the member right after
    // the type-erased pool handle. The resolution that used to be missing is GetHelper()
    // above; nothing is fabricated any more.
    // This is the accessor every arbitrator state uses to copy its behaviour's camera into
    // its own each frame (`lrCamera = mHandle.GetProducedCamera()`), i.e. the last link
    // between a live behaviour and the published director camera.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    inline const Camera& BehaviourHandle<TBehaviour>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return GetHelper().GetCamera();
    }

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
