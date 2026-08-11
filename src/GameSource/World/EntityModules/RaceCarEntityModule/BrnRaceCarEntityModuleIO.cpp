// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.cpp
//
// Out-of-line bodies for the BrnWorld::RaceCarEntityModuleIO IO-buffer accessors that the
// X360 build emitted out-of-line (the const/non-const overloads that did not get inlined).
// Every body asserts the buffer's lock bit then returns &member:
//   write-lock (eStatusLockedForWrite, status>>3 &1) => IsBufferLockedForWriting(), non-const
//   read-lock  (eStatusLockedForRead,  status>>4 &1) => IsBufferLockedForReading(),  const
// Offsets are layout-derived (return &member), not hardcoded. CGS_ASSERT stamps __FILE__/__LINE__,
// so the X360-baked d:\p4 path/line are intentionally not reproduced.
//
// CORRECTION (1): X360 0x8279E310 is the non-const InputBuffer_PostPhysics::GetSceneInputInterface()
//   (write-lock, returns &mSceneInputInterface at this+29856), NOT a non-const GetContactSpyInterface.
// CORRECTION (2): there is no non-const GetContactSpyInterface in the DWARF, so none is emitted.
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>                                    // std::memcpy (SetTrafficToRaceCarInterface_PreScene)

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{

// ---- OutputBuffer_Prepare ---------------------------------------------------

// X360 0x8279CDF0 (R, :126) -- const resource-request accessor.
const OutputBuffer_Prepare::ResourceRequestInterface*
OutputBuffer_Prepare::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mResourceRequestInterface;
}

// X360 0x822B4990 (W, :127) -- mutable resource-request accessor.
OutputBuffer_Prepare::ResourceRequestInterface*
OutputBuffer_Prepare::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mResourceRequestInterface;
}

// ---- InputBuffer_PreScene ---------------------------------------------------

// X360 0x8279D060 (W, :164) -- mutable game-action queue accessor.
InputBuffer_PreScene::GameActionQueue*
InputBuffer_PreScene::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameActionQueue;
}

// X360 0x822B4C30 (R, :163) -- const game-action queue accessor (pairs with the mutable overload
// :164 at the identical member offset). Returns &mGameActionQueue (this+0x22C == 556).
const InputBuffer_PreScene::GameActionQueue*
InputBuffer_PreScene::GetGameActionQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGameActionQueue;
}

// DWARF BrnRaceCarEntityModuleIO.h -- const accessor for the per-car audio (un)load
// REPLY queue. RaceCarAudioStreamer::Update @0x822ECC00 reads it on the way in (the
// audio subsystem's "data loaded / data unloaded" answers) and writes the matching
// request queue on the OutputBuffer_PreScene below. Same member-address idiom as the
// game-action queue above.
const InputBuffer_PreScene::AudioCarLoadedDataQueue*
InputBuffer_PreScene::GetAudioCarLoadedDataQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mAudioCarLoadedDataQueue;
}

// X360 0x8279D308 (:176 W) -- MUTABLE per-car audio (un)load REPLY queue accessor. The
// console body is the write-lock assert citing this header then `return a1 + 15456`. Added
// with the reset-player-car wave: WorldModule::BridgeActionsToRaceCarModule @0x827ABF40 is
// its only caller (it Appends the world input's own queue into this one), and that bridge
// was an inert link stub until now, so nothing had ever needed the non-const overload.
InputBuffer_PreScene::AudioCarLoadedDataQueue*
InputBuffer_PreScene::GetAudioCarLoadedDataQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mAudioCarLoadedDataQueue;
}

// ---- OutputBuffer_PreScene --------------------------------------------------

// DWARF :306 -- mutable per-car audio (un)load REQUEST queue accessor. The audio
// streamer appends its own per-frame queue onto this one at the end of Update.
RaceCarEntityModuleIO::AudioCarLoadedDataQueue*
OutputBuffer_PreScene::GetAudioCarLoadedDataQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mAudioCarLoadedDataQueue;
}

// X360 0x822B4ED0 (W, :283) -- mutable vehicle-input accessor.
OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PreScene::GetVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleInputInterface;
}

// X360 0x822B4F78 (W, :286) -- mutable scene-input accessor.
OutputBuffer_PreScene::SceneInputInterface*
OutputBuffer_PreScene::GetSceneInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mSceneInputInterface;
}

// X360 0x8279D6F8 (R, :300) -- const race-car AI interface accessor.
const OutputBuffer_PreScene::RaceCarAIInterface*
OutputBuffer_PreScene::GetRaceCarAIInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRaceCarAIInterface;
}

