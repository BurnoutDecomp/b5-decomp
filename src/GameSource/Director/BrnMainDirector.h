#ifndef GAMESOURCE_DIRECTOR_BRN_MAIN_DIRECTOR_H
#define GAMESOURCE_DIRECTOR_BRN_MAIN_DIRECTOR_H

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameShared/GameClasses/Graphics/CgsCamera.h"          // CgsGraphics::Camera (mCgsCamera)
#include "GameSource/Director/BrnDirectorICEWrapper.h"          // BrnDirector::ICEWrapper (mICEWrapper)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h" // BrnDirector::Arbitrator (mArbitrator)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"     // Camera::BehaviourManager (mBehaviourManager)
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h" // BrnDirector::NamedParameters (mNamedParameters)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h" // BrnDirector::AllVehicleData (mAllVehicleData)
#include "GameSource/Director/Camera/BrnCameraFinaliser.h"      // BrnDirector::CameraFinaliser (mCameraFinaliser)
#include "GameSource/Director/Camera/Camera.h"                  // BrnDirector::Camera::Camera (mLastCamera)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h" // BrnDirector::GameState (maGameState)

// ============================================================================
// GameSource/Director/BrnMainDirector.h
//
// BrnDirector::MainDirector -- the TOP-LEVEL camera director: the spine of the
// cinematic camera system. It owns and drives the ICE wrapper, the camera ARBITRATOR
// (the top-level camera state machine), the active director Camera, the per-event
// GameState snapshot, the AllVehicleData tracker, and the camera-behaviour manager;
// each frame it pumps the staged-init Prepare/Release lifecycle and the Update spine
// that fans out to UpdateArbitrator / UpdateICE / UpdateMoments / UpdateAttribSys /
// UpdateCameraBehaviours* / UpdateDebug*.
//
// HOME for the 21-function MainDirector class TU (BrnMainDirector.cpp). The compilation
// home of the ENGINE source file path is "..\..\..\GameSource\Director/BrnMainDirector.cpp"
// (the X360 asserts quote it), so the reconstructed .cpp mirrors that path.
//
// ----------------------------------------------------------------------------
// ⚠️ LAYOUT MODEL -- REWRITTEN (BehaviourManager wave). READ THIS BEFORE EDITING.
//
// This class used to be `alignas(16) u8 maStorage[0x35450];` -- a CONSOLE-sized opaque
// buffer -- with every sub-object reached at its CONSOLE byte offset through a char* view.
// That model is UNSOUND on the x64 host and it is exactly why the previous wave had to hold
// `ICEWrapper::Construct` and `Arbitrator::Construct` back:
//
//     the host types are WIDER than their console placement windows (every embedded
//     pointer, vptr and Camera widens 4 -> 8), so constructing a host ICEWrapper at
//     console +0x50 or a host Arbitrator at console +0x12DC0 silently OVERRUNS the next
//     console region -- a memory stomp that would be extremely expensive to find later.
//
// It is NOT fixable by "sizing the window": the windows are defined by console offsets the
// host cannot honour. So the buffer is RETIRED. MainDirector is now a NAMED-MEMBER class
// declared in CONSOLE ORDER with HOST-NATIVE sizes -- the project's x64 rule ("semantic
// parity by named members, not byte offsets") and exactly the model the committed
// BrnDirectorModule.h already uses for the module itself.
//
// Consequences, all intended:
//   * `sizeof(MainDirector)` is NO LONGER 0x35450 and the old placement-window
//     static_assert is gone. Nothing depends on it: DirectorModule embeds MainDirector as a
//     named member (`MainDirector mMainDirector;`) and its own following members are named
//     too, so the module's layout re-flows automatically.
//   * The X360 CONSOLE offsets quoted per member below are PROVENANCE ONLY -- the evidence
//     for the member's identity and its position in the order. NOTHING indexes by them.
//   * Sub-objects whose types are not homed yet are declared as NAMED opaque regions sized
//     by their console span, with a named accessor where a reconstructed body has to hand
//     the region on (e.g. to ArbStateSharedInfo). That keeps them honest storage instead of
//     raw offsets, and each becomes a one-line swap when its type lands.
//
// FLAG: this class is NON-POLYMORPHIC -- the X360 ctor installs the embedded sub-objects'
//   vtables but stores NO vtable pointer at MainDirector +0x00 (DirectorDevTools occupies
//   the head; the lifecycle methods are called directly by DirectorModule, not virtually).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    struct DirectorInputOutput;   // GameSource/Director/DirectorModule/BrnDirectorInputOutput.h
    struct ArbStateSharedInfo;    // GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h
    class  DebugComponent;        // GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h
    class  DirectorResourceManager;
    namespace DirectorIO { struct OutputBuffer; struct InputBuffer; }

    class MainDirector
    {
    public:
        // ---- ctor (X360 0x827E4AB8, EXECUTED in goal trace) ----------------------------
        // Build the director: default-construct the owned sub-objects and seed the -1
        // sentinel index fields the asm stores. Now that the sub-objects are named members,
        // the placement-new builds the previous model needed are gone -- the members'
        // own constructors run for free.
        MainDirector();

        // ---- staged-init lifecycle -----------------------------------------------------

        // X360 0x8225B448 (EXECUTED in goal trace). Construct the runtime: build the
        // dev-tools, the CgsGraphics::Camera, the director Camera, the AllVehicleData, the
        // BehaviourManager, the MomentParameterBank and the Arbitrator, seed the camera-shake
        // RNG slots, and Clear the GameState. See the .cpp banner for exactly which steps are
        // real and which remain documented quiet gates.
        void Construct(const DirectorResourceManager* lpResourceManager, f32 lfTime);

        // X360 0x8224FB38. Staged Prepare state machine (the per-stage init the boot loop
        // pumps): tear the collision generator down, register the dev-tools GameTalk handler,
        // Prepare the ICE wrapper, Prepare the behaviour manager, seed the 20-entry
        // moment-bucket pool free queue, then mark the runtime ready (stage word -> 0).
        // Returns true once the last stage completes.
        //
        // Signature recovered from the only call site, DirectorModule::Prepare @0x822712D8:
        //     MainDirector::Prepare( this+2816, <director OUTPUT buffer>, <s32 forwarded>,
        //                            <the module's DirectorResourceManager> )
        // -- the last two are handed straight on to ICEWrapper::Prepare.
        bool Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg,
                     const DirectorResourceManager* lpResourceManager);

        // X360 0x82236EB0. Staged Release state machine. Returns true on the final stage.
        bool Release();

        // X360 0x8224FCC0. Tear the director down.
        void Destruct();

        // ---- per-frame Update spine ----------------------------------------------------

        // X360 0x82274070. The director's per-frame tick, and THE FUNCTION THAT PUBLISHES THE
        // CAMERA. See the .cpp banner for the shape and for what is still gated.
        void Update(const DirectorInputOutput* lpIO);

        // X360 0x8225BA00. Pre-scene-query update.
        void PreSceneQueryUpdate(const DirectorInputOutput* lpIO);

        // X360 0x82236F88. Post-GUI update. DOCUMENTED QUIET GATE (see the .cpp).
        void PostGuiUpdate(const DirectorInputOutput* lpIO);

        // The shared "is there a live player car this frame" predicate both Update and
        // PreSceneQueryUpdate open with (the X360 inlines the same sequence into both).
        // Returns the EFFECTIVE player-car index, or -1 when there is no live player car --
        // the X360 keeps that index and threads it into every sub-update, so the de-inlined
        // helper returns it rather than a bare bool.
        s32 GetLivePlayerCarIndex(const DirectorInputOutput* lpIO) const;
        bool IsPlayerCarLive(const DirectorInputOutput* lpIO) const
        {
            return GetLivePlayerCarIndex(lpIO) != -1;
        }

        // ---- Update sub-steps ------------------------------------------------------------

        // X360 0x82271120. Build the per-frame ArbStateSharedInfo from the director's members
        // and drive Arbitrator::Update with it, leaving the arbitrator's chosen camera in
        // lrCameraInOut. RECONSTRUCTED in the .cpp (BehaviourManager wave).
        //
        // Signature recovered from the call site in Update @0x82274070:
        //     UpdateArbitrator( this, lpIO, &lCamera, liPlayerCarIndex )
        void UpdateArbitrator(const DirectorInputOutput* lpIO, Camera::Camera& lrCameraInOut,
                              s32 liPlayerCarIndex);

        // Fill lrSharedInfo with this frame's arbitrator context. De-inlined out of
        // UpdateArbitrator (the X360 builds it straight onto the stack frame it then passes).
        void BuildArbStateSharedInfo(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex,
                                     ArbStateSharedInfo& lrSharedInfo) const;

        // X360 0x82238FC0. Advance the embedded ICE wrapper for this frame.
        // DECLARATION-ONLY + FLAG (ICE wrapper Update cone + per-frame reference-space build).
        void UpdateICE(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex);

        // X360 0x82250268. Pump the moment controller.
        // DECLARATION-ONLY + FLAG (moment-controller aggregate un-homed).
        void UpdateMoments(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex);

        // ⭐ X360 0x8221AFD0. RE-READ the two gameplay cameras' authored attribs for the car
        // whose key the behaviour parameter bank has latched. BODIED 2026-08-02.
        //
        // ⚠️ ARGUMENT TYPE CORRECTED IN THE SAME PASS: this was declared taking a
        // `const DirectorInputOutput*`. The console's r4 goes STRAIGHT into
        // DirectorIO::InputBuffer::GetControll (`mr r3, r4 ; bl ...InputBuffer::GetControll`
        // @0x8221AFE4), so a2 is the INPUT BUFFER. The old spelling would have compiled and
        // silently read a DirectorInputOutput's first pointer as a controller block.
        //
        // ⚠️⚠️ AND IT IS NOT A PER-FRAME RE-SEED. Its entire body is gated on
        // ControllerInfo +0x01 == mbGameTalkRefreshRequest (DecFIGS DWARF
        // BrnDirectorControllerInfo.h:49) -- the live-tuning tool's "re-read the attribs"
        // pulse. Nothing on a PC/retail build sets it, so this body is INERT here by design.
        // The seeding that matters is ProcessNewVehicleEvents'.
        void UpdateAttribSys(const DirectorIO::InputBuffer* lpInput);

        // X360 0x82255318 / 0x8224FD30. The pre/post-scene camera-behaviour passes. BOTH ARE
        // BODIED (2026-08-01, car-select hand-off wave) and they are NOT interchangeable:
        // PreScene (called from PreSceneQueryUpdate) runs BehaviourManager::UpdateAllBehaviours
        // == vtable slot 2; PostScene (called from Update) runs
        // BehaviourManager::PostCollisionUpdateAllBehaviours == vtable slot 3. This build used
        // to have only PostScene, calling the PreScene pass's manager entry -- so slot 3 was
        // never dispatched anywhere. See the .cpp banner.
        void UpdateCameraBehavioursPreScene(const DirectorInputOutput* lpIO, s32 liArg);
        void UpdateCameraBehavioursPostScene(const DirectorInputOutput* lpIO, s32 liArg);

        // NOT an X360 function -- the shared prologue of the two passes above (the console
        // builds the same ~1540-byte BehaviourSharedInfo on its own stack inside each). The
        // CameraSpaceHandler is the CALLER's stack object on the console too, so it is passed
        // in by reference rather than returned.
        void BuildBehaviourSharedInfo(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex,
                                      Camera::BehaviourSharedInfo& lSharedInfo,
                                      ICE::CameraSpaceHandler& lCameraSpaces);

        // ⭐ X360 0x822372F8. Drain the input buffer's GAME-ACTION QUEUE and apply each action
        // to the GameState snapshot. BODIED (junkyard/car-select arms; see the .cpp banner).
        //
        // ⚠️ DROPPED-ARGUMENT TRAP, CORRECTED 2026-08-01: this was declared `ProcessInputQueue()`.
        // The console takes TWO arguments and r4 is LIVE -- `0x82237314 lwz r30, 0(r4)` loads
        // the input buffer out of the DirectorInputOutput before anything else happens. A
        // no-argument spelling would have compiled, linked and silently drained nothing.
        void ProcessInputQueue(const DirectorInputOutput* lpIO);

        // ⭐⭐ X360 0x8221A6B0. Drain the input buffer's NewVehicleEvent queue and seed the two
        // shared gameplay cameras' parameter blocks from each car's authored camera attribs.
        // BODIED 2026-08-02 (camera parameter-chain wave) -- it is the ONLY primary writer of
        // BehaviourGameplay{External,Bumper}::Parameters::mbIsValid, i.e. the one function
        // that makes the console chase camera capable of running at all.
        //
        // ⚠️ THE OLD DECLARATION WAS WRONG IN BOTH HALVES, and both halves mattered:
        //   * its FLAG read "(AllVehicleData un-homed)". The asm touches no AllVehicleData at
        //     all -- it is FindCollection + two RefSpec resolves + two Parameters::Set + a key
        //     latch -- and AllVehicleData has been homed (BrnDirectorAllVehicleData.h, used by
        //     three arbitrator states) since well before this wave. Tenth expired gate.
        //   * it took a `const DirectorInputOutput*`. The console's r4 goes straight into
        //     DirectorIO::InputBuffer::GetVehicleInputInterface (`mr r3,r4` @0x8221A6C4), so
        //     a2 is the INPUT BUFFER, which is also what ProcessInputQueue's own banner shows
        //     it passing.
        void ProcessNewVehicleEvents(const DirectorIO::InputBuffer* lpInput);

        // X360 0x8221B0B0. Handle a "prepare for mode" director action.
        // DECLARATION-ONLY + FLAG (GameState action region).
        void HandlePrepareForModeAction(s32 liArg1, s32 liArg2, s32 liArg3, s32 liArg4,
                                        s32 liArg5, s32 liArg6);

        // X360 0x8221A3A8. Compute the traffic-light reference space for the current event.
        // DECLARATION-ONLY + FLAG (multi-stage VMX pipeline; never scalar-paraphrased).
        void CalcTrafficLightSpace(s32 liArg2, s32 liArg3, s32 liArg4, s32 liArg5, s32 liArg6,
                                   s32 liArg7, s32 liArg8, s32 liArg9, s32 liArg10, s32 liArg11,
                                   s32 liArg12, s32 liArg13, s32 liArg14);

        // ---- debug helpers (all DECLARATION-ONLY + FLAGGED) ----------------------------

        // X360 0x82209128. Snapshot the current camera into the debug scratch region.
        // FLAG: selects the near-clip from the un-dumped rodata floats flt_82CDA55C /
        //   flt_82CDA560 (VALUES not recovered) -- the same constants Camera.h FLAGS.
        void UpdateDebugInfo(const DirectorInputOutput* lpIO);
        void UpdateDebugPrinters();                 // X360 0x82208FC8
        void DebugDisplayCurrentCamera(s32 liArg);  // X360 0x821F6740

        // ---- committed sub-object accessors --------------------------------------------

        ICEWrapper&               GetICEWrapper()             { return mICEWrapper; }
        Arbitrator&               GetArbitrator()             { return mArbitrator; }

        // ⭐ The game-intro fly-by latch (GameState +217), raised/cleared by PostGuiUpdate from
        // the GUI's fly-by START/END commands. Exposed by name because the PC bring-up producer
        // in BrnGameModule::DoUpdate_Director reads it -- see the FLAG'd block there.
        bool IsGameIntroFlybyActive() const   { return maGameState.mbGameIntroFlybyActive; }
        bool IsNewProfileIntroActive() const  { return maGameState.mbNewProfileIntroActive; }

        // ⭐ The director's per-event game-state snapshot, by name. THE junkyard / car-select
        // sub-state (GameState::meJunkyardState) lives here; ProcessInputQueue writes it and
        // ArbStateRoaming::ProcessActiveDrivingTransitions is what reads it to enter
        // ArbitratorStateContainer::E_STATE_CAR_SELECT. Exposed const so the PC bring-up can
        // OBSERVE the state machine without any caller being able to forge a transition.
        const GameState& GetGameState() const { return maGameState; }
        Camera::BehaviourManager& GetBehaviourManager()       { return mBehaviourManager; }
        CameraFinaliser&          GetCameraFinaliser()        { return mCameraFinaliser; }

        // The embedded CgsGraphics::Camera (CONSOLE +0x349D0) -- the camera
        // MainDirector::Update fills (via Camera::Camera::CopyToCgsCamera) and publishes with
        // DirectorIO::OutputBuffer::SetCgsCamera, and which DirectorModule::Update then copies
        // into the module's own mCgsCamera.
        //
        // NOTE the region does double duty and that is the binary's doing, not a slip: the
        // same object is torn down through CgsCollision::BaseCollisionGenerator::Destruct by
        // Destruct/Prepare -- the graphics camera and the collision generator are ONE embedded
        // aggregate on this build.
        CgsGraphics::Camera&       GetCgsCamera()       { return mCgsCamera; }
        const CgsGraphics::Camera& GetCgsCamera() const { return mCgsCamera; }

        // The module's back-pointer to its debug component (CONSOLE +0x40; planted by
        // DirectorModule::Construct @0x8225C590, read by Update's panorama-screenshot pass).
        // Exposed by name so the module never reaches into this class's storage.
        void SetDebugComponent(DebugComponent* lpDebugComponent) { mpDebugComponent = lpDebugComponent; }

    private:
        // ================================================================================
        // LAYOUT -- named members in CONSOLE ORDER, HOST-NATIVE sizes.
        // The `// +0xNNNNN` comments are the X360 CONSOLE offsets that prove each member's
        // identity and position. They are PROVENANCE. Nothing indexes by them.
        // Regions whose type is not homed yet are NAMED, SIZED opaque storage (span taken
        // from the console gap to the next attested anchor) -- swap in the real type when it
        // lands, and delete the matching accessor's FLAG.
        // ================================================================================

        // +0x00000  BrnDirector::DirectorDevTools -- occupies the head (its Construct takes
        //           `this`, i.e. the dev-tools object IS at MainDirector +0).
        //           FLAG: un-homed; named opaque storage (console span 0x00 .. 0x40).
        alignas(16) u8 maDirectorDevTools[0x40];

        // +0x00040  the DirectorModule's DebugComponent back-pointer.
        DebugComponent* mpDebugComponent;

        // +0x00044 .. +0x0004F  dev-tools tail (untouched by any reconstructed body).
        u8 maDirectorDevToolsTail[0x0C];

        // +0x00050  the embedded ICE wrapper. Console span 0x50 .. 0x12160.
        ICEWrapper mICEWrapper;

        // +0x12160 .. +0x12480  ShotSelector (+0x121F0), CameraInterpolationController
        //           (+0x121B0) and CrashAnalyser (+0x1245C), plus the AllVehicleData-ready
        //           latch at +0x12160 and the frame counter at +0x121A0 that Construct and
        //           Prepare zero. FLAG: none of those four types is homed; named opaque span.
        //
        // ⭐ CARVED 2026-08-01 (ICE-anim transform wave): the 64 bytes at +0x12170 are the ICE
        // SCENE-SPACE transform -- the 4th matrix MainDirector::UpdateCameraBehavioursPostScene
        // @0x8224FD30 hands ICE::CameraSpaceHandler::Construct (`r7 = this + 0x12170`, landing
        // on the handler's mSceneToWorld @+0x0C0). A whole-image scan for that displacement
        // returns exactly TWO consumers, both in MainDirector: this staging read, and
        // MainDirector::Update @0x82274A64, which WRITES the ICE editor's preview camera
        // transform into it with four lvx128/stvx128 pairs. It is 64 bytes with a 16-byte
        // alignment and it feeds a Matrix44Affine parameter -- hence the type.
        //
        // ⚠️ NOTHING SEEDS IT ON THE CONSOLE. Construct/Prepare never touch it, so on retail it
        // holds whatever the module allocation left (zero) until the in-game ICE editor runs.
        // A scene-space authored take therefore projects through a ZERO matrix on a retail
        // console too -- worth knowing before treating a collapsed scene-space shot as a PC bug.
        u8             maAllVehicleDataReadyLatch[0x12170 - 0x12160];   // +0x12160
        Matrix44Affine mICESceneSpace;                                  // +0x12170
        u8             maShotAndAnalysisBlock[0x12480 - 0x121B0];       // +0x121B0

        // +0x12480  the camera finaliser (inertia + key-anim shake). Console span to +0x124F0;
        //           the KeyAnimShakeController Construct builds at +0x124D0 is its own member.
        CameraFinaliser mCameraFinaliser;

        // +0x124F0 .. +0x12C80  the embedded ICE take. FLAG: un-homed; named opaque span.
        u8 maICETake[0x12C80 - 0x124F0];

        // ⭐ +0x12C80 .. +0x12DC0  BrnDirector::AllVehicleData -- NO LONGER AN OPAQUE SPAN
        // (2026-08-01, junkyard-fire wave). The class IS homed (BrnDirectorAllVehicleData.h has
        // its full DWARF member list and its inline accessors); only this owner still stood it
        // up as 320 raw bytes, and `reinterpret_cast<const AllVehicleData*>(maAllVehicleData)`
        // was therefore publishing an object whose mpRaceCars had never been written. Every
        // consumer that dereferenced it asserted and then crashed -- which is exactly what
        // ArbStateCarSelect::UpdateIntroState -> BehaviourRotateAboutVehicle::BecomeSimilarTo
        // -> AllVehicleData::GetPlayer did the first time the junkyard state ran.
        AllVehicleData mAllVehicleData;

        // +0x12DC0  the camera arbitrator. Console span 0x12DC0 .. 0x172C8.
        Arbitrator mArbitrator;

        // +0x172D0 .. +0x1CA60  BrnDirector::MomentController (the ArbStateSharedInfo's
        //           mpMomentController is exactly +0x172D0). FLAG: un-homed; named opaque span.
        u8 maMomentController[0x1CA60 - 0x172D0];

        // +0x1CA60 .. +0x1CAC0  the moment-bucket pool's free queue (20 slots, seeded 19..0 by
        //           Prepare stage 5), its count word (+0x1CAB0) and its occupancy word
        //           (+0x1CAB8, zeroed by Construct and by Destruct). Modelled as the three
        //           named fields the two reconstructed bodies actually write, so neither pokes
        //           an offset. FLAG: the owning pool's type is un-homed -- when
        //           ObjectPool<MomentBucket,20> lands, replace all three with that member and
        //           call Clear()/Construct() on it.
        s32 maMomentBucketFreeQueue[20];               // +0x1CA60
        s32 miMomentBucketFreeCount;                   // +0x1CAB0
        u8  maMomentBucketPoolPad[0x1CAB8 - 0x1CAB4];  // +0x1CAB4
        u64 muMomentBucketOccupancy;                   // +0x1CAB8

        // +0x1CAC0 .. +0x1CB10  BrnDirector::MomentParameterBank. FLAG: un-homed opaque span.
        u8 maMomentParameterBank[0x1CB10 - 0x1CAC0];

        // +0x1CB10  the camera-behaviour manager. Console span 0x1CB10 .. ~0x32ED8
        //           (its own last field, mbDebugDisplayAllCameras, is at manager +91076).
        Camera::BehaviourManager mBehaviourManager;

        // ⭐ ADDED 2026-08-01 (junkyard-fire wave). The named-camera-parameter bank the
        // arbitrator states reach through ArbStateSharedInfo::mpNamedParameters.
        // ⚠️ PLACEMENT IS THE DEVIATION, and it is stated rather than hidden: on the console
        // this block is INSIDE the behaviour manager's own mBehaviourParameterBank (the shared
        // info's +0x1C == MainDirector +192592 == manager +75072 == bank +0x10), and that bank
        // is still a 4-byte `OpaqueSub<0>` placeholder in BrnBehaviourManager.h. Homing the
        // slice there would mean growing a member every consumer of that header sees; homing it
        // here, next to the manager, keeps the blast radius to the one producer of the pointer
        // (BuildArbStateSharedInfo, immediately below in the .cpp) while the storage is REAL and
        // named instead of null. The block's own contents are the console's tag, not fabricated
        // tunings -- see NamedParameters::Construct.
        // DELETE-WHEN: BehaviourParameterBank is homed inside the manager (then this member goes
        // and BuildArbStateSharedInfo points at mBehaviourManager's own bank).
        NamedParameters mNamedParameters;

        // +0x32ED8 .. +0x32EE0  (untouched)
        u8 maPad_0x32ED8[0x08];

        // +0x32EE0 .. +0x32F10  the camera-shake RNG: an 8-slot float table (+0x32EE0), the
        //           64-bit LCG state (+0x32F00) and its index (+0x32F08). This IS the
        //           `Random* mpRandom` slot ArbStateSharedInfo carries. FLAG: BrnDirector::
        //           Random is un-homed; named opaque span.
        u8 maRandom[0x32F10 - 0x32EE0];

        // +0x32F10  the frame camera the director carries over between frames.
        Camera::Camera mLastCamera;

        // +0x33070 .. +0x330A0  Camera2DRotationController (ArbStateSharedInfo +0x54).
        //           FLAG: un-homed; named opaque span.
        u8 maRotationController[0x330A0 - 0x33070];

        // +0x330A0 .. +0x33100  CameraSphericalRotationController (ArbStateSharedInfo +0x58).
        //           FLAG: un-homed; named opaque span.
        u8 maSphericalRotationController[0x33100 - 0x330A0];

        // +0x33100  the "forced camera car" override index (Construct seeds -1; the
        //           live-player-car predicate and PreSceneQueryUpdate use it).
        s32 miForcedCameraCarIndex;

        // +0x33104 .. +0x33108  two camera-car flag bytes Construct seeds (+0x33104 = 1,
        //           +0x33105 = 0) -- roles not recovered. FLAG: named opaque span.
        u8 maCameraCarFlags[0x33108 - 0x33104];

        // +0x33108 .. +0x33768  BrnDirector::DebugLog (ArbStateSharedInfo +0x08; Construct
        //           seeds its +0 float to 10.0). FLAG: un-homed; named opaque span.
        u8 maDebugLog[0x33768 - 0x33108];

        // +0x33768 / +0x3378C / +0x337B0  the three BrnDirector::DebugPrinters Construct
        //           builds. The THIRD (+0x337B0) is the one ArbStateSharedInfo carries as
        //           mpDebugPrinter, so the three are named separately rather than as one span.
        //           FLAG: DebugPrinter is un-homed; named opaque storage.
        u8 maDebugPrinterA[0x3378C - 0x33768];       // +0x33768
        u8 maDebugPrinterB[0x337B0 - 0x3378C];       // +0x3378C
        u8 maDebugPrinterMain[0x337E0 - 0x337B0];    // +0x337B0  <- ArbStateSharedInfo +0x04

        // ⭐⭐ +0x337E0 .. +0x339E0  BrnDirector::GameState (ArbStateSharedInfo +0x24; also the
        //           block CameraFinaliser::Update takes). REAL TYPE since 2026-08-01.
        //
        // It was three members -- `u8 maGameStateHead[0xD8]`, the two carved-out intro bools,
        // and `u8 maGameStateTail[...]` -- because the type was believed un-homed. It is NOT:
        // BrnDirectorGameState.{h,cpp} have carried the full DWARF-ordered layout and a
        // store-for-store GameState::Clear @0x82218930 for several waves; the TU simply was
        // never mounted and this class never named the member. The cost of that was total:
        // GameState::meJunkyardState -- the field the ENTIRE junkyard / car-select ladder
        // hangs off -- had exactly ONE writer in the tree (Clear, in the unmounted TU), so it
        // was never even reset, let alone driven.
        //
        // The base offset is +0x337E0 == 210912, PROVEN four ways (see the wave log): the
        // compiler materialises it in ProcessInputQueue's prologue
        // (`addis r11, r31, 3; addi r11, r11, 0x37E0`) and then stores at +0x185 / +0x19C,
        // exactly the offsets Clear records; MainDirector::Construct @0x8225B448 calls
        // `GameState::Clear(this + 210912)`; and +0x339E0 (211424) is the VehicleTracker, not
        // the GameState -- taking the END of this range as the base is what hid
        // meJunkyardState (at 210912 + 0x180 = 211296) for ten waves.
        GameState maGameState;                        // +0x337E0 .. +0x339E0

        // +0x339E0 .. +0x33C90  BrnDirector::VehicleTracker (ArbStateSharedInfo +0x3C).
        //           FLAG: un-homed; named opaque span.
        u8 maVehicleTracker[0x33C90 - 0x339E0];

        // +0x33C90 .. +0x349D0  BrnDirector::EffectInterface (ArbStateSharedInfo +0x30).
        //           FLAG: un-homed; named opaque span.
        u8 maEffectInterface[0x349D0 - 0x33C90];

        // +0x349D0  the published graphics camera / collision generator (see GetCgsCamera).
        CgsGraphics::Camera mCgsCamera;

        // +0x34B40 .. +0x35420  the "prepare for mode" action block + the debug camera-info
        //           snapshot scratch. FLAG: un-homed; named opaque span.
        u8 maModeActionAndDebugBlock[0x35420 - 0x34B40];

        // ⭐ THESE ARE TWO SEPARATE STAGE MACHINES, not a stage + a counter. Corrected
        // 2026-07-29 (fly-by campaign) against the asm; the previous reading had the two words
        // swapped AND ran both machines off one of them, which meant MainDirector::Prepare
        // started at case 5 and SKIPPED case 4 -- `BehaviourManager::Prepare()`, the call that
        // carves the three behaviour pools' free queues. Every later NewBehaviour<> then
        // allocated out of an empty pool (BehaviourManager.h:782 `lHelperID >= 0` fired the
        // moment the arbitrator ran). This is the same {prepare stage, release stage} pair the
        // owning DirectorModule keeps.
        //
        // +0x35420 (218144)  the PREPARE stage. Prepare @0x8224FB38 switches on it
        //   (`v6 = a1 + 218144; switch (*v6)`) and writes it at every case; Release's last case
        //   zeroes it (`*(a1 + 218144) = 0`, so a re-prepare restarts at 0); Construct stores 0
        //   (`stwx r31(=0), r30, r11` with r11 = 0x35420).
        s32 miPrepareStage;
        // +0x35424 (218148)  the RELEASE stage. Release @0x82236EB0 switches on it
        //   (`v2 = a1 + 218148; switch (*v2)`) and writes it at every case; Prepare's last case
        //   zeroes it (`*(a1 + 218148) = 0`); Construct stores 5 == "release already complete",
        //   i.e. nothing to release yet (`li r9, 5; stwx r9, r30, r10` with r10 = 0x35424).
        s32 miReleaseStage;
        // +0x35428  the construct timestamp (the X360 stores the incoming time as a double).
        f64 mfConstructTime;

        // +0x35430 .. +0x35450  the director's own flag/latch tail (the ICE-finished latch at
        //           +0x3543C, the replaying latch at +0x3543D, the debug-print toggles at
        //           +0x3543B, ...). Construct seeds fifteen of them. FLAG: only the roles of
        //           the two latches Update/PreSceneQueryUpdate read are recovered; named
        //           opaque span.
        u8 maStateFlagTail[0x35450 - 0x35430];
    };
}

#endif // GAMESOURCE_DIRECTOR_BRN_MAIN_DIRECTOR_H
