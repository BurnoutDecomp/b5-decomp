#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector2 (rw::math::vpu)
#include "GameSource/BurnoutConstants.h"                                 // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsBitArray.h"               // CgsContainers::BitArray<8>
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"               // BrnDirector::Timestep (by value)
#include "GameSource/Director/Utils/BrnVehicleRef.h"                     // BrnDirector::VehicleRef (base of the nested ref)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"         // AllVehicleData -- VehicleRef::IsValid's
                                                                         //   "world" (the used-race-car bit array
                                                                         //   + the player race-car index)
#include "GameSource/Director/Camera/Camera.h"                           // BrnDirector::Camera::Camera (Update/Fail arg)
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"         // ValidityAccount (the flag enums Fail takes)
#include "GameSource/Director/Camera/Utils/BrnCamera2DRotationController.h"        // mRotationController (by value)
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // mSphericalRotationController
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                     // Camera::VehicleInfo (mPlayerInfo, by value)

// ============================================================================
// GameSource/Director/Camera/Behaviours/Behaviour.h
//
// CANONICAL HOME for BrnDirector::Camera::Behaviour -- the abstract base every director
// camera behaviour derives from -- and for the two shared-info blocks its virtuals consume
// (BehaviourSharedInfo / BehaviourSharedPrepareReleaseInfo).
//
// WHY THIS FILE EXISTS: the BehaviourManager pools behaviours through a type-erased
// AbstractPoolVoidHandle and drives every one of them through THIS vtable
// (BehaviourHelper::Prepare @0x82255F48 dispatches slot 0, PrepareBehaviours @0x8221EE08
// dispatches slot 1, BehaviourHelper::Update @0x82220688 dispatches slot 2,
// ReleaseBehaviours @0x8221FDE8 dispatches slot 4). Until this base existed no behaviour
// could be allocated at all, which is why the attract-mode / DJ fly-by camera never moved.
// Three private FORKED slices of this class used to exist (BehaviourRig.h,
// BrnBehaviourIceAnim.h, BrnBehaviourInterpolate.h); they are retired in favour of this home.
//
// LAYOUT AUTHORITY: the DECFIGS DWARF for this TU (Behaviour.h:167 / :334-:346), corroborated
// to the byte by the X360 ARTIST asm:
//
//   console offset | member                       | asm proof
//   ---------------+-----------------------------+--------------------------------------------
//   +0x00          | vptr                        | every dispatch site loads *(behaviour+0)
//   +0x04          | meTimestepType              | BehaviourRoadRunner::Construct @0x8222BCE0
//                  |                             |   `*(this+4) = 0`
//   +0x08          | mbIsPrepared                | Construct `*(this+8) = 0`
//   +0x09          | mbHasFailed                 | Behaviour::SetCantSwitchFromMeNow @0x82206388
//                  |                             |   `lbz r11,9(this)` (the assert operand);
//                  |                             |   BehaviourHelper::Update `lbz r10,9(behaviour)`
//   +0x0A          | mbTweakerAttached           | Construct `*(this+10) = 0`
//   +0x0B          | mbCanSwitchToMeNow          | Fail @0x822063E8 `stb 0,0xB(this)`;
//                  |                             |   PreUpdate `stb (!failed),0xB`
//   +0x0C          | mbCanSwitchFromMeNow        | SetCantSwitchFromMeNow `stb 0,0xC(this)`;
//                  |                             |   Fail `stb 1,0xC(this)`; PreUpdate `stb 1,0xC`
//   +0x10          | mpcDebugParametersName      | Construct `*(this+16) = 0`
//   (base size 0x14 -- BehaviourRoadRunner's own first member mpParameters lands at +0x14,
//    which BehaviourRoadRunner::Construct's last store `*(this+20) = 0` pins exactly.)
//
// x64 NOTE: the console offsets above are 4-byte-pointer; on the host the vptr and
// mpcDebugParametersName widen, so absolute offsets shift. Parity here is BY NAMED MEMBER
// (the project's x64 rule) -- no consumer indexes this base by offset.
//
// VTABLE ORDER (asm-attested where marked; the rest from the DWARF declaration order):
//   0  Construct()                                     <- BehaviourHelper::Prepare  @0x82255F48
//                                                          `lwz r11,0(vt); bctrl`
//   1  Prepare(const BehaviourSharedPrepareReleaseInfo&) <- PrepareBehaviours @0x8221EE08
//                                                          `(*(**v14 + 4))(*v14, v44)`
//   2  Update(Camera&, const BehaviourSharedInfo&)      <- BehaviourHelper::Update @0x82220688
//                                                          `lwz r11,8(vt); bctrl`
//   3  PostCollisionUpdate(Camera&, const BehaviourSharedInfo&)
//   4  Release(const BehaviourSharedPrepareReleaseInfo&) <- ReleaseBehaviours @0x8221FDE8
//                                                          `(*(**v14 + 16))(*v14, v45)`
//   5  GetCollisionPolicy()
//   6  GetParameters() const
//   7  SetParameters(const Parameters*)
//   8  SetupTweaker(Utils::Tweaker&)
//   9  GetName() const
// FLAG (slot 6/7): the DWARF dump of `Behaviour` does not list GetParameters/SetParameters,
//   but the DWARF of BehaviourRoadRunner (and of every other concrete behaviour) lists them
//   as VIRTUAL overrides sitting between PostCollisionUpdate and SetupTweaker. They are
//   therefore declared here in that position. Slots 3/5-9 are NOT individually asm-pinned;
//   only 0/1/2/4 are. Since the x64 gate is semantic-parity-by-named-member and nothing in
//   the reconstruction indexes a vtable by slot number, the residual risk is naming only.
// ============================================================================