// X360 0x822B5020 (W, :289) -- mutable active-race-car output accessor. Pairs with the
// const overload :288 (homed in BrnRaceCarEntityModuleIO_PreSceneAccessors.cpp, X360 0x8279D500).
// Returns &mActiveRaceCarOutputInterface (this+0xEAA40 == 960960).
RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PreScene::GetActiveRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mActiveRaceCarOutputInterface;
}

// ⭐ BODIED 2026-08-01 (car-select hand-off wave) -- the three mutable siblings of the
// accessor above (DWARF :292 / :295 / :298). All three ARE in the export set, just UNNAMED;
// they were recovered together from RaceCarEntityModule::PreSceneUpdate @0x8230D928, which
// fetches all four output interfaces off its OutputBuffer_PreScene in reverse declaration
// order immediately before its UpdateOutputInterfaces call:
//     0x8230E410  bl sub_822B5218  -> replayGlobal   (this + 0xF0000 + 0x0510 == +0xF0510)
//     0x8230E41C  bl sub_822B5170  -> replayActive   (this + 0xF0000 - 0x23E0 == +0xEDC20)
//     0x8230E428  bl sub_822B50C8  -> global         (this + 0xF0000 - 0x2D50 == +0xED2B0)
//     0x8230E434  bl <0x822B5020>  -> active         (this + 0xEAA40, the accessor above)
// Each body is the identical "Not locked for writing" tripwire + `addis/addi this + member`
// pair this whole file is made of, and the four displacements land in the header's member
// DECLARATION ORDER (active < global < replayActive < replayGlobal), which is what pins the
// binding. They are four consecutive functions on a regular 0xA8 stride.
RCEntityGlobalRaceCarOutputInterface*
OutputBuffer_PreScene::GetGlobalRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGlobalRaceCarOutputInterface;
}

RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PreScene::GetReplayActiveRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mReplayActiveRaceCarOutputInterface;
}

RCEntityGlobalRaceCarOutputInterface*
OutputBuffer_PreScene::GetReplayGlobalRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mReplayGlobalRaceCarOutputInterface;
}

// ---- OutputBuffer_PostPhysics -----------------------------------------------

// DWARF :563/:564 -- the resource-request interface every race-car GameData request
// leaves the module through. RaceCarEntityModule::SendStreamerEvents @0x82304F70 takes
// the mutable one and hands it to RaceCarStreamer::AppendGameDataRequests; the const one
// is read by WorldModule::BridgeEntityModulesToOutput_PostPhysics on the way to the
// world's update-output buffer. Same member-address idiom as the rest of this file.
RaceCarEntityModuleIO::ResourceRequestInterface*
OutputBuffer_PostPhysics::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mResourceRequestInterface;
}

const RaceCarEntityModuleIO::ResourceRequestInterface*
OutputBuffer_PostPhysics::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mResourceRequestInterface;
}

// ---- InputBuffer_PostScene --------------------------------------------------

// X360 0x822B5410 (R, :346) -- const PreScene traffic->racecar accessor.
const InputBuffer_PostScene::TrafficToRaceCarInterface_PreScene*
InputBuffer_PostScene::GetTrafficToRaceCarInterface_PreScene() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficToRaceCarInterface_PreScene;
}

// ---- OutputBuffer_PostScene -------------------------------------------------

// X360 0x822B5608 (W, :380) -- mutable AI module-request accessor.
OutputBuffer_PostScene::AIModuleRequestInterface*
OutputBuffer_PostScene::GetAIModuleRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mAIModuleRequestInterface;
}

// X360 0x822B56B0 (W, :383) -- mutable race-car->traffic accessor.
RaceCarToTrafficInterface*
OutputBuffer_PostScene::GetRaceCarToTrafficInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mRaceCarToTrafficInterface;
}

// ---- InputBuffer_PrePhysics -------------------------------------------------

// X360 0x822B59F8 (R, DWARF :427 / X360 baked line 436) -- const online-scoring accessor
// (X360 tail `return this + 212048` == &mOnlineScoringInterface, the same 164-byte block
// SetOnlineScoringInterface @0x8279DCF8 memcpys into).
// ⚠️ ADDRESS CORRECTED 2026-08-11 (was 0x822B5800, which is the const
// GetAIModuleResultInterface -- line 427, returns +196656): the PS3-DWARF-line vs
// X360-baked-line skew (+9 in this buffer) had slid this buffer's read-lock run one slot.
// Bodies were always right (by-name &member); the citations were not.
const InputBuffer_PrePhysics::OnlineScoringInterface*
InputBuffer_PrePhysics::GetOnlineScoringInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mOnlineScoringInterface;
}

