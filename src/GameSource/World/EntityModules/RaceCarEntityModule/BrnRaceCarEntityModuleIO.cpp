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

// X360 0x8279D850 (R, :307) -- const per-car audio (un)load REQUEST queue
// accessor. WorldModule::BridgeRaceCarEntityInfoToOutput_PreScene reads this queue
// after RaceCarAudioStreamer has filled it for the frame.
const RaceCarEntityModuleIO::AudioCarLoadedDataQueue*
OutputBuffer_PreScene::GetAudioCarLoadedDataQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
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

// ⭐ X360 0x8279D458 (R, :285) -- the CONST scene-input accessor, the read-locked twin of
// the :286 writer above. BODIED 2026-08-19 (wave Q5 cluster F3). Its per-address JSON was an
// export-run gap, closed with a targeted headless-IDA run on a private .i64 copy
// (scratchpad/waveQ5/ida_f3/); everything below is off that dump, not inferred:
//   * `lbz r11,0(r28); extrwi r11,r11,1,27` -- bit 4 of the status byte
//     (eStatusLockedForRead) => IsBufferLockedForReading(), i.e. the CONST overload;
//   * the baked assert cites BrnRaceCarEntityModuleIO.h line 0x11D == 285, which is exactly
//     this declaration's DWARF line. The whole OutputBuffer_PreScene read ladder lands on its
//     DWARF lines with NO skew -- 282 (+16 mVehicleInputInterface) / 285 (+142192, here) /
//     288 (+960960) / 291 (+971440) / 294 (+973856) / 300 (+986752) -- so the slot is pinned
//     by two independent facts, not by position alone. (The +9 X360-vs-DWARF line skew this
//     header records applies to InputBuffer_PostPhysics and later, not here.)
//   * epilogue `addis r3,r28,2 ; addi r3,r3,0x2B70` == this + 0x22B70 == 142192, the same
//     displacement OutputBuffer_PreScene::Construct names for the
//     InSceneUpdateInterface::Construct(+142192) leg. Reproduced BY NAME as
//     &mSceneInputInterface, never as an offset.
// SOLE caller: WorldModule::BridgeEntityModulesToSceneModule_PreScene @0x827AB490 (the
// race-car leg, `bl sub_8279D458` at 0x827AB590) -- which is why nothing needed it before.
const OutputBuffer_PreScene::SceneInputInterface*
OutputBuffer_PreScene::GetSceneInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
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