namespace CgsNumeric { class Random; }
namespace ICE        { class CameraSpaceHandler; }

namespace BrnDirector
{
    // Threaded through BehaviourSharedInfo / BehaviourSharedPrepareReleaseInfo by pointer.
    class DirectorResourceManager;
    class EffectInterface;
    class AllVehicleData;
    class WorldMap;
    class VehicleTracker;
    class SceneQueryInterface;
    struct DebugLog;
    struct DebugPrinter;

namespace Camera
{
    // VehicleInfo's real home (Camera/SharedIO/BrnPlayerInfo.h) is included at the top of this
    // file -- BehaviourSharedInfo holds one BY VALUE.
    class BehaviourManager;
    class BehaviourControllerLockInterface;
    class CollisionPolicy;

    // NOTE: `struct`, not `class` -- the real home (Camera/Utils/BrnCameraTweaker.h:45) declares
    // `struct Tweaker`, and MSVC mangles the class-key into the symbol (`V` vs `U`). Declaring it
    // `class` here made every SetupTweaker override define ?...@@UEAAXAEAVTweaker@... while every
    // caller that had seen the real header referenced ?...@@UEAAXAEAUTweaker@... -- two distinct
    // symbols, so the whole SetupTweaker vtable slot came up unresolved at link.
    namespace Utils { struct Tweaker; }

    // ------------------------------------------------------------------------
    // BehaviourSharedPrepareReleaseInfo (DWARF Behaviour.h:149..:155)
    //
    // The two-word block a behaviour's Prepare/Release virtuals receive. X360-attested from
    // BehaviourManager::PrepareBehaviours @0x8221EE08, which builds it on its own stack:
    //   v44[0] = &v43   (a BehaviourControllerLockInterface built beside it, whose
    //                    v43[1] = the manager and whose v43[0] is re-stamped with the
    //                    current helper index before every dispatch)
    //   v44[1] = a2     (the DirectorResourceManager the caller handed in)
    // ReleaseBehaviours @0x8221FDE8 builds the identical pair.
    // ------------------------------------------------------------------------
    struct BehaviourSharedPrepareReleaseInfo
    {
        // The interpolation lock interface the behaviour may use to pin a helper slot while
        // it blends. Re-stamped with the helper index being prepared/released.
        const BehaviourControllerLockInterface* mpInterpolateLockInterface;