// X360 0x822B5A48 (R, DWARF :430 / X360 baked line 439) -- const PostScene
// traffic->racecar accessor (+212212). That address is a HOLE in the .ida-exports dump;
// it is the one remaining 0xA8 slot between 0x822B59F8 (line 436) and 0x822B5AA0
// (line 442, GetControllerActive @byte 212213), and +212212 is the single byte in front
// of mbControllerActive -- i.e. mTrafficToRaceCarInterface_PostScene's 1-byte payload.
// (Was cited as 0x822B5950, which is GetScoringInterface -- same +9 skew.)
const InputBuffer_PrePhysics::TrafficToRaceCarInterface_PostScene*
InputBuffer_PrePhysics::GetTrafficToRaceCarInterface_PostScene() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficToRaceCarInterface_PostScene;
}

// ---- OutputBuffer_PrePhysics ------------------------------------------------

// X360 0x822B5C00 (W, :471) -- mutable vehicle-input accessor.
OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PrePhysics::GetVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleInputInterface;
}

// ============================================================================================
// ⛔⛔ MISATTRIBUTION CORRECTED 2026-08-10 (pre-physics bridge wave).
//
// This block used to read "X360 0x8279E070 (R, :482) -- const game-event queue accessor" over
// GetGameEventQueue() const. It is WRONG, and the proof is the returned offset:
//     0x8279E070   addis r3, r28, 2 ; addi r3, r3, 0x2B70   ->  this + 142192
// and 142192 is mVehicleDriverInterface -- the offset this class's own Construct comment names
// (`VehicleDriverInputInterface::Construct(+142192)`). mGameEventQueue lives at +149312.
//
// The five READ-lock accessors of this buffer (all five test `extrwi r11,r11,1,27` and carry
// "Not locked for reading"), keyed by the ONLY reliable discriminator -- the offset returned --
// and cross-read against the DecFIGS DWARF member order (mVehicleInputInterface /
// mVehicleDriverInterface / mVehicleEffectsInterface / mPlayerResetInterface / mGameEventQueue):
//     0x8279DFC8 -> +16      GetVehicleInputInterface()   const
//     0x8279E070 -> +142192  GetVehicleDriverInterface()  const     <- was called GameEventQueue
//     0x8279E118 -> +147488  GetVehicleEffectsInterface() const
//     0x8279E1C0 -> +149280  GetPlayerResetInterface()    const
//     0x8279E268 -> +149312  GetGameEventQueue()          const     <- the real one
//
// ⚠️ WHY THIS WAS EASY TO GET WRONG, recorded so the next reader does not repeat it: the
// `:470/:473/:476/:479/:482` annotations in the header are **PS3 DWARF** declaration lines, and
// the X360 bodies' baked __LINE__ values for the same five accessors are 479/482/485/488/491 --
// a uniform +9. Matching on the line number therefore names the accessor exactly three
// const/non-const pairs too late. (The PhysicsModuleIO::InputBuffer block a few files over
// matches its DWARF lines EXACTLY, 276/279/282/302, which is why the drift went unnoticed.)
// ============================================================================================

// X360 0x8279DFC8 (R) -- const vehicle-input accessor (this + 16). ⭐ NEW 2026-08-10: the READ
// end of the create-event pipe. WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics
// @0x827AAEC0 calls this, and PlaceOnTrackManager::PrePhysicsUpdate ->
// RaceCarEntityModule::ResetActiveRaceCar -> ActiveRaceCar::AddHandlingModel ->
// VehicleInputInterface::CreateRaceCar is what fills the interface it returns.
const OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PrePhysics::GetVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleInputInterface;
}

// X360 0x8279E070 (R) -- const vehicle-driver accessor (this + 142192).
const OutputBuffer_PrePhysics::VehicleDriverInputInterface*
OutputBuffer_PrePhysics::GetVehicleDriverInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleDriverInterface;
}

// X360 0x8279E118 (R) -- const vehicle-effects accessor (this + 147488).
const OutputBuffer_PrePhysics::VehicleEffectsInputInterface*
OutputBuffer_PrePhysics::GetVehicleEffectsInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleEffectsInterface;
}

// X360 0x8279E268 (R, :482) -- const game-event queue accessor (this + 149312).
const OutputBuffer_PrePhysics::GameEventQueue*
OutputBuffer_PrePhysics::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGameEventQueue;
}

// X360 0x822B5CA8 (W, :483) -- mutable game-event queue accessor.
OutputBuffer_PrePhysics::GameEventQueue*
OutputBuffer_PrePhysics::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameEventQueue;
}

// ---- InputBuffer_PostPhysics ------------------------------------------------

