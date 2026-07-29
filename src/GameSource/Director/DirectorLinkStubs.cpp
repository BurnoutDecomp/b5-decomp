// ============================================================================
// GameSource/Director/DirectorLinkStubs.cpp
//
// PC LINK-MOUNT STUBS for the DirectorModule mount (2026-07-29, DJ fly-by campaign).
//
// The director spine is now in the game exe's source list. Some of its declaration-only
// callees have no owning TU in the link yet -- either because the TU does not exist, or
// because mounting it would drag a whole un-landed sub-system in behind it. Rather than
// fabricate bodies inside the real headers, each one gets a MARKED, QUIET stub here:
//
//   * every stub carries WHY it is a stub and a DELETE-WHEN note;
//   * no stub traps -- these sit on per-frame paths and a trap would make the exe unusable;
//   * a stub that must return a value returns the console's own "nothing happened" value;
//   * NO stub returns a reference to a fabricated object. Where a symbol returns a reference
//     the stub is omitted and the caller's TU is kept out of the link instead.
//
// This file is the Director's twin of GameSource/World/WorldLinkStubs.cpp. When a real TU
// lands for any symbol below, DELETE its stub here (a duplicate definition is a link error,
// so the removal is enforced by the build).
//
// ---------------------------------------------------------------------------
// GROUP A -- the nine arbitrator states that are NOT on the fly-by path.
//   All ten states are reconstructed under Arbitrator/States/, but each of the nine below
//   drags a different un-landed sub-system into the link (ICEMoviePlayer, MomentSelector,
//   BehaviourIceAnim, BehaviourInterpolate, the DirectorResourceManager shot-group getters,
//   the Attrib shot vault). Mounting all ten took the link from 47 to 137 unresolved, most of
//   them reference-returning accessors that cannot be honestly stubbed. Only
//   ArbStateAttractMode -- the DJ fly-by's own state -- is mounted for real.
//   CONSEQUENCE: the arbitrator can build its state container and can still refuse to enter
//   any of these states, but if one IS entered it produces no camera and reports "released".
//   That is the SAME observable state as before the director was mounted at all.
//   DELETE-WHEN: a state's sub-system lands -> mount the state's .cpp, delete its block here.
//
// GROUP B -- the two debug behaviours the BehaviourManager instantiates by template.
//   BehaviourManager::AllocateBehaviour<T> is instantiated for every behaviour type, which
//   forces each type's vtable. DebugFlyWorld and DebugOrbitPlayer are DEV-MENU behaviours
//   (their real TUs pull the Tweaker mapping API and the panorama screenshot callback); they
//   are never allocated on the fly-by path.
//
// GROUP C -- sub-systems with no landed TU at all (ICE/ICEWrapper, the director
//   DebugComponent, SharedPlaylists, the Attrib collection lookup, rw SLerp).
//
// GROUP D -- declared-only leaves of already-mounted TUs.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"

#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h"

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"                       // group E
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // group E

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

// ----------------------------------------------------------------------------
// GROUP A -- the nine off-path arbitrator states.
//
// Every stub below mirrors the BASE ArbitratorState default (see
// Arbitrator/BrnDirectorArbitratorState.cpp), which is what an un-entered state does anyway:
//   Construct() -- build the state's camera and clear the base flags;
//   Prepare()   -- "ready" (the arbitrator only calls it after CanRun said yes);
//   Update()    -- drive nothing;
//   Release()   -- "already released" (ReleaseAll asserts the result);
//   Destruct()  -- own nothing;
//   GetName()   -- the state's own console name literal (these ARE attested: the whole family
//                  sits in .rodata at 0x821F62E0..0x821F6740, one GetName symbol per state).
// ----------------------------------------------------------------------------
#define BRN_DIRECTOR_STUB_ARBSTATE(CLASS, NAME_LITERAL)                             \
    void CLASS::Construct()                                                         \
    {                                                                               \
        ArbitratorState::Construct();                                               \
    }                                                                               \
    bool CLASS::Prepare(ArbStateSharedInfo& lrSharedInfo)                           \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
        return true;                                                                \
    }                                                                               \
    void CLASS::Update(ArbStateSharedInfo& lrSharedInfo)                            \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
    }                                                                               \
    bool CLASS::Release(ArbStateSharedInfo& lrSharedInfo)                           \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
        return true;                                                                \
    }                                                                               \
    const char* CLASS::GetName() const                                              \
    {                                                                               \
        return NAME_LITERAL;                                                        \
    }