// =================================================================================================
// OutputBuffer_PostScene::Construct   X360 0x822EA678   -- RETIRES A memset BOOT GATE, AND THE
// GATE WAS A LIVE DEFECT (resetpump wave 2026-08-26). MEASURED, run rp_crash1:
//
//     [ASSERT 1] mpEvents != NULL (CgsBaseEventQueue.h:35)
//       CgsDev::Assert::FireAssert
//       BrnWorld::RaceCarEntityModule::SendResetOnTrackRequests      <- THE FIRST PRODUCER, EVER
//       BrnWorld::RaceCarEntityModule::PostSceneUpdate
//
// i.e. the crash exit raised mbToBeResetOnTrack, SendResetOnTrackRequests read it and tried to
// AddEvent a ResetOnTrackRequest -- into a queue whose mpEvents had never been pointed at its
// inline storage, because WorldLinkStubs.cpp answered this buffer's Construct with
// `memset(this, 0, sizeof(*this))`.
// ⭐⭐ SEVENTH sighting of this exact shape in this buffer family, and the crash-exit wave's own
// note was already written for it: A memset IS WORSE THAN NO STUB -- zeroing a queue is not
// constructing it, and it LOOKS like initialisation. ⭐ AN UNCONSTRUCTED BUFFER IS INVISIBLE
// UNTIL SOMETHING PUTS DATA IN IT: un-gating a producer CREATES the fault, it does not reveal it.
//
// Console body (r31 == this; the pseudocode types it float*, so the displacements below are the
// float indices x4):
//   0x822EA678  *this = 1                                    IOBuffer::Construct
//   +4          VariableEventQueue<16384,16>::Construct       mSceneCoarseQueryQueue
//   +16416      InEventLineTestFine<256>::Construct          mSceneFineLineTestQueue
//   +32816      ResetOnTrackRequest<128>::Construct          mAIModuleRequestInterface  ⭐
//   +34880      CreateRivalInTrafficSystemEvent<34>::Construct  \ both inside
//   +36528      RemoveRivalFromTrafficSystemEvent<34>::Construct/ mRaceCarToTrafficInterface
//   +36576      muFlags = 0 ; +36580 mfShowtimeTrafficDensityScale = 1.0f
// The offsets close exactly on the committed member sizes (4 + 16400 -> 16416;
// + 16400 -> 32816; + 16 + 128*16 -> 34880), which is what identifies each leg.
//
// PARTIAL SLICE, and every leg it does not run is NAMED here rather than left unmentioned:
//   [FLAG] mSceneCoarseQueryQueue / mSceneFineLineTestQueue are 16400-byte `maReserved` blobs in
//     this tree (CgsSceneManagerIO_CoarseQuery.h / CgsSceneManagerModuleIO.h) with no Construct
//     to call. They keep the zero the memset below gives them -- EXACTLY what they had before
//     this change, so nothing regresses; they gain a real Construct with their own layout.
//   [FLAG] mRaceCarToTrafficInterface's two queues + muFlags/mfShowtimeTrafficDensityScale:
//     RaceCarToTrafficInterface::Construct (DWARF :130) is declaration-only in this tree and its
//     members are private with const-only accessors, so there is no by-name route to them.
//     Same disposition, same zero, same debt.
//   ⚠️ mfShowtimeTrafficDensityScale therefore reads 0.0f here where the console reads 1.0f.
//     That is UNCHANGED from the retired gate (which zeroed it too) and its only consumer is the
//     showtime traffic-density publish, itself a parked leg of PostSceneUpdate.
// DELETE-WHEN those three types get real Constructs; then the memset goes too.
// =================================================================================================
void
OutputBuffer_PostScene::Construct()
{
    // [FLAG PC] the retired gate's zero-fill, kept for the three members above.
    std::memset(this, 0, sizeof(*this));

    CgsModule::IOBuffer::Construct();                                   // X360 *this = 1

    // ⭐ X360 +32816 -- the ONLY leg with a reachable, real Construct today, and the one the
    // reset-on-track pump posts into every time a crashed car asks to be put back.
    mAIModuleRequestInterface.GetResetOnTrackRequestQueue()->Construct();
}


// X360 0x822B5608 (W, :380) -- mutable AI module-request accessor.
OutputBuffer_PostScene::AIModuleRequestInterface*
OutputBuffer_PostScene::GetAIModuleRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mAIModuleRequestInterface;
}

// X360 0x8279DA48 (R, :379) -- the const read-lock twin of the accessor above. LANDED
// 2026-08-26 (resetpump wave): WorldModule::BridgeRaceCarModuleToAIModule_PostScene @0x827AD688
// calls exactly this (`mr r3, r29 ; bl sub_8279DA48`) and hands the result to
// AIModuleIO::InputBuffer::AppendAIModuleRequestInterface. It had no body; the bridge was the
// first caller in the tree.
const OutputBuffer_PostScene::AIModuleRequestInterface*
OutputBuffer_PostScene::GetAIModuleRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
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

// X360 0x822B5800 (R, :418) -- const AI-module result accessor (this + 196656 ==
// &mAIModuleResultInterface; 196656 is mSceneResultQueue's end rounded to 16). LANDED
// 2026-08-26 (resetpump wave). It was declaration-only, and its first caller is
// RaceCarEntityModule::ProcessResetOnTrackResultQueue @0x822F4580, which reaches BOTH of the
// interface's rings through it -- the reset-on-track results at +0 and the place-on-track
// requests at +0x1810 (== 0x10 + 128*48, i.e. the second member).
// This is the address that used to be cited on GetOnlineScoringInterface (corrected
// 2026-08-11); the header's member annotation still carried the stale citation until this wave.
const InputBuffer_PrePhysics::AIModuleResultInterface*
InputBuffer_PrePhysics::GetAIModuleResultInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mAIModuleResultInterface;
}

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