// X360 0x8279E310 (W, :520) -- CORRECTION (1): the NON-const GetSceneInputInterface().
// Write-lock (status>>3 &1) => mutable getter; returns &mSceneInputInterface (this+29856).
// (This is the function the byfile's line-1006 FN mislabeled as GetContactSpyInterface.)
OutputBuffer_PreScene::SceneInputInterface*
InputBuffer_PostPhysics::GetSceneInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mSceneInputInterface;
}

// X360 0x822B5F48 (R, :522) -- const per-entity-module deformation-output accessor.
const InputBuffer_PostPhysics::DeformationOutputInterfaceForEntityModules*
InputBuffer_PostPhysics::GetDeformationOutputInterfaceForEntityModules() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterfaceForEntityModules;
}

// X360 0x822B5FF0 (R, :525) -- const deformation-output accessor (distinct from the
// AI-racecar accessor at 0x822B6290).
const InputBuffer_PostPhysics::DeformationOutputInterface*
InputBuffer_PostPhysics::GetDeformationOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterface;
}

// X360 0x822B6140 (R, :528) -- const contact-spy accessor.
// CORRECTION (2): this is the ONLY GetContactSpyInterface getter (no non-const overload).
const InputBuffer_PostPhysics::ContactSpyInterface*
InputBuffer_PostPhysics::GetContactSpyInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mContactSpyInterface;
}

// X360 0x822B6290 (R, :531) -- const AI race-car interface accessor (last member).
const InputBuffer_PostPhysics::AIRaceCarInterface*
InputBuffer_PostPhysics::GetAIRaceCarInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mAIRaceCarInterface;
}

// ---- OutputBuffer_PostPhysics -----------------------------------------------

// X360 0x822B5D50 (W, :567) -- mutable scene-input accessor.
OutputBuffer_PreScene::SceneInputInterface*
OutputBuffer_PostPhysics::GetSceneInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mSceneInputInterface;
}

// X360 0x8279E7xx (R, :569) / 0x822B6xxx (W, :570) -- the NEW-VEHICLE event interface.
// Bodied 2026-08-02 (camera parameter-chain wave): the pair had stayed declaration-only
// because nothing produced or consumed the queue. Both ends land in this wave --
// RaceCarEntityModule::PublishNewVehicleToDirectorWithoutPhysicsBringUp writes it (standing
// in for the absent physics create-vehicle completion) and
// WorldModule::BridgeEntityModulesToOutput_PostPhysics reads it out. Same shape as every
// sibling: lock tripwire then &member.
const OutputBuffer_PostPhysics::DirectorVehicleInputInterface*
OutputBuffer_PostPhysics::GetDirectorVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDirectorVehicleInputInterface;
}

OutputBuffer_PostPhysics::DirectorVehicleInputInterface*
OutputBuffer_PostPhysics::GetDirectorVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mDirectorVehicleInputInterface;
}

// X360 0x8279E720 (R, :572) / 0x822B6530 (W, :573) -- the LIVE active-race-car output
// accessors. Bodied 2026-08-01: the pair had stayed declaration-only because nothing
// produced or consumed the interface; RaceCarEntityModule::UpdateOutputInterfaces (the
// producer) and WorldModule::BridgeRaceCarEntityInfoToOutput_PostPhysics (the consumer)
// now do. Same shape as every sibling: lock tripwire then &member.
const RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetActiveRaceCarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mActiveRaceCarOutputInterface;
}

RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetActiveRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mActiveRaceCarOutputInterface;
}

// X360 0x8279E7C8 (R, :575) / 0x822B65D8 (W, :576) -- the LIVE global-race-car output
// accessors (same note as the pair above).
const RCEntityGlobalRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetGlobalRaceCarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGlobalRaceCarOutputInterface;
}

RCEntityGlobalRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetGlobalRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGlobalRaceCarOutputInterface;
}

// X360 0x8279E678 (R, :578) -- const replay active-race-car output accessor.
const RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetReplayActiveRaceCarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mReplayActiveRaceCarOutputInterface;
}

// X360 0x822B6488 (W, :579) -- mutable replay active-race-car output accessor
// (pairs with the const overload above at the identical +826992 offset).
RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetReplayActiveRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mReplayActiveRaceCarOutputInterface;
}

// X360 0x822B6920 (W, :582) -- mutable replay global-race-car output accessor.
RCEntityGlobalRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetReplayGlobalRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mReplayGlobalRaceCarOutputInterface;
}

// X360 0x8279E9C0 (R, :590) -- const vehicle-input accessor (re-exposed on PostPhysics).
const OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PostPhysics::GetVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleInputInterface;
}

// X360 0x822B6878 (W, :591) -- mutable vehicle-input accessor (pairs with the const
// overload above at the identical +855152 offset).
OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PostPhysics::GetVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleInputInterface;
}