namespace BrnDirector
{
    // The GetName literals are the ARTIST .rodata names (one symbol each, 0x821F62F0 /
    // 0x821F6300 / 0x821F6330 / 0x821F6730 / 0x821F6480 / 0x821F6310 / 0x821F6320 /
    // 0x821F6710 / 0x821F6238).
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateCrashMode,       "ArbStateCrashMode")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateDriveThru,       "ArbStateDriveThru")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineCarSelect, "ArbStateOnlineCarSelect")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineRaceIntro, "ArbStateOnlineRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStatePostEvent,       "ArbStatePostEvent")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRaceIntro,       "ArbStateRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRankUp,          "ArbStateRankUp")

    // CrashNav and Roaming already own SOME of their virtuals in their mounted-elsewhere
    // headers, so only the missing slots are stubbed.
    void ArbStateCrashNav::Construct()                          { ArbitratorState::Construct(); }
    void ArbStateCrashNav::Update(ArbStateSharedInfo& lrInfo)   { (void)lrInfo; }
    bool ArbStateCrashNav::Release(ArbStateSharedInfo& lrInfo)  { (void)lrInfo; return true; }
    const char* ArbStateCrashNav::GetName() const               { return "ArbStateCrashNav"; }

    void ArbStateRoaming::Construct()                           { ArbitratorState::Construct(); }
    bool ArbStateRoaming::Prepare(ArbStateSharedInfo& lrInfo)   { (void)lrInfo; return true; }
    bool ArbStateRoaming::Release(ArbStateSharedInfo& lrInfo)   { (void)lrInfo; return true; }
    const char* ArbStateRoaming::GetName() const                { return "ArbStateRoaming"; }

    // Two states declare an explicit Destruct() override that their .cpp bodies.
    void ArbStateOnlineRaceIntro::Destruct() {}
    void ArbStatePostEvent::Destruct()       {}
}

#undef BRN_DIRECTOR_STUB_ARBSTATE

// ----------------------------------------------------------------------------
// GROUP B -- the two dev-menu behaviours the manager's AllocateBehaviour<T> forces vtables
// for. Their real TUs (BrnBehaviourDebugFlyWorld.cpp / BrnBehaviourDebugOrbitPlayer.cpp) pull
// the whole Tweaker mapping API + the panorama screenshot callback. Neither is allocated on
// the fly-by path -- only ArbStateAttractMode's BehaviourRoadRunner is.
// Update() returns FALSE = "I produced no camera this frame", the same answer the console's
// own behaviours give when they have nothing to say.
// DELETE-WHEN: Camera/Utils/BrnCameraTweaker.cpp lands -> mount both real TUs, delete this.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    void BehaviourDebugFlyWorld::Construct()
    {
        // NOTE: these two classes are still PRE-BASE forks -- they carry their own
        // `void* mpVTable` at +0x00 instead of deriving from Camera::Behaviour, so there is no
        // base Construct to chain to. (Retiring those two forks the way the road runner's was
        // retired is a separate job.)
    }

    bool BehaviourDebugFlyWorld::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    bool BehaviourDebugFlyWorld::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
    {
        (void)lrCamera;
        (void)lrInfo;
        return false;
    }

    void BehaviourDebugFlyWorld::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    const char* BehaviourDebugFlyWorld::GetName() const
    {
        return "BehaviourDebugFlyWorld";
    }

    void BehaviourDebugFlyWorld::WarpToLookAt(Vector3 lEye, Vector3 lLookAt)
    {
        (void)lEye;
        (void)lLookAt;
    }

    void BehaviourDebugOrbitPlayer::Construct()
    {
        // (same pre-base fork note as BehaviourDebugFlyWorld::Construct above)
    }

    bool BehaviourDebugOrbitPlayer::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    bool BehaviourDebugOrbitPlayer::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
    {
        (void)lrCamera;
        (void)lrInfo;
        return false;
    }

    void BehaviourDebugOrbitPlayer::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    const char* BehaviourDebugOrbitPlayer::GetName() const
    {
        return "BehaviourDebugOrbitPlayer";
    }
}
}