        // The director resource manager the behaviour resolves its authored data through.
        const DirectorResourceManager* mpDirectorResourceManager;

        const DirectorResourceManager* GetDirectorResourceManager() const
        {
            return mpDirectorResourceManager;
        }
        const BehaviourControllerLockInterface* GetInterpolateLockInterface() const
        {
            return mpInterpolateLockInterface;
        }
    };

    // ------------------------------------------------------------------------
    // BehaviourSharedInfo (DWARF Behaviour.h:87..:137)
    //
    // The per-frame context every behaviour's Update/PostCollisionUpdate receives. Built once
    // per frame by MainDirector::UpdateCameraBehavioursPostScene @0x8224FD30 and handed to
    // BehaviourManager::UpdateAllBehaviours @0x82251960.
    //
    // The DWARF member ORDER below is corroborated by the console offsets the two consumers
    // touch (quoted per member). Those offsets fall out of the order exactly:
    //   mTimestep @1360 (0x40) -> mCarModifier @1424 (Vector2 is a 16-byte VMX register alias)
    //   -> mUsedRaceCars @1440 (8) -> mpDirectorResourceManager @1448 ... mCameraModifier @1520
    //   (a full `stvx128 v0,r23,1520` in UpdateAllBehaviours) -> mbUseControlPauseBehaviour
    //   @1536 / mbLookback @1537 (the two `stb`s right after that vector store). Every
    //   independently-observed offset lands on the member the DWARF order predicts.
    //
    // x64: pointer members widen, so the absolute offsets above are PROVENANCE ONLY.
    // ------------------------------------------------------------------------
    struct BehaviourSharedInfo
    {
        // ---- layout (DWARF Behaviour.h:92 -> :137) -------------------------------------
        Utils::Camera2DRotationController        mRotationController;          // :92
        Utils::CameraSphericalRotationController mSphericalRotationController; // :93

        // :95  the player's own vehicle record, BY VALUE.
        // The blocking FLAG that used to sit here (an ODR fork of BrnPhysics::SuspensionSpring
        // between VehiclePhysics.h and PhysicsUtilities/Spring1D.h) is RETIRED: the tree now
        // has exactly one `struct SuspensionSpring` (Spring1D.h:87), so including the real
        // VehicleInfo home is safe.
        // KEYSTONE (asm): mPlayerInfo sits at console +96 and sizeof(VehicleInfo) == 0x4F0,
        // pinned from MainDirector::UpdateCameraBehavioursPostScene @0x8224FD30's frame
        // (48 + 48 == 96, then VehicleInfo::operator= @0x821F49C8 copies into it; and
        // 96 + 1264 == 1360 == mTimestep's console offset). That is what lets the two
        // sub-object accessors below be written against NAMED members instead of offsets.
        VehicleInfo                              mPlayerInfo;
        BrnDirector::Timestep                    mTimestep;                    // :96  console +1360

        Vector2                                  mCarModifier;                 // :98  console +1424
        CgsContainers::BitArray<8u>              mUsedRaceCars;                // :100 console +1440

        const DirectorResourceManager*           mpDirectorResourceManager;    // :102 console +1448
        const EffectInterface*                   mpEffectInterface;            // :104 console +1452

        f32                                      mfTempFOVBoostAmount;         // :106 console +1456
        f32                                      mfSpeedRatio;                 // :107 console +1460
        f32                                      mfCrashTimeRemaining;         // :109 console +1464

        const AllVehicleData*                    mpAllVehicleData;             // :111 console +1468
        const BehaviourManager*                  mpBehaviourManager;           // :113 console +1472
        const VehicleInfo*                       mpRaceCars;                   // :114 console +1476
        EActiveRaceCarIndex                      mePlayerCarIndex;             // :115 console +1480

        BrnDirector::DebugLog*                   mpDebugLog;                   // :117 console +1484
        BrnDirector::DebugPrinter*               mpDebugPrinter;               // :118 console +1488
        CgsNumeric::Random*                      mpRandom;                     // :120 console +1492