// ---- InputBuffer_GenerateDispatchLists --------------------------------------

// X360 0x822B69C8 (R, :627) -- const camera-input accessor.
const BrnDirector::Camera::Camera*
InputBuffer_GenerateDispatchLists::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mCameraInput;
}

// X360 0x822B6B20 (R) -- const trailing dispatch flag A (placeholder name; byte at +8289).
bool
InputBuffer_GenerateDispatchLists::GetDispatchFlagA() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbDispatchFlagA;
}

// X360 0x822B6BD0 (R) -- const trailing dispatch flag B (placeholder name; byte at +8290).
bool
InputBuffer_GenerateDispatchLists::GetDispatchFlagB() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbDispatchFlagB;
}

// ============================================================================
// Wave 6 accessors (23 funcs) -- the Set*/Get* bodies the X360 build emitted
// out-of-line for the RaceCarEntityModuleIO input buffers. Verified against
// BURNOUT_X360_ARTIST.XEX. The two InputBuffer_PostScene setters reproduce the
// X360 rodata trailing "\n" on their write-lock asserts (verifier correction);
// the rest follow the sibling-body convention already in this file.
// ============================================================================

// ---- InputBuffer_PreScene ---------------------------------------------------

// X360 0x822B4A38 (R, :154) -- const timer-status accessor (IDA truncates the
// mangled name to "Ge"). Returns &mTimerStatusInterface (this+0x5C); member held
// BY VALUE (DWARF BrnRaceCarEntityModuleIO.h dump :53).
const TimerStatusInterface*
InputBuffer_PreScene::GetTimerStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTimerStatusInterface;
}

// X360 0x822B4AE0 (R, :157) -- const camera-input accessor (IDA "GetCam").
// Returns &mCameraInput (this+0x90).
const BrnDirector::Camera::Camera*
InputBuffer_PreScene::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mCameraInput;
}

// X360 0x822B4CD8 (R, :166) -- const active-payback-type accessor (IDA
// "GetActivePaybackTy"). Returns the enum value meActivePaybackType (this+0x363C).
BrnNetwork::EPaybackType
InputBuffer_PreScene::GetActivePaybackType() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return meActivePaybackType;
}

// X360 0x822B4D80 (R, :169) -- const active-payback-aggressor accessor. Returns
// the enum value meActivePaybackAggressor (this+0x3640).
EActiveRaceCarIndex
InputBuffer_PreScene::GetActivePaybackAggressor() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return meActivePaybackAggressor;
}

// X360 0x8279CFA8 (W, :161) -- copy player vehicle-controls into the buffer
// (memcpy 60 bytes; this+0x1F0). PlayerVehicleControls is a 60-byte POD.
void
InputBuffer_PreScene::SetPlayerVehicleControls(const BrnWorld::PlayerVehicleControls* pControls)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mPlayerVehicleControls = *pControls;
}

// X360 0x8279D258 (W, :174) -- snapshot the replay status interface into the
// buffer (StatusInterface::operator= into mReplayStatusInterface at this+0x3644).
// Member held BY VALUE (DWARF BrnRaceCarEntityModuleIO.h dump :79).
void
InputBuffer_PreScene::SetReplayStatusInterface(const InputBuffer_PreScene::ReplayStatusInterface* pStatus)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mReplayStatusInterface = *pStatus;
}

// ---- ADDITIVE GROW (2026-08-11, WorldBridgeInputToEntityModules mount) -------
// The four remaining out-of-line InputBuffer_PreScene mutators that bridge drives.
// Every one is the family shape: write-lock assert (status>>3 &1, "Not locked for
// writing", the X360 baked line of this buffer's own header) then the member write.

// X360 0x8279CE98 (W, :155) -- snapshot the frame's timer status into the buffer.
// The console copies FIELD-FOR-FIELD in two 6-field blocks at member+0 and member+0x18
// (v2[23]=*a2 ... v2[34]=*(a2+44), base = this+0x5C) -- i.e. CgsSystem::
// TimerStatusInterface::operator= over its two 24-byte TimerStatus sub-blocks, NOT a
// flat 48-byte memcpy (which would drag each block's trailing bool pad). Same call
// shape as the traffic twin @0x8279FAD8. Member held BY VALUE (DWARF :53).
void
InputBuffer_PreScene::SetTimerStatusInterface(const TimerStatusInterface* lpTimerStatusInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTimerStatusInterface = *lpTimerStatusInterface;
}