// ----------------------------------------------------------------------------
// GROUP C -- sub-systems with no landed TU.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // -- ICEWrapper (the ICE/in-car-entertainment take player). MainDirector embeds one by
    //    value and Constructs/Prepares/Destructs it; nothing on the fly-by path drives a take.
    //    Prepare returns TRUE = "staged Prepare finished", so DirectorModule::Prepare's stage
    //    machine advances instead of spinning for ever.
    //    DELETE-WHEN: BrnDirectorICEWrapper.cpp's Construct/Prepare/Destruct land.
    ICEWrapper::ICEWrapper() {}
    void ICEWrapper::Construct() {}
    // (The two ICE sub-objects ICEWrapper embeds by value -- their ctors' TUs are un-landed;
    //  see the ICE::* block at the end of this file.)
    void ICEWrapper::Destruct() {}
    bool ICEWrapper::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg,
                             const DirectorResourceManager* lpResourceManager)
    {
        (void)lpOutputBuffer;
        (void)liPrepareArg;
        (void)lpResourceManager;
        return true;
    }

    // -- The ICE resource manager's two take-data lookups. "No take data" is a valid answer
    //    (the console returns null when a take id is not resident) and every caller null-checks.
    const ICE::ICETakeData* ICEResourceMgr::GetTakeData(CgsResource::ID lID) const
    {
        (void)lID;
        return 0;
    }

    const ICE::ICETakeData* ICEResourceMgr::GetTakeData(s32 liTakeIndex) const
    {
        (void)liTakeIndex;
        return 0;
    }

    // -- DirectorResourceManager::Prepare. The real body resolves the director's Attrib shot
    //    vault (shot groups, key anims, playlists) through the output buffer's request
    //    interfaces; none of that is reachable yet and the road runner reads none of it.
    //    TRUE = "prepared", so DirectorModule::Prepare advances past stage 2.
    //    DELETE-WHEN: the shot-vault resolve lands.
    bool DirectorResourceManager::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer,
                                          ICEWrapper* lpHACKIceWrapper)
    {
        (void)lpOutputBuffer;
        (void)lpHACKIceWrapper;
        return true;
    }

    // -- The director's own CgsDev::DebugComponent page ("Camera"). Its five recovered
    //    functions all index DirectorModule regions this reconstruction does not model yet.
    //    QUIET no-ops: the debug menu simply has an empty Camera page.
    void DebugComponent::Construct(DirectorModule* lpDirectorModule)
    {
        (void)lpDirectorModule;
    }

    void DebugComponent::UpdatePanoramaScreenshots(Camera::Camera* lpCamera)
    {
        (void)lpCamera;
    }

    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    const char* DebugComponent::GetName() const
    {
        return "Camera";   // the page name the console registers (DWARF + the header's note)
    }

    void DebugComponent::OnActivate() {}

    // -- SharedPlaylists::Construct. Home = Utils/BrnICEMoviePlayer.cpp (bodied there), which
    //    is not in the link because ICEMoviePlayer pulls the whole ICE take/movie chain.
    //    The container Constructs one by value; nothing reads it on this path.
    //    DELETE-WHEN: BrnICEMoviePlayer.cpp joins the link.
    void SharedPlaylists::Construct() {}

    // -- The two scene-query post-office free functions BrnSceneQueryInterface.h declares.
    //    OutEventVolumeTestDeepest mints the 16-bit query id for a staged volume test; 0 is
    //    the console's "no id" value and SceneQueryInterface treats it as a failed post.
    //    sub_8221CC98 resets slot 1's post office and returns a pointer into it.
    //    DELETE-WHEN: the two post-office TUs land.
    u32* sub_8221CC98(void* lpSlot)
    {
        return static_cast<u32*>(lpSlot);
    }
}

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    u32 OutEventVolumeTestDeepest(void* lpPostOffice, void* lpQueryParams)
    {
        (void)lpPostOffice;
        (void)lpQueryParams;
        return 0u;
    }
}

namespace CgsCollision
{
    // BaseCollisionGenerator::Destruct @0x8284CB38 -- the console body is a single `blr`
    // (empty). It is referenced from MainDirector::Destruct through the inherited
    // CgsDev::DebugComponent teardown. Attested-empty, not a placeholder.
// FLAG PC-platform leaf: attested-empty console body (@0x8284CB38 is a bare `blr`, corroborated at CgsLanguageManager.cpp:474) standing in for the un-landed CgsCollisionGenerator TU on the PC link (director mount 2026-07-29).
    void BaseCollisionGenerator::Destruct() {}
}
}