// ⭐ X360 0x822B5950 (R, DWARF :424 / X360 baked line 433) -- const scoring accessor
// (X360 tail `addis r3, r28, 3 ; addi r3, r3, 0x31A0` == this + 209312 == &mScoringInterface).
// LANDED 2026-08-11 (player-input wave): it was declaration-only, and
// RaceCarEntityModule::ProcessPlayerVehicleInput @0x82300494 is its first caller (the
// online-race catch-up arm asserts on it by name -- "lpInput->GetScoringInterface()").
// The +9 PS3-DWARF-line vs X360-baked-line skew this file documents above is what identifies
// it: baked 433 - 9 == DWARF :424, the const GetScoringInterface. (The comment on the
// TrafficToRaceCarInterface_PostScene member in the header still cites 0x822B5950; that
// citation is the pre-correction one and this body is the offset-proven owner of the address.)
// (BODY NOT HERE: the byte-identical definition already lives in the mounted
// BrnRaceCarEntityModuleIO_PreSceneAccessors.cpp:94 with the same +9-skew correction --
// a second copy landed here 2026-08-11 and was deleted by the conductor on the LNK2005.)

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

// ⭐ X360 0x822B5CA8 (W, DWARF :474 / X360 baked line 483) -- mutable vehicle-driver accessor
// (X360 tail `addis r3, r28, 2 ; addi r3, r3, 0x2B70` == this + 142192 ==
// &mVehicleDriverInterface). LANDED 2026-08-11 (player-input wave); it was declaration-only and
// RaceCarEntityModule::ProcessPlayerVehicleInput @0x82300148/0x82300704 is its caller -- both
// the per-stompee AddTargetAssist loop and the final
// AddEvent<BrnPlayerDriverControls> go through it.
//
// ⛔ ANNOTATION DEBT PAID: the "X360 0x822B5CA8 (W, :483)" line used to sit on the NON-CONST
// GetGameEventQueue() below. Same +9 skew the READ-lock block above documents in full: baked
// 483 - 9 == DWARF :474, which is this function, not :483. The returned offset settles it --
// 142192 is mVehicleDriverInterface (the offset this class's own Construct comment names),
// while mGameEventQueue is at +149312.
OutputBuffer_PrePhysics::VehicleDriverInputInterface*
OutputBuffer_PrePhysics::GetVehicleDriverInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleDriverInterface;
}

// X360 (W, :477) -- the mutable vehicle-EFFECTS accessor, the write half of the pair whose read
// half is 0x8279E118 (+147488) above. LANDED 2026-08-29 (crash-play wave): it was
// declaration-only, and CrashPlayManager::UpdateTrafficStomp is its first caller -- the console
// inlines VehicleEffectsInputInterface::CreateAirRam there and reaches this interface's air-ram
// queue directly (`bl RaceCarEntityModuleIO::OutputBuffer @0x822F9150` then
// BaseEventQueue<CreateAirRamEvent>::AddEventSafe).
BrnPhysics::Vehicle::VehicleEffectsInputInterface*
OutputBuffer_PrePhysics::GetVehicleEffectsInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleEffectsInterface;
}

// X360 sub_822B5DF8 (W, :480) -- the mutable player-reset accessor (write-lock bit 3; tail
// `addis r3, r28, 2 ; addi r3, r3, 0x4720` == this + 149280 == &mPlayerResetInterface).
// LANDED 2026-08-26 (resetpump wave): RaceCarEntityModule::ProcessResetOnTrackResultQueue calls
// it (`bl sub_822B5DF8` at 0x822F4718) and then stores the placed car's position and raises
// mbPlayerResetThisFrame -- i.e. SetPlayerResetPos, which the console inlines there.
RCEntityPlayerResetInterface*
OutputBuffer_PrePhysics::GetPlayerResetInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mPlayerResetInterface;
}