// X360 0x827A9790 (W, :158) -- latch the director's camera for the frame. The console
// tail is a single `BrnDirector::Camera::Camera::operator=(this + 144, src)` -- the
// committed Camera's own copy-assignment -- and +144 (0x90) is mCameraInput, the offset
// the const twin @0x822B4AE0 returns. Sole producer: BridgeInputToEntityModules
// @0x827ADF88, which passes WorldModule::GetLastCameraInput().
void
InputBuffer_PreScene::SetCameraInput(const BrnDirector::Camera::Camera* lpCameraInput)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mCameraInput = *lpCameraInput;
}

// X360 0x8279D108 (W, :167) -- store the active payback type (console `v2[3471] = a2`
// == the word at this+0x363C == meActivePaybackType, the member the const getter
// @0x822B4CD8 reads).
void
InputBuffer_PreScene::SetActivePaybackType(BrnNetwork::EPaybackType lePaybackType)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    meActivePaybackType = lePaybackType;
}

// X360 0x8279D1B0 (W, :170) -- store the active payback aggressor (console
// `v2[3472] = a2` == this+0x3640 == meActivePaybackAggressor, read back by
// @0x822B4D80).
void
InputBuffer_PreScene::SetActivePaybackAggressor(EActiveRaceCarIndex leAggressor)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    meActivePaybackAggressor = leAggressor;
}

// ---- InputBuffer_PostScene --------------------------------------------------

// X360 0x827ACA40 (W, :344) -- publishes the crash module's race-car crash-complete
// events into this buffer's CrashInterface. The X360 body write-asserts, INLINES
// RaceCarOutputInterface::Clear() (miLength = 0 on the first-member event queue) and
// calls the de-inlined BaseEventQueue<RaceCarCrashCompleteEvent>::Append (0x827A7D70,
// the committed instantiation) to block-copy the source's live events. CrashInterface
// (BrnWorld::CrashIO::RaceCarOutputInterface) is carried as an opaque sized slice here;
// its queue is the interface's first member, so &mCrashInterface aliases &(its queue),
// matching the X360 `addi r3, this, 8` used as both the Clear target and Append `this`.
void
InputBuffer_PostScene::SetCrashInterface(const CrashInterface* lpCrashInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

    typedef CgsModule::EventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent, 10>
        RaceCarCrashCompleteEventQueue;
    RaceCarCrashCompleteEventQueue& lrDestQueue =
        *reinterpret_cast<RaceCarCrashCompleteEventQueue*>(&mCrashInterface);
    const RaceCarCrashCompleteEventQueue& lrSourceQueue =
        *reinterpret_cast<const RaceCarCrashCompleteEventQueue*>(lpCrashInterface);

    lrDestQueue.Clear();                 // inlined RaceCarOutputInterface::Clear() -> miLength = 0
    lrDestQueue.Append(lrSourceQueue);   // 0x827A7D70 BaseEventQueue<...>::Append
}

// X360 0x827BAF30 (W, :347) -- publishes the pre-scene traffic->racecar interface into
// this buffer by value (memcpy dst this+0xC0, size 0x220 == 544 == sizeof). Byte-exact
// via the canonical 544-byte BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene.
void
InputBuffer_PostScene::SetTrafficToRaceCarInterface_PreScene(
        const TrafficToRaceCarInterface_PreScene* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mTrafficToRaceCarInterface_PreScene, lpInterface,
                sizeof(mTrafficToRaceCarInterface_PreScene));   // X360: 0x220 (544) bytes
}

// ---- InputBuffer_PrePhysics -------------------------------------------------

// X360 0x822B5AA0 (R, :433) -- const controller-active flag accessor (byte at +212213).
bool
InputBuffer_PrePhysics::GetControllerActive() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbControllerActive;
}

// X360 0x822B5B50 (R, :436) -- const hard-stop-camera flag accessor (byte at +212214).
bool
InputBuffer_PrePhysics::GetInHardStopCamera() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbInHardStopCamera;
}

// X360 0x827ACAF8 (W, :419) -- empties both of the member interface's event rings
// (mResetPosResultEventQueue, then mPlaceOnTrackRequestQueue) and Appends the source
// interface's matching rings onto them.
void
InputBuffer_PrePhysics::SetAIModuleResultInterface(const AIModuleResultInterface* lpAIModuleResultInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mAIModuleResultInterface.GetResetOnTrackResultQueue()->Clear();
    mAIModuleResultInterface.GetResetOnTrackResultQueue()->Append(
        *lpAIModuleResultInterface->GetResetOnTrackResultQueue());
    mAIModuleResultInterface.GetPlaceOnTrackRequestQueue()->Clear();
    mAIModuleResultInterface.GetPlaceOnTrackRequestQueue()->Append(
        *lpAIModuleResultInterface->GetPlaceOnTrackRequestQueue());
}