// ----------------------------------------------------------------------------
// GROUP D -- declared-only leaves of TUs that ARE in the link.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    // BehaviourManager::DebugDumpToTTY @0x82220750 -- walks every helper slot and prints
    // GetDebugFullName. It is called from the manager's allocation-failure path only.
    // QUIET no-op: the allocation failure still asserts through its own CGS_ASSERT.
    // DELETE-WHEN: BehaviourHelper::GetDebugFullName lands.
// FLAG PC-platform leaf: debug-TTY dump no-op on the PC link (director mount 2026-07-29) -- the real body walks BehaviourHelper::GetDebugFullName, which is declaration-only behind the un-homed helper interior.
    void BehaviourManager::DebugDumpToTTY() const {}
}
}

namespace BrnTraffic
{
    // ------------------------------------------------------------------------
    // ⚠️ THESE FOUR ARE ON THE FLY-BY'S OWN DATA PATH. They are declared in
    // SharedClasses/Traffic/BrnTrafficSection.h and their siblings (GetNumSegments @0x821F4B78,
    // CalcPositionAtParameter @0x821F4BD8, GetGlobalRungForSegment @0x821F5068,
    // CalcSignedDistanceAlongSection @0x82705BC0) ARE bodied in BrnTrafficSection.cpp -- these
    // four simply have not been transcribed yet.
    //
    // WHAT A STUB COSTS HERE, precisely:
    //   * CalcDirectionAtParameter / CalcTransformAtParameter feed WorldMap::
    //     GetSafePositionNearest and the lane truck's frame. Zeroed outputs mean a lane frame
    //     with no forward and no up.
    //   * FindNeighbourForRung feeds WorldMap::WalkLaneLeft; 0xFFFF == "no neighbour" is the
    //     console's own sentinel, so the walk simply stops at the current section.
    //   * CalcDistanceAlongSection feeds CalcSignedDistanceAlongSection (which IS real).
    // The road-runner's own Update leg is still gated, so nothing calls these yet -- but the
    // moment that gate lifts these MUST be transcribed or the fly-by will fly a degenerate
    // lane. They are deliberately left OBVIOUSLY wrong (zeroed / sentinel) rather than
    // plausibly wrong.
    // DELETE-WHEN: transcribed into BrnTrafficSection.cpp beside their four landed siblings.
    // ------------------------------------------------------------------------
    void Section::CalcDirectionAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,
                                           u32 luSegment, Vector3& lrDirection) const
    {
        (void)lpaGlobalRungs;
        (void)lfParam;
        (void)luSegment;
        lrDirection.SetZero();
    }

    void Section::CalcTransformAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,
                                           u32 luSegment, Vector3& lrPosition,
                                           Vector3& lrDirection, Vector3& lrUp) const
    {
        // The POSITION leg is the real one (its sibling is landed); only the frame axes are
        // missing, so hand back the true position and zeroed axes.
        CalcPositionAtParameter(lpaGlobalRungs, lfParam, luSegment, lrPosition);
        lrDirection.SetZero();
        lrUp.SetZero();
    }

    u16 Section::FindNeighbourForRung(u32 luRung, Side leSide, const Hull* lpHull) const
    {
        (void)luRung;
        (void)leSide;
        (void)lpHull;
        return 0xFFFFu;   // the console's "no neighbour" sentinel
    }

    f32 Section::CalcDistanceAlongSection(f32 lfParam, u32 luSegment,
                                          const f32* lpafRungLengths) const
    {
        (void)lfParam;
        (void)luSegment;
        (void)lpafRungLengths;
        return 0.0f;
    }
}

namespace ICE
{
    // The two ICE sub-objects BrnDirector::ICEWrapper embeds BY VALUE, so its (stubbed) ctor
    // has to default-construct them. Their real ctors live in un-landed TUs
    // (SDKs/Packages/ICE/ICEManager + GameSource/Director/Camera/ICECameraMover.cpp).
    // Empty is the honest body here: every member of both is either default-init or set by
    // the matching Construct(), and neither Construct runs on this build (ICEWrapper::Construct
    // is itself stubbed above -- no ICE take is ever played).
    // DELETE-WHEN: ICEManager / ICECameraMover join the link.
    ICEManager::ICEManager() {}
    ICECameraMover::ICECameraMover() {}
    ICETake::ICETake() {}   // pulled in by ICEManager's embedded ICEController
}