// X360 sub_822B5EA0 (W, :483) -- ATTRIBUTED 2026-08-29 (crash-play wave). It was carrying
// "X360 address NOT ATTRIBUTED (W)". The address is settled by the two things that identify any
// accessor in this class: the OFFSET IT RETURNS and the assert it fires. sub_822B5EA0 ends
// `return a1 + 149312` -- mGameEventQueue, exactly this function -- behind
// `if (((*a1 >> 3) & 1) == 0)` firing "Not locked for writing" at BrnRaceCarEntityModuleIO.h:492,
// which is the write-lock bit-3 test IsBufferLockedForWriting() compiles to. It is reached as
// `bl sub_822B5EA0` from all three CrashPlayManager event posters (UpdateCarLeaping @0x822F91F8,
// UpdateNewRoad @0x822F9254/0x822F9284, SetBouncePromptNeeded @0x822F92DC), each time with
// lpOutput in r3 and the result handed straight to VariableEventQueue<1536,16>::AddEvent.
// It is NOT 0x822B5CA8 -- that address returns +142192 and belongs to GetVehicleDriverInterface
// above. The body is unchanged and was always right (by-name &member); only the citation moved.
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

// ⚠️ ADDRESS PIN for the const getters below. The six out-of-line const getters are, by
// returned offset, 0x822B5F48/+16, 0x822B5FF0/+27680, 0x822B6098/+848624, 0x822B6140/+868400,
// 0x822B61E8/+879392 and 0x822B6290/+879408 -- exactly the member order below. Full proof
// (offsets, layouts, the 522/525/531/534/537/540 assert-line ladder) is in the header's banner.
// Attributions that omit 0x822B6098 or 0x822B61E8 slide every later address one-to-two rungs.

// X360 0x822B5F48 (R, :513 / X360 h:522, +16) -- const vehicle-output accessor. This is the
// interface ReadUpdatedActiveRaceCarDataFromPhysics reads every race car's published
// RaceCarState out of.
const InputBuffer_PostPhysics::VehicleOutputInterface*
InputBuffer_PostPhysics::GetVehicleOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleOutputInterface;
}

// X360 0x822B5FF0 (R, :516 / X360 h:525, +27680) -- const vehicle-manager-output accessor.
const InputBuffer_PostPhysics::VehicleManagerOutputInterface*
InputBuffer_PostPhysics::GetVehicleManagerOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleManagerOutputInterface;
}

// X360 0x822B6098 (R, :522 / X360 h:531, +848624) -- const per-entity-module
// deformation-output accessor.
const InputBuffer_PostPhysics::DeformationOutputInterfaceForEntityModules*
InputBuffer_PostPhysics::GetDeformationOutputInterfaceForEntityModules() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterfaceForEntityModules;
}

// X360 0x822B6140 (R, :525 / X360 h:534, +868400) -- const deformation-output accessor.
const InputBuffer_PostPhysics::DeformationOutputInterface*
InputBuffer_PostPhysics::GetDeformationOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterface;
}

// X360 0x822B61E8 (R, :528 / X360 h:537, +879392) -- const contact-spy accessor.
// CORRECTION (2): this is the ONLY GetContactSpyInterface getter (no non-const overload).
const InputBuffer_PostPhysics::ContactSpyInterface*
InputBuffer_PostPhysics::GetContactSpyInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mContactSpyInterface;
}

// X360 0x822B6290 (R, :531 / X360 h:540, +879408) -- const AI race-car accessor (last member).
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