        const SceneQueryInterface*               mpSceneQueryInterface;        // :122 console +1496
        const BrnDirector::WorldMap*             mpWorldMap;                   // :124 console +1500
        const VehicleTracker*                    mpPlayerTracker;              // :125 console +1504
        const ICE::CameraSpaceHandler*           mpCameraSpaceHandler;         // :126 console +1508

        f32                                      mfLastHandbrakeTime;          // :128 console +1512
        bool                                     mbAllStreamed;                // :129 console +1516
        bool                                     mbIceDataBeingEdited;         // :130 console +1517
        u8                                       muRenderMetricsActivationID;  // :132 console +1518

        Vector2                                  mCameraModifier;              // :134 console +1520
        bool                                     mbUseControlPauseBehaviour;   // :136 console +1536
        bool                                     mbLookback;                   // :137 console +1537

        // ---- named accessors the committed behaviour bodies already call ---------------
        // (These are the accessor NAMES the three retired forked slices exposed; each now
        // resolves to the DWARF member the fork's own offset comment pinned, so no consumer
        // changes meaning. Kept inline where the member is unambiguous.)

        // The "world" a Behaviour::VehicleRef resolves against == the AllVehicleData block
        // (console +1468 -- the offset the retired IceAnim slice called `GetWorld`).
        const AllVehicleData* GetWorld() const { return mpAllVehicleData; }

        // The scalar frame delta for a behaviour's declared timestep flavour.
        f32 GetTimestep(BrnDirector::Timestep::EType leType) const { return mTimestep.Get(leType); }
        const BrnDirector::Timestep& GetTimestep() const { return mTimestep; }

        const BrnDirector::WorldMap*   GetWorldMap() const   { return mpWorldMap; }
        const VehicleTracker*          GetPlayerTracker() const { return mpPlayerTracker; }
        const DirectorResourceManager* GetDirectorResourceManager() const { return mpDirectorResourceManager; }
        const ICE::CameraSpaceHandler* GetCameraSpaceHandler() const { return mpCameraSpaceHandler; }
        BrnDirector::DebugPrinter*     GetDebugPrinter() const { return mpDebugPrinter; }
        CgsNumeric::Random*            GetRandom() const     { return mpRandom; }
        const BehaviourManager*        GetBehaviourManager() const { return mpBehaviourManager; }
        EActiveRaceCarIndex            GetPlayerCarIndex() const { return mePlayerCarIndex; }

        // The ICE editor is live-editing the take this frame, so a controller-driven
        // behaviour must re-Prepare (console +1517 -- the retired IceAnim slice's
        // `ShouldRePrepareController`).
        bool ShouldRePrepareController() const { return mbIceDataBeingEdited; }
        bool IsLookback() const { return mbLookback; }

        // The block itself, as the controller-facing opaque context the ICE key-anim
        // controller takes (the console passes `&lrInfo`). Untyped, as the retired slice
        // declared it -- the controller stores it without knowing the type.
        void* GetInfoPointer() const { return const_cast<BehaviourSharedInfo*>(this); }

        // The camera-space handler the ICE take resolves its reference spaces through
        // (console +1508 -- the retired IceAnim slice's `GetSourceSpaces`). Returned as an
        // opaque pointer, as the slice declared it: the ICE handler's own ctor takes the
        // source-space block untyped.
        const void* GetSourceSpaces() const { return mpCameraSpaceHandler; }

        // The debug print sink (console +1488 -- the slice's `GetDebugSink`).
        void* GetDebugSink() const { return static_cast<void*>(mpDebugPrinter); }