// X360 0x827A9840 (W, :413) -- drops the standing potential-contact queue (miLength = 0)
// then Appends the caller's queue onto the freshly-emptied member ring.
void
InputBuffer_PrePhysics::SetPotentialContactQueue(const PotentialContactQueue* lpPotentialContactQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mPotentialContactQueue.Clear();
    mPotentialContactQueue.Append(*lpPotentialContactQueue);
}

// X360 0x8279DDB0 (W, :431) -- single-byte copy of the source
// TrafficToRaceCarInterface_PostScene (a 1-byte muDUMMY payload) into the member
// (this+0x33CF4 == &mTrafficToRaceCarInterface_PostScene).
void
InputBuffer_PrePhysics::SetTrafficToRaceCarInterface_PostScene(const TrafficToRaceCarInterface_PostScene* lpTrafficToRaceCarInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTrafficToRaceCarInterface_PostScene = *lpTrafficToRaceCarInterface;
}

// ---- ADDITIVE GROW (2026-08-11, WorldBridgeInputToEntityModules mount) -------
// The five remaining out-of-line InputBuffer_PrePhysics mutators that bridge drives.
// The console offsets they touch pin the tail of this buffer end-to-end and agree with
// the member list: mTakedownEventQueue @+208976 (336 B) -> mScoringInterface @+209312
// (2736 B) -> mOnlineScoringInterface @+212048 (164 B) ->
// mTrafficToRaceCarInterface_PostScene @+212212 (1 B) -> mbControllerActive @+212213 ->
// mbInHardStopCamera @+212214.

// X360 0x827A98F8 (W, :431) -- replace the standing takedown-event queue with the
// caller's. The console body is `*(_DWORD *)(this + 208984) = 0` (== member+8 ==
// BaseEventQueue::miLength, i.e. the inlined Clear()) followed by
// `BrnGameState::TakedownEvent_::Append(this + 208976, src)` -- the committed
// CgsModule::EventQueue<BrnGameState::TakedownEvent,8> (== BaseEventQueue<TakedownEvent>)
// instantiation's own Append merge, called here by name. (The member was a 256-byte
// opaque blob until this wave -- see the GROWN note in BrnRaceCarEntityModuleIOQueues.h;
// Appending through the blob would have overrun into mScoringInterface.)
void
InputBuffer_PrePhysics::SetTakedownEventQueue(const TakedownEventQueue* lpTakedownEventQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mTakedownEventQueue.Clear();
    mTakedownEventQueue.Append(*lpTakedownEventQueue);
}

// X360 0x8279DC40 (W, :434) -- copy the frame's scoring output block into the buffer
// (console memcpy of the 2736-byte interface into this+209312 == &mScoringInterface).
// [EVIDENCE NOTE] 0x8279DC40 is a HOLE in the .ida-exports dump. It is the middle member
// of this buffer's uniform 0xB8-stride setter run -- 0x8279DB98 (the write-lock
// GetSceneResultQueue, X360 line 425) + 0xA8, and 0x8279DC40 + 0xB8 == 0x8279DCF8
// (SetOnlineScoringInterface) -- and the 2736-byte block it must fill is exactly the gap
// the two bracketing offsets leave (212048 - 209312). Shape taken from its immediate
// sibling below, which is the same memcpy-of-a-flat-interface body.
void
InputBuffer_PrePhysics::SetScoringInterface(const ScoringInterface* lpScoringInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    std::memcpy(&mScoringInterface, lpScoringInterface, sizeof(mScoringInterface));
}

// X360 0x8279DCF8 (W, :437) -- copy the frame's online-scoring output block into the
// buffer. Console: `addis r3,r28,3 / addi r3,r3,0x3C50` == this+0x33C50 (212048) ==
// &mOnlineScoringInterface, `li r5,0xA4` == 164 bytes == sizeof the committed
// BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface.
void
InputBuffer_PrePhysics::SetOnlineScoringInterface(const OnlineScoringInterface* lpOnlineScoringInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    std::memcpy(&mOnlineScoringInterface, lpOnlineScoringInterface,
                sizeof(mOnlineScoringInterface));   // X360: 0xA4 (164) bytes
}

// X360 0x8279DE68 (W, :443) -- single byte store at this+212213 == mbControllerActive
// (the flag the const getter @0x822B5AA0 reads back).
void
InputBuffer_PrePhysics::SetControllerActive(bool lbControllerActive)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mbControllerActive = lbControllerActive;
}

// X360 0x8279DF18 (W, :446) -- single byte store at this+212214 == mbInHardStopCamera
// (read back by @0x822B5B50). The bridge feeds it bit 0x100 of the last camera input's
// state-flag word.
void
InputBuffer_PrePhysics::SetInHardStopCamera(bool lbInHardStopCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mbInHardStopCamera = lbInHardStopCamera;
}