// ⭐ X360 0x8279E5D0 (R, :566) -- the CONST scene-input accessor of OutputBuffer_PostPhysics.
// BODIED 2026-08-19 (wave Q5 cluster F3). ⭐ THIS IS THE ACCESSOR THE CAR'S WHOLE SCENE
// REGISTRATION HANGS OFF: ActiveRaceCar::AddToScene / AddToCollision /
// RaceCarEntityModule::GenerateSceneUpdateEvents stage AddEntity / AddDynamicVolume /
// AddVolumeInstance / AddForCollision / SetVolumeInstanceTransform into THIS buffer's
// mSceneInputInterface, and WorldModule::BridgeEntityModulesToScene_PostPhysics @0x827AB608
// (`bl sub_8279E5D0` at 0x827AB6E0) is the only thing that carries them into the scene
// manager's InputBuffer_Update. Off the raw asm:
//   * `lbz r11,0(r28); extrwi r11,r11,1,27` -- bit 4 (eStatusLockedForRead) =>
//     IsBufferLockedForReading(): the CONST overload, and the read lock is exactly what
//     WorldModule::Update holds on this buffer (LockBuffersForIO read-locks every source);
//   * baked assert line 0x23F == 575 == this declaration's DWARF line 566 + the +9 X360-vs-
//     DWARF skew this header's CORRECTION (3) measured for the post-physics buffers;
//   * epilogue `addi r3, r28, 0x2020` == this + 8224 -- the SAME displacement
//     OutputBuffer_PostPhysics::Construct names for its InSceneUpdateInterface::Construct
//     (+8224) leg. Two independent votes for the member. Returned BY NAME.
// ⚠️ REPORTED, NOT FIXED (this cluster does not own the header): the declaration comments on
// :777/:797 attribute the WRITE twin 0x822B5D50 to "+147488" for this same member. Both the
// console Construct and this read getter say +8224, so that citation is stale/mis-slotted --
// same class of one-slot drift as CORRECTION (3). The BODIES are unaffected (both return
// &mSceneInputInterface by name); only the citation is wrong.
const OutputBuffer_PreScene::SceneInputInterface*
OutputBuffer_PostPhysics::GetSceneInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
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

// [tut-ticker] X360 0x8279E268 (R, :482, +149312) / 0x822B5CA8 (W, :483) -- the game-event
// queue accessors. Bodied 2026-08-24: the pair had stayed declaration-only because nothing
// produced or consumed the queue; RaceCarEntityModule::SendGameEvents (the producer, the
// training-request drain) and WorldModule::BridgeRaceCarEntityInfoToOutput_PostPhysics (the
// consumer append into the world game-event queue) now do. Same shape as every sibling:
// lock tripwire then &member.
const RaceCarEntityModuleIO::GameEventQueue*
OutputBuffer_PostPhysics::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGameEventQueue;
}

RaceCarEntityModuleIO::GameEventQueue*
OutputBuffer_PostPhysics::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameEventQueue;
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

// X360 0x822B6B20 (R, DWARF :636) -- const blobby-shadow-buffer accessor. Returns the POINTER
// VALUE stored at this+0x8184 == mpBlobbyShadowBuffer, the member SetBlobbyShadowBuffer
// @0x8279EB18 writes:
//     0x822B6BBC  ori   r11, r11, 0x8184
//     0x822B6BC0  lwzx  r3, r28, r11
// (This body replaces the invented `GetDispatchFlagA` -- see the header banner.)
BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*
InputBuffer_GenerateDispatchLists::GetBlobbyShadowBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpBlobbyShadowBuffer;
}

// X360 0x822B6BD0 (R, DWARF :639) -- const corona-submission-interface accessor. Returns the
// POINTER VALUE stored at this+0x8188 == mpCoronaSubmissionInterface, the member
// SetCoronaSubmissionInterface @0x8279EBC8 writes (on the console seeded from the renderer's
// output buffer through GameBridgeRendererToX -> BrnWorldIO::DispatchInputBuffer; on this build
// BrnWorldModule::GenerateDispatchListsBringUp seeds it every frame beside SetShadowMap):
//     0x822B6C6C  ori   r11, r11, 0x8188
//     0x822B6C70  lwzx  r3, r28, r11
// (This body replaces the invented `GetDispatchFlagB` -- see the header banner.)
BrnCoronaManager::BrnSubmissionInterface*
InputBuffer_GenerateDispatchLists::GetCoronaSubmissionInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpCoronaSubmissionInterface;
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

// X360 0x822B4B88 (R, :160) -- const player vehicle-controls accessor. The console body is
// exactly this shape: the read-lock assert (`(*a1 >> 4) & 1`, message "Not locked for reading",
// citing BrnRaceCarEntityModuleIO.h:160 -- which is the line the header annotates this getter
// with) and then `return a1 + 496`, i.e. &mPlayerVehicleControls at this+0x1F0. 496 == 0x1F0
// confirms the member independently of the declaration order.
//
// ⛔ IT WAS DECLARE-ONLY, and the setter right below it was not. That asymmetry is why the
// player's pad state could be written into this buffer and never read back out of it: the one
// console caller of this getter is PreSceneUpdate @0x8230D928's `memcpy(a1 + 99240, v56, 60)`,
// the store that fills RaceCarEntityModule::mPlayerVehicleControls. Only a LINK found it.
const BrnWorld::PlayerVehicleControls*
InputBuffer_PreScene::GetPlayerVehicleControls() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPlayerVehicleControls;
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