        // ---- carried forward from the retired BrnBehaviourIceAnim.h slice ------------------
        // RESOLVED (2026-08-01). Both are `addi`, not `lwz`, in the asm -- they hand back
        // SUB-OBJECT ADDRESSES inside mPlayerInfo, not loaded pointers, so with mPlayerInfo
        // embedded by value above they resolve to named members:
        //   console +592  == 96 + 0x1F0 -> mPlayerInfo.mRaceCarState.mTransform (Matrix44Affine)
        //   console +1280 == 96 + 0x4A0 -> mPlayerInfo.mAABB                     (AABBox)
        // Corroborated independently by their only consumer: IsLookingAtTarget @0x822331F0
        // reads argument 2 at +0x00/+0x10/+0x20/+0x30 (four matrix rows) and argument 3 at
        // +0x00/+0x10 only ({min,max}), i.e. exactly these two types.
        // ⚠️ The DWARF NAMES are misleading -- "eye target" is the target car's world
        // transform and "look target" is its object-space bounding box -- but they are the
        // console's own names and are kept.
        const Matrix44Affine& GetEyeTarget()  const { return mPlayerInfo.mRaceCarState.mTransform; }
        const AABBox&         GetLookTarget() const { return mPlayerInfo.mAABB; }

        // RETIRED (2026-08-01): `GetSpaceArgs()` named the SAME member as GetLookTarget
        // (console +1280) and had zero consumers -- a duplicate carried in from the same
        // retired fork. Removed rather than re-homed.
    };

    // ------------------------------------------------------------------------
    // Behaviour -- the abstract base of every director camera behaviour.
    // ------------------------------------------------------------------------
    class Behaviour
    {
    public:
        // --------------------------------------------------------------------
        // Behaviour::Parameters (DWARF Behaviour.h:265..:294) -- the head every authored
        // behaviour parameter block starts with: the behaviour-type tag the SetParameters
        // asserts compare, and the debug name the tweaker/printers show.
        // --------------------------------------------------------------------
        class Parameters
        {
        public:
            u32         GetType() const { return mType; }                   // :271
            void        SetDebugName(const char* lpcName) { mpcDebugName = lpcName; } // :276
            const char* GetDebugName() const { return mpcDebugName; }       // :279

        protected:
            Parameters() : mType(0), mpcDebugName(0) {}                     // :285
            void Construct() { mType = 0; mpcDebugName = 0; }               // :290

            u32 mType;                                                      // :287 +0x00

        private:
            const char* mpcDebugName;                                       // :294 +0x04

            friend class Behaviour;
        };

        // --------------------------------------------------------------------
        // Behaviour::VehicleRef (DWARF Behaviour.h:353..:362) -- a BrnDirector::VehicleRef
        // that knows how to resolve itself against a BehaviourSharedInfo. Adds NO data
        // members (DWARF lists none); only the two resolution helpers.
        // --------------------------------------------------------------------
        class VehicleRef : public BrnDirector::VehicleRef
        {
        public:
            // The live vehicle this reference names (DECLARATION-ONLY: the resolution walks
            // the shared info's AllVehicleData block, whose own TU owns the walk).
            const VehicleInfo& GetVehicle(const BehaviourSharedInfo& lrInfo) const;   // :358
            // That vehicle's world transform.                                        // :362
            const rw::math::vpu::Matrix44Affine& GetTransform(const BehaviourSharedInfo& lrInfo) const;

            // Does this reference currently resolve. (The retired forked slices exposed this
            // name; it is the base VehicleRef's own populated flag, X360-pinned at +0x0C.)
            bool IsValid() const { return mbSet; }