// ---- InputBuffer_PostPhysics ------------------------------------------------

// X360 0x8279E3B8 (W, :529) -- single-word store of the source into mContactSpyInterface
// (this+0xD6B20). ContactSpyInterface is a 4-byte type; the store is the whole copy.
void
InputBuffer_PostPhysics::SetContactSpyInterface(const ContactSpyInterface* lpContactSpyInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mContactSpyInterface = *lpContactSpyInterface;
}

// X360 0x827A9A68 (W, :526) -- DeformationOutputInterface::operator= on this+0xD4030
// (mDeformationOutputInterface).
void
InputBuffer_PostPhysics::SetDeformationOutputInterface(const DeformationOutputInterface* lpDeformationOutputInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mDeformationOutputInterface = *lpDeformationOutputInterface;
}

// X360 0x827ACC78 (W, :517) -- VehicleManagerOutputInterface::operator= on this+0x6C20
// (mVehicleManagerOutputInterface).
void
InputBuffer_PostPhysics::SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mVehicleManagerOutputInterface = *lpVehicleManagerOutputInterface;
}

// X360 0x827ACBC8 (W, :514) -- VehicleOutputInterface::operator= on this+0x10
// (mVehicleOutputInterface).
void
InputBuffer_PostPhysics::SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mVehicleOutputInterface = *lpVehicleOutputInterface;
}

// ---- InputBuffer_GenerateDispatchLists --------------------------------------

// X360 0x822B6A70 (R, :633) -- const dispatch-frame accessor (returns the pointer VALUE
// stored at this+0x8180 == mpDispatchFrame).
CgsGraphics::DispatchFrame*
InputBuffer_GenerateDispatchLists::GetDispatchFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpDispatchFrame;
}

// X360 0x822B6C80 (R, :642) -- const shadow-map accessor (returns the pointer VALUE stored
// at this+0x818C == mpShadowMap). IDA 'GetShad' truncation == GetShadowMap.
BrnWorld::ShadowMap*
InputBuffer_GenerateDispatchLists::GetShadowMap() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpShadowMap;
}

// X360 0x8279EB18 (W, :637) -- write-lock store of the blobby-shadow buffer pointer
// (this+0x8184 == mpBlobbyShadowBuffer).
void
InputBuffer_GenerateDispatchLists::SetBlobbyShadowBuffer(
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobbyShadowBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpBlobbyShadowBuffer = lpBlobbyShadowBuffer;
}

// X360 0x827A9BF8 (W, :628) -- write-lock copy-assign of the camera input (this+0x10 ==
// mCameraInput) via BrnDirector::Camera::Camera::operator=.
void
InputBuffer_GenerateDispatchLists::SetCameraInput(const BrnDirector::Camera::Camera* lpCameraInput)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mCameraInput = *lpCameraInput;
}

// X360 0x8279EBC8 (W, :640) -- write-lock store of the corona submission interface pointer
// (this+0x8188 == mpCoronaSubmissionInterface).
void
InputBuffer_GenerateDispatchLists::SetCoronaSubmissionInterface(
        BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpCoronaSubmissionInterface = lpCoronaSubmissionInterface;
}

// X360 0x8279EA68 (W, :634) -- write-lock store of the dispatch frame pointer (this+0x8180
// == mpDispatchFrame; same offset GetDispatchFrame @0x822B6A70 reads).
void
InputBuffer_GenerateDispatchLists::SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpDispatchFrame = lpDispatchFrame;
}

// X360 0x8279EC78 (W, :643) -- RECOVERED FROM AN EXPORT HOLE, not from a named export.
// The four setters of this buffer are emitted as one 0xB0-byte-apiece run:
//   0x8279EA68 SetDispatchFrame            (last insn 0x8279EB14)
//   0x8279EB18 SetBlobbyShadowBuffer       (last insn 0x8279EBC4)
//   0x8279EBC8 SetCoronaSubmissionInterface(last insn 0x8279EC74)
//   0x8279EC78 <hole>                      -- next export is 0x8279ED28, exactly 0xB0 on
// so the run has a fourth member the .ida-exports set does not carry. It is SetShadowMap:
// the DWARF declares the four in exactly this order (:634, :637, :640, :643), and
// GetShadowMap @0x822B6C80 reads the very member left unwritten (this+0x818C). The body
// is the same two statements as its three neighbours.
void
InputBuffer_GenerateDispatchLists::SetShadowMap(BrnWorld::ShadowMap* lpShadowMap)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpShadowMap = lpShadowMap;
}

}
}