// ====================================================================================
// InputBuffer_PostScene::Construct   X360 0x822EA5E8   [crash exit 2026-08-25]
//
//   *this = 1                                       -- IOBuffer::Construct (the status bit)
//   RaceCarCrashCompleteEvent_10_::Construct(this+8) -- mCrashInterface's event queue
//   then the inlined TrafficToRaceCarInterface_PreScene init over this+192..this+732
//   (the 7 `std 0x700000000` bit-array words, the two Array counts, the stompee count, the
//    static-vehicle count and the four 0.0f distances) -- i.e. that interface's own Construct,
//   which this tree already de-inlines by name in BrnTrafficToRaceCarInterface.h.
//
// ⭐⭐ THIS WAS MISSING, AND IT WAS THE SECOND HALF OF THE SAME DEFECT AS THE CRASH IO BUFFERS'.
// The buffer's Construct() call from CgsIOBufferStack::CreateIOBuffer<T> bound to the BASE
// CgsModule::IOBuffer::Construct, so mCrashInterface's EventQueue had mpEvents == nullptr.
// It was harmless for as long as SetCrashInterface's SOURCE was always empty -- which it was,
// because the crash module was inert. The FIRST frame this wave posted a real
// RaceCarCrashCompleteEvent, SetCrashInterface's Clear()+Append() wrote through the null and the
// process took an access violation inside memcpy.
// ⚠️ MEASURED: "access violation WRITING 0x0" at
//   memcpy +0x131 <- InputBuffer_PostScene::SetCrashInterface +0xEE
//                 <- WorldModule::EntityModulePostSceneUpdate
// immediately after the log line "[crash-exit] CRASH COMPLETE posted for active race car 0".
// ⭐ The general shape, twice in one wave: AN UNCONSTRUCTED BUFFER IS INVISIBLE UNTIL SOMETHING
// FINALLY PUTS DATA IN IT. Un-gating a producer is what creates the fault, not what reveals it.
void
InputBuffer_PostScene::Construct()
{
    CgsModule::IOBuffer::Construct();

    // The crash interface's first (and only) member is the crash-complete ring; the console
    // reaches it as `this + 8`, which is &mCrashInterface, and SetCrashInterface above already
    // documents that aliasing.
    typedef CgsModule::EventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent, 10>
        RaceCarCrashCompleteEventQueue;
    reinterpret_cast<RaceCarCrashCompleteEventQueue*>(&mCrashInterface)->Construct();

    mTrafficToRaceCarInterface_PreScene.Construct();
}

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

// (DWARF :343) -- the READ side of the pair above. [crash exit 2026-08-25] declared since this
// buffer landed and never defined, because until this wave NOTHING READ IT: the crash module's
// RaceCarCrashCompleteEvent ring arrived here every frame and was dropped on the floor.
// Its first consumer is RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents.
// The console folds it into that caller (`bl InputBuffer_Po` @0x822F3FFC returns the interface
// address directly), so there is no separate out-of-line symbol to cite -- it is a read-lock
// tripwire plus &mCrashInterface, the exact mirror of SetCrashInterface's write side.
const InputBuffer_PostScene::CrashInterface*
InputBuffer_PostScene::GetCrashInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mCrashInterface;
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

// DecFIGS declares this const accessor at BrnRaceCarEntityModuleIO.h:421.  It
// is inlined into ARTIST's RaceCarEntityModule::UpdateBoost, where the returned
// queue is immediately consumed through BaseEventQueue::GetEvent; the same
// read-lock assertion shape is emitted by the neighbouring const accessors.
const InputBuffer_PrePhysics::TakedownEventQueue*
InputBuffer_PrePhysics::GetTakedownEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTakedownEventQueue;
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