            // The world-taking overload the ICE-anim behaviour calls -- BrnDirector::VehicleRef::
            // IsValid @0x822336A8. The "world" is the shared info's AllVehicleData block (see
            // BehaviourSharedInfo::GetWorld).
            //
            // ⭐ CORRECTED 2026-07-30. This used to be `return mbSet;` behind a FLAG that read
            // "whether the console's validity test actually CONSULTS the world ... is not
            // attested". It is attested, and it does. @0x822336A8 is:
            //     if (!mbSet) return false;
            //     switch (meType) {
            //       case E_PLAYER_CAR:               index = world->mePlayerRaceCarIndex;   // +0xC4
            //       case E_RACE_CAR:                 index = miRaceCarIndex;
            //       case E_RACE_CAR_NEAREST_PLAYER:  index = world->GetNearestRaceCarIndexToPlayer(muRef);
            //       case E_TRAFFIC_VEHICLE:          assert("not implemented yet"); return false;
            //       default:                         assert("unknown type");        return false;
            //     }
            //     assert(index < 8);                                    // CgsBitArray.h:203
            //     return world->mUsedRaceCars.IsBitSet(index);          // +0xC8
            // (the two displacements land exactly on the committed AllVehicleData members --
            //  mePlayerRaceCarIndex @+0xC4 == 196 and mUsedRaceCars @+0xC8 == 200, which the
            //  pseudocode reads as *(world+196) and *(world + 8*((index>>6)+25)); independent
            //  confirmation of that header's layout.)
            //
            // ⚠️ WHY THE OLD `return mbSet;` MATTERED: it made every populated reference valid,
            // including references to race cars that were never spawned. The used-race-car bit is
            // the console's ONLY guard against exactly that -- and note that it is a guard on the
            // BIT, not on the vehicle data, so a car whose bit is set but whose VehicleInfo is
            // zeroed still passes here on the console too (see the ⚠️⚠️ block in
            // BrnGameModule::DoUpdate_Director).
            // INLINE deliberately: AllVehicleData::GetNearestRaceCarIndexToPlayer @0x82233380 is
            // its own (still unreconstructed) ledger function, so an out-of-line body would put an
            // unresolved external into every link whether or not anything calls IsValid. Inline,
            // the body materialises only where a caller actually needs it -- which today is
            // BrnBehaviourIceAnim.cpp, and that TU cannot be mounted until the ICE take evaluator
            // lands anyway (by which point that accessor must be real regardless).
            bool IsValid(const void* lpWorld) const
            {
                if (!mbSet)
                    return false;

                const BrnDirector::AllVehicleData& lrWorld =
                    *static_cast<const BrnDirector::AllVehicleData*>(lpWorld);

                EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

                switch (meType)
                {
                case E_PLAYER_CAR:
                    leIndex = lrWorld.GetPlayerRCIndex();                       // *(world+196)
                    break;

                case E_RACE_CAR:
                    leIndex = static_cast<EActiveRaceCarIndex>(miRaceCarIndex); // *(this+4)
                    break;

                case E_RACE_CAR_NEAREST_PLAYER:
                    leIndex = lrWorld.GetNearestRaceCarIndexToPlayer(muRef);    // @0x82233380
                    break;

                case E_TRAFFIC_VEHICLE:
                    CGS_ASSERT(false, "not implemented yet");   // BrnVehicleRef.h:301 (non-gating)
                    return false;

                default:
                    CGS_ASSERT(false, "unknown type");          // BrnVehicleRef.h:307 (non-gating)
                    return false;
                }

                // CgsBitArray.h:203 "invalid index : N < 8" (non-gating)
                CGS_ASSERT(static_cast<u32>(leIndex) < 8u, "invalid index");

                // *(world + 8*((index>>6)+25))  ==  world+200  ==  mUsedRaceCars
                return lrWorld.GetUsedRaceCarsBitArray().IsBitSet(static_cast<u32>(leIndex));
            }
        };

        Behaviour()
            : meTimestepType(BrnDirector::Timestep::E_WORLD),
              mbIsPrepared(false), mbHasFailed(false), mbTweakerAttached(false),
              mbCanSwitchToMeNow(false), mbCanSwitchFromMeNow(false),
              mpcDebugParametersName(0)
        {}
        virtual ~Behaviour() {}

        // ---- the virtual interface (see the vtable table in the file banner) -----------
        virtual void Construct();                                                    // slot 0
        virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);       // slot 1
        virtual bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo);    // slot 2
        virtual bool PostCollisionUpdate(Camera& lrCamera, const BehaviourSharedInfo& lrInfo); // 3
        virtual void Release(const BehaviourSharedPrepareReleaseInfo& lrInfo);        // slot 4
        virtual CollisionPolicy* GetCollisionPolicy();                                // slot 5
        virtual const Parameters* GetParameters() const;                              // slot 6
        virtual void SetParameters(const Parameters* lpParameters);                   // slot 7
        virtual void SetupTweaker(Utils::Tweaker& lrTweaker);                         // slot 8
        virtual const char* GetName() const;                                          // slot 9