namespace Attrib
{
    // Attrib::FindCollection @0x82808378 is DELIBERATELY not reconstructed -- see the block
    // comment at the top of SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/
    // attribsupport.cpp: the committed declaration `FindCollection(int liKey, void* lpOwner)`
    // that 57 generated Attrib::Gen ctors bind against is irreconcilable with the true X360
    // signature `FindCollection(Attribute::Key classKey, Attribute::Key collectionKey)`, and
    // fixing that spans the declaration plus every consumer.
    // It reaches this link through ONE path: ArbStateRaceIntro's ctor, which the arbitrator's
    // state container runs when it default-constructs all ten states by value. Returning NULL
    // is the resolver's own "no such collection" answer; the state holds an unresolved
    // Attrib::Gen handle and is never entered on the fly-by path.
    // DELETE-WHEN: the coordinated decl+consumer pass lands the real two-key resolver.
    Collection* FindCollection(int liKey, void* lpOwner)
    {
        (void)liKey;
        (void)lpOwner;
        return 0;
    }
}

namespace rw
{
namespace math
{
namespace vpu
{
    // The vendor affine-matrix SLerp (declared in rw/math/vpu/matrix44affine_operation.h,
    // body owned by the SDK and not reconstructed). ONE caller reaches it:
    // InertiaController::Update @0x8221ECD0, and only on the branch where the camera has
    // requested LAG (`1 - CameraEffects::mfCameraLag < 1`, i.e. lag > 0). With no lag the
    // console returns before the call, which is the state on every frame of this build.
    // The stub therefore returns lrTo unchanged -- exactly the t == 1 endpoint, i.e. "adopt
    // the freshly-finalised transform", which is what the no-lag path already does. It is the
    // inert answer, not an approximation of the interpolation.
    // DELETE-WHEN: the vendor op is reconstructed (then camera lag starts working).
    Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,
                         const float* lpafBlend)
    {
        (void)lrFrom;
        (void)lpafBlend;
        return lrTo;
    }
}
}
}

namespace BrnTrigger
{
    // The three trigger-region accessors BrnDirectorWorldMap reaches for the drive-thru /
    // region queries. Nothing on the fly-by path reads a trigger region.
    // DELETE-WHEN: the region accessors are transcribed into their SharedClasses/Trigger TUs.
    GenericRegion::Type GenericRegion::GetType() const
    {
        return static_cast<GenericRegion::Type>(0);
    }

    const GenericRegion* TriggerData::GetGenericRegion(s32 liIndex) const
    {
        (void)liIndex;
        return 0;
    }

    rw::math::vpu::Matrix44Affine BoxRegion::ComputeTransform() const
    {
        // Identity: an unrotated, unit-scaled box at the world origin. Deliberately NOT the
        // region's real transform (which the console builds from its stored basis + centre).
        rw::math::vpu::Matrix44Affine lResult;
        lResult.SetIdentity();
        return lResult;
    }
}

// ============================================================================
// GROUP E (NEW 2026-07-29, with the two shared gameplay cameras' RE-BASE) -- the two embedded
// sub-object Constructs BehaviourGameplayExternal::Construct / ::Prepare call by name.
//
// Both are DECLARED in their own canonical homes and have no TU yet. They are stubbed rather
// than bodied because neither shape is attested: the console INLINES
// CameraSphericalRotationController::Construct into each owner (so there is no standalone
// body to read, and its tail lands inside an un-mapped SmoothMover), and
// CameraShakeICEController::Construct is a real call whose body was not dumped.
//
// SAFE TODAY, and provably so: the manager's pools construct every behaviour with
// `new (slot) T()` (BrnAbstractPool.h:148) -- value-initialisation, which zero-initialises the
// whole object before the ctor runs -- so both sub-objects start zeroed regardless. Nothing
// reads either one: BehaviourGameplayExternal::Update is not transcribed, and
// MainDirector::UpdateCameraBehavioursPostScene (the only path that would dispatch it) is
// gated. The CALL SHAPE is what matters here; keeping it means the day these land, the
// behaviour is already correct.
// DELETE-WHEN: BrnCameraSphericalRotationController.cpp / BrnCameraShake.cpp land.
// ============================================================================
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    void CameraSphericalRotationController::Construct()
    {
    }

    void CameraShakeICEController::Construct()
    {
    }
}
}
}