// =================================================================================================
// ⭐⭐ X360 0x822EA838 (47 insns, DWARF :510) -- InputBuffer_PostPhysics::Construct, PARTIAL SLICE.
// Its base-only boot gate in WorldLinkStubs.cpp is RETIRED by the same commit (same treatment, and
// for the same reason, as the InputBuffer_PrePhysics slice retired 2026-07-27).
//
// ⛔ WHY THIS HAD TO LAND WITH THE PUBLISH LEG, MEASURED NOT GUESSED. The gate ran only
// `CgsModule::IOBuffer::Construct()`, so every event queue embedded in this buffer stayed
// un-Constructed (mpEvents null, miMaxLength 0). That was invisible while
// BridgePhysicsModuleToRaceCarModule_PostPhysics was inert. The moment that bridge went live the
// FIRST boot run died here:
//     [ASSERT] Base event queue overflow (CgsBaseEventQueue.h:122)
//         BrnPhysics::Vehicle::VehicleManagerOutputInterface::operator=
//         WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics
//     [EXCEPTION] EXCEPTION_ACCESS_VIOLATION writing 0x0 in memcpy <- the same operator=
// -- because that operator= Clear()s and Append()s each destination queue, and Append into a
// never-Constructed queue memcpy's through a null mpEvents. Same family as the
// PhysicsModuleIO::OutputBuffer::Construct root cause (2026-08-10).
//
// The console body, decoded (r29 == this, r30 == this + 0x10 == &mVehicleOutputInterface):
//   0x822EA854  stb 1, 0(this)                       -- IOBuffer::Construct
//   0x822EA850..0x822EA88C  VehicleOutputInterface::Construct INLINED over +0x10: the traffic-state
//               queue (+0x2620), the impact queue (+0x2310), the game-event VariableEventQueue
//               <1536,16> (+0x65F0), then `std 0` over the used-cars bitset (+0) and five `stb 0`
//               over the aggressive-driving flags (+0x6C00..+0x6C04). That IS the committed
//               VehicleOutputInterface::Construct, so it is called by name.
//   0x822EA88C  VehicleManagerOutputInterface::Construct(this + 0x6C20)
//   0x822EA894  InSceneUpdateInterface::Construct(this + 0x74A0)                     [PARKED]
//   0x822EA8A4..0x822EA8E8  the deformation-for-entity-modules queues + counters, the deformation
//               output interface, the contact-spy interface and a trailing 16-byte zero  [PARKED]
//
// ⛔ PARKED, and the park is EXACTLY the bridge's park -- these are the same four legs
// BridgePhysicsModuleToRaceCarModule_PostPhysics cannot carry (opaque physics-output seats /
// const-accessor gap), so nothing writes into them and leaving them un-Constructed changes no
// observable. Constructing them here and NOT feeding them would be the misleading half.
// DELETE-WHEN those bridge legs land: this Construct must grow with them or the same overflow
// returns, one interface further along.
// =================================================================================================
void
InputBuffer_PostPhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
    mVehicleOutputInterface.Construct();
    mVehicleManagerOutputInterface.Construct();
    // ⭐ GROWN 2026-08-24 (deform-land wave, P1(b)) exactly per the DELETE-WHEN above: bridge
    // legs 3/4 are LIVE, so both deformation interfaces are now written every frame -- their
    // queues must be Constructed or the first copy overflows (the same family this banner
    // documents).
    mDeformationOutputInterfaceForEntityModules.Construct();
    mDeformationOutputInterface.Construct();
}

// X360 0x827A99B0 (W, :523) -- DeformationOutputInterfaceForEntityModules::operator= on
// this+0xCF3B0 (mDeformationOutputInterfaceForEntityModules). ADDED 2026-08-24 (deform-land
// wave, P1(b)) -- it was declaration-only while bridge leg 3 was parked.
void
InputBuffer_PostPhysics::SetDeformationOutputInterfaceForEntityModules(
        const DeformationOutputInterfaceForEntityModules* lpDeformationOutputInterfaceForEntityModules)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mDeformationOutputInterfaceForEntityModules = *lpDeformationOutputInterfaceForEntityModules;
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