        // ---- non-virtual API (DWARF Behaviour.h:368..:510) ------------------------------

        // Reset the per-frame switch gates before the behaviour's Update runs. X360-attested:
        // BehaviourHelper::Update @0x82220688 inlines exactly this pair of stores immediately
        // before dispatching slot 2 --
        //     if (behaviour->mbHasFailed) behaviour->mbCanSwitchToMeNow = false;
        //     else                        behaviour->mbCanSwitchToMeNow = true;
        //     behaviour->mbCanSwitchFromMeNow = true;
        void PreUpdate()                                                              // :368
        {
            mbCanSwitchToMeNow   = !mbHasFailed;
            mbCanSwitchFromMeNow = true;
        }

        void        SetTweakerAttached(bool lbAttached) { mbTweakerAttached = lbAttached; } // :413
        const char* GetDebugParametersName() const { return mpcDebugParametersName; }       // :242

        bool CanSwitchToMeNow() const   { return mbCanSwitchToMeNow; }                // :421
        bool CanSwitchFromMeNow() const { return mbCanSwitchFromMeNow; }              // :429
        bool HasFailed() const          { return mbHasFailed; }                       // :437

        // Give up following. Bodies in Behaviour.cpp (@0x822063E8).
        void Fail(Camera& lrCamera, s32 leFailedFlag);                                // :487

        BrnDirector::Timestep::EType GetTimestepType() const { return meTimestepType; }        // :468
        void SetTimestepType(BrnDirector::Timestep::EType leType) { meTimestepType = leType; } // :477

    protected:
        void SetDebugParametersName(const char* lpcName) { mpcDebugParametersName = lpcName; } // :246

        bool IsPrepared()     { return mbIsPrepared; }                                // :388
        void SetPrepared()    { mbIsPrepared = true; }                                // :396
        void SetNotPrepared() { mbIsPrepared = false; }                               // :404

        // Bodies in Behaviour.cpp (SetCantSwitchFromMeNow @0x82206388).
        void SetCantSwitchToMeNow(Camera& lrCamera, s32 leNoCutToFlag);               // :447
        void SetCantSwitchFromMeNow(Camera& lrCamera, s32 leNoCutFromFlag);           // :458

        bool IsTweakerAttached() const { return mbTweakerAttached; }                  // :502
        bool IsDebugDisplayActive() const;                                            // :510

    protected:
        // ---- layout (DWARF Behaviour.h:334..:346; see the file banner for the asm pins) --
        // FLAG (access level): the DWARF marks these six PRIVATE, with the protected
        //   accessors above (SetPrepared/SetNotPrepared/IsPrepared/SetTimestepType/...) as
        //   the intended path. They are `protected` here because the concrete behaviours'
        //   reconstructed bodies write several of them directly -- which is exactly what the
        //   console does too, since every one of those writes is an INLINED base helper (e.g.
        //   BehaviourRoadRunner::Construct @0x8222BCE0 opens with the six stores that ARE
        //   Behaviour::Construct). Widening private->protected changes no layout and no
        //   semantics; it only avoids fabricating a setter for each field.
        //   DELETE-WHEN: every derived body has been re-expressed through the named helpers.
        BrnDirector::Timestep::EType meTimestepType;          // :334  console +0x04
        bool                         mbIsPrepared;            // :335  console +0x08
        bool                         mbHasFailed;             // :336  console +0x09
        bool                         mbTweakerAttached;       // :337  console +0x0A
        bool                         mbCanSwitchToMeNow;      // :342  console +0x0B
        bool                         mbCanSwitchFromMeNow;    // :343  console +0x0C
        const char*                  mpcDebugParametersName;  // :346  console +0x10
    };

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_H
