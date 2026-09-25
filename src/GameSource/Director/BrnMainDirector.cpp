// ============================================================================
// GameSource/Director/BrnMainDirector.cpp
//
// BrnDirector::MainDirector -- the top-level cinematic camera director. Compilation
// home for the 21-function MainDirector class TU. The ENGINE source path the X360
// asserts quote is "..\..\..\GameSource\Director/BrnMainDirector.cpp", so this file
// mirrors it.
//
// ⚠️ LAYOUT: this TU was rewritten by the BehaviourManager wave together with its header.
// MainDirector is NO LONGER a console-sized opaque buffer addressed at CONSOLE byte offsets
// through a char* view -- that model could not host x64-width sub-objects (see the header's
// LAYOUT MODEL banner) and is what forced the previous wave to hold ICEWrapper::Construct
// and Arbitrator::Construct back. Every field is now a NAMED member; the `+0xNNNNN` console
// offsets quoted throughout are PROVENANCE for the member's identity, never an index.
//
// BODIED here (faithfully reconstructed from the X360 asm):
//   * MainDirector()  -- ctor
//   * Construct()     -- the runtime build, incl. the BehaviourManager + Arbitrator that the
//                        previous wave had to hold back
//   * Prepare()       -- the staged PREPARE machine, incl. the BehaviourManager stage
//   * Release()       -- the staged RELEASE machine
//   * Destruct()      -- the four pool resets + the collision-generator / ICE teardown
//   * GetLivePlayerCarIndex() -- the shared player-car predicate both entry points inline
//   * PreSceneQueryUpdate()   -- the whole-body guard (its guarded steps stay gated)
//   * Update()        -- the prologue, the no-player path, the arbitrator leg of the
//                        gameplay middle, and the whole publish tail
//   * UpdateArbitrator() + BuildArbStateSharedInfo() -- the per-frame arbitrator context
//   * ProcessInputQueue()     -- the game-action queue drain (junkyard/car-select arms plus
//                                the mode-lifecycle arms that drive the event-state journal)
//   * PostGuiUpdate()         -- every leg whose destination is a GameState field
//   * HandlePrepareForModeAction() -- the event-entry push (PRE_INTRO + meEventType)
//
// DECLARATION-ONLY + FLAGGED (in the header): UpdateICE / UpdateAttribSys /
// UpdateCameraBehaviours* / UpdateDebug* / ProcessNewVehicleEvents /
// CalcTrafficLightSpace / DebugDisplayCurrentCamera.
// Each indexes a NOT-HOMED aggregate, paraphrases a VMX pipeline, or depends on un-dumped
// rodata.
// ============================================================================

#include "GameSource/Director/BrnMainDirector.h"
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"          // Camera::CollisionPolicySharedInfo (the scene-query pair)
#include "GameSource/Director/BrnDirectorHarness.h"                 // [FX-DIRECTOR2 opt-in] Harness::SceneQueryClosureEnabled
#include "GameSource/Gui/Events/BrnGuiPFXEvents.h"                  // BrnGui::GuiPFXHookEnumeration (the 501 record PostGuiUpdate consumes)

#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h" // BrnDirector::DirectorInputOutput
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"    // DirectorIO::InputBuffer
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h" // BrnDirector::DebugComponent
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h" // ArbStateSharedInfo
#include "GameSource/Director/BrnDirectorResourceManager.h"            // BrnDirector::DirectorResourceManager
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatus
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"                 // ICE::CameraSpaceHandler
                                                                       //   (UpdateCameraBehavioursPostScene
                                                                       //    stages one per frame)
// -- ProcessNewVehicleEvents / UpdateAttribSys: the car's authored camera attribs ----------
#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h"   // NewVehicleEvent queue
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"      // MomentSharedInfo (UpdateMoments)
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"          // Attrib::Gen::burnoutcarasset
#include "GameSource/AttribSys/Generated/classes/camerabumperbehaviour.h"    // Attrib::Gen::camerabumperbehaviour
#include "GameSource/AttribSys/Generated/classes/cameraexternalbehaviour.h"  // Attrib::Gen::cameraexternalbehaviour

// -- the event-state journal legs: the game-action records they read, and the traffic-light
//    lookup the prepare-for-mode arm resolves the event's junction logic box through ----------
#include "GameSource/GameState/BrnGameActions.h"                   // the mode-lifecycle records
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // BrnGameState::StuntInfo (case 132's record)
#include <cstddef>   // offsetof (the alignas(16) records read by console offset)
#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"         // WorldMap::GetTrafficData
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"      // GetJunctionLogicBoxForTrafficLight

#include <cstring>   // std::memcpy (the game actions' packed CgsID / word payloads)
#include <cstdlib>   // [diag] getenv -- BRN_SLOMO_DIAG
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] CgsDev::Log::gpDebugPrint
#include <cstdlib>
// [diag] BRN_PFX_DIAG -- the director-side witness of the post-FX hook hand-over.
static bool PfxDirDiag()
{
    static const bool sbOn = (getenv("BRN_PFX_DIAG") != 0);
    return sbOn && CgsDev::Log::gpDebugPrint != 0;
}


// FLAG: CgsSceneManager::CgsCollision::BaseCollisionGenerator has no reconstructed home
//   layout yet (the committed CgsSceneManagerModule.h forward-declares it only). Destruct /
//   Release / Prepare call its Destruct() on the embedded CgsGraphics::Camera / collision-
//   generator object; declared here as a minimal external with just that member so the call
//   compiles. Replace with the real home when the collision-generator TU lands.
namespace CgsSceneManager { namespace CgsCollision {
    struct BaseCollisionGenerator { void Destruct(); };
} }

namespace BrnDirector
{
    namespace
    {
        // [diag, NOT X360] BRN_DIRECTOR_TRAFFIC_DIAG -- the witness of the world -> director traffic
        // hop and the team words (FX-DIRECTOR2, 2026-09-25). What AllVehicleData::Update stored: the
        // traffic record count behind mpTrafficVehicleArray, its first record (entity index, id,
        // world position, speed), every record's traffic entity index (to match the crash module's
        // `[traffic-crash] added vehicle=V`), and every nearest-car row with its team. Printed when
        // the traffic count changes and on every 300th frame; at most 600 lines. Silent without the
        // variable.
        void DirectorTrafficDiag(const AllVehicleData& lrAllVehicleData)
        {
            static const bool sbOn = (getenv("BRN_DIRECTOR_TRAFFIC_DIAG") != 0);
            static s32 siLinesLeft = 600;
            static s32 siLastCount = -1;
            static u32 suFrame = 0;
            if (!sbOn || CgsDev::Log::gpDebugPrint == 0 || siLinesLeft <= 0)
                return;
            ++suFrame;
            const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* lpTraffic = lrAllVehicleData.GetTraffic();
            const s32 liCount = (lpTraffic != 0) ? static_cast<s32>(lpTraffic->GetLength()) : -1;
            if (liCount == siLastCount && (suFrame % 300u) != 0u)
                return;
            siLastCount = liCount;
            --siLinesLeft;
            CgsDev::Log::DebugPrint& lrOut = *CgsDev::Log::gpDebugPrint;
            lrOut << "[director-traffic] frame " << static_cast<s32>(suFrame) << " AllVehicleData traffic "
                  << liCount;
            if (liCount > 0)
            {
                const BrnTraffic::BrnTrafficIO::TrafficDirectorEntity& lrFirst = (*lpTraffic)[0u];
                const Vector3& lrPos = lrFirst.mLocalTransform.wAxis;
                const f32 lfSpeedSq = lrFirst.mVelocity.x * lrFirst.mVelocity.x
                                    + lrFirst.mVelocity.y * lrFirst.mVelocity.y
                                    + lrFirst.mVelocity.z * lrFirst.mVelocity.z;
                lrOut << " first{entity " << static_cast<s32>(lrFirst.mu16EntityIndex)
                      << " id " << static_cast<u32>(lrFirst.mVehicleId & 0xFFFFFFFFu)
                      << " pos " << lrPos.x << "," << lrPos.y << "," << lrPos.z
                      << " |v|^2 " << lfSpeedSq << "} entities";
                for (u32 luRecord = 0; luRecord < static_cast<u32>(liCount); ++luRecord)
                {
                    lrOut << (luRecord == 0u ? " " : ",")
                          << static_cast<s32>((*lpTraffic)[luRecord].mu16EntityIndex);
                }
            }
            const AllVehicleData::NearestCarInfoArray& lrRows = lrAllVehicleData.GetNearestRaceCarRowsForDiag();
            lrOut << " rows";
            for (u32 luRow = 0; luRow < lrRows.GetLength(); ++luRow)
            {
                const AllVehicleData::NearestCarInfo& lrRow = lrRows[luRow];
                lrOut << " car" << static_cast<s32>(lrRow.meRaceCarIndex) << ":team" << lrRow.miTeam;
            }
            lrOut << "\n";
        }

        // ⭐ The FLOOR MainDirector::Update clamps the camera's requested sim time scale to
        // before publishing it as the sim timer's multiplier (`fsel f1, f13, f31, f0` with
        // f0 == flt_8200CE04, @0x8227513C..0x82275144). A camera may not slow the simulation
        // below 1/200 of real time.
        // ⚠️ THIS IS A READ VALUE, NOT A PLACEHOLDER. Hex-Rays renders it 0.0049999999, which
        // is the exact decimal expansion of the IEEE-754 f32 0x3BA3D70A -- i.e. it is the
        // literal that is in .rdata, printed at double precision, not a decompiler guess.
        const f32 KF_MINIMUM_SIM_TIME_SCALE = 0.005f;   // flt_8200CE04

        // The game-camera blend's gate (Update @0x822749D8: `fcmpu f0, f29` with f29 = flt_82001CC0 ==
        // 0x00000000, loaded @0x822743B4): a blend above it runs the interpolation controller.
        const f32 KF_GAME_CAMERA_BLEND_OFF = 0.0f;

        // The event-win screen effect (Update @0x82274524..0x82274554): a frame camera whose requested
        // post-FX id is 578869 (`lis r11, 8 ; ori r11, r11, 0xD535 ; cmplw`) asks for this GUI hook
        // ("Event_Win(50)", the image string at 0x8200401C) at this blend (flt_82004018 == 0x3F400000).
        const u32   KU_EVENT_WIN_POSTFX_ID  = 0x8D535u;
        const char* const KPC_EVENT_WIN_HOOK = "Event_Win(50)";
        const f32   KF_EVENT_WIN_HOOK_BLEND = 0.75f;

        // The crash window ProcessInputQueue reloads GameState::mfCrashTimeRemaining to on
        // every frame the player's car is NOT crashing (X360 @0x822372F8, the `else` of the
        // mbCrashActive leg). ArbStateCrashing::Update compares what is left of it against 1.0
        // to decide whether the director may still cut to a different camera, so it is the
        // "how long a crash is allowed to run before we stop re-framing it" budget.
        const f32 KF_CRASH_TIME_WINDOW = 6.0f;

        // The record ProcessInputQueue case 42 (E_ACTION_IMPACT_TIME_START) reads: DWARF
        // BrnGameActions.h:1662 ImpactTimeStartAction {f32 mfTimestepMultiplier; bool
        // mbDoForceAdditiveAftertouch; bool mbEnteringShowtime;}, 8 bytes on the wire (the
        // producer, GameStateModule::UpdateRoadRulesManager, posts `li r6,8` @0x823814FC).
        // TU-LOCAL MIRROR: the record's home is GameSource/GameState/BrnGameActions.h, which this
        // lane does not own and which carries no ImpactTimeStartAction yet (the producer keeps
        // its own TU-local ImpactTimeStartActionRecord, GameStateModule_RoadRules.cpp:72, the same
        // 8 bytes under provisional names). The director reads ONE field: `lfs f0, 0(r30)`
        // @0x82237E68. Replace with the header's type when it lands there.
        struct ImpactTimeStartActionRecord
        {
            f32  mfTimestepMultiplier;          // +0x00  DWARF :1665
            bool mbDoForceAdditiveAftertouch;   // +0x04  DWARF :1666
            bool mbEnteringShowtime;            // +0x05  DWARF :1667
        };
        static_assert(sizeof(ImpactTimeStartActionRecord) == 8,
                      "X360 UpdateRoadRulesManager posts action 42 with size 8");

        // Action 223's record, E_ACTION_CAR_ADDITION_PRESENTATION_START (DWARF 215): a TU-local
        // mirror of the DWARF's CarAdditionPresentationStartAction (BrnGameActions.h:4208..4220),
        // which this build's BrnGameActions.h (a GameState-side header) does not carry. The console
        // reads the three members ProcessInputQueue needs at +0x10 / +0x14 / +0x18 (0x82238278..
        // 0x822382A8), which the DWARF member order puts exactly there after the two CgsIDs.
        struct CarAdditionPresentationStartActionRecord
        {
            CgsID               mRivalID;                  // +0x00  DWARF :4212
            CgsID               mCarID;                    // +0x08  DWARF :4213
            EGlobalRaceCarIndex meAddedCarGlobalIndex;     // +0x10  DWARF :4214
            f32                 mfPresentationDuration;    // +0x14  DWARF :4217
            bool                mbDoCamera;                // +0x18  DWARF :4220
        };
        static_assert(offsetof(CarAdditionPresentationStartActionRecord, meAddedCarGlobalIndex) == 0x10 &&
                      offsetof(CarAdditionPresentationStartActionRecord, mfPresentationDuration) == 0x14 &&
                      offsetof(CarAdditionPresentationStartActionRecord, mbDoCamera) == 0x18,
                      "the console reads action 223's global index / duration / camera flag at +0x10 / +0x14 / +0x18");

        // [DIAG] BRN_DIRECTOR_ACTION_DIAG -- NOT IN THE X360 BINARY. One line per director
        // game-action arm this lane landed, printing the GameState VALUES the arm left behind
        // (not "the arm ran"), capped so a per-hit Showtime stream cannot flood the log.
        // DELETE-WHEN: the crash-parity Showtime / director audit closes.
        const s32 KI_DIRECTOR_ACTION_DIAG_MAX_LINES = 400;

        bool BrnDiag_DirectorActionDiagOn()
        {
            static const bool sbOn = (getenv("BRN_DIRECTOR_ACTION_DIAG") != 0);
            static s32 siLines = 0;
            if (!sbOn || CgsDev::Log::gpDebugPrint == 0 || siLines >= KI_DIRECTOR_ACTION_DIAG_MAX_LINES)
                return false;
            ++siLines;
            return true;
        }

        // [DIAG] BRN_SLOMO_DIAG -- NOT IN THE X360 BINARY. Edge-triggered per call site, so a
        // steady 1.0 costs one line for the whole session. It exists because the published time
        // scale is a value that arrives from a camera several copies away, and a wrong one is
        // indistinguishable from a right one in every other log this build writes.
        // [DIAG] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. The game-camera blend's live witness (CC-14,
        // MainDirector::Update 0x822749D4). Before the blend statement it notes the keyed blend, its two shape
        // selectors and the frame camera's position; after it, on the first frame of a keyed stretch, it prints
        // the frame camera's distance to the chase camera before and after the controller, and when the stretch
        // ends, its length and peak. Reads only.
        struct GameCameraBlendDiag
        {
            f32     mfBlend;
            u32     muCurve;
            u32     muMethod;
            Vector3 mPrePosition;
            s32     miFrames;
            f32     mfPeak;
            f32     mfLast;
        };
        GameCameraBlendDiag gGameCameraBlendDiag = { 0.0f, 0u, 0u, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0, 0.0f, 0.0f };

        bool GameCameraBlendDiagOn()
        {
            static const bool sbOn = (getenv("BRN_CRASHCAM_DIAG") != 0);
            return sbOn && CgsDev::Log::gpDebugPrint != 0;
        }

        f32 GameCameraBlendDiagDistance(const Vector3& lrA, const Vector3& lrB)
        {
            const f32 lfX = lrA.x - lrB.x;
            const f32 lfY = lrA.y - lrB.y;
            const f32 lfZ = lrA.z - lrB.z;
            return sqrtf(lfX * lfX + lfY * lfY + lfZ * lfZ);
        }

        void BrnDiag_GameCameraBlendBefore(const Camera::Camera& lrCamera)
        {
            if (!GameCameraBlendDiagOn())
                return;
            gGameCameraBlendDiag.mfBlend      = lrCamera.GetEffects().mfGameCameraBlend;
            gGameCameraBlendDiag.muCurve      = lrCamera.GetEffects().mu8BlendCurve;
            gGameCameraBlendDiag.muMethod     = lrCamera.GetEffects().mu8InterpolateType;
            gGameCameraBlendDiag.mPrePosition = lrCamera.mTransform.wAxis;
        }

        void BrnDiag_GameCameraBlendAfter(const Camera::Camera& lrCamera,
                                          const Camera::BehaviourHandle<Camera::BehaviourGameplayExternal>& lrChase,
                                          bool lbResetThisFrame)
        {
            if (!GameCameraBlendDiagOn())
                return;
            GameCameraBlendDiag& lrDiag = gGameCameraBlendDiag;
            if (!lbResetThisFrame)
            {
                if (lrDiag.miFrames == 0 && lrChase.IsAllocated())
                {
                    const Vector3& lrChasePosition = lrChase.GetProducedCamera().mTransform.wAxis;
                    *CgsDev::Log::gpDebugPrint
                        << "[gcblend] ON blend=" << lrDiag.mfBlend
                        << " curve=" << lrDiag.muCurve << " method=" << lrDiag.muMethod
                        << " camera->chase " << GameCameraBlendDiagDistance(lrDiag.mPrePosition, lrChasePosition)
                        << " m -> " << GameCameraBlendDiagDistance(lrCamera.mTransform.wAxis, lrChasePosition)
                        << " m, blend left " << lrCamera.GetEffects().mfGameCameraBlend << "\n";
                }
                ++lrDiag.miFrames;
                lrDiag.mfPeak = (lrDiag.mfBlend > lrDiag.mfPeak) ? lrDiag.mfBlend : lrDiag.mfPeak;
                lrDiag.mfLast = lrDiag.mfBlend;
            }
            else if (lrDiag.miFrames > 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[gcblend] OFF after " << lrDiag.miFrames << " blended frame(s), peak " << lrDiag.mfPeak
                    << " last " << lrDiag.mfLast << "\n";
                lrDiag.miFrames = 0;
                lrDiag.mfPeak   = 0.0f;
            }
        }

        void BrnDiag_ReportSimTimeScale(const char* lpcWhere, f32 lfScale);

        void BrnDiag_ReportSimTimeScale(const char* lpcWhere, f32 lfScale)
        {
            static const bool sbOn = (getenv("BRN_SLOMO_DIAG") != 0);
            if (!sbOn || CgsDev::Log::gpDebugPrint == 0)
                return;

            // One remembered value per distinct call site (the sites pass string literals).
            static const char* spcLastWhere[4] = { 0, 0, 0, 0 };
            static f32         safLastScale[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            for (int liSlot = 0; liSlot < 4; ++liSlot)
            {
                if (spcLastWhere[liSlot] == 0 || spcLastWhere[liSlot] == lpcWhere)
                {
                    if (spcLastWhere[liSlot] != 0 && safLastScale[liSlot] == lfScale)
                        return;
                    spcLastWhere[liSlot] = lpcWhere;
                    safLastScale[liSlot] = lfScale;
                    *CgsDev::Log::gpDebugPrint
                        << "[slomo] camera " << lpcWhere << " mfSimTimeScale=" << lfScale << "\n";
                    return;
                }
            }
        }

        // [DIAG BRN_CAMERA_TRACE] [FLAG PC witness] -- NOT IN THE X360 BINARY. ONE line per
        // frame at THE PUBLISH POINT (the last read of the frame camera before
        // OutputBuffer::SetCameraOutput), carrying the numbers the camera bug report is about:
        // where the camera is, where it looks, its FOV, and the shake REQUEST the effects block
        // is carrying (amplitude / frequency / type). The whole point of printing the request
        // next to the pose is that a shake that is requested but not applied is invisible in
        // every other log this build writes -- the pose simply stays smooth.
        // BOUNDED: KI_CAMERA_TRACE_MAX_LINES lines, then silent for the rest of the run.
        // Off unless BRN_CAMERA_TRACE is set (flow_run clears every BRN_* first).
        // DELETE-WHEN: the camera-shake bring-up closes.
        const s32 KI_CAMERA_TRACE_MAX_LINES = 12000;

        void BrnDiag_ReportCamera(const Camera::Camera& lrCamera);

        void BrnDiag_ReportCamera(const Camera::Camera& lrCamera)
        {
            static const bool sbOn = (getenv("BRN_CAMERA_TRACE") != 0);
            if (!sbOn || CgsDev::Log::gpDebugPrint == 0)
                return;

            static s32 siFrame = 0;
            if (siFrame >= KI_CAMERA_TRACE_MAX_LINES)
                return;

            const rw::math::vpu::Matrix44Affine& lrXform = lrCamera.mTransform;
            const Camera::CameraEffects&         lrFx    = lrCamera.mEffects;

            *CgsDev::Log::gpDebugPrint
                << "[cam] f=" << siFrame
                << " pos=" << lrXform.wAxis.x << "," << lrXform.wAxis.y << "," << lrXform.wAxis.z
                << " fwd=" << lrXform.zAxis.x << "," << lrXform.zAxis.y << "," << lrXform.zAxis.z
                << " fov=" << lrCamera.mfFOV
                << " amp=" << lrFx.mfShakeAmplitude
                << " frq=" << lrFx.mfShakeFrequency
                << " typ=" << static_cast<u32>(lrFx.mu8ShakeType)
                << " lag=" << lrFx.mfCameraLag
                << "\n";
            ++siFrame;
        }

        // The collision-generator view of an embedded aggregate. Takes a NAMED member's
        // address (never an offset into this class's storage) -- the X360 tears the embedded
        // CgsGraphics::Camera down through BaseCollisionGenerator::Destruct because on this
        // build they are one object.
        inline CgsSceneManager::CgsCollision::BaseCollisionGenerator* lpAsCollisionGenerator(void* lpObject)
        {
            return reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(lpObject);
        }
    }

    // ------------------------------------------------------------------------
    // MainDirector (ctor)  @ X360 0x827E4AB8  (EXECUTED in goal trace)
    //
    // The X360 sequence is: construct the owned sub-objects in place (ICEWrapper @+0x50,
    // ICETake @+0x124F0, ArbitratorStateContainer @+0x130D0 == arbitrator +0x310,
    // BehaviourManager @+0x1CB10, CarScoreData @+0x33B50), then seed the -1 sentinel index
    // fields (+0x122B8, +0x12384, +0x12450, +0x12DB4, +0x34974, +0x34DC0, a 16-entry
    // stride-0x2C table from +0x34DF8, +0x35090, +0x353A8).
    //
    // With the sub-objects now declared as NAMED MEMBERS, every one of those in-place builds
    // is performed by the members' own constructors -- the placement-new list the previous
    // (opaque-buffer) model needed is gone, and with it the host-size overrun risk.
    //
    // ⚠️ THE -1 SENTINEL SEEDS ARE A DOCUMENTED QUIET GATE. Each one lands inside a region
    // whose type is still un-homed (the ICE-wrapper tail / the shot-selector block / the
    // arbitrator-region head / the per-rival score table inside maModeActionAndDebugBlock),
    // so there is no named field to write. Under the previous model they were raw stores into
    // a console-addressed buffer; writing them now would mean re-introducing exactly the
    // offset arithmetic this rewrite removed, and into regions whose host contents differ.
    // CONSEQUENCE: those index fields start at 0 rather than -1. Every one is consumed by an
    // un-homed aggregate that is itself gated, so nothing reconstructed reads them today.
    // DELETE-WHEN: each owning type is homed -- then the sentinel becomes that type's own
    // constructor's business, which is where the console puts it too (the compiler inlined
    // the sub-object ctors into this one).
    // ------------------------------------------------------------------------
    MainDirector::MainDirector()
        : mpDebugComponent(0)
        , miForcedCameraCarIndex(-1)
        , miPrepareStage(0)
        , miReleaseStage(0)
        , mfDisplayAspectRatio(0.0f)
    {
        // Owned sub-objects: mICEWrapper / mCameraFinaliser / mArbitrator /
        // mBehaviourManager / mLastCamera / mCgsCamera run their own constructors.
        // ⚠️ GATE: the -1 sentinel seeds (see the banner).
    }

    // ------------------------------------------------------------------------
    // Construct  @ 0x8225B448   (EXECUTED in goal trace)
    //
    // The X360 call/store sequence, in order:
    //
    //     mStage         = 5;                                   // +0x35424
    //     mDisplayAspectRatio = lfTime;                         // +0x35428 (a FLOAT: stfs)
    //     mStageCounter  = 0;                                   // +0x35420
    //     DirectorDevTools::Construct( this, this, lpResourceManager );
    //     <CgsGraphics::Camera ctor>( this + 0x349D0 );          // sub_827F94E8
    //     CgsGraphics::Camera::SetFovHorizontal( this + 0x349D0, ... );
    //     CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix( this + 0x349D0 );
    //     Camera::Camera::Construct( &mLastCamera );             // +0x32F10
    //     AllVehicleData::Construct( this + 0x12C80 );
    //     <five flag bytes cleared around +0x34974 .. +0x349C9>
    //     Camera::BehaviourManager::Construct( &mBehaviourManager );          // +0x1CB10
    //     CGS_ASSERT( lpResourceManager != NULL );               // BrnBehaviourManager.h:168
    //     *(this + 208588) = lpResourceManager;                  // == manager +91068
    //     MomentController::Construct( this + 0x172D0 )         // inlined: pool occupancy
    //                                                           // cleared + MomentParameterBank::Construct
    //     Arbitrator::Construct( &mArbitrator );                 // +0x12DC0
    //     <the VMX + 1284865837-multiplier LCG camera-shake seed pipeline into +0x32EE0..>
    //     KeyAnimShakeController::Construct( this + 0x124D0, lpResourceManager );
    //     ShotSelector::Construct( this + 0x121F0, lpResourceManager );
    //     ICEWrapper::Construct( &mICEWrapper );                 // +0x50
    //     GameState::Clear( this + 0x337E0 );
    //     DebugPrinter::Construct( this + 0x337B0 / +0x33768 / +0x3378C );
    //     <the flag/latch tail seeds +0x3542F .. +0x3543F, and +0x33100 = -1>
    //
    // ⭐ `*(this + 208588) = lpResourceManager` IS NOT A MainDirector FIELD. 117520 + 91068 ==
    // 208588: it is `mBehaviourManager.mpDirectorResourceManager`, which is exactly why the
    // guard immediately before it quotes **BrnBehaviourManager.h:168**. The previous wave had
    // it as an un-named raw store; it is now the named setter.
    //
    // ⭐ THE TWO HELD-BACK BUILDS ARE NOW REAL. `Camera::BehaviourManager::Construct` and
    // `Arbitrator::Construct` were held back by the previous wave for a HOST-SIZE reason (the
    // host types are wider than their console placement windows inside the old opaque
    // buffer). Both are named members now, so there is no window to overrun and both run.
    // `ICEWrapper::Construct` runs for the same reason.
    //
    // ⚠️ STILL DOCUMENTED QUIET GATES -- every one is an un-homed aggregate, not a size issue:
    //   * DirectorDevTools::Construct -- un-homed (maDirectorDevTools). CONSEQUENCE: the
    //     GameTalk "Camera" dev-tools commands have no handler state. Nothing in the game path
    //     uses them.
    //   * AllVehicleData::Construct + the five flag bytes -- un-homed (maAllVehicleData).
    //     CONSEQUENCE: the per-frame vehicle tracker starts zeroed; the arbitrator states that
    //     read it are the gameplay ones, not the attract/flyby path.
    //   * (MomentController::Construct -- REAL since 2026-09-24: the controller is a member.)
    //   * the camera-shake LCG seed pipeline (+0x32EE0..) -- a multi-stage VMX + 1284865837
    //     multiplier LCG the reconstruction rules forbid paraphrasing to scalar, writing into
    //     the un-homed maRandom region. CONSEQUENCE: the shake RNG table starts zeroed.
    //   * KeyAnimShakeController::Construct / ShotSelector::Construct -- un-homed.
    //   * GameState::Clear + the three DebugPrinter::Constructs + the flag/latch tail seeds --
    //     un-homed regions (maGameState / maDebugPrinter* / maStateFlagTail).
    //     ⚠️ CONSEQUENCE WORTH KNOWING: the GameState block is what tells the arbitrator to
    //     enter attract mode, and DebugPrinter is what the arbitrator states print through.
    //     Both are handed to ArbStateSharedInfo as zeroed storage today.
    //   DELETE-WHEN: per sub-object, as each type is homed -- then swap that member's opaque
    //   span in the header for the real type and un-gate its line here.
    //
    // The SetFovHorizontal call the asm makes right after the camera ctor is deliberately not
    // reproduced: its FOV argument is an UNINITIALISED fp register in the decompilation (a
    // Hex-Rays artefact of the PPC fp calling convention), i.e. NOT recovered, and
    // CgsGraphics::Camera::Construct already seeds the default FOV through the same setter.
    // ------------------------------------------------------------------------
    void MainDirector::Construct(const DirectorResourceManager* lpResourceManager, f32 lfTime)
    {
        // ASM (@0x8225B448): `stwx r31(=0), r30, r11` with r11 = 0x35420 and
        // `li r9,5; stwx r9, r30, r10` with r10 = 0x35424 -- the PREPARE stage starts at 0
        // (so Prepare runs its whole machine, including case 4's
        // BehaviourManager::Prepare) and the RELEASE stage starts at 5 == already released.
        miPrepareStage  = 0;
        miReleaseStage  = 5;
        // +0x35428 `stfs f31, 0(r27)` (0x8225B498): DWARF mDisplayAspectRatio, a FLOAT.
        mfDisplayAspectRatio = lfTime;

        // ⚠️ GATE: DirectorDevTools::Construct( this, this, lpResourceManager );

        // The published graphics camera (+0x349D0).
        mCgsCamera.Construct();
        mCgsCamera.UpdatePerspectiveProjectionMatrix();
        // @0x8225B448 pseudocode 35..39 -- the EffectInterface's own Construct, inlined there.
        reinterpret_cast<EffectInterface*>(maEffectInterface)->Construct();

        // mLastCamera (+0x32F10) -- the carried-over frame camera Update reads and writes.
        // Without this the director would publish RAW UNINITIALISED STORAGE on frame 1 and
        // ValidateTransformWithDebugInfo would assert on the NaNs.
        mLastCamera.Construct();

        // ⭐ REAL (2026-08-01): the director's per-frame vehicle snapshot. Its three player
        // spaces go to identity and its race-car pointer/index/bitset are cleared, so a frame
        // that reaches it before PreSceneQueryUpdate has published sees a defined object rather
        // than pool garbage. ⚠️ GATE (unchanged): the five flag bytes that follow it.
        mAllVehicleData.Construct();

        // -- REAL (2026-08-29, crash-camera wave): the player-car tracker. Its four history
        // journals have to be Construct()ed before anything reads them -- an unconstructed
        // DataJournal reports miSize 0, so every GetCurrent()/GetPrevious() is an assert plus a
        // garbage sample. The tracked slot is bound to the player each frame in
        // PreSceneQueryUpdate, where the console binds it.
        mVehicleTracker.Construct();

        // ⭐ REAL (was held back for host size): the camera-behaviour manager.
        mBehaviourManager.Construct();

        CGS_ASSERT(lpResourceManager != 0, "lpDirectorResourceManager != NULL");
        mBehaviourManager.SetDirectorResourceManager(lpResourceManager);

        // ⭐ REAL (2026-09-24, FX-DIRECTOR moment tick): the moment controller -- its pool's
        // occupancy word cleared and its MomentParameterBank::Construct (0x8225B540..0x8225B554,
        // the inlined MomentController::Construct).
        mMomentController.Construct();

        // ⭐ REAL (was held back for host size): the camera arbitrator -- and with it the
        // 11-state container, the shared-camera container and the three special-cam states
        // (crash-nav / ATTRACT MODE / render-metrics).
        mArbitrator.Construct();

        // ⚠️ GATE: the VMX/LCG camera-shake seed pipeline into maRandom.

        // ⭐ REAL (2026-09-06, camera-shake lane): the camera finaliser's own seed --
        // InertiaController::miFrame = 0, mfShakeScale = 0 and
        // KeyAnimShakeController::Construct(lpResourceManager), which the console emits inline
        // here at 0x8225B7E0..0x8225B7E8 with r9 == &mCameraFinaliser and r4 == r29 == this
        // function's own lpResourceManager argument. It was gated together with
        // ShotSelector::Construct on the grounds that KeyAnimShakeController had no home;
        // it has one now, and WITHOUT this seed its shot group pointer, its RNG ring and its
        // shot clock would all start as allocator garbage the first time a shake is requested.
        mCameraFinaliser.Construct(lpResourceManager);

        // ⭐ REAL (2026-09-24, FX-DIRECTOR2): ShotSelector::Construct(this + 0x121F0, lpResourceManager)
        // @0x8225B7F8, between the finaliser's KeyAnimShakeController::Construct and the ICE wrapper,
        // as on the console. The selector keeps the resource manager for GetCrashShot's shot banks.
        mShotSelector.Construct(lpResourceManager);

        // ⭐ REAL (was held back for host size): the ICE wrapper.
        mICEWrapper.Construct();

        // ⭐ REAL (2026-08-01): `BrnDirector::GameState::Clear(this + 210912)` @0x8225B448.
        // This is the ONLY thing that seeds meJunkyardState to E_JY_INACTIVE, meEventType to
        // E_MODE_NONE, the three director-space transforms to identity, and the -1 sentinels.
        // While the region was an opaque byte span this never ran, so the whole snapshot --
        // including the junkyard sub-state the car-select ladder tests -- started as whatever
        // the allocator's memory happened to hold.
        maGameState.Clear();

        // ⭐ REAL (2026-09-25, FX-DIRECTOR2 CC-14): the camera-interpolation controller's two
        // interpolaters zeroed (0x8225B810..0x8225B840: `stvx128 v127(=0)` + `stb r31(=0), 0x10` at
        // +0x121B0 and again at +0x121D0) -- the inlined CameraInterpolationController::Construct,
        // interleaved on the console with the DebugLog stores below.
        mCameraInterpolationController.Construct();

        // ⭐ REAL (2026-09-24, FX-DIRECTOR2): the inlined DebugLog::Construct over +0x33108
        // (0x8225B824..0x8225B87C), straight after GameState::Clear as on the console. The moments
        // and the crashing arbitrator state append to this log; unconstructed, its first Append
        // wrote through pool index -1.
        mDebugLog.Construct();

        // ⚠️ GATE: the three DebugPrinter::Constructs.

        // [FLAG PC bring-up] The ICE scene-space transform (+0x12170). Construct does not seed it on the
        // console: it starts at the module allocation's zero, and the PC allocation carries no such
        // guarantee, so it is zeroed here explicitly. [CORRECTED 2026-09-25, FX-DIRECTOR2] This comment
        // used to say nothing in the image writes it but the ICE editor. That was wrong: Update writes the
        // published camera's transform into it on EVERY live frame whose camera is not in a scene-space
        // shot (0x82274A28..0x82274A80, four lvx128 / stvx128 pairs through r18 = 0x12170). The indexed
        // `stvx128 v0, r0, r11` (r11 = this + r18) is invisible to a literal displacement scan, so the
        // zero only lasts until the first live frame.
        mICESceneSpace.xAxis.SetZero();
        mICESceneSpace.yAxis.SetZero();
        mICESceneSpace.zAxis.SetZero();
        mICESceneSpace.wAxis.SetZero();

        // The forced-camera-car override starts at the -1 "none" sentinel (+0x33100).
        miForcedCameraCarIndex = -1;

        // ⭐ REAL (2026-09-24, FX-DIRECTOR): the flag/latch tail seeds, 0x8225B8EC..0x8225B9E8, in the
        // console's value order (r31 == 0, r11 == 1). Names are the DecFIGS DirectorModule members.
        mbShowAllCameraNames  = false;                                            // +0x3542F
        maStateFlagTail[E_FLAG_TAIL_DISABLE_AFTERTOUCH_CAMERA]            = 0;    // +0x35430
        maStateFlagTail[E_FLAG_TAIL_ALWAYS_SUPER_WIDE]                    = 0;    // +0x35432
        maStateFlagTail[E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN] = 1;    // +0x35437
        maStateFlagTail[E_FLAG_TAIL_DISABLE_DOF]                          = 0;    // +0x35438
        maStateFlagTail[E_FLAG_TAIL_FORCE_SLOMO_NOT_ALLOWED]              = 0;    // +0x35431
        maStateFlagTail[E_FLAG_TAIL_FORCE_SUPER_SLOMO_IN_CRASHES]         = 0;    // +0x35433
        maStateFlagTail[E_FLAG_TAIL_SHOW_WORLD_MAP_DEBUG]                 = 0;    // +0x35434
        maStateFlagTail[E_FLAG_TAIL_FORCE_CUTSCENE_BARS_ON]               = 0;    // +0x35435
        maStateFlagTail[E_FLAG_TAIL_REQUEST_HOOK_ENUMERATION]             = 0;    // +0x35436
        mbAllowJumpMoment     = false;                                            // +0x3542C
        mbAllowStuntMoment    = true;                                             // +0x3542D
        mbAllowHardStopMoment = true;                                             // +0x3542E
        maStateFlagTail[E_FLAG_TAIL_IN_ONLINE_POST_EVENT]                 = 0;    // +0x35439
        maStateFlagTail[E_FLAG_TAIL_MODE_ACTION_DEFERRED]                 = 0;    // +0x3543A
        maStateFlagTail[E_FLAG_TAIL_SHOW_CAMERA_STATE_FLAGS]              = 0;    // +0x3543B
        mfDisplayAspectRatio  = lfTime;                                           // 0x8225B9B4 (again)
        maStateFlagTail[E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP]                  = 0;    // +0x3543C
        maStateFlagTail[E_FLAG_TAIL_DEBUG_SINGLE_TIMESTEP]                = 0;    // +0x3543D
        maStateFlagTail[E_FLAG_TAIL_EVENT_END_REQUEST]                    = 0;    // +0x3543E
        maStateFlagTail[E_FLAG_TAIL_EVENT_END_FORCED]                     = 0;    // +0x3543F
        maAllVehicleDataReadyLatch[0]                                     = 0;    // +0x12160
    }

    // ------------------------------------------------------------------------
    // Prepare  @ 0x8224FB38
    //
    // The staged PREPARE state machine DirectorModule::Prepare pumps at its own stage 4. The
    // stage word selects the entry case; the cases fall through, so one call advances as far
    // as it can:
    //
    //   0 -> tear the collision generator down, then register the dev-tools GameTalk message
    //        handler under the name "Camera", and zero the dev-tools head word
    //   1 -> zero the frame counter (+0x121A0-ish, inside the shot block)
    //   2 -> ICEWrapper::Prepare
    //   4 -> Camera::BehaviourManager::Prepare
    //   5 -> MomentController::Prepare (inlined): the moment pool's free queue refilled 19..0,
    //        count 20, occupancy word (+0x1CAB8) cleared
    //   6 -> (empty)
    //   7 -> done: clear the stage counter, report success
    // (there is deliberately NO case 3 in the X360 jump table -- reproduced.)
    //
    // Any sub-Prepare returning false reports false without advancing; the framework retries.
    // An out-of-range stage asserts (BrnMainDirector.cpp:224).
    //
    // ⭐ STAGE 4 IS NOW REAL. It was the wave-1 "THE blocker" gate on the (mistaken) grounds
    // that BehaviourManager had no layout; the layout has been committed since d5612215 and
    // BehaviourManager::Prepare @0x8223DBE0 is now bodied, so the three behaviour pools get
    // their free queues and the manager is genuinely ready to allocate behaviours.
    //
    // ⭐ STAGE 5 IS NOW REAL TOO, and it is NOT a behaviour-manager table. +117344/+117424/
    // +117432 sit BEFORE MomentParameterBank (+117440) and nowhere near the manager (+117520):
    // the 20-entry descending seed + count + head is a pool free-queue refill, i.e. the
    // moment-bucket pool's. Written through this class's three named fields.
    //
    // ⚠️ ONE DOCUMENTED QUIET GATE REMAINS: stage 0's GameTalk registration --
    //   EA::GameTalk::GameTalkManager has no reconstructed home. CONSEQUENCE: the "Camera"
    //   dev-tools commands (Start/StopRenderMetrics, the ICE editor hooks) are not reachable
    //   from GameTalk. Nothing in the game path uses them.
    //   DELETE-WHEN: the GameTalk manager is homed.
    // ------------------------------------------------------------------------
    bool MainDirector::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer,
                               const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                               const DirectorResourceManager* lpResourceManager)
    {
        switch (miPrepareStage)
        {
        case 0:
            lpAsCollisionGenerator(&mCgsCamera)->Destruct();
            // ⚠️ GATE: EA::GameTalk::GameTalkManager::GetInstance()->RegisterMessageHandler(
            //              BrnDirector::DirectorDevTools::GameTalkMsgHandler, "Camera" );
            //   -- un-homed GameTalk API (see the banner). The `*this = 0` dev-tools head
            //   store that follows it belongs to the same un-homed object.
            // fall through
        case 1:
            miPrepareStage = 1;
            // The inlined ShotSelector::Prepare (PS3 @0x15000): `stwx r28(=0), r31, 0x12454`
            // @0x8224FBE4 -- the selector's use clock miCurrentTimeID, NOT a frame counter at +0x121A0
            // as this line used to say. The console ignores its `return true`.
            mShotSelector.Prepare();
            // fall through
        case 2:
            miPrepareStage = 2;
            // The wrapper pumps its own ICE resource acquisition through the director output
            // buffer, forwarding the prepare arg and the module's resource manager.
            if (!mICEWrapper.Prepare(lpOutputBuffer, lpAllocatorList, lpResourceManager))
                return false;
            // fall through
        case 4:
            miPrepareStage = 4;
            if (!mBehaviourManager.Prepare())
                return false;
            // fall through
        case 5:
            miPrepareStage = 5;
            // The inlined MomentController::Prepare -> AbstractPool::Prepare -> ObjectPool::Clear
            // (occupancy 0, free queue 19..0, count 20). It cannot fail; the console has no branch.
            mMomentController.Prepare();
            // fall through
        case 6:
            miPrepareStage = 6;
            // fall through
        case 7:
            miPrepareStage = 7;
            miReleaseStage = 0;   // asm: *(this+218148) = 0
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Release  @ X360 0x82236EB0
    //
    // The staged RELEASE state machine. The stage word selects the case; each case advances
    // it and the cases fall through (1->2->3->5). On the first (stage 0) it destructs the
    // object at the ICE-wrapper region; on the last (stage 5) it clears the stage counter and
    // reports completion. An out-of-range stage asserts and reports failure.
    // (The asm "case 4" is the default branch -- there is no case 4 in the jump table.)
    // ------------------------------------------------------------------------
    bool MainDirector::Release()
    {
        switch (miReleaseStage)
        {
        case 0:
            // asm 0x82236F0C `addi r3,r30,0x50` -> BaseCollisionGenerator::Destruct(this+0x50),
            // i.e. the ICE-wrapper region. (Destruct() below targets the +0x349D0 graphics
            // camera instead; these are different call sites and must not be conflated.)
            lpAsCollisionGenerator(&mICEWrapper)->Destruct();
            // fall through
        case 1:
            miReleaseStage = 1;
            // fall through
        case 2:
            miReleaseStage = 2;
            // fall through
        case 3:
            miReleaseStage = 3;
            // fall through
        case 5:
            miReleaseStage = 5;
            miPrepareStage = 0;   // asm: *(this+218144) = 0
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Destruct  @ X360 0x8224FCC0
    //
    // The X360 body stores a 64-bit zero into four words, destructs the embedded
    // CgsGraphics::Camera / collision generator, then destructs the embedded ICE wrapper.
    //
    // ⭐ THE FOUR WORDS ARE POOL OCCUPANCY WORDS, not "list heads" as the previous wave
    // recorded them. Cross-checked against BehaviourManager::Prepare @0x8223DBE0's per-pool
    // free-queue layout:
    //     +0x1CAB8 (117432)  the moment-bucket pool's occupancy word
    //     +0x24848 (149576) == manager +32056  mLargeBehaviourPool's occupancy
    //     +0x2C5B8 (181688) == manager +64168  mSmallBehaviourPool's occupancy
    //     +0x2F038 (192568) == manager +75048  mBehaviourHelperPool's occupancy
    // So Destruct is "reset the four pools", and all four are now named operations.
    // ------------------------------------------------------------------------
    void MainDirector::Destruct()
    {
        mMomentController.Destruct();     // the moment pool's occupancy word (+0x1CAB8)
        mBehaviourManager.Destruct();     // the manager's three pools

        lpAsCollisionGenerator(&mCgsCamera)->Destruct();
        mICEWrapper.Destruct();
    }

    // ------------------------------------------------------------------------
    // GetLivePlayerCarIndex -- the shared "which player car is live this frame" resolution.
    //
    // Both Update @0x82274070 and PreSceneQueryUpdate @0x8225BA00 open with the SAME sequence
    // (the X360 emits it twice, inlined):
    //
    //     index  = lpInputBuffer->GetPlayerCarIndex();
    //     forced = miForcedCameraCarIndex;                       // +0x33100
    //     if ( forced > -1 && lpInputBuffer->GetUsedRaceCars()->IsBitSet(forced) )
    //         index = forced;                                    // the override wins
    //     if ( index == -1 )  return -1;
    //     return lpInputBuffer->GetUsedRaceCars()->IsBitSet(index) ? index : -1;
    //
    // (the CgsBitArray.h:203 "invalid index : N < 8" assert both call sites bake is the
    //  BitArray bounds check inlined -- CGS_ASSERT carries it here.)
    //
    // ⚠️ NOTE ON THE BIT TEST. The X360 emits it as
    //     ((1 << (index & 0x3F)) & ((1 << (index & 0x3F)) >> 32)) != 0
    // which is a Hex-Rays artefact of the 64-bit `rldicl`/`and` pair the PPC uses to test one
    // bit of the 64-bit BitArray<8> word -- NOT a literal shift-by-32 of a shifted 1.
    // Reproduced as the committed BitArray query it actually is, which is what the DWARF
    // member type (CgsContainers::BitArray<8u> mUsedRaceCars) says it must be.
    //
    // The X360 keeps this index and threads it into UpdateCameraBehavioursPostScene /
    // UpdateMoments / UpdateICE / UpdateArbitrator, so the de-inlined helper returns it.
    // ------------------------------------------------------------------------
    s32 MainDirector::GetLivePlayerCarIndex(const DirectorInputOutput* lpIO) const
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        s32 liIndex = static_cast<s32>(lpInput->GetPlayerCarIndex());

        const CgsContainers::BitArray<8u>* lpUsedRaceCars = lpInput->GetUsedRaceCars();

        if (miForcedCameraCarIndex > -1)
        {
            CGS_ASSERT(miForcedCameraCarIndex < 8, "invalid index");
            if (miForcedCameraCarIndex < 8 &&
                lpUsedRaceCars->IsBitSet(static_cast<u32>(miForcedCameraCarIndex)))
            {
                liIndex = miForcedCameraCarIndex;
            }
        }

        if (liIndex == -1)
            return -1;

        CGS_ASSERT(liIndex < 8, "invalid index");
        if (liIndex < 8 && lpUsedRaceCars->IsBitSet(static_cast<u32>(liIndex)))
            return liIndex;

        return -1;
    }

    // ------------------------------------------------------------------------
    // BuildArbStateSharedInfo -- the per-frame arbitrator context.
    //
    // De-inlined out of UpdateArbitrator @0x82271120, which builds this record directly onto
    // the stack frame it then hands to Arbitrator::Update. Every slot is asm-attested; the
    // console offsets in the trailing comments are the X360 stack offsets of the record
    // (sp+0x50 is the record base), matched against the committed ArbStateSharedInfo layout
    // recovered from the DecFIGS DWARF.
    //
    // TWO SLOTS ARE DELIBERATELY LEFT NULL because the X360 leaves them for the callee:
    // Arbitrator::Update's own prologue writes `mpSharedCameraContainer` (+0x00) and
    // `mpStateContainer` (+0x14) before dispatching. Zeroing them here is what the console's
    // uninitialised stack slots amount to, and the callee overwrites both unconditionally.
    //
    // ⛔⛔ THE `mpNamedParameters` GATE IS CLOSED (2026-08-01, junkyard-fire wave) -- AND ITS
    // OWN JUSTIFICATION IS WHY IT HAD TO BE. The gate used to publish null with the note
    // "CONSEQUENCE: an arbitrator state that reads named camera parameters gets null. None of
    // the states on the attract/flyby path does". That was true when it was written and stopped
    // being true the moment the junkyard fired: ArbStateCarSelect::Prepare does
    //     mLookAroundCarCam.GetBehaviour()->SetParameters(
    //         &lrSharedInfo.mpNamedParameters->GetLookAroundCarCamParameters());
    // which is a null dereference, and it crashed on the first frame the car-select state was
    // ever entered. (ArbStateOnlineCarSelect::Prepare has the identical line.) A stub's
    // "not on the live path" reasoning expires silently -- nothing in the build, the linker or
    // any boot test can tell you it has.
    // ⭐ UPDATED 2026-09-11: the slot points at the record's ONE storage, the behaviour
    // manager's own parameter bank (bank +0x10), reached by name through
    // GetBehaviourParameterBank().GetNamedParameters() and seeded by that bank's Construct.
    // The director owns no named-parameter storage of its own: the record has exactly one
    // home, the bank, which is where the console keeps it too.
    // ⭐ UPDATED 2026-08-02 (framing wave): the block now carries the console's TYPE TAG *AND*
    // its authored tunings. Both halves are transcribed --
    // BehaviourRotateAboutVehicle::Parameters::Construct @0x821FB300's thirteen re-tunes, plus
    // the FOUR the BANK's own Construct @0x8223DC90 applies to this block afterwards (the
    // Looker subject size 0.75/0.75 and screen offset +0.125/-0.125, transcribed in
    // NamedParameters::Construct with their asm and .rdata provenance). There is no data file
    // to load: BehaviourParameterBank::LoadParameters reads "d:\\camera.txt" and has no callers.
    // The bank that carries the record is a SLICE: the other ~40 named blocks are unmodelled,
    // so a state that reaches one of those still has nothing to reach.
    //
    // ⚠️ THE UN-HOMED REGION POINTERS. mpDebugPrinter / mpDebugLog / mpMomentController /
    // mpGameState / mpRandom / mpEffectInterface / mpAllVehicleData / mpPlayerTracker and the
    // two rotation controllers are handed on as their declared pointer types over this
    // class's NAMED opaque regions. That is honest storage of the right size in the right
    // order -- not an offset poke -- and each becomes a plain `&mMember` when its type lands.
    // ------------------------------------------------------------------------
    void MainDirector::BuildArbStateSharedInfo(const DirectorInputOutput* lpIO,
                                               s32 liPlayerCarIndex,
                                               ArbStateSharedInfo& lrSharedInfo) const
    {
        const DirectorIO::InputBuffer* lpInput  = lpIO->mpInputBuffer;
        DirectorIO::OutputBuffer*      lpOutput = lpIO->mpOutputBuffer;

        // The two timesteps. X360: `timer[+8] * timer[+4]` and `timer[+32] * timer[+28]`.
        // The accessor hands back the CgsSystem::TimerStatusInterface, whose committed layout
        // is `TimerStatus mGameTimerStatus @+0; TimerStatus mSimTimerStatus @+24;` and whose
        // TimerStatus is `miFrameCount@+0, mfBaseTimeStep@+4, mfTimeStepMultiplier@+8`. So
        // +8*+4 IS the GAME timer's GetCurrentTimeStep() (the same reach the committed
        // CameraFinaliser::Update makes) and +32*+28 is the SIM timer's.
        // (The GAME half used to be a reinterpret_cast of the interface pointer straight to
        // TimerStatus*, which only worked because mGameTimerStatus happens to sit at +0. Now
        // that the accessors are defined it goes through GetGameTimerStatus() like its sibling.)
        const CgsSystem::TimerStatusInterface* lpTimerStatusInterface =
            lpInput->GetTimerStatusInterface();

        f32 lfTimestep = lpTimerStatusInterface->GetGameTimerStatus()->GetCurrentTimeStep();

        // ✅✅ THE QUIET GATE ON mfSimTimestep IS GONE (2026-08-28, crash-camera wave). It was
        // STALE, and BOTH halves of its justification had stopped being true:
        //
        //   (1) "GetSimTimerStatus() is DECLARATION-ONLY". It is not, and has not been for a
        //       while: CgsTimerStatusInterface.h:100-103 define all four accessors inline
        //       (`{ return &mSimTimerStatus; }`), which is verbatim the recipe this gate's own
        //       DELETE-WHEN asked for. Nobody came back to flip it -- the "gates are STALE, not
        //       dead" class: ask when a gate's condition last held, not whether it reads true.
        //   (2) "no committed state reads +0x60". EIGHT sites do, and seven of them are on the
        //       crash path: ArbStateCrashMode reads mfSimTimestep for mfTiltChangeTime,
        //       mfFlashTime, mfBlurInTime, mfBlurOutTime, mfCurrentBlur (twice) and
        //       mfBordersTime; ArbStateOnlineRaceIntro reads it for mfTimeInState. Both states
        //       are stubbed in DirectorLinkStubs.cpp today, which is the only reason the zero
        //       has not shown up on screen.
        //
        // ⛔ WHY THIS MATTERED FOR THE CRASH CAMERA. Every crash-mode intro timer is a
        // `mfXxxTime -= lrSharedInfo.mfSimTimestep` countdown. Published as a hard 0 they
        // decrement by nothing, so the entry flash, the black borders, the motion-blur ramp and
        // the camera tilt would each have hung at their seeded value FOREVER the moment
        // BrnArbStateCrashMode.cpp was mounted -- a landmine sitting directly under this goal,
        // with a green link, no assert, and a plausible-looking timer that simply never expires.
        // It is the placeholder-identity class again: 0 is the identity of `+=`, not of `-=`
        // against a deadline.
        //
        // The console reads `timer[+32] * timer[+28]`, which the committed layout above makes
        // exactly mSimTimerStatus.mfTimeStepMultiplier * mfBaseTimeStep -- i.e.
        // GetSimTimerStatus()->GetCurrentTimeStep(). Reached BY NAME, no offset arithmetic.
        f32 lfSimTimestep = lpTimerStatusInterface->GetSimTimerStatus()->GetCurrentTimeStep();

        // ⚠️ GATE: `if ( maStateFlagTail[+0x3543C] ) { lfTimestep = 0; lfSimTimestep = 0; }`
        //   -- the ICE-owns-the-frame latch lives in the un-homed flag tail. CONSEQUENCE: the
        //   arbitrator keeps advancing its timers during an ICE take instead of freezing them.
        //   DELETE-WHEN: the MainDirector flag tail is named.

        const BrnDirector::Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();
        const BrnDirector::Camera::VehicleInfo* lpPlayerCar =
            (liPlayerCarIndex >= 0) ? (lpRaceCars + liPlayerCarIndex) : 0;

        lrSharedInfo.mpSharedCameraContainer = 0;                                   // +0x00 (callee-filled)
        lrSharedInfo.mpDebugPrinter          = reinterpret_cast<DebugPrinter*>(
                                                  const_cast<u8*>(maDebugPrinterMain));         // +0x04
        lrSharedInfo.mpDebugLog              = const_cast<DebugLog*>(&mDebugLog);               // +0x08
        lrSharedInfo.mpICEWrapper            = const_cast<ICEWrapper*>(&mICEWrapper);           // +0x0C
        lrSharedInfo.mpOutputInterface       = reinterpret_cast<DirectorOutputInterface*>(
                                                  lpOutput->GetDirectorOutputIn());             // +0x10
        lrSharedInfo.mpStateContainer        = 0;                                   // +0x14 (callee-filled)
        lrSharedInfo.mpBehaviourManager      = const_cast<Camera::BehaviourManager*>(
                                                  &mBehaviourManager);                          // +0x18
        lrSharedInfo.mpNamedParameters       =
            &mBehaviourManager.GetBehaviourParameterBank().GetNamedParameters();    // +0x1C
        lrSharedInfo.mpMomentController      = const_cast<MomentController*>(&mMomentController); // +0x20
        lrSharedInfo.mpGameState             = const_cast<GameState*>(&maGameState);            // +0x24
        lrSharedInfo.mpRandom                = reinterpret_cast<Random*>(
                                                  const_cast<u8*>(maRandom));                   // +0x28
        lrSharedInfo.mpDirectorResourceManager = lpIO->mpResourceManager;                       // +0x2C
        lrSharedInfo.mpEffectInterface       = reinterpret_cast<const EffectInterface*>(
                                                  maEffectInterface);                           // +0x30
        // The input buffer hands the slot out as raw storage (its own FLAG); the cast is the
        // one place that types it. ⭐ The pointee is now the REAL, DWARF-declared
        // Camera::PlayerCrashInfo (2026-08-29) rather than the BrnDirector::PlayerCrashInfo
        // namespace fork -- so consumers read mbWrecked / mbHitWater by name.
        lrSharedInfo.mpPlayerCrashInfo       = static_cast<const Camera::PlayerCrashInfo*>(
                                                  lpInput->GetPlayerCrashInfo());               // +0x34
        lrSharedInfo.mpAllVehicleData        = &mAllVehicleData;                                // +0x38
        lrSharedInfo.mpPlayerTracker         = &mVehicleTracker;                                // +0x3C
        lrSharedInfo.mpControllerInfo        = reinterpret_cast<const ControllerInfo*>(
                                                  lpInput->GetControll());                      // +0x40
        // (The two reinterpret_casts that used to launder these through the retired
        //  BrnDirector::VehicleInfo namespace fork are gone -- 2026-08-29, crash-camera wave.)
        lrSharedInfo.mpRaceCars              = lpRaceCars;                                       // +0x44
        lrSharedInfo.mpPlayerCar             = lpPlayerCar;                                      // +0x48
        // X360: mpPlayerCar + 496 -- RaceCarState::mTransform, reached by name.
        lrSharedInfo.mpPlayerCarTransform    = lpPlayerCar ? &lpPlayerCar->mRaceCarState.mTransform
                                                           : 0;                                 // +0x4C
        lrSharedInfo.mePlayerActiveRaceCarIndex = liPlayerCarIndex;                             // +0x50
        lrSharedInfo.mpRotationController    = reinterpret_cast<const Camera2DRotationController*>(
                                                  maRotationController);                        // +0x54
        lrSharedInfo.mpSphericalRotationController =
            reinterpret_cast<const CameraSphericalRotationController*>(
                maSphericalRotationController);                                                 // +0x58
        lrSharedInfo.mfTimestep              = lfTimestep;                                      // +0x5C
        lrSharedInfo.mfSimTimestep           = lfSimTimestep;                                   // +0x60
    }

    // ------------------------------------------------------------------------
    // UpdateArbitrator  @ 0x82271120
    //
    // Build this frame's ArbStateSharedInfo (above) and drive the arbitrator with it. The
    // X360 tail is:
    //
    //     Arbitrator::Update( this + 77248,                       // == &mArbitrator
    //                         lpInputBuffer[+0x7AC8],             // mbSimPaused
    //                         lrCameraInOut,                      // the frame camera
    //                         lSharedInfo,
    //                         controller[+3],                     // cycle-camera
    //                         controller[+2] );                   // cycle-camera-held
    //
    // The two control bytes are read straight off the committed ControlInput block the input
    // buffer publishes (GetControll()); the X360 fetches the block twice because it re-issues
    // the accessor, not because they come from different places.
    // ------------------------------------------------------------------------
    void MainDirector::UpdateArbitrator(const DirectorInputOutput* lpIO,
                                        Camera::Camera& lrCameraInOut,
                                        s32 liPlayerCarIndex)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        ArbStateSharedInfo lSharedInfo;
        BuildArbStateSharedInfo(lpIO, liPlayerCarIndex, lSharedInfo);

        const DirectorIO::ControlInput* lpControl = lpInput->GetControll();

        mArbitrator.Update(lpInput->IsSimPaused(), lrCameraInOut, lSharedInfo,
                           lpControl->IsCycleCameraPressed(),
                           lpControl->IsCycleCameraHeld());
    }

    // ------------------------------------------------------------------------
    // PreSceneQueryUpdate  @ 0x8225BA00
    //
    // The pre-scene-query pass. Its ENTIRE body is guarded by one condition, reproduced here
    // faithfully: the director only does pre-query work when there is a LIVE PLAYER CAR.
    //
    // Inside that guard the X360 runs, in order:
    //     AllVehicleData::Update( ..., usedRaceCars, vehicleInfoArray, playerIndex, contacts )
    //     MainDirector::ProcessInputQueue( this, lpIO )
    //     if ( !lpInputBuffer->IsSimPaused() )
    //     {
    //         miForcedCameraCarIndex = playerIndex;
    //         VehicleTracker::Update( maVehicleTracker, maGameState, input, playerIndex, ... )
    //     }
    //     CrashAnalyser::Update( &mCrashAnalyser /*+0x1245C*/, input, &maGameState, playerIndex )
    //     Camera::BehaviourManager::ReleaseBehaviours( &mBehaviourManager )
    //     if ( mbDisableAftertouchCamera )  maGameState.mbImpactTimeActive = false
    //     MainDirector::UpdateCameraBehavioursPreScene( this, lpIO, playerIndex )
    // and at the very top, before the guard, a debug-tweakable latch clear:
    //     if ( mbDebugSingleTimestep /*+0x3543D*/ )  mbDebugZeroTimestep /*+0x3543C*/ = 0
    //                                                    (0x8225BA1C..0x8225BA40)
    //
    // ⭐ STEP 2 IS NOW REAL. ProcessInputQueue is the ONLY consumer of the input buffer's
    // GAME-ACTION QUEUE and therefore the only writer of GameState::meJunkyardState in the
    // whole director. Until it ran, the junkyard sub-state could never leave E_JY_INACTIVE and
    // ArbStateRoaming could never hand the arbitrator to E_STATE_CAR_SELECT -- i.e. neither
    // the junkyard nor the retail intro camera was reachable, at ANY link-closure count.
    //
    // ⭐ EVERY STEP OF THE GUARDED BODY NOW RUNS (2026-09-24, FX-DIRECTOR2 landed the last one,
    // CrashAnalyser::Update). What is still deferred lives INSIDE the callees and is flagged there:
    // VehicleTracker's score copy. (AllVehicleData::Update is the console's whole body since
    // 2026-09-25 -- the traffic array and the team words now arrive.) The one step NOT reproduced
    // is the debug-tweakable latch clear at the top (mbDebugSingleTimestep -> mbDebugZeroTimestep,
    // both seeded 0 by Construct at 0x8225B9DC / 0x8225B9C8, `li r31, 0` @0x8225B484, and inert on
    // retail).
    //
    // ⚠️ THE CONSOLE'S SECOND TEST is reproduced: `usedRaceCars.IsBitSet(playerCarIndex)`.
    // GetLivePlayerCarIndex already folds it in (see the header), so the guard below IS both
    // halves -- the X360 re-tests the bit because it inlines the index fetch, not because
    // there is a second condition.
    //
    // ------------------------------------------------------------------------
    void MainDirector::PreSceneQueryUpdate(const DirectorInputOutput* lpIO)
    {
        // ⚠️ GATE (debug only): `if (+0x3543D) stbx 0 -> +0x3543C` @0x8225BA24..0x8225BA40 --
        // maStateFlagTail[E_FLAG_TAIL_DEBUG_SINGLE_TIMESTEP] clearing
        // maStateFlagTail[E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP]. Not reproduced here.

        const s32 liPlayerCarIndex = GetLivePlayerCarIndex(lpIO);
        if (liPlayerCarIndex == -1)
            return;

        // ⭐⭐ X360 line 1 of the guarded body: AllVehicleData::Update @0x8221D938 -- the ONLY
        // producer of the snapshot every camera behaviour resolves its VehicleRefs against.
        // ⭐ THE CONSOLE'S FULL CALL since 2026-09-25 (crash parity FX-DIRECTOR2), 0x8225BC28..
        // 0x8225BC70, every argument out of the input buffer:
        //   r4  `ld 0(GetUsedRaceCars())`            the used-race-car bits (0x8225BC34 / BC5C)
        //   r5  sub_82207040(input)                  the VehicleInfo[8] @+0x990 (0x8225BC50)
        //   r6  r18                                  the live player index
        //   r7  input + 0x6AC0 + 0x10                GetTrafficOutputInterface()'s entity array
        //                                            (0x8225BC30 / 0x8225BC4C) -- lpTrafficVehicleArray
        //   r8  GetVehicleInfoArray() 0x82206EF0     the eight team words @+0x3238 -- lpaVehicleTeams
        // The PC used to run an extracted leg (UpdateRaceCarsBringUp) with neither of the last two:
        // the traffic array had no home in the input and every nearest-car row carried team 0.
        {
            const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
            const CgsContainers::BitArray<8u>* lpUsedRaceCars = lpInput->GetUsedRaceCars();
            const u32* lpaVehicleTeams = lpInput->GetVehicleInfoArray();
            const BrnDirector::Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();
            mAllVehicleData.Update(*lpUsedRaceCars, lpRaceCars,
                                   static_cast<EActiveRaceCarIndex>(liPlayerCarIndex),
                                   &lpInput->GetTrafficOutputInterface()->GetTrafficDirectorEntityArray(),
                                   lpaVehicleTeams);
            DirectorTrafficDiag(mAllVehicleData);   // [diag, NOT X360] BRN_DIRECTOR_TRAFFIC_DIAG
        }

        // ⭐ X360 line 2 of the guarded body.
        ProcessInputQueue(lpIO);

        // ⭐⭐ X360 LINE 5 IS NO LONGER GATED (2026-08-29, crash-camera wave):
        //         if (!input->IsSimPaused()) {
        //             miForcedCameraCarIndex = playerIndex;
        //             VehicleTracker::Update(maVehicleTracker, maGameState, input, playerIndex, ...)
        //         }
        //
        // ⛔ IT WAS THE SIXTH BREAK IN THE CRASH-CAMERA CHAIN, and the quietest. The tracker owns
        // the tracked car's LINEAR-VELOCITY JOURNAL, and ImpactSlomoController::Update -- the
        // crash slow motion itself -- decides whether to start a burst from that journal alone
        // (speed over 30 MPH, the .y lane rising last frame and falling this one, car airborne).
        // With the tracker never updated the journal is empty, its magnitude reads 0, and the
        // entry gate can NEVER pass. MEASURED with the whole rest of the chain already landed:
        // a forced player crash held mbCrashing for 900+ frames and the sim scale never left
        // 1.000000. Nothing asserted and nothing looked wrong -- the camera simply never slowed
        // down, which is exactly the failure mode a "documented quiet gate" produces.
        //
        // The gate's own justification -- "VehicleTracker is a named opaque region with no
        // interior" -- had expired: the type has a full reconstruction and Update has a full
        // body; what was missing was that the body reached a private ODR fork of
        // DirectorIO::InputBuffer whose accessors nothing defined. That fork is retired and the
        // body is re-fitted to the real InputBuffer, so this call is now honest.
        //
        // ⚠️ STILL GATED INSIDE Update: the score copy (see the GATE note in
        // BrnDirectorVehicleTracker.cpp). The crash-ENERGY classifier is live since 2026-09-24
        // (FX-DIRECTOR2: its two bands read out of the image), and the fourth argument is the
        // console's: `lbzx r7, r28, 0x35437` @0x8225BCBC -- mbForceNextWorldCrashToBeFastTopDown,
        // a ONE-SHOT: Construct seeds it 1 (0x8225B924) and Update's tail clears it on the first frame
        // of a crash (0x822752B4..0x822752D0), so only the first wall crash after boot is forced HIGH.
        // It used to be a literal false here, so that crash could never classify HIGH the console's way.
        if (!lpIO->mpInputBuffer->IsSimPaused())
        {
            miForcedCameraCarIndex = liPlayerCarIndex;
            mVehicleTracker.SetVehicleIndex(liPlayerCarIndex);
            mVehicleTracker.Update(&maGameState, lpIO->mpInputBuffer,
                                   static_cast<EActiveRaceCarIndex>(liPlayerCarIndex),
                                   maStateFlagTail[E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN] != 0);
        }

        // ⭐ REAL (2026-09-24, FX-DIRECTOR2): CrashAnalyser::Update(this + 0x1245C, input, &maGameState,
        // playerIndex) @0x8225BCDC -- unconditional, AFTER the sim-paused tracker block, exactly as the
        // console orders it. It is the only producer of the crash analysis UpdateMoments publishes, so
        // until it ran every moment read a static zero and MomentHardStop's CrashStart gate never held.
        // UN-GATED 2026-09-25: the chain it feeds is proven against the ARTIST export frame for frame --
        // the HardStop moment allocates on the crash frame N (ArbStateCrashing::Prepare 0x822655E8 ->
        // NewMoment 0x82255850), goes VALID on N+1, MainDirector::Update publishes its 0.005..0.01 scale
        // (0x82275148) and, with the step's timer order corrected the same day, the world integrates
        // step N+2 at it (2 full-dt crash frames, the console's count). Until the camera scene-query
        // closure is live (BRN_FXD2_SCENEQUERY, waiting on FineIntersectionTestModule::
        // ComputeLineTestNearest @0x828C8CC8, the octree line walk's next hop) the console's
        // visibility failure of the shot cannot happen here: the HardStop's cameras are always valid.
        mCrashAnalyser.Update(lpIO->mpInputBuffer, &maGameState,
                              static_cast<EActiveRaceCarIndex>(liPlayerCarIndex));
        // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. The analysis on the frames it raises
        // an event (a crash's first frame only), next to the [crashcam] line that names the moment
        // the crash is then filmed through.
        if (mCrashAnalyser.GetAnalysis().mxEventFlags != 0)
        {
            static const bool sbCrashCamDiag = (getenv("BRN_CRASHCAM_DIAG") != 0);
            if (sbCrashCamDiag && CgsDev::Log::gpDebugPrint != 0)
            {
                const CrashAnalysis& lrAnalysis = mCrashAnalyser.GetAnalysis();
                *CgsDev::Log::gpDebugPrint << "[crashcam] crash analysis: flags "
                    << static_cast<s32>(lrAnalysis.mxEventFlags)
                    << " crashing " << (lrAnalysis.mbIsPlayerCrashing ? 1 : 0)
                    << " suggestLeft " << (lrAnalysis.mbSuggestLeftOfLineOfAction ? 1 : 0) << "\n";
            }
        }

        // ⭐⭐ X360 LINE 6 (@0x8225BCF0). BODIED SINCE THE BANNER ABOVE WAS WRITTEN AND STILL
        // NOT CALLED -- BehaviourManager::ReleaseBehaviours has a full body in
        // BrnBehaviourManager.cpp and had NO caller anywhere in the image, so a released
        // BehaviourHandle only ever set its needs-releasing bit and NOTHING ever handed the
        // pool slot back. MEASURED consequence (2026-08-01): ArbStateCarSelect cycling
        // ROTATE_ABOUT_CAR <-> ACTIVE allocates one BehaviourInterpolate per cycle, and after
        // 20 cycles the 20-slot behaviour pool is exhausted -- "Array index out of bounds"
        // (CgsObjectPool.h:139) out of NewBehaviour<BehaviourInterpolate>, plus a rising tide
        // of "the set limit (N) has been reached" interpolation-lock asserts from the source
        // helpers that were never unlocked either (an interpolate behaviour releases its two
        // CameraReference locks in its Release virtual, i.e. from HERE).
        // It has to run BEFORE the pre-scene behaviour pass below, which is exactly where the
        // console puts it.
        mBehaviourManager.ReleaseBehaviours();

        // The GameState latch clear, @0x8225BCEC..0x8225BD08:
        //     if (lbzx +0x35430 /*mbDisableAftertouchCamera*/) stbx r16(=0) -> +0x338E5
        // i.e. maGameState + 0x105 == GameState::mbImpactTimeActive. A tweakable the console seeds 0
        // (0x8225B914) and never sets, so this is inert on retail -- reproduced for order.
        if (maStateFlagTail[E_FLAG_TAIL_DISABLE_AFTERTOUCH_CAMERA])
            maGameState.mbImpactTimeActive = false;

        // ⭐⭐ X360 LINE 7, THE LAST STEP OF THE GUARDED BODY (@0x8225BD18). This is where the
        // console runs Behaviour::Update for every live behaviour -- NOT inside MainDirector::
        // Update, which runs the COLLISION pass instead. Until this call existed, the PostScene
        // entry stood in for both and vtable slot 3 was never dispatched at all; see the banner
        // on UpdateCameraBehavioursPreScene.
        UpdateCameraBehavioursPreScene(lpIO, liPlayerCarIndex);
    }

    // ========================================================================================
    // THE GAMEPLAY-CAMERA ATTRIBUTE SEED  (camera parameter-chain wave, 2026-08-02)
    //
    // ⭐⭐ These two functions are the ONLY writers of BehaviourGameplayExternal::Parameters::
    // mbIsValid and BehaviourGameplayBumper::Parameters::mbIsValid anywhere on this build
    // (the third console writer, ReplayDirector::PreSceneQueryUpdate @0x8225BD28, is
    // declaration-only and the PC has no replay path). Both camera behaviours' whole Update
    // body sits inside `if (mpParameters->mbIsValid)`, so until one of these runs, the chase
    // and bumper cameras are structurally incapable of doing anything -- which is why
    // `sBringUpCamera` still draws the world after car select.
    //
    // Both seed the SAME two blocks, which live in the behaviour manager's parameter bank:
    //     bank + 0x2488  the external ("chase") block   == director + 0x314C8
    //     bank + 0x2538  the bumper ("in car") block    == director + 0x31578
    //     bank + 0x2480  the latched car attribs key    == director + 0x314C0
    // (the director-relative displacements are the ones these two functions inline; the
    // bank-relative ones are SharedCameraContainer::Prepare's. The bridge between them is
    // BehaviourManager == director + 0x1CB10, read off UpdateCameraBehavioursPreScene's own
    // `addis r26,r31,2 / addi r26,r26,-0x34F0`. See BrnBehaviourParameterBank.h.)
    //
    // The source of the tuning is the car's `burnoutcarasset` collection -- resolved by the
    // 64-bit key the world publishes -- and, inside it, the two RefSpecs at data +0x1A0
    // (cameraexternalbehaviour) and +0x1B8 (camerabumperbehaviour). Each resolved instance's
    // attribute data area IS Parameters::Source: Set re-loads `lwz r11, 4(source)` before
    // every field copy, and +0x04 of an Attrib::Instance is mpAttributeData.
    // ========================================================================================

    namespace
    {
        // Stage a Parameters::Source over a resolved generated instance's attribute data
        // area. The console passes the stack Attrib::Gen::* object straight to Set and lets
        // Set read its +0x04 slot; on x64 that slot is at +0x08, so the source block is
        // staged BY NAMED MEMBER here instead of reinterpret_cast'ing the instance.
        template <class TParameters, class TInstance>
        void SeedGameplayCameraParameters(TParameters& lrParameters, const TInstance& lrInstance)
        {
            typename TParameters::Source lSource;
            lSource.mpfValues = static_cast<const f32*>(lrInstance.GetLayoutPointer());
            lrParameters.Set(&lSource);
        }

        // The two Boost-FOV tripwire operands, as byte offsets into each camera's attribute
        // data area (`lfs f0, 0x18(r11)` / `lfs f0, 0x40(r11)`). They are the same two source
        // slots Parameters::Set copies mfBoostFOV from.
        const u32 KU_BUMPER_SOURCE_BOOST_FOV_OFFSET   = 0x18;
        const u32 KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET = 0x40;

        f32 ReadCameraSourceFloat(const void* lpAttributeData, u32 luByteOffset)
        {
            if (lpAttributeData == 0)
            {
                return 0.0f;
            }
            return *reinterpret_cast<const f32*>(
                static_cast<const u8*>(lpAttributeData) + luByteOffset);
        }
    }

    // ------------------------------------------------------------------------
    // ProcessNewVehicleEvents  @ 0x8221A6B0   -- ⭐⭐ THE ONLY PRIMARY SEED
    //
    // Console body, per event in lpInput->GetVehicleInputInterface()'s NewVehicleEvent queue:
    //     assert lEvent.mAttribsKey != 0                                         // .cpp:1783
    //     burnoutcarasset lCar( lEvent.mAttribsKey, 0 )    // FindCollection(class, key)
    //     assert lCar.IsValid()                                                  // .cpp:1787
    //     camerabumperbehaviour   lBumper  ( RefSpec(carData + 0x1B8).GetCollection(), 0 )
    //     cameraexternalbehaviour lExternal( RefSpec(carData + 0x1A0).GetCollection(), 0 )
    //     <lpcName = *(carData + 0x1E8), assert text only>
    //     assert lBumper.IsValid()                                               // .cpp:1795
    //     assert bumperData[0x18]   > 0.0f                                       // .cpp:1796
    //     assert lExternal.IsValid()                                             // .cpp:1797
    //     assert externalData[0x40] > 0.0f                                       // .cpp:1798
    //     BehaviourGameplayBumper  ::Parameters::Set( director + 0x31578, &lBumper   )
    //     BehaviourGameplayExternal::Parameters::Set( director + 0x314C8, &lExternal )
    //     std  lEvent.mAttribsKey, 0(director + 0x314C0)          <- latch, for UpdateAttribSys
    //
    // ⚠️ THE FIVE ASSERTS ARE NON-GATING, AND SO ARE THE TWO Set CALLS -- the console runs
    // both unconditionally, even when the collections did not resolve (in which case each
    // generated ctor has substituted Attrib::DefaultDataArea, i.e. zeros). That is
    // reproduced verbatim: an invalid car asset produces a VALID-but-zero parameter block on
    // the console too. UpdateAttribSys below is the one that gates its Sets on IsValid().
    // ------------------------------------------------------------------------
    void MainDirector::ProcessNewVehicleEvents(const DirectorIO::InputBuffer* lpInput)
    {
        if (lpInput == 0)
            return;

        const BrnDirectorVehicleInputInterface* lpVehicleInput = lpInput->GetVehicleInputInterface();
        if (lpVehicleInput == 0)
            return;

        const BrnDirectorVehicleInputInterface::NewVehicleEventQueue* lpQueue =
            lpVehicleInput->GetNewVehicleEventQueue();

        Camera::BehaviourParameterBank& lrBank = mBehaviourManager.GetBehaviourParameterBank();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const NewVehicleEvent& lrEvent = lpQueue->GetEvent(liEvent);

            CGS_ASSERT(lrEvent.mAttribsKey != 0, "lEvent.mAttribsKey!=0");            // :1783

            Attrib::Gen::burnoutcarasset lCarAsset(lrEvent.mAttribsKey, 0);
            CGS_ASSERT(lCarAsset.IsValid(), "Invalid car asset, key:");               // :1787

            Attrib::RefSpec* lpBumperRef   = lCarAsset.GetBumperCamRefSpec();
            Attrib::RefSpec* lpExternalRef = lCarAsset.GetExternalCamRefSpec();

            Attrib::Gen::camerabumperbehaviour lBumperCam(
                (lpBumperRef != 0)
                    ? const_cast<Attrib::Collection*>(lpBumperRef->GetCollection()) : 0, 0);
            Attrib::Gen::cameraexternalbehaviour lExternalCam(
                (lpExternalRef != 0)
                    ? const_cast<Attrib::Collection*>(lpExternalRef->GetCollection()) : 0, 0);

            // The console loads the asset name here; it only feeds the assert messages.
            (void)lCarAsset.GetAssetName();

            CGS_ASSERT(lBumperCam.IsValid(), "Invalid bumpercam asset, key:");        // :1795
            CGS_ASSERT(ReadCameraSourceFloat(lBumperCam.GetLayoutPointer(),
                                             KU_BUMPER_SOURCE_BOOST_FOV_OFFSET) > 0.0f,
                       "Invalid bumpercam Boost FOV, key:");                          // :1796
            CGS_ASSERT(lExternalCam.IsValid(), "Invalid externalcam asset, key:");    // :1797
            CGS_ASSERT(ReadCameraSourceFloat(lExternalCam.GetLayoutPointer(),
                                             KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET) > 0.0f,
                       "Invalid externalcam Boost FOV, key:");                        // :1798

            SeedGameplayCameraParameters(lrBank.GetGameplayBumperCameraParamsForCar(), lBumperCam);
            SeedGameplayCameraParameters(lrBank.GetGameplayExternalCameraParamsForCar(), lExternalCam);

            lrBank.SetGameplayCameraCarAttribsKey(lrEvent.mAttribsKey);

            // [diag, one-shot -- NOT console code] the far end of the chain the world's
            // new-vehicle publish starts. It reports the two bytes that decide whether the
            // console gameplay cameras can run at all. Remove when the chase camera's own
            // Update lands and the camera itself is the evidence.
            {
                static bool sbReported = false;
                if (!sbReported && (CgsDev::Message::gxMessageFilterFlags & 1) &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    sbReported = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[newveh] MainDirector::ProcessNewVehicleEvents: seeded from key hi "
                        << static_cast<s32>(lrEvent.mAttribsKey >> 32) << " lo "
                        << static_cast<s32>(lrEvent.mAttribsKey & 0xFFFFFFFFu)
                        << " -- externalValid "
                        << (lrBank.GetGameplayExternalCameraParamsForCar().mbIsValid ? 1 : 0)
                        << " FOV " << lrBank.GetGameplayExternalCameraParamsForCar().mrFOV
                        << " boostFOV " << lrBank.GetGameplayExternalCameraParamsForCar().mfBoostFOV
                        << " | bumperValid "
                        << (lrBank.GetGameplayBumperCameraParamsForCar().mbIsValid ? 1 : 0)
                        << "\n";
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // UpdateAttribSys  @ 0x8221AFD0   -- the GameTalk live-tuning re-read (54 asm lines)
    //
    //     if ( !lpInput->GetControll()->mbGameTalkRefreshRequest )   return;   // controller +1
    //     burnoutcarasset lCar( bank.mxGameplayCameraCarAttribsKey, 0 );       // director+0x314C0
    //     camerabumperbehaviour   lBumper  ( RefSpec(carData + 0x1B8).GetCollection(), 0 );
    //     cameraexternalbehaviour lExternal( RefSpec(carData + 0x1A0).GetCollection(), 0 );
    //     if ( lBumper.IsValid()   )  bumperParams  .Set( &lBumper   );        // director+0x31578
    //     if ( lExternal.IsValid() )  externalParams.Set( &lExternal );        // director+0x314C8
    //
    // ⚠️⚠️ THIS IS NOT A PER-FRAME RE-SEED, and the camera-chain map carried into this wave
    // said it was. Its ONE gate is ControllerInfo +0x01, which the DecFIGS DWARF names
    // mbGameTalkRefreshRequest (BrnDirectorControllerInfo.h:49) -- the authoring tool's
    // "I changed a value, re-read it" pulse. Nothing on this build ever sets it, so this body
    // is inert here BY DESIGN, not by omission. It is transcribed anyway because it is cheap,
    // it is the console's, and it is the consumer that explains why the latched key exists at
    // all; but ProcessNewVehicleEvents above is the function that does the work.
    //
    // Unlike ProcessNewVehicleEvents this one GATES each Set on the resolve, and it carries
    // no asserts.
    // ------------------------------------------------------------------------
    void MainDirector::UpdateAttribSys(const DirectorIO::InputBuffer* lpInput)
    {
        if (lpInput == 0)
            return;

        const DirectorIO::ControlInput* lpControl = lpInput->GetControll();
        if (lpControl == 0 || !lpControl->IsGameTalkRefreshRequested())
            return;

        Camera::BehaviourParameterBank& lrBank = mBehaviourManager.GetBehaviourParameterBank();

        Attrib::Gen::burnoutcarasset lCarAsset(lrBank.GetGameplayCameraCarAttribsKey(), 0);

        Attrib::RefSpec* lpBumperRef   = lCarAsset.GetBumperCamRefSpec();
        Attrib::RefSpec* lpExternalRef = lCarAsset.GetExternalCamRefSpec();

        Attrib::Gen::camerabumperbehaviour lBumperCam(
            (lpBumperRef != 0)
                ? const_cast<Attrib::Collection*>(lpBumperRef->GetCollection()) : 0, 0);
        Attrib::Gen::cameraexternalbehaviour lExternalCam(
            (lpExternalRef != 0)
                ? const_cast<Attrib::Collection*>(lpExternalRef->GetCollection()) : 0, 0);

        if (lBumperCam.IsValid())
        {
            SeedGameplayCameraParameters(lrBank.GetGameplayBumperCameraParamsForCar(), lBumperCam);
        }
        if (lExternalCam.IsValid())
        {
            SeedGameplayCameraParameters(lrBank.GetGameplayExternalCameraParamsForCar(), lExternalCam);
        }
    }

    // ------------------------------------------------------------------------
    // The event-presentation reset both event-boundary legs open-code (the prepare-for-mode
    // handler and ProcessInputQueue's stop-mode arm run the identical store list): it is
    // GameState::ShowTimeInfo::Clear (DWARF BrnDirectorGameState.h:224), inlined by the console at
    // both sites (case 39: 0x82237DAC..0x82237DCC off `addi r11, r11, 0x39BC` == GameState +0x1DC).
    // [FX-DIRECTOR 2026-09-24] The sub-object is a named DWARF type now; this used to poke its
    // bytes through two misplaced opaque blobs (same offsets, same values).
    // ------------------------------------------------------------------------
    static void ClearEventPresentationBlock(GameState& lrGameState)
    {
        lrGameState.mShowTimeInfo.Clear();
    }

    // ------------------------------------------------------------------------
    // HandlePrepareForModeAction   -- ⭐⭐ THE EVENT-ENTRY PUSH
    //
    // Fold a prepare-for-mode action into the GameState snapshot. This is the ONLY producer of
    // GameState::E_EVENT_STATE_PRE_INTRO in the image, and (with the two event-end legs) one of
    // only three writers of GameState::meEventType -- which is why the roaming state's PRE_INTRO
    // arm and the whole race-intro camera ladder were dead while it was declaration-only.
    //
    // Console shape, store for store:
    //   * run only for the FIRST prepare of an event (all-in-one, or the first of a split pair);
    //     the second half of a split prepare is ignored here.
    //   * if the online post-event is on screen, DEFER: copy the whole record into
    //     maModeActionAndDebugBlock and raise the deferred bit. PostGuiUpdate replays it from
    //     there when the post-event ends.
    //   * otherwise push PRE_INTRO and seed the event snapshot from the mode params.
    //
    // The record type is BrnGameState::GameStateModuleIO::PrepareForModeAction (BrnGameActions.h), whose layout is
    // byte-exact against the console's own 0x8E0 post, so every field below is a named member --
    // no offset indexing, which matters because the embedded GameModeParams does NOT keep the
    // console's byte offsets on this host.
    // ------------------------------------------------------------------------
    void MainDirector::HandlePrepareForModeAction(const BrnGameState::GameStateModuleIO::PrepareForModeAction& lrAction,
                                                  const DirectorInputOutput* lpIO)
    {
        // The console tests the stage enum inline (`== 0 || == 1`); PrepareForModeAction spells
        // that predicate by name.
        if (!lrAction.IsFirstPrepareForMode())
            return;

        // Deferred arm: the online post-event owns the camera, so park the record and leave the
        // event state alone. The copy is the console's `memcpy(this + <block>, action, 0x8E0)`.
        if (maStateFlagTail[E_FLAG_TAIL_IN_ONLINE_POST_EVENT])
        {
            static_assert(sizeof(BrnGameState::GameStateModuleIO::PrepareForModeAction) <= sizeof(maModeActionAndDebugBlock),
                          "the deferred prepare-for-mode record must fit the block the console copies it into");
            std::memcpy(maModeActionAndDebugBlock, &lrAction, sizeof(BrnGameState::GameStateModuleIO::PrepareForModeAction));
            maStateFlagTail[E_FLAG_TAIL_MODE_ACTION_DEFERRED] = 1;
            return;
        }

        const BrnGameState::GameModeParams& lrParams = *lrAction.GetGameModeParams();

        // ⭐ THE PUSH. `idx = (idx + 1) % 2; entries[idx] = v; if (size < 2) ++size` IS
        // DataJournal<T,2>::SetCurrent, so the console's open-coded push is spelt by name.
        maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_PRE_INTRO);
        maGameState.meEventType = lrParams.GetGameModeType();                       // +0x120

        maGameState.mbGoToCrashModeAfterIntro =
            lrParams.GetFlag(BrnGameState::GameModeParams::KU_FLAG_SET_DIRECTOR_TO_CRASH_MODE_AFTER_INTRO);
        // The console's `cntlzw`-normalised `mbIsOnline == 0`: slow motion is an offline-only
        // privilege, and this is where an event withdraws it.
        maGameState.mbCanUseSlomo = !lrParams.mbIsOnline;                            // +0x100
        maGameState.miEventSpecificShotGroup = lrAction.GetShotGroup();              // +0x134

        // The event's own per-frame presentation state, reset for the new event.
        maGameState.mfHowCloseToTotalled            = 0.0f;                          // +0x1BC
        maGameState.mbRoadRageOneMoreCrashToWrecked = false;                         // +0x1C0
        maGameState.mbRoadRageTotalled              = false;                         // +0x1C1

        // The showtime / profile sub-objects' per-event fields. Their field layouts are
        // unreliable (see BrnDirectorGameState.h), so the console's stores go through the
        // opaque storage at its own documented offsets -- the same shape GameState::Clear and
        // GameState::ResetPerFrameData already use for this region. The stop-mode arm in
        // ProcessInputQueue runs the identical block; the console open-codes it in both.
        ClearEventPresentationBlock(maGameState);

        // The event's junction logic box, resolved from the mode's traffic-light trigger. The
        // handle's invalid form is "hull == 0xFFFF or index == 0xFF", which is exactly what
        // GameModeParams::Construct seeds it to, so a mode with no start lights lands here with
        // a null box rather than a query.
        // ⚠️ DIVERGENCE, deliberate: the console dereferences the traffic data unconditionally
        //   on the valid arm. WorldMap::GetTrafficData returns null until the map's traffic
        //   resource is loaded (it gates on the load state), and this handler can run before
        //   that on this build, so the null is checked instead of crashed on. The stored value
        //   is the same in every case the console reaches.
        const u32 luLightTriggerId = static_cast<u32>(lrParams.GetTrafficLightTriggerId());
        const bool lbLightTriggerIdValid = ((luLightTriggerId & 0x00FFFF00u) != 0x00FFFF00u) &&
                                           ((luLightTriggerId & 0x000000FFu) != 0x000000FFu);
        const BrnTraffic::TrafficData* lpTrafficData =
            lbLightTriggerIdValid ? lpIO->mpWorldMap->GetTrafficData() : 0;
        maGameState.mpEventJLBox =
            (lpTrafficData != 0)
                ? lpTrafficData->GetJunctionLogicBoxForTrafficLight(luLightTriggerId)
                : 0;                                                                 // +0x000
    }

    // ------------------------------------------------------------------------
    // ProcessInputQueue  @ 0x822372F8   -- ⭐⭐ THE GAME-ACTION -> GAMESTATE SEAM
    //
    // Drain the input buffer's game-action queue (DirectorIO::InputBuffer::GetGameActionQueue
    // @0x82206C50 == the buffer's embedded CgsModule::VariableEventQueue<13312,16>) and fold
    // each action into the director's GameState snapshot. The console's shape is:
    //
    //     lpQueue = lpIO->mpInputBuffer->GetGameActionQueue();
    //     mGameState.ResetPerFrameData();                       // inlined; see the GameState TU
    //     ProcessNewVehicleEvents( lpIO->mpInputBuffer );
    //     <the takedown pair + three straight copies out of the input buffer>
    //     for ( action = queue.GetFirstEvent(); action; action = queue.GetNextEvent(action) )
    //         switch ( action.type ) { ... 45 handled cases out of a 225-entry table ... }
    //     <the three waiting -> receivedThisFrame handshakes>
    //     <the slomo / crash / debug-render tail>
    //
    // ⚠️ WHAT IS BODIED HERE: the prologue, the walk, and the NINE junkyard / car-select cases
    // (62, 63, 64, 65, 73, 75, 76, 77, 85) plus the three handshakes. Those are the complete
    // set of arms that touch GameState +0x180..+0x1AC, i.e. the whole junkyard sub-machine.
    // ⭐⭐ [bugwave 2026-08-23] PLUS cases 56 and 58 -- THE STUNT/JUMP CAMERA ARMS. Both were
    // on the gated list below with the blanket reason "writes into a part of the GameState that
    // is still opaque". MEASURED AND FALSE for these two: both write ONLY
    // miThisFramesActionFlags (GameState +0x0E4 == MainDirector +211140) and
    // miActionRequestedCamera (+0x0E8 == +211144), and BOTH are real named members of
    // BrnDirector::GameState (BrnDirectorGameState.h:102/:105). Their consumers are already
    // mounted and already read those exact bits by name:
    //     flags 0x10 -> ArbStateRoaming::ProcessPossibleFX "Smash_Effect"
    //     flags 0x04 -> ArbStateRoaming::ProcessPossibleFX "Billboard_Effect"
    //     flags 0x08 -> MomentPlayerJumping::Update  (the plain airborne jump camera)
    //     flags 0x01 / 0x02 + miActionRequestedCamera
    //                 -> MomentPlayerStunt::Update   (the AUTHORED super-jump camera and its
    //                    first-time variant)
    // With these two arms gated, the smash/billboard camera post-FX never fired either, even
    // though the smash HUD banner chain has been proven end to end since the gateui wave -- so
    // this was a live defect for SMASHES today, not only for jumps.
    //
    // ⚠️ WHAT IS GATED, and why (each is a NO-OP here, never a wrong value):
    //   * 113 and 223 only -- both convert a global race-car index through the director input's
    //     global race-car interface (input + 0x10), which this build does not publish (see the
    //     gate at the foot of the switch).
    //     ⭐ 0 / 53 / 54 / 107 / 120 / 132 / 150 / 151 / 215 / 216 / 218 / 224 CAME OFF THIS LIST
    //     2026-09-24 (FX-DIRECTOR, crash-parity audit): the reason once given for all of them
    //     ("writes into a part of the GameState that is still opaque") was STALE -- every field
    //     they write is a named DWARF member. So did the tail's mbForceSloMoNotAllowed drop and
    //     the two inactivity clocks.
    //     ⭐ 42 / 43 / 140 / 144 / 145 / 146 (the Showtime arms) CAME OFF THIS LIST 2026-09-24
    //     (FX-DIRECTOR): every field they write is a named DWARF member (mbImpactTimeActive,
    //     mfImpactTimeSloMoFactor, GameState::ShowTimeInfo). Their absence floored Showtime's sim
    //     at 0.005x and left ArbStateCrashMode's close-ups and blurs with no request.
    //     ⭐ 205 is bodied (2026-09-17).
    //     ⭐ 24 (E_ACTION_BROADCAST_MODE_FINISH_LINES) CAME OFF THIS LIST 2026-09-24 (FX-FLOW,
    //     NEW-FINISHLINE): both fields it writes are named DWARF members (mFinishLineID,
    //     mFinishLineNorthmostDir) and its one call, BoxRegion::ComputeDirection, is bodied
    //     (SharedClasses/Trigger/BrnRegion.cpp). Its absence left mFinishLineID at 0 and fired
    //     "Unknown finish line" on every offline race finish.
    //     ⭐ 6 (E_ACTION_SET_TAKEDOWN_CAMERA_STATE) CAME OFF THIS LIST 2026-09-13. Its blanket
    //     reason -- "writes into a part of the GameState that is still opaque" -- had expired:
    //     all five fields the arm touches are named members of BrnDirector::GameState
    //     today (+0xDA mbTakedownActive, +0xDB mbIsRevengeTD, +0xDC mbIsShutdown,
    //     +0xDD mbIsSignatureTD, +0xE0 meTakedownVictimID). It is the ONLY writer of
    //     mbTakedownActive in the image, i.e. the takedown camera's entire gate
    //     (ArbStateRoaming takes its takedown edge on that flag).
    //     ⭐ 97 / 98 / 100 / 102 CAME OFF THIS LIST 2026-08-29 and are bodied below. The reason
    //     they were on it -- "an opaque GameState part / the un-homed VMX drive-thru transform
    //     pipeline" -- had expired: every field they touch is a named DWARF member today and the
    //     "pipeline" is two 64-byte matrix copies out of a serialised payload. They are the ONLY
    //     writers of mbDriveThruActive in the image, i.e. the drive-thru camera's entire gate.
    //   * the post-loop debug-render block (development drawing only).
    // The console's own default arm is a no-op `b def_...`, so an unhandled id costs nothing.
    //
    // ⚠️ ACTION IDs ARE X360 ids, which run +5 above the PS3 DecFIGS DWARF's E_ACTION_* enum
    // across this range. The shift is pinned at BOTH ends: case 77's own assert string is
    // "lpExitAction" (BrnMainDirector.cpp:1410) and DWARF 72 == E_ACTION_CAR_SELECT_EXIT.
    // ------------------------------------------------------------------------
    void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)
    {
        // 0x82237314 `lwz r30, 0(r4)` -- the console's FIRST act is to take the input buffer
        // out of the DirectorInputOutput. r4 being live is the whole reason this function's
        // declaration needed the argument back.
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
        if (lpInput == 0)
            return;

        const CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpInput->GetGameActionQueue();

        // 0x82237350..0x822373AC -- the inlined GameState::ResetPerFrameData().
        maGameState.ResetPerFrameData();

        // ⭐⭐ REAL as of 2026-08-02 (camera parameter-chain wave), at the console's own
        // position (immediately after ResetPerFrameData). The gate this replaces read
        // "declaration-only (AllVehicleData un-homed)" -- both halves were wrong; see the
        // declaration in BrnMainDirector.h. This is the ONLY writer of the two shared gameplay
        // cameras' Parameters::mbIsValid on this build.
        ProcessNewVehicleEvents(lpInput);

        // 0x822373B0 -- the takedown pair. The console re-reads the flag to assert on it
        // ("mbPlayerTakenDown", BrnMainDirector.cpp:232) between the two stores.
        if (lpInput->GetPlayerTakenDown())
        {
            maGameState.mbPlayerWasTakenDown = true;                        // +0x1C3
            CGS_ASSERT(lpInput->GetPlayerTakenDown(), "mbPlayerTakenDown");
            maGameState.mePlayerKillerIndex = lpInput->GetPlayerKillerCarIndex();  // +0x1C4
        }

        // 0x82237408..0x82237440 -- three straight copies out of the input buffer into the
        // GameState's three X360-only members (+0x1CC / +0x1D1 / +0x1D0 -- see the GameState
        // header: they sit BEFORE RankUpInfo, which is at +0x1D4).
        maGameState.miPlayerTeam = lpInput->GetPlayerTeam();                          // +0x1CC
        // ⭐ UN-GATED 2026-09-24 (crash parity FX-DIRECTOR) with its producer: the player's
        // end-of-event pair, which BridgeGameStateToDirector now publishes (0x823CD4DC /
        // 0x823CD510). `lbz 0x7AD6(r30)` -> `stbx 0x339B1` first, then `lbz 0x7AD5(r30)` ->
        // `stbx 0x339B0`; a plain copy every frame, no latch.
        maGameState.mbModeTimeExpired  = lpInput->GetModeTimeExpired();               // +0x1D1 (0x82237430 / 0x82237438)
        maGameState.mbPlayerEliminated = lpInput->GetPlayerEliminated();              // +0x1D0 (0x8223743C / 0x82237440)

        // [DIAG] BRN_DIRECTOR_ACTION_DIAG -- NOT IN THE X360 BINARY. Edge-triggered: one line each
        // time either copied flag changes, with the event type and team they arrive in. The one
        // reader, ArbStateRoaming::ProcessPossibleFX, acts on them in the online modes only. Its
        // own budget (at most 40 lines), so a stunt-heavy drive that spends the per-arm lines'
        // shared 400 cannot silence it.
        {
            static const bool sbEndOfEventDiag       = (getenv("BRN_DIRECTOR_ACTION_DIAG") != 0);
            static s32        siEndOfEventLinesLeft  = 40;
            static bool       sbLastModeTimeExpired  = false;
            static bool       sbLastPlayerEliminated = false;
            if ((maGameState.mbModeTimeExpired != sbLastModeTimeExpired ||
                 maGameState.mbPlayerEliminated != sbLastPlayerEliminated) &&
                sbEndOfEventDiag && siEndOfEventLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                --siEndOfEventLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[director-state] end-of-event pair -> +0x1D1 mbModeTimeExpired "
                    << (maGameState.mbModeTimeExpired ? 1 : 0) << " +0x1D0 mbPlayerEliminated "
                    << (maGameState.mbPlayerEliminated ? 1 : 0) << " (event type "
                    << maGameState.meEventType << ", team " << maGameState.miPlayerTeam << ")\n";
            }
            sbLastModeTimeExpired  = maGameState.mbModeTimeExpired;
            sbLastPlayerEliminated = maGameState.mbPlayerEliminated;
        }

        const CgsModule::Event* lpAction = 0;
        s32 liActionSize = 0;
        s32 liActionType = lpQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != 0)
        {
            // The console reads every payload as raw bytes off the record pointer (r30); the
            // per-action structs live in BrnGameActions.h, which is a GameState-side header
            // this TU does not own. Same access, named per case.
            const u8* lpacPayload = reinterpret_cast<const u8*>(lpAction);

            switch (liActionType)
            {
            // ---- 6  E_ACTION_SET_TAKEDOWN_CAMERA_STATE (8 bytes) ---------------------
            // ⭐⭐ THE TAKEDOWN CAMERA'S GATE. This arm is the ONLY writer of
            // GameState::mbTakedownActive in the whole image, and that flag is what
            // ArbStateRoaming takes its takedown edge on -- so with the arm absent the takedown
            // camera was unreachable no matter how complete the state below it was. Same shape
            // as the drive-thru break below: the state, its slot and its shot chain were all
            // there; nothing raised the flag the roaming state tests.
            //
            // The producer is TakedownManager::PostTakedownCameraState, which posts the 8-byte
            // SetTakedownCameraAction {victim index @+0x00, active @+0x04, signature @+0x05,
            // revenge @+0x06}: StartTakedownCamera posts {victim, 1, 0, isRevenge} and both
            // EndTakedownCamera and ClearAllTakedowns post {invalid, 0, 0, 0}. That second post
            // IS the clear -- the flag is not sticky and nothing in the arbitrator has to drop
            // it. Transcribed store-for-store:
            //     lbz  payload+0x04 -> mbTakedownActive            (+0xDA)
            //     lwz  payload+0x00 -> meTakedownVictimID          (+0xE0)
            //     re-read +0xDA; if set:
            //       lbz payload+0x05 -> mbIsSignatureTD            (+0xDD)
            //       lbz payload+0x06 -> mbIsRevengeTD              (+0xDB)
            //     else:  mbIsSignatureTD = mbIsShutdown = mbIsRevengeTD = 0
            // ⚠ The victim index is a full WORD at +0x00 while the three flags are BYTES at
            // +0x04..+0x06 -- read them at those widths, not as one packed word.
            // ⚠ mbIsShutdown (+0xDC) is cleared on the inactive arm only; the active arm never
            // writes it, exactly as here. Its producer is elsewhere.
            case 6:
            {
                s32 liFocusOnRaceCarIndex = 0;                       // payload +0x00
                std::memcpy(&liFocusOnRaceCarIndex, lpacPayload + 0x00, sizeof(s32));

                maGameState.mbTakedownActive   = (lpacPayload[0x04] != 0);
                maGameState.meTakedownVictimID =
                    static_cast<EActiveRaceCarIndex>(liFocusOnRaceCarIndex);

                if (maGameState.mbTakedownActive)
                {
                    maGameState.mbIsSignatureTD = (lpacPayload[0x05] != 0);
                    maGameState.mbIsRevengeTD   = (lpacPayload[0x06] != 0);
                }
                else
                {
                    maGameState.mbIsSignatureTD = false;
                    maGameState.mbIsShutdown    = false;
                    maGameState.mbIsRevengeTD   = false;
                }
                break;
            }

            // ---- 56  E_ACTION_ON_JUMP_START (24 bytes) -------------------------------
            // ⭐⭐ THE JUMP CAMERA REQUEST. Producer: StuntManager::UpdateJumps @0x8239D460
            // (`li r6,0x18 / li r5,0x38`), whose record the asm builds as
            //     std @+0x00 the stunt-element key (group id, or own id when grouped to 0)
            //     stw @+0x08 the camera CUT   (lbz region+0x34 forwards / +0x35 backwards, extsb)
            //     stw @+0x0C the camera TYPE  (lhz region+0x30 forwards / +0x32 backwards, extsh)
            //     stb @+0x10 the FIRST-TIME flag (a BYTE -- see the note below)
            // This arm, transcribed from 0x822381B4..0x82238230:
            //     lwz r11, 8(r30)        ; cut
            //     cmpwi r11, 1 ; bne ->  ; lwz r10, 0xC(r30) ; cmpwi r10,0 ; ble ->
            //     lbz r11, 0x10(r30)     ; first-time
            //     flags |= firstTime ? 2 : 1                   (stw  +0x338C4)
            //     miActionRequestedCamera = extsw(cameraType)  (stdx +0x338C8)
            //   else if (cut == 2 || cut == 0)  flags |= 8      (stw  +0x338C4)
            //
            // ⚠️ THE FIRST-TIME FIELD IS A BYTE AT +0x10, NOT A WORD. The console's consumer
            // reads it with `lbz 0x10(r30)`; on big-endian PPC that is the MOST-significant byte
            // of the word at +0x10, so a 0/1 stored there as an s32 would read as 0 on the
            // console every single time. The producer's `stb r30, 0xE0+var_80(r1)` @0x8239D6B4
            // proves the field really is a byte -- and the producer-side record in
            // BrnStuntManager.cpp is corrected to match in this same change. Read as a byte here
            // for the same reason.
            case 56:
            {
                // [bugwave 2026-08-23] NAMES CORRECTED (offsets unchanged, so behaviour here is
                // unchanged): +0x08 carries the StuntCameraType enum and +0x0C the authored camera
                // CUT index -- see the producer banner in BrnStuntManager.cpp, which had the two
                // values in each other's slots. The predicates below always tested the right
                // OFFSETS; only these local names were misleading.
                s32 liCameraType = 0;   // +0x08 -- E_STUNT_CAMERA_TYPE_{NO_CUTS,CUSTOM,NORMAL}
                s32 liCameraCut  = 0;   // +0x0C -- the authored camera index
                std::memcpy(&liCameraType, lpacPayload + 0x08, sizeof(s32));
                std::memcpy(&liCameraCut,  lpacPayload + 0x0C, sizeof(s32));
                const bool lbFirstTime = (lpacPayload[0x10] != 0);

                if (liCameraType == 1 && liCameraCut > 0)
                {
                    // An AUTHORED jump camera: raise the stunt-camera request bit
                    // (0x02 == "first time on this jump", 0x01 == a repeat) and publish which
                    // authored camera the region asks for. MomentPlayerStunt::Update reads both.
                    maGameState.miThisFramesActionFlags |= (lbFirstTime ? 0x2 : 0x1);
                    maGameState.miActionRequestedCamera  = static_cast<s64>(liCameraCut);
                }
                else if (liCameraType == 2 || liCameraType == 0)
                {
                    // No authored camera on this jump: raise the plain "player is jumping" bit
                    // MomentPlayerJumping::Update gates its attached-rig shot on.
                    maGameState.miThisFramesActionFlags |= 0x8;
                }

                // [DIAG] NOT IN THE X360 BINARY. One-shot: the jump ladder reached the Director.
                {
                    static bool sbLoggedJumpStart = false;
                    if (!sbLoggedJumpStart && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLoggedJumpStart = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] [jump-ladder] Director action=56 OnJumpStart"
                               " cut=" << liCameraCut
                            << " type=" << liCameraType
                            << " firstTime=" << (lbFirstTime ? 1 : 0)
                            << " -> actionFlags(dec)=" << maGameState.miThisFramesActionFlags << "\n";
                    }
                }
                break;
            }

            // ---- 97 / 98 / 100 / 102  THE DRIVE-THRU CAMERA'S GATE -----------------------
            // ⭐⭐⭐ [drive-thru camera wave 2026-08-29] THESE FOUR ARMS ARE THE ONLY WRITERS OF
            // GameState::mbDriveThruActive IN THE WHOLE X360 IMAGE, and that flag is the gate
            // on ArbStateRoaming::ProcessActiveDrivingTransitions -> E_STATE_DRIVETHRU
            // (BrnArbStateRoaming.cpp:1059). Structurally identical to the mbCrashActive break
            // the crash camera hit yesterday: the state, its container slot and its whole shot
            // chain can be perfect and the camera still never runs, because nothing raises the
            // flag the roaming state tests.
            //
            // ⛔ THE GATE THAT STOOD HERE SAID THESE CASES "write into a part of the GameState
            // that is still opaque, or call an un-homed aggregate (... the VMX drive-thru
            // transform pipeline ...)". BOTH HALVES ARE STALE. Every field these arms touch is
            // a NAMED DWARF member of BrnDirector::GameState today --
            //   +0x050 mBaseDriveThruTransform   +0x090 mDriveThruTransform
            //   +0x0D0 mbDriveThruActive         +0x0D4 meDriveThruType
            //   +0x1BC mfHowCloseToTotalled      +0x1C0 mbRoadRageOneMoreCrashToWrecked
            // -- and the "VMX pipeline" is four 16-byte row copies of a 64-byte matrix out of a
            // serialised action payload. Read from the ASM (0x82237EA4 / 0x82237FE8 /
            // 0x822380C8 / 0x822381A8), not the pseudocode, which renders the payload base as
            // `_R30 + 8` (it is `addi r11, r30, 0x40`) and gives cases 97 and 98 the same body.
            //
            // The 144-byte shop payload, from DriveThruManager::PostShopAction:
            //   +0x00  the drive-thru region's transform (Matrix44Affine, 64 bytes)
            //   +0x40  the identity block the producer builds inline (64 bytes)
            //   +0x80.. the per-action scalars, which differ per shop -- WHICH IS WHY THE
            //           "not online" BYTE IS AT A DIFFERENT OFFSET IN EVERY ARM
            //           (gas +0x80, body +0x84, paint +0x88). Do not fold these into one arm.
            //
            // ⚠️ THE HARDCODED BASE TRANSFORM IS THE CONSOLE'S, NOT AN INVENTION. All three
            // shop arms copy payload+0x40 into mBaseDriveThruTransform and then OVERWRITE all
            // four of its rows with a compile-time matrix (a Y-rotation plus a fixed world
            // position that sits within a few metres of one real shop bay: the body/paint pair
            // use (2915.99, 2.6198201, -1590.3199), the gas arm (2624.5, 2.99, -641.42999)).
            // It is a shipped dev leftover -- the copy it discards is dead in the console too.
            // Reproduced store for store, including the discarded copy, because "tidying" it
            // away would be a behaviour change and because nothing in this tree reads
            // mBaseDriveThruTransform yet, so nobody would notice if we got it wrong.
            case 97:   // E_ACTION_BODY_SHOP_DRIVE_THRU  @0x82237EA4
            case 98:   // E_ACTION_PAINT_SHOP_DRIVE_THRU @0x822380C8
            case 100:  // E_ACTION_GAS_STATION_DRIVE_THRU @0x82237FE8
            {
                // The type the ARBITRATOR STATE switches on. ⓘ Paint shop really does publish
                // BODY_SHOP (2): `li r4,2` in BOTH the 97 and the 98 arm, so the two shops share
                // one camera shot-group. Gas station is 3 (`li r4,3`).
                const bool lbGasStation = (liActionType == 100);
                maGameState.meDriveThruType =
                    lbGasStation ? GameState::E_DRIVETHRU_GAS_STATION
                                 : GameState::E_DRIVETHRU_BODY_SHOP;

                // The two 64-byte matrix copies out of the serialised payload (allowed raw
                // access: an external action blob, not a C++ object -- see the producer).
                std::memcpy(&maGameState.mBaseDriveThruTransform, lpacPayload + 0x40,
                            sizeof(Matrix44Affine));
                std::memcpy(&maGameState.mDriveThruTransform,     lpacPayload + 0x00,
                            sizeof(Matrix44Affine));

                // ...then the compile-time base transform that discards the copy above.
                Matrix44Affine& lrBase = maGameState.mBaseDriveThruTransform;
                if (lbGasStation)
                {
                    lrBase.Right() = Vector3{ -0.24346f,    0.0f,  -0.96991098f, 0.0f };
                    lrBase.Up()    = Vector3{  0.0f,        1.0f,   0.0f,        0.0f };
                    lrBase.At()    = Vector3{  0.96991098f, 0.0f,  -0.24346f,    0.0f };
                    lrBase.Pos()   = Vector3{  2624.5f,     2.99f, -641.42999f,  0.0f };
                }
                else
                {
                    lrBase.Right() = Vector3{ 0.61846203f, 0.0f,       -0.785815f,   0.0f };
                    lrBase.Up()    = Vector3{ 0.0f,        1.0f,        0.0f,        0.0f };
                    lrBase.At()    = Vector3{ 0.785815f,   0.0f,        0.61846203f, 0.0f };
                    lrBase.Pos()   = Vector3{ 2915.99f,    2.6198201f, -1590.3199f,  0.0f };
                }

                if (liActionType == 100)
                {
                    // `lbz r10, 0x80(r30); stbx r10, r31, r14`
                    maGameState.mbDriveThruActive = (lpacPayload[0x80] != 0);
                }
                else if (liActionType == 98)
                {
                    // `lbz r10, 0x88(r30); stbx r10, r31, r14`
                    maGameState.mbDriveThruActive = (lpacPayload[0x88] != 0);
                }
                else
                {
                    // Body shop only: the flag is the AND of the not-online byte and the
                    // "the repair did something" byte (`lbz 0x84` / `lbz 0x85`, both tested).
                    maGameState.mbDriveThruActive =
                        (lpacPayload[0x84] != 0) && (lpacPayload[0x85] != 0);

                    // ...and the repair clears the road-rage damage book-keeping:
                    //   *(+0x1C0) = *(+0x1C0) && !payload[0x85]
                    //   if (payload[0x85]) *(f32*)(+0x1BC) = 0.0
                    // (payload+0x85 is a hardcoded 1 at the producer, so on this build both
                    //  always take their repaired arm -- see PostShopAction.)
                    maGameState.mbRoadRageOneMoreCrashToWrecked =
                        maGameState.mbRoadRageOneMoreCrashToWrecked && (lpacPayload[0x85] == 0);
                    if (lpacPayload[0x85] != 0)
                        maGameState.mfHowCloseToTotalled = 0.0f;
                }

                // [DIAG] NOT IN THE X360 BINARY. The DIRECTOR rung of the drive-thru ladder,
                // on the same `[drivethru]` tag as the producer's GATE/ENTER/POST rungs and the
                // bridge's BRIDGE rung, so one grep reads the chain end to end. It prints the
                // transform POSITION because "the flag was raised" and "the camera was aimed at
                // a real bay" are different claims and a zero transform looks like neither
                // [[diagnostics-that-lie]]. Delete with the rest of the drive-thru bring-up.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    const Vector3& lrPos = maGameState.mDriveThruTransform.Pos();
                    *CgsDev::Log::gpDebugPrint
                        << "[drivethru] DIRECTOR action=" << liActionType
                        << " -> active=" << (maGameState.mbDriveThruActive ? 1 : 0)
                        << " type=" << static_cast<s32>(maGameState.meDriveThruType)
                        << " pos=(" << lrPos.x << "," << lrPos.y << "," << lrPos.z << ")\n";
                }
                break;
            }

            // ---- 102  E_ACTION_STOPPED_DRIVE_THRU (1 byte) -------------------------------
            // @0x822381A8, two stores and nothing else: `stbx r29, r31, r14` /
            // `stwx r29, r31, r20` with r29 == 0. This is the EXIT edge -- without it
            // mbDriveThruActive would latch on and ArbStateRoaming would re-enter
            // E_STATE_DRIVETHRU on every update after the first shop.
            // ---- 205  E_ACTION_ROAD_RAGE_PLAYER_DAMAGE (8 bytes) ------------------------
            // X360 ProcessInputQueue @0x822372F8 pseudocode 782..786: the three stores off the
            // record ({f32 how close to totalled @+0, one-more-crash @+4, totalled @+5}, the
            // same RoadRagePlayerDamageAction the GUI sees as event 348). This is the ONLY
            // writer of mfHowCloseToTotalled / mbRoadRageOneMoreCrashToWrecked outside the
            // resets, and ArbStateRoaming::ProcessPossibleFX scales the Damage_Crit hook's
            // blend by it -- the screen desaturating a step per crash in Road Rage / Marked
            // Man. Missing until 2026-09-17, so the blend stayed 0 and the hook never showed.
            // [FX-FLOW 2026-09-24] read through the record's DWARF home (BrnGameActions.h
            // RoadRagePlayerDamageAction: f32 +0x00, bool +0x04, bool +0x05, size-pinned there)
            // instead of a raw payload offset; same three reads, same widths.
            case 205:
            {
                const BrnGameState::GameStateModuleIO::RoadRagePlayerDamageAction& lrDamageAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRagePlayerDamageAction*>(lpacPayload);
                maGameState.mfHowCloseToTotalled            = lrDamageAction.mfHowCloseToTotalled;       // +0x1BC
                maGameState.mbRoadRageOneMoreCrashToWrecked = lrDamageAction.mbOneMoreCrashToTotalled;   // +0x1C0
                maGameState.mbRoadRageTotalled              = lrDamageAction.mbPlayerTotalled;           // +0x1C1
                break;
            }

            case 102:
            {
                maGameState.mbDriveThruActive = false;                       // +0x0D0 = 0
                maGameState.meDriveThruType   = GameState::E_DRIVETHRU_INVALID; // +0x0D4 = 0

                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "[drivethru] DIRECTOR action=102 -> active=0\n";
                }
                break;
            }

            // ---- 58  E_ACTION_ON_STUNT_ELEMENT_COMPLETE (24 bytes) -------------------
            // ⭐⭐ THE SMASH / BILLBOARD CAMERA POST-FX REQUEST. Producer:
            // StuntManager::ProcessStuntElement @0x8239CDB0, whose 24-byte
            // OnStuntElementCompleteAction is { mID @+0x00, meStuntElementType @+0x08,
            // miCurrentCount @+0x0C, miTotalCount @+0x10, meCurrentGameMode @+0x14 }.
            // This arm, transcribed from 0x82238234..0x82238274 -- it switches on the STUNT
            // ELEMENT TYPE word at +0x08 (0 JUMP / 1 SMASH / 2 BILLBOARD):
            //     type == 1 -> flags |= 0x10   (ArbStateRoaming: "Smash_Effect")
            //     type == 2 -> flags |= 0x04   (ArbStateRoaming: "Billboard_Effect")
            // JUMP (0) raises nothing here -- the jump's camera comes from case 56 above, which
            // fires at TAKE-OFF, not at the landing completion this action reports.
            case 58:
            {
                s32 liStuntElementType = 0;
                std::memcpy(&liStuntElementType, lpacPayload + 0x08, sizeof(s32));
                if (liStuntElementType == 1)
                {
                    maGameState.miThisFramesActionFlags |= 0x10;
                }
                else if (liStuntElementType == 2)
                {
                    maGameState.miThisFramesActionFlags |= 0x04;
                }
                break;
            }

            // ================= THE SHOWTIME ARMS (crash mode) ============================
            // ⭐⭐⭐ [FX-DIRECTOR 2026-09-24] Cases 42 / 43 / 140 / 144 / 145 / 146 were missing, and
            // with them every GameState field ArbStateCrashMode reads: its Update @0x82235488
            // requests the camera's sim time scale from mfImpactTimeSloMoFactor whenever no
            // close-up runs (`lfs f0, 0x108` -> `stfs f0, 0x114(camera)` @0x822356FC), so with no
            // case 42 that factor stayed 0 and MainDirector::Update floored the sim at
            // KF_MINIMUM_SIM_TIME_SCALE (0.005x) for the whole of Showtime -- the car barely moved
            // (FX-CAMRIG live log: `mfSimTimeScale=0.000000` -> `simScale=0.005000`). Its close-up
            // requests (+0x1EA / +0x1EB) and blur requests (+0x1E9 / +0x1EC) had no writer either.
            // All six are transcribed store for store from the ARTIST asm below; the GameState
            // fields are the DWARF names (BrnDirectorGameState.h:124/:125 and ShowTimeInfo :229..:237).

            // ---- 42  E_ACTION_IMPACT_TIME_START (8 bytes) @0x82237E54 -------------------
            //     stbx r23(=1), r31, 0x338E5   -> +0x105 mbImpactTimeActive = true
            //     lfs  f0, 0(r30) ; stfsx 0x338E8 -> +0x108 mfImpactTimeSloMoFactor = record +0x00
            // The only writer of +0x108 in the image (FX-CAMRIG's scan of every ARTIST export for
            // the director-relative 211176). Producer: UpdateRoadRulesManager, 1.0 (flt_82001C98).
            case 42:
            {
                const ImpactTimeStartActionRecord& lrImpactTimeStart =
                    *reinterpret_cast<const ImpactTimeStartActionRecord*>(lpacPayload);
                maGameState.mbImpactTimeActive      = true;                                        // +0x105
                maGameState.mfImpactTimeSloMoFactor = lrImpactTimeStart.mfTimestepMultiplier;      // +0x108

                // [diag] BRN_DIRECTOR_ACTION_DIAG -- NOT IN THE X360 BINARY.
                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 42 IMPACT_TIME_START -> mbImpactTimeActive "
                        << (maGameState.mbImpactTimeActive ? 1 : 0)
                        << " mfImpactTimeSloMoFactor " << maGameState.mfImpactTimeSloMoFactor << "\n";
                }
                break;
            }

            // ---- 43  E_ACTION_IMPACT_TIME_END (1 byte) @0x82237E74 ----------------------
            //     stbx r29(=0), r31, 0x338E5   -> +0x105 mbImpactTimeActive = false
            // The factor is NOT touched: it keeps the last start's value.
            case 43:
            {
                maGameState.mbImpactTimeActive = false;                                            // +0x105

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 43 IMPACT_TIME_END -> mbImpactTimeActive "
                        << (maGameState.mbImpactTimeActive ? 1 : 0)
                        << " mfImpactTimeSloMoFactor " << maGameState.mfImpactTimeSloMoFactor << "\n";
                }
                break;
            }

            // ---- 140  E_ACTION_VEHICLE_HIT (36 bytes) @0x822386CC -----------------------
            //     lwz r11, 0x1C(r30) ; cmpwi 0 ; bne default     -> a combo-bonus hit requests nothing
            //     lwz r11, 8(r30) ; (mulhw 0x66666667 ...) % 10 ; cntlzw/extrwi -> == 0
            //        stbx 0x339CA  -> +0x1EA mbCrushComboThisFrame = (total hit % 10 == 0)
            //     lwz r11, 0x14(r30) ; cmpwi 0 ; bgt -> 1 else 0
            //        stbx 0x339CB  -> +0x1EB mbEarntMultiplierThisFrame = (multiplier earned > 0)
            // (both signed, as the console's cmpwi / srawi+srwi-31 truncating division are).
            // ArbStateCrashMode starts its close-up on either bit and caches +0x1EA as the
            // super-slow-mo selector.
            case 140:
            {
                const BrnGameState::GameStateModuleIO::VehicleHitAction& lrVehicleHit =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::VehicleHitAction*>(lpacPayload);
                if (lrVehicleHit.miComboBonusEarned != 0)
                    break;

                maGameState.mShowTimeInfo.mbCrushComboThisFrame      =
                    (lrVehicleHit.miTotalVehiclesCrashed % 10) == 0;                               // +0x1EA
                maGameState.mShowTimeInfo.mbEarntMultiplierThisFrame =
                    lrVehicleHit.miScoreMultiplierEarned > 0;                                      // +0x1EB

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 140 VEHICLE_HIT total " << lrVehicleHit.miTotalVehiclesCrashed
                        << " multiplier " << lrVehicleHit.miScoreMultiplierEarned
                        << " -> mbCrushComboThisFrame "
                        << (maGameState.mShowTimeInfo.mbCrushComboThisFrame ? 1 : 0)
                        << " mbEarntMultiplierThisFrame "
                        << (maGameState.mShowTimeInfo.mbEarntMultiplierThisFrame ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 144  E_ACTION_JUST_BOUNCED (48 bytes) @0x8223864C ----------------------
            //     lwz r11, 0x14(r30) ; lwz r9, +0x1E0 ; cmpw r9, r11 ; blt -> 1 else 0
            //        stbx 0x339C8  -> +0x1E8 mbComboLevelIncreasedThisFrame = (old level < record's)
            //     lwz 0x14 -> stw +0x1E0 miComboLevel ; lwz 0x18 -> stwx 0x339C4 +0x1E4 miTotalVehiclesHit
            //     lbz 0x21 ; beq -> 0 ; lbz 0x23 ; bne -> 1 else 0
            //        stbx 0x339C9  -> +0x1E9 mbVehicleImpactThisFrame = (+0x21 && +0x23)
            // Record fields by the PC's JustBouncedAction names (BrnGameActions.h): +0x14
            // miCurrentComboCount, +0x18 miTotalVehiclesCrashed, +0x21 mbOnCar, +0x23 mu8EventByte7
            // (the DWARF's fourth bool, mbGoodImpact, :3442 -- the GameState header's owner names it).
            case 144:
            {
                const BrnGameState::GameStateModuleIO::JustBouncedAction& lrJustBounced =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::JustBouncedAction*>(lpacPayload);
                GameState::ShowTimeInfo& lrShowTime = maGameState.mShowTimeInfo;

                lrShowTime.mbComboLevelIncreasedThisFrame =
                    lrShowTime.miComboLevel < lrJustBounced.miCurrentComboCount;                   // +0x1E8
                lrShowTime.miComboLevel       = lrJustBounced.miCurrentComboCount;                 // +0x1E0
                lrShowTime.miTotalVehiclesHit = lrJustBounced.miTotalVehiclesCrashed;              // +0x1E4
                lrShowTime.mbVehicleImpactThisFrame =
                    lrJustBounced.mbOnCar && (lrJustBounced.mu8EventByte7 != 0);                   // +0x1E9

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 144 JUST_BOUNCED combo " << lrShowTime.miComboLevel
                        << " total " << lrShowTime.miTotalVehiclesHit
                        << " -> mbComboLevelIncreasedThisFrame "
                        << (lrShowTime.mbComboLevelIncreasedThisFrame ? 1 : 0)
                        << " mbVehicleImpactThisFrame " << (lrShowTime.mbVehicleImpactThisFrame ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 145  E_ACTION_JUST_APPLIED_EXTRA_SPIN (1 byte) @0x822386BC ------------
            //     stbx r23(=1), r31, 0x339CC   -> +0x1EC mbExtraSpinThisFrame = true
            // (DWARF 137 + the +8 band; producer ProcessGameEvents case 53, `li r5,0x91` @0x823A3E14.)
            case 145:
            {
                maGameState.mShowTimeInfo.mbExtraSpinThisFrame = true;                             // +0x1EC

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 145 JUST_APPLIED_EXTRA_SPIN -> mbExtraSpinThisFrame 1\n";
                }
                break;
            }

            // ---- 146  E_ACTION_SHOWTIME_INTRO_START (32 bytes) @0x82238608 --------------
            //     lbz r11, 0x10(r30) ; stbx 0x339CD -> +0x1ED mbInIntro = record's mbStart
            // Posted twice by DetectModeStarts: mbStart 1 arms the intro, mbStart 0 cancels it.
            case 146:
            {
                const BrnGameState::GameStateModuleIO::ShowtimeIntroAction& lrShowtimeIntro =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::ShowtimeIntroAction*>(lpacPayload);
                maGameState.mShowTimeInfo.mbInIntro = lrShowtimeIntro.mbStart;                     // +0x1ED

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 146 SHOWTIME_INTRO_START -> mbInIntro "
                        << (maGameState.mShowTimeInfo.mbInIntro ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 62  E_ACTION_NEW_CAR_UNLOCKED (16 bytes) ----------------------------
            case 62:
                maGameState.meJunkyardState          = GameState::E_JY_CAR_UNLOCK;  // = 3
                maGameState.mbIsRivalUnlock          = true;
                maGameState.mbNewCarUnlockedThisFrame = true;
                maGameState.miJunkyardPosIndex       = 0;
                // `ld r10, 0(r30); stdx r10, r31, 0x33968` -- the unlocked vehicle's CgsID.
                std::memcpy(&maGameState.mUnlockedVehicleType, lpacPayload + 0, sizeof(CgsID));
                break;

            // ---- 63  E_ACTION_CAR_UNLOCK_END (1 byte) --------------------------------
            case 63:
                if (maGameState.meJunkyardState == GameState::E_JY_CAR_UNLOCK)
                    maGameState.meJunkyardState = GameState::E_JY_CAR_SELECT;
                break;

            // ---- 64  E_ACTION_CAR_SELECTION_CHANGED (0x40 bytes) ---------------------
            case 64:
                if (maGameState.meJunkyardState == GameState::E_JY_INACTIVE)
                    maGameState.meJunkyardState = GameState::E_JY_CAR_SELECT;
                maGameState.mbNewCarUnlockedThisFrame = false;
                maGameState.miJunkyardPosIndex        = 0;
                maGameState.mbJunkyardPosJustChanged  = true;
                // ⭐ the ONLY writer of mJunkyardId in the image (see the GameState header).
                std::memcpy(&maGameState.mJunkyardId, lpacPayload + 0, sizeof(CgsID));
                maGameState.mbJunkyardPosIsLeft = (lpacPayload[0x30] != 0);
                break;

            // ---- 65  E_ACTION_CAR_SELECTION_CHANGED_DROPIN (16 bytes) ----------------
            case 65:
                maGameState.mbJunkyardPlayerRespawnedThisFrame = true;
                maGameState.mbJunkyardPosIsLeft = (lpacPayload[0x08] != 0);
                break;

            // ---- 73  E_ACTION_CAR_SELECT_TRANSITION_IN (2 bytes) ---------------------
            // ⭐⭐ THE ENTRY ACTION. payload[0] is "this is the transition IN"; payload[1] is
            // "there are cars to unlock". A brand-new profile has one unlocked car, so
            // CarSelectManager::StartTransitionInState @0x823929D0 posts {1,0} == INTRO_NO_CARS
            // and its EndTransitionInState @0x82392B30 posts {0,x} == CAR_SELECT.
            case 73:
                if (lpacPayload[0] != 0)
                {
                    maGameState.meJunkyardState = (lpacPayload[1] != 0)
                        ? GameState::E_JY_INTRO_UNLOCKING_CARS     // = 1
                        : GameState::E_JY_INTRO_NO_CARS;           // = 2
                }
                else
                {
                    maGameState.meJunkyardState = GameState::E_JY_CAR_SELECT;   // = 4
                }
                maGameState.mbJunkyardCarModActive = false;
                break;

            // ---- 75  E_ACTION_CAR_SELECT_READY (4 bytes) -----------------------------
            case 75:
            {
                s32 liReadyMode = 0;
                std::memcpy(&liReadyMode, lpacPayload + 0, sizeof(s32));
                if (liReadyMode == 2)
                {
                    maGameState.mbIsOnlineCarSelectActive          = true;
                    maGameState.mbHasOnlineCarSelectBeenAborted    = false;
                    maGameState.mbOnlineCarSelectCanStartRaceIntro = false;
                    maGameState.mbOnlineCarSelectMustClampToCar    = false;
                }
                break;
            }

            // ---- 76  E_ACTION_CAR_SELECT_MODIFICATION_SCREEN (8 bytes) ---------------
            case 76:
                maGameState.mbJunkyardCarModActive = (lpacPayload[0x04] != 0);
                break;

            // ---- 77  E_ACTION_CAR_SELECT_EXIT (0x20 bytes) ---------------------------
            case 77:
                CGS_ASSERT(lpAction != 0, "lpExitAction");   // BrnMainDirector.cpp:1410
                if (lpacPayload[0x10] != 0)
                {
                    maGameState.mbIsOnlineCarSelectActive = false;
                }
                else
                {
                    maGameState.meJunkyardState        = GameState::E_JY_INACTIVE;
                    maGameState.mbJunkyardCarModActive = false;
                }
                break;

            // ---- 85  (1 byte) --------------------------------------------------------
            case 85:
                maGameState.mbOnlineCarSelectCarIsShowable = (lpacPayload[0] != 0);
                break;

            // ================= THE EVENT-STATE JOURNAL ARMS ==============================
            // Every arm below pushes GameState::mEventState, GameState::meEventType, or both.
            // Together with HandlePrepareForModeAction they are the journal's ONLY producers;
            // with them gated, the journal never left the ACTIVE that GameState::Clear seeds,
            // so ArbStateRoaming's INTRO and POST_EVENT ladders could not fire and the race-
            // intro / post-event cameras were unreachable however well they worked.
            // The push itself is DataJournal<EEventState,2>::SetCurrent in every case (the
            // console open-codes `idx = (idx + 1) % 2; entries[idx] = v; if (size < 2) ++size`,
            // which is that member's body).

            // ---- 23  E_ACTION_PREPARE_FOR_MODE (0x8E0 bytes) -------------------------
            case 23:
                HandlePrepareForModeAction(
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(lpacPayload), lpIO);
                break;

            // ---- 24  E_ACTION_BROADCAST_MODE_FINISH_LINES (48 bytes) --------------------
            // ⭐ [FX-FLOW 2026-09-24, NEW-FINISHLINE] THE ONLY RETAIL WRITER of
            // GameState::mFinishLineID (+0x160) and mFinishLineNorthmostDir (+0x170).
            // ModeManager::PrepareForMode posts the event's finish landmark -- its BoxRegion and
            // its id -- once per prepare, right after the action-23 post (so this arm always runs
            // after case 23 above in the same drain). The post-event camera keys its shot group
            // off the id: ArbStatePostEvent::Prepare `ld r5,0x160` @0x8226E268 ->
            // DirectorResourceManager::GetEventCompletionShots, and PickAppropriateShot @0x8221A34C.
            // With this arm absent the id kept GameState::Clear's 0, so every offline race finish
            // fired "Unknown finish line" (BrnDirectorResourceManager.cpp:325 -- a dev assert,
            // which PAUSES the PC build) and fell back to the plain race group.
            // Console @0x82238738..0x82238764, store for store:
            //     ld    r11, 0x28(r30)                  ; record->mFinishLineID
            //     stdx  r11, r31, 0x33940               ; GameState +0x160 (GameState @ +0x337E0)
            //     bl    BoxRegion::ComputeDirection     ; sret r3 = stack, r4 = r30 = &record->mBoxRegion
            //     stvx  v0, r31, 0x33950                ; GameState +0x170
            // No gate, no assert. The only other store to +0x160 in the image is the
            // mbDebugTestFinishLines debug-render walk at this function's tail (0x82238EC0) --
            // a DirectorModule debug toggle, not a retail path.
            case 24:
            {
                const BrnGameState::GameStateModuleIO::BroadcastModeFinishLinesAction& lrFinishLinesAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::BroadcastModeFinishLinesAction*>(lpacPayload);

                maGameState.mFinishLineID           = lrFinishLinesAction.mFinishLineID;                // +0x160
                maGameState.mFinishLineNorthmostDir = lrFinishLinesAction.mBoxRegion.ComputeDirection(); // +0x170

                // [diag] BRN_FINISHLINE_DIAG -- NOT IN THE X360 BINARY. One line per broadcast: the
                // live witness that the arm is dispatched and what the post-event camera will key on.
                if (getenv("BRN_FINISHLINE_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[finish-line] action 24 -> GameState.mFinishLineID "
                        << static_cast<u64>(maGameState.mFinishLineID)
                        << " dir (" << maGameState.mFinishLineNorthmostDir.x
                        << ", " << maGameState.mFinishLineNorthmostDir.y
                        << ", " << maGameState.mFinishLineNorthmostDir.z << ")\n";
                }
                break;
            }

            // ---- 29  E_ACTION_START_MODE_INTRO (604 bytes) ---------------------------
            // ⭐ THE RACE-INTRO TRIGGER. mbDoIntro is the producer's own
            // `mfDurationSeconds > 0.0f`: an event WITH an intro goes to INTRO (which is what
            // ArbStateRoaming::ProcessActiveDrivingTransitions turns into ArbStateRaceIntro);
            // an event without one goes straight to ACTIVE.
            case 29:
            {
                const BrnGameState::GameStateModuleIO::StartModeIntroAction& lrIntroAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::StartModeIntroAction*>(lpacPayload);

                if (!lrIntroAction.mbDoIntro)
                {
                    maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_ACTIVE);
                    maGameState.mbGoToCrashModeAfterIntro = false;          // +0x11C
                    break;
                }

                maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_INTRO);

                maGameState.muNumberOfCarsInIntro = static_cast<u32>(lrIntroAction.mFlybyData.miNumberOfCars);
                maGameState.mfStateTimeLeft       = lrIntroAction.mfDurationSeconds;   // +0x13C
                CGS_ASSERT(maGameState.muNumberOfCarsInIntro <=
                               static_cast<u32>(BrnGameState::GameStateModuleIO::FlybyRivalData::KI_MAX_CARS_IN_FLYBY),
                           "mGameState.muNumberOfCarsInIntro <= (uint32_t)BrnGameState::GameStateModuleIO::FlybyRivalData::KI_MAX_CARS_IN_FLYBY");

                // The console walks the record through FlybyData's indexed accessor; that
                // accessor is non-const in this tree and the queue hands out a const record, so
                // the walk reads the (public) slot array directly. Same slots, same asserts.
                for (u32 luRivalIndex = 0; luRivalIndex < maGameState.muNumberOfCarsInIntro; ++luRivalIndex)
                {
                    const EActiveRaceCarIndex leRaceCarIndex =
                        lrIntroAction.mFlybyData.mRivalsToShow[luRivalIndex].meRaceCarIndex;
                    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                               "lpIntroAction->mFlybyData.GetCarFlybyData(luRivalIndex)->meRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "lpIntroAction->mFlybyData.GetCarFlybyData(luRivalIndex)->meRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                    maGameState.maeIntroCarID[luRivalIndex] = leRaceCarIndex;          // +0x110
                }
                break;
            }

            // ---- 30  E_ACTION_STOP_MODE_INTRO / 47  E_ACTION_SET_COUNTDOWN -----------
            // The same COUNTDOWN push, split by mode: the intro's END starts the countdown for
            // every mode EXCEPT the four below, and for those four the explicit countdown
            // action does it instead. FLAG: the membership is the console's literal comparison
            // set; why those four and not the whole online range is not recovered.
            case 30:
            case 47:
            {
                const s32 leEventType = maGameState.meEventType;
                const bool lbCountdownComesFromCountdownAction =
                    (leEventType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE)     ||
                    (leEventType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE) ||
                    (leEventType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN)||
                    (leEventType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END);

                if (lbCountdownComesFromCountdownAction != (liActionType == 47))
                    break;

                maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_COUNTDOWN);

                // ⚠️ GATE: CalcTrafficLightSpace( lpIO ) -- declaration-only, and the one thing
                //    in this TU that is still a multi-stage VMX pipeline (see its declaration).
                //    The push above is what the roaming ladder reads; the traffic-light space it
                //    would compute is only consumed by the start-line camera's framing.
                break;
            }

            // ---- 12 / 33 / 34  the ACTIVE pushes ------------------------------------
            // 33 is E_ACTION_STOP_MODE_COUNTDOWN and 34 E_ACTION_START_PLAYING_MODE: the event
            // goes live. 12 is the online car-select ABORT (it clears the car-select latch and
            // raises the aborted one before making the same push). FLAG: 12 has no recovered
            // enumerator name.
            case 12:
                maGameState.mbIsOnlineCarSelectActive       = false;        // +0x1A5
                maGameState.mbHasOnlineCarSelectBeenAborted = true;         // +0x1A6
                maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_ACTIVE);
                break;

            case 33:
            case 34:
                maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_ACTIVE);
                break;

            // ---- 37  E_ACTION_SHOW_MODE_RESULTS (232 bytes) --------------------------
            // ⭐ THE POST-EVENT TRIGGER, and the only writer of mbWonLastEvent -- which is what
            // ArbStatePostEvent::PickAppropriateShot picks its take from, so this arm feeds both
            // halves of the post-event camera.
            case 37:
            {
                const BrnGameState::GameStateModuleIO::ShowModeResultsAction& lrResults =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::ShowModeResultsAction*>(lpacPayload);

                maGameState.mbWonLastEvent =
                    (lrResults.mu8FieldE0 != 0) &&
                    (lrResults.mu8FieldE1 == 0) &&
                    (lrResults.mu8FieldE3 == 0) &&
                    ((lrResults.mu8FieldE2 == 0) ||
                     (lrResults.meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE));

                if (lrResults.mu8FieldE5 != 0)
                    maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_POST_EVENT);
                break;
            }

            // ---- 39  E_ACTION_STOP_MODE (24 bytes) ----------------------------------
            // The event teardown: back to ACTIVE (i.e. "no event state"), meEventType cleared to
            // E_MODE_NONE, and the per-event presentation block reset.
            case 39:
            {
                CGS_ASSERT(lpAction != 0, "lpStopModeAction");
                const BrnGameState::GameStateModuleIO::StopModeAction& lrStopAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::StopModeAction*>(lpacPayload);

                // Skipped while the online post-event owns the camera, so stopping the mode
                // behind the results screen does not cut the post-event take short.
                if (!maStateFlagTail[E_FLAG_TAIL_IN_ONLINE_POST_EVENT])
                    maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_ACTIVE);

                maGameState.mbGoToCrashModeAfterIntro = false;              // +0x11C
                // Slow motion comes back offline, and online only once the last round is done.
                maGameState.mbCanUseSlomo = (lrStopAction.mu8Field10 == 0) ||
                                            (lrStopAction.mu8Field12 != 0);  // +0x100
                maGameState.meEventType   = BrnGameState::GameStateModuleIO::E_MODE_NONE;

                maGameState.mfHowCloseToTotalled            = 0.0f;          // +0x1BC
                maGameState.meTargetRaceCarIndex            = E_ACTIVE_RACE_CAR_INDEX_0;   // +0x140
                maGameState.meTargetVehicleRefType          = VehicleRef::E_PLAYER_CAR;    // +0x144
                maGameState.mbRoadRageOneMoreCrashToWrecked = false;         // +0x1C0
                maGameState.mbRoadRageTotalled              = false;         // +0x1C1
                ClearEventPresentationBlock(maGameState);

                maGameState.mbStartingFreeburnDueToPlayerJoinThisFrame =
                    (lrStopAction.mu8Field15 != 0);                           // +0x1C8
                break;
            }

            // ================= THE REST OF THE CONSOLE'S ARMS (crash-parity audit) =========
            // [FX-DIRECTOR 2026-09-24] Every arm below was on the gated list with the blanket
            // reason "writes into a part of the GameState that is still opaque". MEASURED AND
            // STALE: every field they write is a named DWARF member of BrnDirector::GameState.
            // Transcribed store for store from the ARTIST asm (GameState = MainDirector + 0x337E0;
            // r23 = 1 and r29 = 0 for the whole function, 0x82237500 / 0x82237330).

            // ---- 0  E_ACTION_RESET_PLAYER_CAR (80 bytes) @0x82238478 -------------------
            //     lbz r11, 0x42(r30) ; stbx r11, 0x33931   -> +0x151 mbShouldResetPlayerCameraThisFrame
            // Record +0x42 is DWARF ResetPlayerCarAction::mbResetPlayerCamera (BrnGameActions.h:669;
            // this tree names the byte muReserved0x42 -- HandleChangePlayerCarEvent posts it from
            // ChangePlayerCarEvent::mbResetPlayerCamera). Per-frame (ResetPerFrameData, 0x82237360).
            // Consumer: Arbitrator::Update @0x8226ADE0 -> SharedCameraContainer::mbUseGameplayExternal.
            // Read by offset: the record is alignas(16) and the queue does not align payloads.
            case 0:
            {
                static_assert(offsetof(BrnGameState::GameStateModuleIO::ResetPlayerCarAction, muReserved0x42) == 0x42,
                              "the console reads the reset-camera byte at record +0x42");
                maGameState.mbShouldResetPlayerCameraThisFrame =
                    (lpacPayload[offsetof(BrnGameState::GameStateModuleIO::ResetPlayerCarAction, muReserved0x42)] != 0);

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 0 RESET_PLAYER_CAR -> mbShouldResetPlayerCameraThisFrame "
                        << (maGameState.mbShouldResetPlayerCameraThisFrame ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 53  E_ACTION_PLAYER_HIT_RIVAL / 54  E_ACTION_RIVAL_HIT_PLAYER (12 bytes) ----
            //     53 @0x822384E8: stbx 1 -> 0x33991 (+0x1B1) ; stbx 1 -> 0x33992 (+0x1B2)
            //     54 @0x82238504: stbx 1 -> 0x33991 (+0x1B1) ; stbx 0 -> 0x33992 (+0x1B2)
            // No payload read. Both per-frame. Consumer: ArbStateRoaming::ProcessPossibleFX.
            case 53:
            {
                maGameState.mbPlayerAndRivalImpactOccured = true;                                  // +0x1B1
                maGameState.mbPlayerWonImpactAgainstRival = true;                                  // +0x1B2

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 53 PLAYER_HIT_RIVAL -> impact 1 won 1\n";
                }
                break;
            }
            case 54:
            {
                maGameState.mbPlayerAndRivalImpactOccured = true;                                  // +0x1B1
                maGameState.mbPlayerWonImpactAgainstRival = false;                                 // +0x1B2

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 54 RIVAL_HIT_PLAYER -> impact 1 won 0\n";
                }
                break;
            }

            // ---- 107  E_ACTION_ON_TRAFFIC_CHECKING (2 bytes) @0x82238520 ----------------
            //     stbx 1 -> 0x33993 (+0x1B3 mbPlayerCheckedTraffic). No payload read; per-frame.
            case 107:
            {
                maGameState.mbPlayerCheckedTraffic = true;                                         // +0x1B3

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 107 ON_TRAFFIC_CHECKING -> mbPlayerCheckedTraffic 1\n";
                }
                break;
            }

            // ---- 120  E_ACTION_SHUTDOWN (24 bytes) @0x82237E44 -------------------------
            //     stbx 1 -> 0x338BC (+0xDC mbIsShutdown). No payload read. NOT per-frame: the
            //     inactive arm of case 6 (the takedown camera ending) is what clears it.
            // Consumer: ArbStateTakedown (the shutdown camera / its shot selection).
            case 120:
            {
                maGameState.mbIsShutdown = true;                                                   // +0xDC

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 120 SHUTDOWN -> mbIsShutdown 1\n";
                }
                break;
            }

            // ---- 132  E_ACTION_HUD_MESSAGE_STUNT_PERFORMED (DWARF 242; 24 bytes) @0x82238530 --
            //     lwz r11, 0xC(r30) ; cmpwi 0 ; bgt -> 1 else 0 ; stbx 0x33995 (+0x1B5)
            // The record is DWARF HUDMessageStuntPerformed { StuntInfo mStuntInfo; } (h:4528);
            // +0xC is StuntInfo::miStuntMultiplier (StuntModeScoring stores it there, `stw r3,
            // 0xC(r31)`). Producer: HUDMessageLogic::GenerateStuntMessage (`li r5, 0x84`). Per-frame.
            case 132:
            {
                static_assert(offsetof(BrnGameState::StuntInfo, miStuntMultiplier) == 0x0C,
                              "the console reads the stunt multiplier at record +0x0C");
                const BrnGameState::StuntInfo& lrStunt =
                    *reinterpret_cast<const BrnGameState::StuntInfo*>(lpacPayload);
                maGameState.mbPlayerPerformedStunt = lrStunt.miStuntMultiplier > 0;               // +0x1B5

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 132 HUD_MESSAGE_STUNT_PERFORMED multiplier " << lrStunt.miStuntMultiplier
                        << " -> mbPlayerPerformedStunt " << (maGameState.mbPlayerPerformedStunt ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 150  E_ACTION_GAME_TRAINING_PAUSE / 151  ..._UNPAUSE (DWARF 142/143) ----
            //     150 @0x82237E84: stbx 1 -> 0x339A2 (+0x1C2 mbTrainingPause)
            //     151 @0x82237E94: stbx 0 -> 0x339A2
            // Producer: TrainingManager (0x96 / 0x97). Not per-frame.
            case 150:
            {
                maGameState.mbTrainingPause = true;                                                // +0x1C2

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 150 GAME_TRAINING_PAUSE -> mbTrainingPause 1\n";
                }
                break;
            }
            case 151:
            {
                maGameState.mbTrainingPause = false;                                               // +0x1C2

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 151 GAME_TRAINING_UNPAUSE -> mbTrainingPause 0\n";
                }
                break;
            }

            // ---- 215  E_ACTION_PAYBACK_ACTIVATED (12 bytes) @0x82238488 ----------------
            //     the console asserts the record ("lpAction", BrnMainDirector.cpp:1437, r5 0x59D)
            //     stbx 1 -> 0x338D0 (+0xF0 mbPaybackActive) ; lwz 0(r30) -> 0x338D4 (+0xF4)
            case 215:
            {
                CGS_ASSERT(lpAction != 0, "lpAction");
                const BrnGameState::GameStateModuleIO::PaybackActivatedAction& lrPayback =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::PaybackActivatedAction*>(lpacPayload);
                maGameState.mbPaybackActive     = true;                                            // +0xF0
                maGameState.meActivePaybackType = lrPayback.mePaybackType;                         // +0xF4

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 215 PAYBACK_ACTIVATED -> mbPaybackActive 1 meActivePaybackType "
                        << static_cast<s32>(maGameState.meActivePaybackType) << "\n";
                }
                break;
            }

            // ---- 216  E_ACTION_PAYBACK_OVER (1 byte) @0x822384C8 -----------------------
            //     stbx 0 -> 0x338D0 (+0xF0) ; li r11, 3 ; stwx -> 0x338D4 (+0xF4)
            // 3 is the "no payback" sentinel GameState::Clear also stores (E_PAYBACK_TYPE_SIX_AXIS_
            // STEERING, the one type ArbStateRoaming asserts it never plays).
            case 216:
            {
                maGameState.mbPaybackActive     = false;                                           // +0xF0
                maGameState.meActivePaybackType = BrnNetwork::E_PAYBACK_TYPE_SIX_AXIS_STEERING;    // +0xF4 = 3

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 216 PAYBACK_OVER -> mbPaybackActive 0 meActivePaybackType 3\n";
                }
                break;
            }

            // ---- 218  E_ACTION_SOUND_TRIGGER (32 bytes) @0x82238550 --------------------
            //     lwz r28, 0x10(r30)                     SoundTriggerAction::mEntityId
            //     AllVehicleData::GetPlayer(this + 0x12C80) ; lwz 0x3C8 ; cmplw ; bne -> skip
            //                                            the player's RaceCarState::mEntityId
            //     lwz 0x14(r30) ; cmpwi 1 ; bne -> skip  meResultType == E_TYPE_AT_ENTITY
            //     lwz 0x18(r30) ; six single-bit tests (bits 0..5, 0x8223857C..0x822385C0)
            //     any set -> stbx 1 -> 0x338D8 (+0xF8 mbPlayerInTunnel)
            // Per-frame. Producer: TriggerQueryManager::PreWorldUpdate's maSoundActions drain
            // (0x8239F848, `li r5, 0xDA`).
            case 218:
            {
                static_assert(offsetof(BrnGameState::GameStateModuleIO::SoundTriggerAction, mEntityId) == 0x10 &&
                              offsetof(BrnGameState::GameStateModuleIO::SoundTriggerAction, meResultType) == 0x14 &&
                              offsetof(BrnGameState::GameStateModuleIO::SoundTriggerAction, muActiveTriggers) == 0x18,
                              "the console reads the sound trigger's entity / type / bits at +0x10 / +0x14 / +0x18");
                const BrnGameState::GameStateModuleIO::SoundTriggerAction& lrTrigger =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::SoundTriggerAction*>(lpacPayload);
                if (lrTrigger.mEntityId.muValue == mAllVehicleData.GetPlayer().mRaceCarState.mEntityId.muValue &&
                    lrTrigger.meResultType == BrnGameState::GameStateModuleIO::SoundTriggerAction::E_TYPE_AT_ENTITY)
                {
                    const u32 luActiveTriggers = lrTrigger.muActiveTriggers;
                    if ((luActiveTriggers & 0x01) != 0 || (luActiveTriggers & 0x02) != 0 ||
                        (luActiveTriggers & 0x04) != 0 || (luActiveTriggers & 0x08) != 0 ||
                        (luActiveTriggers & 0x10) != 0 || (luActiveTriggers & 0x20) != 0)
                    {
                        maGameState.mbPlayerInTunnel = true;                                       // +0xF8
                    }
                }

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 218 SOUND_TRIGGER entity " << lrTrigger.mEntityId.muValue
                        << " type " << static_cast<s32>(lrTrigger.meResultType)
                        << " triggers " << lrTrigger.muActiveTriggers
                        << " -> mbPlayerInTunnel " << (maGameState.mbPlayerInTunnel ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 224  E_ACTION_CAR_ADDITION_PRESENTATION_END (DWARF 216) @0x822382C0 ----
            //     stbx 0 -> 0x33932 (+0x152 mbNewCarAdded) ; stfsx f31 -> 0x33934 (+0x154)
            // f31 is flt_82001CC0 == 0.0 (0x82237340, the same register ResetPerFrameData stores).
            // meAddedCarID (+0x158) is not touched.
            case 224:
            {
                maGameState.mbNewCarAdded                       = false;                          // +0x152
                maGameState.mfCarAddedPresentationTimeRemaining = 0.0f;                           // +0x154

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint << "[director-action] 224 CAR_ADDITION_PRESENTATION_END -> mbNewCarAdded 0\n";
                }
                break;
            }

            // ---- 113  E_ACTION_RACE_CAR_REACHED_CHECKPOINT (16 bytes) @0x822385D4 --------
            //     ⭐ UN-GATED 2026-09-24 (crash parity FX-DIRECTOR) with the input's global race-car
            //     table (BridgeWorldToDirector step 8 publishes it now).
            //     lwz r4, 4(r30)                          meGlobalRaceCarIndex
            //     addi r3, <input>, 0x10 ; bl 0x821F46C8  GlobalRaceCarInterface::GetActiveRaceCarIndex
            //     bl InputBuffer::GetPlayerCarIndex (0x82206E48) ; cmpw ; bne -> skip
            //     stbx r23(=1) -> 0x33994 (+0x1B4 mbPlayerHitCheckpointThisFrame)
            // Every car's checkpoint posts one (ModeManager::TransmitAndIncrementCheckPointsReached);
            // only the player's raises the flag. Its reader is ArbStateRoaming::ProcessPossibleFX's
            // "Checkpoint" hook, in event types 10 / 11 / 13 / 15 / 16. Per-frame (ResetPerFrameData).
            case 113:
            {
                static_assert(offsetof(BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction,
                                       meGlobalRaceCarIndex) == 0x04,
                              "the console reads the checkpoint action's global race-car index at +0x04");
                const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction& lrCheckpoint =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction*>(lpacPayload);
                // The console converts first, then fetches the player's index (0x822385E0, 0x822385EC).
                const EActiveRaceCarIndex leCheckpointCar =
                    lpInput->GetGlobalRaceCarInterface()->GetActiveRaceCarIndex(lrCheckpoint.meGlobalRaceCarIndex);
                const EActiveRaceCarIndex lePlayerCar = lpInput->GetPlayerCarIndex();
                if (leCheckpointCar == lePlayerCar)
                {
                    maGameState.mbPlayerHitCheckpointThisFrame = true;                            // +0x1B4
                }

                // [diag] BRN_DIRECTOR_ACTION_DIAG -- NOT IN THE X360 BINARY. Its own budget (at most
                // 60 lines): every car's checkpoint posts one, and a long race would otherwise spend
                // the shared 400 before the player's.
                {
                    static const bool sbCheckpointDiag      = (getenv("BRN_DIRECTOR_ACTION_DIAG") != 0);
                    static s32        siCheckpointLinesLeft = 60;
                    if (sbCheckpointDiag && siCheckpointLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                    {
                        --siCheckpointLinesLeft;
                        *CgsDev::Log::gpDebugPrint
                            << "[director-action] 113 RACE_CAR_REACHED_CHECKPOINT global "
                            << static_cast<s32>(lrCheckpoint.meGlobalRaceCarIndex) << " -> active "
                            << static_cast<s32>(leCheckpointCar) << " (player " << static_cast<s32>(lePlayerCar)
                            << ") -> mbPlayerHitCheckpointThisFrame "
                            << (maGameState.mbPlayerHitCheckpointThisFrame ? 1 : 0) << "\n";
                    }
                }
                break;
            }

            // ---- 223  E_ACTION_CAR_ADDITION_PRESENTATION_START (DWARF 215) @0x82238278 --
            //     ⭐ UN-GATED 2026-09-24 (crash parity FX-DIRECTOR), as 113.
            //     lbz 0x18(r30) ; beq -> skip             mbDoCamera
            //     stbx r23(=1) -> 0x33932 (+0x152 mbNewCarAdded)
            //     lfs 0x14(r30) -> stfsx 0x33934          (+0x154 mfCarAddedPresentationTimeRemaining)
            //     lwz r4, 0x10(r30) ; GetActiveRaceCarIndex(input + 0x10) -> stwx 0x33938 (+0x158 meAddedCarID)
            // The online new-car presentation; MomentSharedInfo reads +0x152 / +0x158.
            case 223:
            {
                const CarAdditionPresentationStartActionRecord& lrAddition =
                    *reinterpret_cast<const CarAdditionPresentationStartActionRecord*>(lpacPayload);
                if (lrAddition.mbDoCamera)
                {
                    maGameState.mbNewCarAdded                       = true;                           // +0x152
                    maGameState.mfCarAddedPresentationTimeRemaining = lrAddition.mfPresentationDuration; // +0x154
                    maGameState.meAddedCarID =
                        lpInput->GetGlobalRaceCarInterface()->GetActiveRaceCarIndex(lrAddition.meAddedCarGlobalIndex); // +0x158
                }

                if (BrnDiag_DirectorActionDiagOn())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[director-action] 223 CAR_ADDITION_PRESENTATION_START camera "
                        << (lrAddition.mbDoCamera ? 1 : 0) << " -> mbNewCarAdded "
                        << (maGameState.mbNewCarAdded ? 1 : 0) << " meAddedCarID "
                        << static_cast<s32>(maGameState.meAddedCarID) << "\n";
                }
                break;
            }

            default:
                // The console's own default arm, plus the GATED cases listed in the banner.
                break;
            }

            liActionType = lpQueue->GetNextEvent(lpAction, &lpAction, &liActionSize);
        }

        // 0x822387FC..0x82238868 -- THE THREE waiting -> receivedThisFrame HANDSHAKES.
        // PostGuiUpdate raises the *waiting* bit on the frame the GUI event lands; the NEXT
        // ProcessInputQueue clears it and raises the *receivedThisFrame* bit, which
        // ResetPerFrameData drops again one frame later. That one-frame delivery contract is
        // the shape consumers depend on, so it is reproduced exactly.
        if (maGameState.mbJunkyardSelectionChangedMessageWaiting)
        {
            maGameState.mbJunkyardSelectionChangedMessageWaiting = false;
            maGameState.mbJunkyardSelectionChangedMessageReceivedThisFrame = true;
        }
        if (maGameState.mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting)
        {
            maGameState.mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting = false;
            maGameState.mbJunkyardCarUnlockTickedClosedThisFrame = true;
        }
        if (maGameState.mbRankUpMessageWaiting)
        {
            maGameState.mbRankUpMessageWaiting           = false;
            maGameState.mbRankUpMessageReceivedThisFrame = true;
        }

        // 0x8223886C..0x82238888 -- `lbzx +0x35431 ; beq ; stbx r29(=0), 0x338E0`: while the
        // director's mbForceSloMoNotAllowed latch is up, slow motion is withdrawn every drain.
        if (maStateFlagTail[E_FLAG_TAIL_FORCE_SLOMO_NOT_ALLOWED])
        {
            maGameState.mbCanUseSlomo = false;                              // +0x100
        }

        // ⚠️ GATE: the debug-render tail (0x82238A40.., gated on the flag-tail bytes +0x3543E /
        //   +0x3543F and a DebugInterface draw of the event journal) -- development-only drawing.
        //
        // ⭐⭐⭐ THE CRASH-ACTIVE LEG IS LANDED (2026-08-29, crash-camera wave). It is the ONLY
        // writer of GameState::mbCrashActive in the whole image -- an image-wide scan of every
        // ARTIST export for the director-relative offset 211161 (== maGameState +0xF9) returns
        // exactly this function -- and mbCrashActive is the gate on ArbStateRoaming::
        // ProcessPossibleStateChanges' crash edge. While this leg was absent the arbitrator
        // could NEVER attempt E_STATE_CRASHING, no matter what the world did, so the entire
        // crash camera was unreachable out of a build that was green and asserted nothing.
        // (CONTROL for "no writer": the same scan over the tree found 13 assignments to the
        //  neighbouring meEventType and 1 to mbPlayerWasTakenDown, so it does find writers when
        //  they exist. mbCrashActive returned zero.)
        //
        // ⛔⛔ IT SHIPS WITH BrnArbStateCrashing.cpp, IN ONE COMMIT, AND MUST STAY THAT WAY.
        // Until that file existed, E_STATE_CRASHING's container slot was
        // `class ArbStateCrashing : public ArbitratorState {};` -- an empty shell whose
        // inherited Update never writes meState and which has no exit edge. Landing this leg
        // alone would have handed the first crash of the session to that shell and frozen the
        // camera for the rest of the run, out of a green build. The previous wave reasoned that
        // from the shape and deliberately did NOT test it; this commit removes the shell rather
        // than testing it.
        //
        // The block, off the asm (pseudocode 854-873 @0x822372F8). The published record's +1098
        // is Camera::VehicleInfo::mRaceCarState.mbCrashing, pinned twice independently:
        // BrnVehicleEvents.h:88 maps serialised @1098 back to physics +0x710, and
        // ArbStateCrashing::CanRun @0x821F6258 is nothing but
        // `lwz r11, 0x48(info) / lbz r3, 0x44A(r11)` on that same byte off mpPlayerCar. The
        // 1264 stride Hex-Rays prints is 0x4F0 == sizeof the published VehicleInfo, i.e. the
        // ordinary indexed read this function already makes -- no offset arithmetic needed.
        {
            const BrnDirector::Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();
            const s32 liPlayerCarIndex = static_cast<s32>(lpInput->GetPlayerCarIndex());

            const bool lbPlayerCarCrashing =
                (lpRaceCars != 0 && liPlayerCarIndex >= 0)
                    ? lpRaceCars[liPlayerCarIndex].mRaceCarState.mbCrashing
                    : false;

            // The crash just ENDED: drop the takedown latch so the next one starts clean.
            if (maGameState.mbCrashActive && !lbPlayerCarCrashing)
            {
                maGameState.mbPlayerWasTakenDown = false;              // +0x1C3
            }

            // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. Edge-triggered: one line
            // when the crash window opens and one when it closes. This is the FIRST link in the
            // crash-camera chain, and without it "the camera never slowed down" cannot be told
            // apart from "the crash was never published to the director at all".
            if (maGameState.mbCrashActive != lbPlayerCarCrashing &&
                getenv("BRN_CRASHCAM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[crashcam] mbCrashActive -> " << (lbPlayerCarCrashing ? 1 : 0)
                    << " (playerIdx=" << liPlayerCarIndex
                    << " raceCars=" << (lpRaceCars != 0 ? 1 : 0) << ")\n";
            }

            maGameState.mbCrashActive = lbPlayerCarCrashing;           // +0x0F9

            if (maGameState.mbCrashActive)
            {
                // Count the crash window down by this frame's SIM timestep. The console spells
                // it `-((timer[+32] * timer[+28]) - mfCrashTimeRemaining)`, which is this
                // subtraction; timer +28/+32 are the SIM TimerStatus' base step and multiplier,
                // i.e. GetSimTimerStatus()->GetCurrentTimeStep() -- the same accessor
                // UpdateArbitrator already uses for lfSimTimestep, reached by name.
                maGameState.mfCrashTimeRemaining -=
                    lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep();
            }
            else
            {
                // Not crashing: reload the window (flt: 6.0). ⭐ This value is what
                // ArbStateCrashing::Update turns into SelectNormalCrashCamera's
                // lbTooLateToSwitchCameras (`<= 1.0f`), so the countdown is load-bearing, not
                // cosmetic -- it is what stops the director cutting to a fresh camera in the
                // last second of a crash.
                maGameState.mfCrashTimeRemaining = KF_CRASH_TIME_WINDOW;
            }
        }

        // 0x8223893C..0x82238A3C -- THE TWO INACTIVITY CLOCKS and the been-active latch
        // (GameState +0x148 / +0x14C / +0x150). Each clock runs on the SIM timestep
        // (`lfs 0x20 ; lfs 0x1C ; fmadds` off GetTimerStatusInterface() -- the same sim step the
        // crash window above counts down by) and is zeroed (f31 == flt_82001CC0 == 0.0) by any
        // pad input or a paused sim; the player clock is also zeroed by the player's engine
        // (VehicleInfo +0x4E6 mbEngineOn), which latches mbPlayerBeenActive. Consumers:
        // ArbStateRoaming's picture-paradise idle entry and ArbStateCarSelect's orbit.
        {
            const DirectorIO::ControlInput* lpControl = lpInput->GetControll();
            const bool lbAnyInput  = lpControl->mbAnyInput;                  // lbz 0(GetControll())
            const bool lbSimPaused = lpInput->IsSimPaused();                 // lbz 0x7AC8(input)
            const f32  lfSimTimestep =
                lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep();

            // The player's VehicleInfo, read the way the crash block above reads it (the console
            // indexes the published array without a guard; the host guard is the same one).
            const BrnDirector::Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();
            const s32 liPlayerCarIndex = static_cast<s32>(lpInput->GetPlayerCarIndex());
            const bool lbEngineOn =
                (lpRaceCars != 0 && liPlayerCarIndex >= 0)
                    ? lpRaceCars[liPlayerCarIndex].mbEngineOn
                    : false;

            if (lbAnyInput || lbSimPaused)
                maGameState.mfPadInactiveTime = 0.0f;                         // +0x148
            else
                maGameState.mfPadInactiveTime += lfSimTimestep;

            if (lbEngineOn || lbAnyInput || lbSimPaused)
                maGameState.mfPlayerInactiveTime = 0.0f;                      // +0x14C
            else
                maGameState.mfPlayerInactiveTime += lfSimTimestep;

            if (lbEngineOn)
                maGameState.mbPlayerBeenActive = true;                        // +0x150
        }
    }

    // ------------------------------------------------------------------------
    // UpdateCameraBehavioursPostScene  @ 0x8224FD30
    //
    // ⭐ THE PASS THAT RUNS EVERY LIVE CAMERA BEHAVIOUR. It builds the ~1540-byte
    // Camera::BehaviourSharedInfo on its own stack and ends in
    //     BehaviourManager::PostCollisionUpdateAllBehaviours(&mBehaviourManager, <paused>,
    //                                                        lSharedInfo, <controller>, 1,
    //                                                        <debug printer>)
    // (its sibling entry point UpdateCameraBehavioursPreScene @0x82255318 ends in
    // UpdateAllBehaviours over the same block). Without this pass nothing ever dispatches a
    // Behaviour::Update, so every behaviour-produced camera stays at whatever
    // BehaviourHelper::Prepare constructed -- which is exactly why the fly-by camera could not
    // move no matter what the arbitrator did.
    //
    // The X360 prologue, in order (all of it reproduced below except where FLAGged):
    //   assert(input->GetTimerStatusInterface()->GetGameTimerStatus()->IsRunning())  // .cpp:1935
    //   lfGame    = gameTimer[+8] * gameTimer[+4];
    //   if (simTimer.mbRunning /*[+36]*/) { lfWorld = simTimer[+32] * simTimer[+28];
    //                                       lfWorldNoSlomo = simTimer[+28]; }
    //   else                              { lfWorld = 0; lfWorldNoSlomo = 0; }
    //   if (<ICE owns the frame>) lfGame = lfWorld = lfWorldNoSlomo = 0;
    //   <broadcast all three into the Timestep's VecFloat lanes>
    //   mUsedRaceCars = *input->GetUsedRaceCars(); mpRaceCars = input->GetRaceCarInfo();
    //   mePlayerCarIndex = liPlayerCarIndex; mpAllVehicleData = &mAllVehicleData; ...
    //   BehaviourManager::ProcessSceneQueryResults(...);
    //   <the ICE::CameraSpaceHandler build from the player + nearest race car>
    //   Camera2DRotationController::Update / CameraSphericalRotationController::Update
    //   <the ~90-line VMX copy of the two vehicle transforms into the shared info>
    //
    // ⭐⭐ THE SIX-SLOT BLACK HOLE IS CLOSED (2026-08-01, ICE-anim transform wave).
    // The banner that used to sit here said mPlayerInfo / mpAllVehicleData / mpPlayerTracker /
    // mpEffectInterface / mpDebugLog / mpDebugPrinter / mpCameraSpaceHandler were "left null
    // (the fly-by path reads none of them)". THAT JUSTIFICATION HAD ALREADY EXPIRED: three of
    // them are on BehaviourIceAnim::Update's straight-line path --
    //     mpAllVehicleData     -> every VehicleRef::IsValid / ::Get resolves against it
    //     mpCameraSpaceHandler -> Update COPY-CONSTRUCTS its own handler off it, frame 1
    //     mpDebugPrinter       -> the "Can/Can't see player" readout at the end of Update
    // -- and each was a null dereference waiting for the frame the ICE behaviour actually ran.
    // Nothing could observe that while BehaviourIceAnim::Construct's missing VehicleRef seeds
    // made Update fail out on its first line (see that file). Same lesson as the junkyard
    // wave's mpNamedParameters: a "not on the live path" note expires SILENTLY.
    //
    // Every one of them now points at storage this class already owns and already seeds; none
    // needed a new type. mPlayerInfo is a real VehicleInfo by value (Behaviour.h's ODR blocker
    // was retired when the tree collapsed to one SuspensionSpring), so it is filled with the
    // console's own `VehicleInfo::operator=` copy of the player's race-car record.
    //
    // The behaviour manager publishes speed/boost and controller state in each pass.
    // Crash-time staging and the debug-printer interior retain their existing gaps.
    // ------------------------------------------------------------------------
    // ⭐⭐ SPLIT 2026-08-01 (car-select hand-off wave). This staging used to sit inside
    // UpdateCameraBehavioursPostScene; it is now shared, because the console has TWO entry
    // points over it and this build only ever ran one of them. See the banner on
    // UpdateCameraBehavioursPreScene below.
    void MainDirector::BuildBehaviourSharedInfo(const DirectorInputOutput* lpIO,
                                                s32 liPlayerCarIndex,
                                                Camera::BehaviourSharedInfo& lSharedInfo,
                                                ICE::CameraSpaceHandler& lCameraSpaces)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        const CgsSystem::TimerStatusInterface* lpTimerStatus = lpInput->GetTimerStatusInterface();

        const CgsSystem::TimerStatus* lpGameTimer = lpTimerStatus->GetGameTimerStatus();
        const CgsSystem::TimerStatus* lpSimTimer  = lpTimerStatus->GetSimTimerStatus();

        // .cpp:1935 -- the console asserts the game timer is running. On this bring-up the
        // director input buffer is not staged (DoUpdate_Director zeroes it), so it is not:
        // firing a dev assert every frame would block the sim, so the condition is evaluated
        // and reported, not trapped.
        // DELETE-WHEN: the per-frame director input staging lands (BridgeTimers -> the
        // director input's timer status interface).
        const bool lbGameTimerRunning = lpGameTimer->IsRunning();
        (void)lbGameTimerRunning;

        const f32 lfGameTimestep = lpGameTimer->GetCurrentTimeStep();   // [+8] * [+4]

        f32 lfWorldTimestep       = 0.0f;
        f32 lfWorldNoSlomoTimestep = 0.0f;
        if (lpSimTimer->IsRunning())                                    // [+36]
        {
            lfWorldTimestep        = lpSimTimer->GetCurrentTimeStep();  // [+32] * [+28]
            lfWorldNoSlomoTimestep = lpSimTimer->GetBaseTimeStep();     // [+28]
        }

        // ⚠️ GATE: `if (maStateFlagTail[+0x35400]) { all three = 0; }` -- the ICE-owns-the-frame
        // latch lives in the un-homed flag tail (same gate BuildArbStateSharedInfo documents).

        lSharedInfo.mTimestep.Set(BrnDirector::VecFloat(lfGameTimestep),
                                  BrnDirector::VecFloat(lfWorldTimestep),
                                  BrnDirector::VecFloat(lfWorldNoSlomoTimestep),
                                  lfGameTimestep, lfWorldTimestep, lfWorldNoSlomoTimestep);

        lSharedInfo.mUsedRaceCars             = *lpInput->GetUsedRaceCars();
        lSharedInfo.mpRaceCars                = lpInput->GetRaceCarInfo();
        lSharedInfo.mePlayerCarIndex          = static_cast<EActiveRaceCarIndex>(liPlayerCarIndex);
        lSharedInfo.mpDirectorResourceManager = lpIO->mpResourceManager;
        lSharedInfo.mpBehaviourManager        = &mBehaviourManager;
        lSharedInfo.mpWorldMap                = lpIO->mpWorldMap;
        lSharedInfo.mpSceneQueryInterface     = lpIO->mpSceneQueryInterface;
        lSharedInfo.mpRandom                  = reinterpret_cast<CgsNumeric::Random*>(
                                                    const_cast<u8*>(maRandom));

        // ---- the six slots that used to be published as null (see the banner) --------------
        // Each is the console's own `this + <offset>`, reached through the named member.
        lSharedInfo.mpAllVehicleData          = &mAllVehicleData;
        lSharedInfo.mpPlayerTracker           = &mVehicleTracker;
        lSharedInfo.mpEffectInterface         = reinterpret_cast<const EffectInterface*>(
                                                    maEffectInterface);
        lSharedInfo.mpDebugLog                = const_cast<DebugLog*>(&mDebugLog);
        lSharedInfo.mpDebugPrinter            = reinterpret_cast<DebugPrinter*>(maDebugPrinterMain);

        // The player's own vehicle record, BY VALUE (console: VehicleInfo::operator= @0x821F49C8
        // into the shared info's mPlayerInfo, from `raceCars + 1264 * playerIndex` -- 1264 is the
        // X360 sizeof(VehicleInfo); the indexed member read below is the same element).
        // IsLookingAtTarget reads its mRaceCarState.mTransform and mAABB at the end of every
        // ICE-anim Update, so a zeroed record made every take report "can't see player".
        if (lSharedInfo.mpRaceCars != 0 && liPlayerCarIndex >= 0)
        {
            lSharedInfo.mPlayerInfo = lSharedInfo.mpRaceCars[liPlayerCarIndex];
        }

        // ---- the per-frame ICE reference-space cache ---------------------------------------
        // X360 @0x82250074. The eight matrices, in the console's own argument order (recovered
        // from the asm register/home-slot map, NOT from the 8-argument Hex-Rays rendering --
        // which drops the two stack arguments, as it always does):
        //     mCarToWorld          = the PLAYER's world transform
        //     mCar2ToWorld         = GetRaceCar(GetNearestRaceCarIndexToPlayer(1))'s transform
        //     mTrafficLightToWorld = GameState::mTrafficLightSpace          (maGameState + 0x10)
        //     mSceneToWorld        = the ICE editor's scene space           (this + 0x12170)
        //     mImpactToWorld       = AllVehicleData::GetPlayerImpactSpace()
        //     mHeadingToWorld      = AllVehicleData::GetPlayerHeadingSpace()
        //     mLooseHeadingToWorld = AllVehicleData::GetPlayerLooseHeadingSpace()
        //     mHeading2ToWorld     = the nearest race car's transform AGAIN (the console
        //                            recomputes the same index and re-reads the same +0x1F0;
        //                            reproduced, not "cleaned up")
        //     mpGamePlayCam        = &mSharedCameraContainer.mGameplayExternal (this + 0x166A4
        //                            == mArbitrator + 0x38E4 == container + 0x04)
        // The handler is a STACK object here exactly as it is on the console (its frame slot
        // var_2C0); every behaviour that needs it copy-constructs its own inside the
        // UpdateAllBehaviours call below, so its lifetime is this function.
        //
        // ⚠️ GUARD (not console code): the two vehicle reads go through AllVehicleData, which
        // asserts and then indexes mpRaceCars. On the frames before PreSceneQueryUpdate has
        // published one, that pointer is null. The console cannot reach this function in that
        // state (its caller is inside the live-player-car guard); this build can, so the
        // handler is staged only when the snapshot is populated and is otherwise left as
        // Construct's default. DELETE-WHEN: MainDirector::Update's own live-player-car
        // prologue is real.
        if (mAllVehicleData.GetRaceCars() != 0)
        {
            const EActiveRaceCarIndex leNearest =
                mAllVehicleData.GetNearestRaceCarIndexToPlayer(1u);

            const Matrix44Affine& lrPlayerToWorld =
                mAllVehicleData.GetPlayer().mRaceCarState.mTransform;
            const Matrix44Affine& lrNearestToWorld =
                mAllVehicleData.GetRaceCar(leNearest).mRaceCarState.mTransform;

            lCameraSpaces.Construct(lrPlayerToWorld,
                                    lrNearestToWorld,
                                    maGameState.mTrafficLightSpace,
                                    mICESceneSpace,
                                    mAllVehicleData.GetPlayerImpactSpace(),
                                    mAllVehicleData.GetPlayerHeadingSpace(),
                                    mAllVehicleData.GetPlayerLooseHeadingSpace(),
                                    lrNearestToWorld,
                                    &mArbitrator.GetSharedCameras().mGameplayExternal);
        }
        lSharedInfo.mpCameraSpaceHandler = &lCameraSpaces;
    }

    // ------------------------------------------------------------------------
    // BuildCollisionPolicySharedInfo -- NOT an X360 function (see the header). The console's two
    // builds store, in this order (PreScene 0x822557B4..0x82255830):
    //   mpRequestInterface  = lpIO->mpSceneQueryInterface          (lwz 0x10(lpIO))
    //   mpAllVehicleData    = this + 0x12C80
    //   mUsedRaceCars       = *input->GetUsedRaceCars()            (ld / std)
    //   mpRaceCars          = input->GetRaceCarInfo()              (sub_82207040)
    //   mePlayerCarIndex    = the player index
    //   mpRandom            = this + 0x32EE0                       (the camera Random -- maRandom)
    //   mpPlayerCar         = mpRaceCars + 0x4F0 * index           (mulli 0x4F0 = sizeof VehicleInfo)
    //   mpPlayerCarTransform= mpPlayerCar + 0x1F0                  (mRaceCarState.mTransform)
    //   mTimestep           = the 64-byte timestep block the BehaviourSharedInfo was built from
    //   mpDebugPrinter      = this + 0x337B0
    // ⚠️ GUARD (not console code): the console computes mpPlayerCar unconditionally; its callers sit
    // inside the live-player-car guard. This build can reach the passes with no race-car snapshot
    // (the same guard BuildBehaviourSharedInfo documents), so the two player pointers stay null then.
    // ------------------------------------------------------------------------
    void MainDirector::BuildCollisionPolicySharedInfo(const DirectorInputOutput* lpIO,
                                                      s32 liPlayerCarIndex,
                                                      const Camera::BehaviourSharedInfo& lrBehaviourInfo,
                                                      Camera::CollisionPolicySharedInfo& lrPolicyInfo)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        lrPolicyInfo.mpRequestInterface   = lpIO->mpSceneQueryInterface;
        lrPolicyInfo.mpAllVehicleData     = &mAllVehicleData;
        lrPolicyInfo.mUsedRaceCars        = *lpInput->GetUsedRaceCars();
        lrPolicyInfo.mpRaceCars           = lpInput->GetRaceCarInfo();
        lrPolicyInfo.mePlayerCarIndex     = static_cast<EActiveRaceCarIndex>(liPlayerCarIndex);
        lrPolicyInfo.mpRandom             = reinterpret_cast<CgsNumeric::Random*>(const_cast<u8*>(maRandom));
        lrPolicyInfo.mpPlayerCar          = (lrPolicyInfo.mpRaceCars != 0 && liPlayerCarIndex >= 0)
                                                ? &lrPolicyInfo.mpRaceCars[liPlayerCarIndex] : 0;
        lrPolicyInfo.mpPlayerCarTransform = (lrPolicyInfo.mpPlayerCar != 0)
                                                ? &lrPolicyInfo.mpPlayerCar->mRaceCarState.mTransform : 0;
        lrPolicyInfo.mTimestep            = lrBehaviourInfo.mTimestep;
        lrPolicyInfo.mpDebugPrinter       = reinterpret_cast<DebugPrinter*>(maDebugPrinterMain);
    }

    // ------------------------------------------------------------------------
    // UpdateCameraBehavioursPreScene  @ 0x82255318
    //
    // ⭐⭐ ADDED 2026-08-01 (car-select hand-off wave). THE CONSOLE HAS TWO PASSES OVER THE
    // BEHAVIOUR SET AND THIS BUILD ONLY EVER RAN ONE OF THEM -- and, worse, ran it from the
    // wrong entry point. Verified in the ARTIST asm, not inferred:
    //     MainDirector::PreSceneQueryUpdate @0x8225BA00
    //         -> @0x8225BD18  bl UpdateCameraBehavioursPreScene   @0x82255318
    //                             -> @0x822557B0 bl BehaviourManager::UpdateAllBehaviours
    //     MainDirector::Update @0x82274070
    //         -> @0x82274338  bl UpdateCameraBehavioursPostScene  @0x8224FD30
    //                             -> @0x8225024C bl BehaviourManager::PostCollisionUpdateAllBehaviours
    // The PC's PostScene entry called UpdateAllBehaviours -- i.e. it stood in for the PreScene
    // pass -- so vtable slot 3 (Behaviour::PostCollisionUpdate) was NEVER DISPATCHED anywhere
    // in the image. Every behaviour whose per-frame work lives in slot 3 was inert, silently:
    // BehaviourInterpolate does its ENTIRE blend, its parametric-time advance and its
    // mbHasFinished latch there, which is why ArbStateCarSelect's hand-off out of
    // GAME_INTRO_PART_THREE published an unwritten camera and then never finished.
    //
    // The two entries share BuildBehaviourSharedInfo above (the console builds the same
    // ~1540-byte block on its own stack in both; its per-entry prologue differences are inside
    // the same GATE list that banner already carries).
    //
    // ⚠️ ORDERING: PreSceneQueryUpdate runs before Update in the same sub-step, and both gate
    // on the SAME live-player-car predicate, so this pass runs on exactly the frames the old
    // single pass did -- and the arbitrator now reads THIS frame's behaviour output instead of
    // last frame's (the one-frame staleness the Update banner used to record is gone).
    // ------------------------------------------------------------------------
    void MainDirector::UpdateCameraBehavioursPreScene(const DirectorInputOutput* lpIO,
                                                      s32 liPlayerCarIndex)
    {
        Camera::BehaviourSharedInfo lSharedInfo = Camera::BehaviourSharedInfo();
        // ⭐ VALUE-INITIALISED, deliberately (2026-08-16, camcrash2). See the identical note
        // on the PostScene twin below: BuildBehaviourSharedInfo publishes &lCameraSpaces
        // UNCONDITIONALLY but only Constructs it behind a guard, so a default-initialised
        // object hands every ICE consumer 528 bytes of the PREVIOUS frame's stack.
        ICE::CameraSpaceHandler     lCameraSpaces = ICE::CameraSpaceHandler();
        BuildBehaviourSharedInfo(lpIO, liPlayerCarIndex, lSharedInfo, lCameraSpaces);

        // The behaviour manager does not consume the debug-printer interior.
        static u8 saOpaqueDebugPrinter[64]   = { 0 };

        mBehaviourManager.UpdateAllBehaviours(
            lpIO->mpInputBuffer->IsSimPaused(),
            lSharedInfo,
            *lpIO->mpInputBuffer->GetControll(),
            true,                                                         // the console's `1`
            *reinterpret_cast<DebugPrinter*>(saOpaqueDebugPrinter));

        // ⭐ @0x82255834 -- the cameras ASK: every live behaviour's collision policy issues its scene
        // queries for this frame (BehaviourManager::GenerateSceneQueries @0x8221F1C0), answered
        // between the two director passes by DoUpdate_Director's external leg.
        // ⚠️ [FX-DIRECTOR2 opt-in, NOT X360] behind BRN_FXD2_SCENEQUERY with the rest of the query
        // path until one live run with it ON is clean (see BrnDirectorHarness.h); the console calls
        // it unconditionally. The debug printer is the manager's unread fourth argument, as above.
        if (Harness::SceneQueryClosureEnabled())
        {
            Camera::CollisionPolicySharedInfo lPolicyInfo;
            BuildCollisionPolicySharedInfo(lpIO, liPlayerCarIndex, lSharedInfo, lPolicyInfo);
            mBehaviourManager.GenerateSceneQueries(lpIO->mpInputBuffer->IsSimPaused(), lPolicyInfo,
                                                   *reinterpret_cast<DebugPrinter*>(saOpaqueDebugPrinter));
        }
    }

    // ------------------------------------------------------------------------
    // UpdateCameraBehavioursPostScene  @ 0x8224FD30 -- the COLLISION-pass twin (see above).
    // ------------------------------------------------------------------------
    void MainDirector::UpdateCameraBehavioursPostScene(const DirectorInputOutput* lpIO,
                                                       s32 liPlayerCarIndex)
    {
        Camera::BehaviourSharedInfo lSharedInfo = Camera::BehaviourSharedInfo();
        // ⭐⭐ VALUE-INITIALISED, deliberately (2026-08-16, camcrash2 wave). MEASURED, not
        // inferred: `ICE::CameraSpaceHandler` has `= default` special members, so a plain
        // `ICE::CameraSpaceHandler lCameraSpaces;` is 528 bytes of RAW STACK -- and
        // BuildBehaviourSharedInfo publishes its ADDRESS into mpCameraSpaceHandler
        // UNCONDITIONALLY while calling Construct() only inside the
        // `mAllVehicleData.GetRaceCars() != 0` guard. On any frame that guard is shut, every
        // ICE consumer (BehaviourIceAnim copy-constructs one per frame; ICEAuthorSpaceOps and
        // KeyAnimController read through it) sees the PREVIOUS frame's stack image of this
        // same local -- a fully-formed-looking handler whose mpGamePlayCam back-pointer at
        // +0x200 is stale. That is the "valid pointer, invalid object" class, and it is
        // COMPILER-LAYOUT DEPENDENT (a stack object), which is exactly the signature of the
        // public-build AV in GetTransformToWorld's eICE_GAMEPLAY_SPACE arm: a 4-byte-shifted
        // read there lands on mHeading2ToWorld.wAxis.w at +0x1FC -- MEASURED as 0x78160E11,
        // the don't-care w lane every TRIGGERS.DAT spawn position carries into the car's
        // world transform -- and the chain then faults at +0x10.
        // Zeroing costs one 528-byte stack clear per frame and makes the unconstructed case
        // DETERMINISTIC (mpGamePlayCam == 0), where the arm's own assert can see it.
        ICE::CameraSpaceHandler     lCameraSpaces = ICE::CameraSpaceHandler();
        BuildBehaviourSharedInfo(lpIO, liPlayerCarIndex, lSharedInfo, lCameraSpaces);

        static u8 saOpaqueDebugPrinter[64]   = { 0 };

        // ⭐ @0x8224FF30 -- the cameras READ THEIR ANSWERS: every live behaviour's collision policy
        // consumes this frame's scene-query results and may fail its camera
        // (BehaviourManager::ProcessSceneQueryResults @0x8221F438). The console calls it right after
        // building the CollisionPolicySharedInfo, before the ICE camera-space handler and the
        // collision-pass behaviour update below.
        // ⚠️ [FX-DIRECTOR2 opt-in, NOT X360] behind BRN_FXD2_SCENEQUERY (see the PreScene twin).
        if (Harness::SceneQueryClosureEnabled())
        {
            Camera::CollisionPolicySharedInfo lPolicyInfo;
            BuildCollisionPolicySharedInfo(lpIO, liPlayerCarIndex, lSharedInfo, lPolicyInfo);
            mBehaviourManager.ProcessSceneQueryResults(lpIO->mpInputBuffer->IsSimPaused(), lPolicyInfo,
                                                       *reinterpret_cast<DebugPrinter*>(saOpaqueDebugPrinter));
        }

        // ⭐ @0x8225024C -- the console's own call here, and the one this build was missing.
        mBehaviourManager.PostCollisionUpdateAllBehaviours(
            lpIO->mpInputBuffer->IsSimPaused(),
            lSharedInfo,
            *lpIO->mpInputBuffer->GetControll(),
            true,                                                         // the console's `1`
            *reinterpret_cast<DebugPrinter*>(saOpaqueDebugPrinter));
    }

    // ------------------------------------------------------------------------
    // UpdateMoments  @ 0x82250268   -- THE MOMENT TICK
    //
    // [FX-DIRECTOR 2026-09-24] BODIED. Every crash highlight / establishing camera the director can
    // cut to is a MOMENT (MomentTumbling, MomentHardStop, MomentBystanderSeesAction, ...) owned by
    // the moment controller, and a moment only becomes VALID inside its own Update. The console
    // runs this tick every frame from Update (0x82274348); the PC had the call commented out and
    // no body, so every MomentSelector counted 0 valid moments and every crash fell through to the
    // external gameplay camera (FX-CRASHSND2 item 4, scratch/bugtest/runs/fxcrashsnd2_resetcam_diag).
    // ⚠️ THE CALL IS STILL GATED in Update -- see the GATE there for the two hollow-shell behaviours.
    //
    // The console, in order:
    //     if (input->IsSimPaused())  return;                              lbz 0x7AC8 ; bne tail
    //     lfTimestep = gameTimer[+8] * gameTimer[+4];                     fmuls f31
    //     if (mbDebugZeroTimestep /*+0x3543C*/) lfTimestep = 0.0f;        flt_82001CC0
    //     <build a MomentSharedInfo on the stack -- the fields below, each named at its slot>
    //     MomentController::UpdateAllMoments(this + 0x172D0, this + 0x1CB10 (the behaviour
    //                                        manager), &lSharedInfo);
    // ------------------------------------------------------------------------
    void MainDirector::UpdateMoments(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;          // lwz r3, 0(r30)
        if (lpInput->IsSimPaused())
            return;

        const CgsSystem::TimerStatusInterface* lpTimerStatusInterface = lpInput->GetTimerStatusInterface();
        f32 lfTimestep = lpTimerStatusInterface->GetGameTimerStatus()->GetCurrentTimeStep();
        if (maStateFlagTail[E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP])
            lfTimestep = 0.0f;

        const Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();    // sub_82207040
        const Camera::VehicleInfo& lrPlayerCar = lpRaceCars[liPlayerCarIndex]; // mulli 0x4F0

        MomentSharedInfo lSharedInfo;
        lSharedInfo.mPlayerInfo              = lrPlayerCar;                     // VehicleInfo::operator=
        lSharedInfo.mUsedRaceCars            = *lpInput->GetUsedRaceCars();     // ld / std var_90
        lSharedInfo.mpRandom                 = reinterpret_cast<CgsNumeric::Random*>(maRandom);          // +0x32EE0
        lSharedInfo.mpDebugLog               = const_cast<DebugLog*>(&mDebugLog);                        // +0x33108
        lSharedInfo.mpDebugPrinter           = reinterpret_cast<DebugPrinter*>(maDebugPrinterB);         // +0x3378C
        lSharedInfo.mpGameState              = &maGameState;                                             // +0x337E0
        lSharedInfo.mpAllVehicleData         = &mAllVehicleData;                                         // +0x12C80
        lSharedInfo.mpPlayerCar              = &lrPlayerCar;
        lSharedInfo.mpRaceCars               = lpRaceCars;
        lSharedInfo.mpPlayerCarTransform     = &lrPlayerCar.mRaceCarState.mTransform;                    // +0x1F0
        lSharedInfo.mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(liPlayerCarIndex);
        lSharedInfo.mfTimestep               = lfTimestep;
        lSharedInfo.mfSimTimestep            = lpTimerStatusInterface->GetSimTimerStatus()->GetCurrentTimeStep();
        lSharedInfo.mbAllowJumpMoment        = mbAllowJumpMoment;                                        // +0x3542C
        lSharedInfo.mbAllowStuntMoment       = mbAllowStuntMoment;                                       // +0x3542D
        lSharedInfo.mbAllowHardStopMoment    = mbAllowHardStopMoment;                                    // +0x3542E
        lSharedInfo.mbForceNextWorldCrashToBeFastTopDown =
            (maStateFlagTail[E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN] != 0);                   // +0x35437
        lSharedInfo.mbForceCollisionPolicysToStart = false;                                              // li r10, 0
        lSharedInfo.mpBehaviourParameterBank = &mBehaviourManager.GetBehaviourParameterBank();           // +0x2F040
        lSharedInfo.mpNamedBehaviourParams   =
            &mBehaviourManager.GetBehaviourParameterBank().GetNamedParameters();                         // +0x2F050
        lSharedInfo.mpPlayerTracker          = &mVehicleTracker;                                         // +0x339E0
        lSharedInfo.mpContacts               =
            reinterpret_cast<const MomentSharedInfo::ContactSpyInterface*>(lpInput->GetContacts());     // GetCont
        lSharedInfo.mpDirectorResourceManager = lpIO->mpResourceManager;                                 // lwz 8(r30)
        lSharedInfo.mpShotSelector           = &mShotSelector;                                           // +0x121F0
        lSharedInfo.mpCrashAnalysis          = &mCrashAnalyser.GetAnalysis();                            // +0x1245C
        lSharedInfo.mpEffectInterface        = reinterpret_cast<const EffectInterface*>(maEffectInterface); // +0x33C90
        lSharedInfo.mpPlayerCrashInfo        = lpInput->GetPlayerCrashInfo();                            // input +0x78E0

        // (2026-09-24, FX-DIRECTOR2) The shot selector and the crash analyser are real members now and
        //   PreSceneQueryUpdate runs CrashAnalyser::Update, so MomentHardStop's CrashStart gate can hold.

        mMomentController.UpdateAllMoments(mBehaviourManager, lSharedInfo);
    }

    // ------------------------------------------------------------------------
    // Update  @ 0x82274070   -- THE FUNCTION THAT PUBLISHES THE CAMERA
    //
    // Shape of the X360 body (935 lines of pseudocode; the structure is what matters):
    //
    //     Camera::Camera lCamera;  lCamera.Construct();          // line 191, a STACK camera
    //     <the live-player-car prologue>                          // lines 190-247
    //     if ( !live )   lCamera = mLastCamera;                    // LABEL_100, line 250
    //     else
    //     {
    //         UpdateDebugPrinters();                              // line 258
    //         <one debug byte -> the output buffer>               // line 259
    //         DebugLog::Print / DebugLog::Update                  // lines 260-265
    //         UpdateCameraBehavioursPostScene( lpIO, playerIdx );  // line 266
    //         UpdateMoments( lpIO, playerIdx );                    // line 267
    //         if ( !<ICE-owns-frame latch> ) UpdateICE( ... );     // lines 268-269
    //         ⭐ UpdateArbitrator( lpIO, lCamera, playerIdx );      // line 270
    //         <~550 lines of VMX AllVehicleData debug-render, the camera-interpolation
    //          controller (LIVE since 2026-09-25, CC-14), the effect-hook registration
    //          (live) and the world-map safe-position work>       // lines 271-823
    //     }
    //     <two small bookkeeping stores>                          // lines 824-834
    //     CameraFinaliser::Update( &mCameraFinaliser, input, maGameState, resourceMgr,
    //                              &lCamera );                    // line 835
    //     <slomo clamp + assert>                                  // lines 836-848
    //     mLastCamera = lCamera;                                  // line 851  <-- carry over
    //     DebugComponent::UpdatePanoramaScreenshots( mpDebugComponent, &lCamera );  // line 866
    //     lCamera.ValidateTransformWithDebugInfo();               // line 867
    //     lCamera.CopyToCgsCamera( &mCgsCamera );                 // line 868
    //     output->SetCgsCamera( mCgsCamera );                     // line 869  <-- PUBLISH
    //     output->SetCameraOutput( lCamera );                     // line 870  <-- PUBLISH
    //     TimerRequests::SetTimestepMultiplier( ... );            // lines 871-875
    //     UpdateDebugInfo / DebugDisplayCurrentCamera /
    //     BehaviourManager::PrepareBehaviours / UpdateAttribSys   // lines 876-879
    //     <debug flag printers + latches>                         // lines 880-924
    //
    // ⭐ THE ARBITRATOR LEG OF THE MIDDLE IS NOW REAL. `UpdateArbitrator` at line 270 is the
    // single call that lets a director camera MOVE: it runs the arbitrator's outer state
    // machine, which runs whichever arbitrator state owns the frame (including
    // ArbStateAttractMode -- the DJ fly-by's path) and copies that state's camera into the
    // frame camera. The previous wave gated the whole middle on the belief that
    // BehaviourManager had no layout; it does, so the leg is transcribed.
    //
    // ⚠️ WHAT IS STILL GATED INSIDE THE MIDDLE (each a documented quiet gate, none of which
    // touches the camera the arbitrator just produced):
    //   * UpdateDebugPrinters / DebugLog::Print / DebugLog::Update -- DebugPrinter and
    //     DebugLog are un-homed named regions.
    //   * UpdateICE -- declaration-only (the ICE take is un-homed). (UpdateMoments is bodied and
    //     CALLED since 2026-09-24 -- see the call site.)
    //     ⚠️ ORDERING NOTE: the console runs those BEFORE UpdateArbitrator, so the
    //     arbitrator sees last frame's behaviour output rather than this frame's. That is a
    //     one-frame staleness in the behaviour-driven camera, not a wrong camera.
    //   * lines 271-823 -- ~550 lines of VMX AllVehicleData debug-render work, the ICE-editor preview
    //     and the world-map safe-position search. All reach un-homed aggregates and/or VMX pipelines, and
    //     are dev-only. Live since 2026-09-25: the Event_Win(50) request (CC-16) and the camera
    //     interpolation controller (CC-14); the effect-hook registration cascade has been live since
    //     2026-09-17.
    //   * the tail after the publish (lines 871-924): ONLY the debug-info/overlay passes
    //     (UpdateDebugInfo, DebugDisplayCurrentCamera, the camera-state flag printer). The
    //     time-step multiplier, PrepareBehaviours, UpdateAttribSys, the event-end push and the two
    //     closing latches (0x82275290..0x822752D0, incl. the one-shot clear of
    //     mbForceNextWorldCrashToBeFastTopDown) all run -- see each call site.
    //
    // DELETE-WHEN: per item above, as each aggregate is homed.
    // ------------------------------------------------------------------------
    void MainDirector::Update(const DirectorInputOutput* lpIO)
    {
        // The frame camera is a STACK local on the console too (v211) -- built fresh every
        // frame, reaching the director's storage only through mLastCamera below.
        Camera::Camera lCamera;
        lCamera.Construct();

        const s32 liPlayerCarIndex = GetLivePlayerCarIndex(lpIO);

        if (liPlayerCarIndex == -1)
        {
            // LABEL_100 -- carry last frame's finalised camera forward.
            lCamera = mLastCamera;
        }
        else
        {
            // ⚠️ GATE: UpdateDebugPrinters / the debug byte / DebugLog::Print+Update.

            // ⭐ @0x8224FD30 -- the pass that dispatches every live behaviour's Update. It is
            // REAL now (partially: see the body's FLAG list), which is what lets
            // BehaviourRoadRunner::Update run at all.
            UpdateCameraBehavioursPostScene(lpIO, liPlayerCarIndex);

            // ⭐ @0x82250268 -- the MOMENT tick (the call at 0x82274348). It builds the frame's
            // MomentSharedInfo and runs MomentController::UpdateAllMoments, which Updates every allocated
            // highlight moment (tumbling, hard stop, bystander / passenger sees action, static cam, stunt
            // ...). It is what gives a crash its highlight camera, and what drops the racing-gameplay
            // flag the reset-on-track sting rises on. Unconditional, right after the behaviour pass, as on
            // the console.
            // UN-GATED 2026-09-24 (FX-DIRECTOR). The call was held back after its first live run AV'd
            // in BehaviourHelper::Prepare: BehaviourBystanderCam and BehaviourFixedCam were hollow shells
            // with no vtable (made real in c65dea67 / 22091bb2), the bank's passenger block was zeroed
            // (8d9b0b73), and the stunt moment sign-extended its take id (b6b8c178). Clean with it on:
            // scratch/bugtest/runs/fxdirector_moment_tick/20260924_181711 and
            // scratch/bugtest/runs/fxdirector_stunt_jump/20260924_182507.
            UpdateMoments(lpIO, liPlayerCarIndex);
            // ⚠️ GATE: if ( !<ICE-owns-frame latch> ) UpdateICE( lpIO, liPlayerCarIndex );

            // ⭐ The arbitrator picks and runs the state that owns this frame's camera.
            UpdateArbitrator(lpIO, lCamera, liPlayerCarIndex);

            // [diag] BRN_SLOMO_DIAG -- where the published time scale comes from.
            BrnDiag_ReportSimTimeScale("post-arbitrator", lCamera.GetEffects().mfSimTimeScale);

            // ⭐ (2026-09-25, FX-DIRECTOR2 CC-16) @0x82274524..0x82274554 -- THE EVENT-WIN SCREEN EFFECT. The
            // console runs it on every live frame: it is the join point after the ICE-editor preview block
            // (gated on +0x12160, dev-only, not reconstructed), before DirectorDevTools::Update. An ICE take
            // keys POSTFX_HOOK into the frame camera (KeyAnimController::UpdateCameraFromICE ->
            // CameraEffects::muRequestedPostFxId, camera +0xE4). 578869 is authored in 26 retail takes:
            // Event_Win2..5, the 21 World_Win_*, Online_End_Win, and Takedown_ICE_Shut's fourth interval.
            // For those the director asks the EffectInterface (+0x33C90) for the "Event_Win(50)" GUI hook
            // at 0.75 through the shared helper. That helper drops the camera's own requests (the post-FX id
            // among them) and re-requests the hook unless it is already live at that blend.
            if (lCamera.GetEffects().muRequestedPostFxId == KU_EVENT_WIN_POSTFX_ID)
            {
                Camera::EnsureEffectIsPlaying(lCamera, *reinterpret_cast<const EffectInterface*>(maEffectInterface),
                                              KPC_EVENT_WIN_HOOK, KF_EVENT_WIN_HOOK_BLEND);
            }

            // ⭐ (2026-09-25, FX-DIRECTOR2 CC-14) @0x822749D4..0x82274A24 -- THE GAME-CAMERA BLEND.
            // An ICE take keys CAMERA_BLEND_AMOUNT into the frame camera (KeyAnimController::
            // UpdateCameraFromICE stores it * 0.01 as CameraEffects::mfGameCameraBlend, +0xA0 == camera
            // +0x108). When it is keyed the director eases the frame camera toward the shared chase
            // camera -- the gameplay-EXTERNAL behaviour's produced camera (sub_82212288(this + 0x166A4),
            // mArbitrator's SharedCameraContainer +0x04, whichever gameplay camera is selected) -- about
            // the player car's transform (GetRaceCarInfo() [the frame's car] + 0x1F0, r24 @0x82274640),
            // through the director's own controller (+0x121B0), which also consumes the blend. That is
            // how a take such as "Takendown" eases out of and back into the chase camera instead of
            // cutting. `fcmpu f0, f29(=0.0, flt_82001CC0) ; ble` -- anything not above zero, a NaN
            // included, takes the reset arm: both interpolaters zeroed (0x82274A08..0x82274A24).
            BrnDiag_GameCameraBlendBefore(lCamera);                         // [DIAG] NOT X360
            const bool lbGameCameraBlendReset = !(lCamera.GetEffects().mfGameCameraBlend > KF_GAME_CAMERA_BLEND_OFF);   // [DIAG]
            if (lCamera.GetEffects().mfGameCameraBlend > KF_GAME_CAMERA_BLEND_OFF)
            {
                mCameraInterpolationController.Update(
                    lCamera,
                    mArbitrator.GetSharedCameras().mGameplayExternal.GetProducedCamera(),
                    lpIO->mpInputBuffer->GetRaceCarInfo()[liPlayerCarIndex].mRaceCarState.mTransform);
            }
            else
            {
                mCameraInterpolationController.Construct();
            }
            BrnDiag_GameCameraBlendAfter(lCamera, mArbitrator.GetSharedCameras().mGameplayExternal,
                                         lbGameCameraBlendReset);        // [DIAG] NOT X360

            // ⭐ (2026-09-25, FX-DIRECTOR2) @0x82274A28..0x82274A80 -- THE ICE SCENE SPACE FOLLOWS THE
            // PUBLISHED CAMERA. Unless the frame camera is in a scene-space shot, its transform becomes the
            // scene space: `ld` camera +0x140, `rlwinm 0,18,18` (flag 13) ; `bne` skip ; four lvx128 /
            // stvx128 into this + 0x12170. KeyAnimController::UpdateTransformationMatrix raises flag 13 for
            // a take whose eye or look is authored in eICE_SCENE_SPACE. So such a take is anchored to the
            // last camera before it (UpdateCameraBehavioursPostScene stages this matrix into the frame's
            // ICE::CameraSpaceHandler as mSceneToWorld), and the space holds still while the take runs.
            // 43 of the 549 retail takes author scene space (Race_StartFX, Race_Event_Start, the RaceIntros,
            // Road_Rage_Start, Stunt_Intro / Loop, the World_Signature_* jump outros, CrashBreaker_lvl1_v3/4,
            // End_State, Burning_Finish, the CarUnlock_* ...). Without this they all projected through the
            // zero matrix.
            if (!lCamera.mState.IsFlagSet(Camera::CameraState::E_FLAG_DONT_UPDATE_SCENESPACE))
            {
                mICESceneSpace = lCamera.mTransform;
            }

            // ⭐ X360 @0x82274070 pseudocode 602..653 -- THE HOOK-REQUEST HAND-OVER. The camera
            // the arbitrator just produced carries this frame's post-FX requests (its
            // CameraEffects: a start name + blend, a stop name, the background request); this
            // is where they are registered on the director's EffectInterface -- the record
            // Camera::EnsureEffectIsPlaying compares against next frame (without it every
            // frame re-requests the same hook and BridgeDirectorToGui restarts it every
            // frame). A request naming a hook the GUI has not enumerated is dropped by
            // clearing its presence flag on the camera, so the bridge never posts it.
            // (v216 == sp+0x128 == lCamera+0x68 == its CameraEffects; v217 == +0xB8 == the
            // background request; v224/v225 == the two presence flags -- IDA's frame split.)
            {
                EffectInterface* lpEffectInterface =
                    reinterpret_cast<EffectInterface*>(maEffectInterface);          // this + 212112
                Camera::CameraEffects& lrEffects = lCamera.GetEffects();

                if (PfxDirDiag() && (lrEffects.mbHasStartHookNameString || lrEffects.mbHasStopHookNameString))
                {
                    *CgsDev::Log::gpDebugPrint << "[pfx-dir] hand-over: start "
                        << (lrEffects.mbHasStartHookNameString ? lrEffects.GetStartHookNameString().mHookNameString : "-")
                        << " exists " << (lrEffects.mbHasStartHookNameString && lpEffectInterface->HookExists(lrEffects.GetStartHookNameString().mHookNameString) ? 1 : 0)
                        << " stop " << (lrEffects.mbHasStopHookNameString ? lrEffects.GetStopHookNameString().mHookNameString : "-")
                        << " gotHooks " << (lpEffectInterface->HasGotHooks() ? 1 : 0) << "\n";
                }
                if (lrEffects.mbHasStartHookNameString)                             // +0xB7
                {
                    // v123: a stop is also requested this frame AND it names a different hook.
                    const bool lbStartDiffersFromStop =
                        lrEffects.mbHasStopHookNameString &&
                        strcmp(lrEffects.GetStartHookNameString().mHookNameString,
                               lrEffects.GetStopHookNameString().mHookNameString) != 0;
                    if (lpEffectInterface->HookExists(lrEffects.GetStartHookNameString().mHookNameString))
                    {
                        if (!lbStartDiffersFromStop)
                            lpEffectInterface->RegisterStartingEffectWithName(
                                lrEffects.GetStartHookNameString(),
                                lrEffects.GetStartHookNameBlendAmount());
                    }
                    else
                    {
                        lrEffects.mbHasStartHookNameString = false;                 // v224 = 0
                    }
                }
                lrEffects.GetBackgroundEffectRequest().RegisterAndUpdateRequest(lpEffectInterface);
                if (lrEffects.mbHasStopHookNameString)                              // +0xB8
                {
                    if (lpEffectInterface->HookExists(lrEffects.GetStopHookNameString().mHookNameString))
                    {
                        if (!lrEffects.mbHasStartHookNameString)
                            lpEffectInterface->RegisterStoppingEffectWithName(
                                lrEffects.GetStopHookNameString());
                    }
                    else
                    {
                        lrEffects.mbHasStopHookNameString = false;                  // v225 = 0
                    }
                }
            }

            // ⚠️ GATE: the rest of the ~550-line VMX / world-map remainder (lines 271-823; the
            //   effect-hook hand-over above is live since 2026-09-17).
        }

        // ⭐ (2026-09-24, FX-DIRECTOR2) @0x82274F6C..0x82274FE4, right before the finaliser: when the
        // published camera is a new shot (E_FLAG_NEW_THIS_FRAME, `rlwinm 0,25,25` on camera +0x140) the
        // crash analyser latches the analysis that shot was chosen against (the inlined SetShotChanged),
        // then the inlined ShotSelector::Update ticks the use clock and stamps the shot the camera came
        // from.
        if (lCamera.mState.IsFlagSet(Camera::CameraState::E_FLAG_NEW_THIS_FRAME))
            mCrashAnalyser.SetShotChanged();
        mShotSelector.Update(lCamera);

        // Finalise: camera inertia + shake (the CameraFinaliser owns the InertiaController).
        mCameraFinaliser.Update(lpIO->mpInputBuffer, &maGameState, lpIO->mpResourceManager,
                                &lCamera);

        // [diag] BRN_SLOMO_DIAG
        BrnDiag_ReportSimTimeScale("post-finaliser", lCamera.GetEffects().mfSimTimeScale);

        // ⭐⭐ X360 lines 836-848 (@0x82275008..0x82275064) -- THE SLOW-MOTION PERMISSION GATE.
        // ⚠️ THE VALUE IT GUARDS IS NOT A SEPARATE LOCAL: IDA calls it `v220` / `var_40C`, but
        // var_40C == sp+0x1C4, and the stack camera `v211` is var_510 == sp+0xC0, so
        // 0xC0 + 0x104 == 0x1C4 -- i.e. var_40C IS lCamera.mEffects.mfSimTimeScale, split out
        // of the stack object by IDA's frame analysis. (That also explains why the asm has only
        // two stores to it and no initialiser: the "initialiser" is Camera::Construct.) This is
        // the reading that makes the tail below the game's slow-motion channel rather than a
        // read of uninitialised stack.
        //
        // `_R30 + 211168` is maGameState + 0x100 == GameState::mbCanUseSlomo (Construct seeds
        // it to 1, so slomo IS permitted by default); `_R30 + 0x33105` is the second of the two
        // camera-car flag bytes, whose role is recovered HERE for the first time: it is the
        // "assert if a slomo request survives into a no-slomo frame" dev flag, and Construct
        // seeds it to 0, so the assert is off in retail.
        if (!maGameState.mbCanUseSlomo)
        {
            CGS_ASSERT(!(maCameraCarFlags[1] != 0 && lCamera.GetEffects().mfSimTimeScale != 1.0f),
                       "Trying to use slomo when not allowed");
            lCamera.GetEffects().mfSimTimeScale = 1.0f;
        }

        // ⭐ X360 line 851 (@0x82275068..0x8227508C) -- THE CAMERA-STATE FRAME ROLL, and the only
        // write of the published camera's PREVIOUS flag set:
        //   0x82275074  ori r10, r10, 0x3050 ; this + 0x33050 == mLastCamera.mState.mFlags
        //   0x82275088  ldx r11, r30, r10    ; read BEFORE the operator= below overwrites it
        //   0x8227508C  std r11, var_3C8     ; lCamera.mState.mPreviousFlags (sp+0x208)
        //   0x82275090  bl  Camera::operator=(this + 0x32F10, &lCamera)
        // So the published camera's previous set is LAST FRAME'S published current set, and every
        // HasChanged consumer of it (the sound CameraControl stings and mixer snapshots, HUDEffect,
        // EffectsModule) sees one edge per real change. Without it the previous set was whatever the
        // arbitrator state's camera carried: the reset-on-track sting re-posted on every gameplay-
        // camera frame and kept all four FxEffect voices busy, dropping every other FX sting.
        //
        // ⭐ X360 lines 850 / 853-866 -- THE POST-FX ID BOOKKEEPING (RESTORED 2026-09-24, crash parity
        // FX-DIRECTOR; FX-CRASHSND's finding). An ICE camera publishes its take's authored post-FX
        // hook id in mEffects.muRequestedPostFxId every frame it plays (KeyAnimController), and
        // BridgeDirectorToGui turns a non-zero id into GUI event 495 (start the PFX by GUID). This
        // bookkeeping makes that an EDGE: the id travels only on the frame it changes (or on a camera's
        // first frame), and the director's EffectInterface records it, which is what later lets
        // Camera::StopCurrentEffect ask for the null effect (0x7BEC6). Without it the PC re-started the
        // take's post-FX on every frame of the shot and never stopped it.
        //   0x82275084  lwzx r31, r30, 0x32FF4    ; mLastCamera.mEffects.muRequestedPostFxId -- read
        //                                         ; BEFORE the operator= below
        //   0x82275090  bl   Camera::operator=    ; mLastCamera = lCamera (keeps the real id)
        //   0x82275094  lwz  r10, 0x1A4(r1)       ; lCamera.mEffects.muRequestedPostFxId
        //   0x82275098  cmplw ; bne -> 0x822750CC ; changed since last frame
        //   0x822750A0  ld 0x200(r1) ; & 0x40     ; E_FLAG_NEW_THIS_FRAME (bit 6) set -> 0x822750CC
        //   0x822750C4  stw r28(=0), 0x1A4(r1)    ; neither: publish no id this frame
        //   0x822750CC  cmplwi r10, 0 ; beq       ; changed / new to 0: nothing to register
        //   0x822750D4..0x822750E4                ; the inlined EffectInterface::RegisterStartingEffectWithId
        const u32 luLastRequestedPostFxId = mLastCamera.GetEffects().muRequestedPostFxId;   // 0x82275084

        lCamera.GetState().CopyFlagsToPrevious(mLastCamera.GetState());

        // Carry the finalised camera into the next frame.
        mLastCamera = lCamera;

        const u32 luRequestedPostFxId = lCamera.GetEffects().muRequestedPostFxId;          // 0x82275094
        if (luRequestedPostFxId == luLastRequestedPostFxId &&
            !lCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_NEW_THIS_FRAME))
        {
            lCamera.GetEffects().muRequestedPostFxId = 0;                                   // 0x822750C4
        }
        else if (luRequestedPostFxId != 0)
        {
            reinterpret_cast<EffectInterface*>(maEffectInterface)->RegisterStartingEffectWithId(
                luRequestedPostFxId);                                                        // 0x822750D4..E4

            // [DIAG] BRN_PFX_DIAG (the GUI PFX switch, default off) -- NOT IN THE X360 BINARY. One line
            // per registration, i.e. per frame the id actually travels to the GUI (at most 40 lines).
            static const bool sbPostFxIdDiag      = (getenv("BRN_PFX_DIAG") != 0);
            static s32        siPostFxIdLinesLeft = 40;
            if (sbPostFxIdDiag && siPostFxIdLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                --siPostFxIdLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[postfx-id] director publishes + registers post-FX id " << luRequestedPostFxId
                    << " (last frame " << luLastRequestedPostFxId << ", new camera "
                    << (lCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_NEW_THIS_FRAME) ? 1 : 0)
                    << ")\n";
            }
        }

        // The debug component's panorama-screenshot pass gets the finished camera.
        if (mpDebugComponent != 0)
            mpDebugComponent->UpdatePanoramaScreenshots(&lCamera);

        // Validate (asserts on NaN / unreasonable position), then convert to the graphics
        // camera and publish BOTH forms into the director output buffer.
        lCamera.ValidateTransformWithDebugInfo();

        // [diag] BRN_CAMERA_TRACE -- the per-frame published-camera witness (see the helper).
        BrnDiag_ReportCamera(lCamera);

        lCamera.CopyToCgsCamera(&mCgsCamera);

        lpIO->mpOutputBuffer->SetCgsCamera(mCgsCamera);
        lpIO->mpOutputBuffer->SetCameraOutput(lCamera);
        {   // [cam-flags] BRN_CAM_INPUT_DIAG: the state flags as published.
            static const bool sbCamDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
            static u32 suCamDiagCalls = 0;
            if (sbCamDiag && (suCamDiagCalls++ % 60u) == 0 && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[cam-flags] director publish flags " << lCamera.mState_uFlags << "\n";
        }
        // @0x82274070 pseudocode 255 (`*(out + 1872) = *(this + 218166)`): publish the
        // hook-enumeration request byte. The console stores it in the un-paused branch right
        // after UpdateDebugPrinters; the value is PostGuiUpdate's latch either way.
        lpIO->mpOutputBuffer->SetRequestHookEnumeration(
            maStateFlagTail[E_FLAG_TAIL_REQUEST_HOOK_ENUMERATION] != 0);

        // ⭐⭐ X360 lines 871-875 (@0x82275128..0x82275148) -- THE TIME-DILATION PUBLISH, and the
        // whole reason a camera can slow the game down. It was GATED here with the note "none of
        // them alters the published camera -- the two publish calls are already done", which is
        // true and beside the point: this call does not alter the camera, it converts the camera
        // into the SIM TIMESTEP. Every crash / takedown / close-up / hard-stop camera in the
        // game writes its request into Camera::mEffects.mfSimTimeScale (Camera::
        // SetRequestedTimeDilation is a one-line setter on exactly that field), and THIS is the
        // only place that value leaves the director. While the line was commented out the whole
        // director side of the game's slow motion was inert -- the requests landed on the camera
        // and were dropped.
        //
        //   0x82275128  lwz  r3, 4(r14)                  ; the director OUTPUT buffer
        //   0x8227512C  lfs  f31, 0x5D0+var_40C(r1)      ; lCamera.mEffects.mfSimTimeScale
        //   0x82275130  bl   OutputBuffer::GetTimerRequestInterfac
        //   0x82275138  addi r3, r3, 8                   ; <- GetSimTimerRequests()
        //   0x8227513C  lfs  f0, flt_8200CE04            ; 0.005f
        //   0x82275140  fsubs f13, f31, f0
        //   0x82275144  fsel  f1, f13, f31, f0           ; (scale - 0.005 >= 0) ? scale : 0.005
        //   0x82275148  bl   TimerRequests::SetTimestepMultiplier
        //
        // `addi r3, r3, 8` is the inlined GetSimTimerRequests(): this is the SIM timer, not the
        // game timer -- the same +8 ModeManager::FinishCurrentMode and DriveThruManager emit.
        // The 0.005 is a FLOOR, not a default: a camera may not slow the simulation below 1/200
        // of real time. It is a genuine clamp with a non-identity value, so it is transcribed
        // rather than dropped.
        //
        // ⚠️ The consumer end is BrnGameModule::UpdateTimers, whose Append/ApplyToTimers half
        // was gated for the same reason and is live as of the same wave. Landing only one of
        // the two would still be inert.
        {
            const f32 lfRequestedTimeScale = lCamera.GetEffects().mfSimTimeScale;
            const f32 lfClampedTimeScale   =
                ((lfRequestedTimeScale - KF_MINIMUM_SIM_TIME_SCALE) >= 0.0f)
                    ? lfRequestedTimeScale
                    : KF_MINIMUM_SIM_TIME_SCALE;

            // ✅✅ THE GATE IS GONE (2026-08-28, crash-camera wave). THE PUBLISH IS THE CONSOLE'S,
            // UNCONDITIONAL, AS THE ASM ABOVE HAS IT. Its own DELETE-WHEN condition -- "an
            // unauthored TIME_SCALE reads 100, verify with BRN_ICE_TIMESCALE_DIAG" -- is MET
            // and MEASURED on this build:
            //     [ice-timescale] raw TIME_SCALE=100.000000 -> mfSimTimeScale=1.000000
            //     [slomo] simScale=1.000000 gameScale=1.000000 simStep=0.016667 gameStep=0.016667
            // ONE [slomo] line for a 150 s run that reached DRIVING with 0 asserts -- the probe is
            // edge-triggered, so one line means the published scale never left 1.0. The same
            // command on the previous build printed simScale=0.005000 simStep=0.000083, i.e. the
            // frozen game the gate existed to prevent.
            //
            // ⛔ AND BOTH CANDIDATE DIAGNOSES BELOW WERE WRONG -- recorded so nobody re-chases them.
            // The take's value table being memclear-seeded is CORRECT (the console memclears it
            // too, ICEDataICETake.cpp:26), and the TIME channel is NOT lost in the asset port.
            // The zero was OURS: ICEElementDescription::GetDefaultFloat() was written as a raw
            // union read, `return mDefault.GetFloat();`, so an INTEGER-typed default came back as
            // its own bits reinterpreted as IEEE-754. TIME_SCALE's DEFAULT = (s32)100 read as
            // 1.401e-43, and the eICE_UINT round-half-up in ICETake::SetParameter stored 0. The
            // console's inlined body (@0x82530894, the only call site) branches on mDataType
            // FIRST and converts with fcfid/frsp. Fixed in ICEDataEnums.hpp with that asm quoted.
            //
            // ⭐ THE HISTORY BELOW IS KEPT because the *mechanism* it documents is still the
            // mechanism: this call is what converts a camera into the sim timestep, and every
            // crash / takedown / close-up camera reaches the simulation through it.
            //
            // MEASURED 2026-08-28, twice, with BRN_SLOMO_DIAG: the frame camera arrives here
            // carrying mfSimTimeScale == 0.000000 from the moment the junkyard ICE cameras take
            // over ("[junkyard] LEAVE" / "[ice-prepare] guid 610132"). The probe pins it to the
            // ARBITRATOR leg -- `camera post-arbitrator mfSimTimeScale=1.000000` while the
            // roaming camera owns the frame, then 0.000000 one entry later.
            //
            // THE PRODUCER OF THE ZERO IS KeyAnimController::UpdateCameraFromICE @0x8221E630
            // (Shots/ShotControllers/BrnKeyAnimController.cpp):
            //     lrEffects.mfSimTimeScale = lrTake.GetValueFloat(E_ICE_TIME_SCALE) * 0.01f;
            // TIME_SCALE is authored as a PERCENTAGE, and this build's take reads 0 for it. Its
            // own element description says what 0 means (ICEData.cpp:254, element [19]):
            //     { "TIME_SCALE", "Time Scale", channel 4, eICE_UINT, 7,
            //       ICEValue((s32)100),   <- DEFAULT
            //       ICEValue((s32)0),     <- min
            //       ICEValue((s32)100) }  <- max
            // The authored DEFAULT is 100 (== real time); the take's value table is seeded to
            // (s32)0 for every element instead (ICEData.cpp:1665). So an ICE camera whose take
            // does not author the TIME channel requests 0% of real time -- and 0 is NOT the
            // identity of a percentage scale. Retail cannot behave this way: with the console's
            // own 0.005 floor applied it would run the entire junkyard intro at 1/200 speed.
            //
            // Publishing it unconditionally therefore does exactly that here -- measured:
            //     [slomo] simScale=0.005000 gameScale=1.000000 simStep=0.000083
            // -- a playable build turned into a frozen one, with a green link and green logs.
            //
            // ⭐ THE TRANSPORT ITSELF IS PROVEN BY THAT SAME LINE: a value written by a camera
            // reached CgsSystem::Timer::mfScaleCurrent and changed the simulation timestep by a
            // factor of 200. What is missing is a correct value, not a path.
            //
            // (BRN_DIRECTOR_SLOMO is retired. It gated only this call; nothing else read it.)
            lpIO->mpOutputBuffer->GetTimerRequestInterfac()
                ->GetSimTimerRequests()
                ->SetTimestepMultiplier(lfClampedTimeScale);
        }

        // ⭐ X360 line 878 -- BehaviourManager::PrepareBehaviours(&mBehaviourManager,
        // lpIO->mpResourceManager). UNCONDITIONAL (outside the live-player-car branch above),
        // and it runs AFTER the publish, so a behaviour allocated during this frame's
        // arbitrator pass gets its first Prepare before the next frame reads it.
        //
        // This one call is what makes the attract state's poll terminate: NewBehaviour<> raises
        // the manager's "needs preparing" bit, ArbStateAttractMode::Prepare returns false while
        // it is set, and THIS is the only thing that clears it (by dispatching the behaviour's
        // own Prepare). Without it the fly-by state would poll for ever.
        mBehaviourManager.PrepareBehaviours(lpIO->mpResourceManager);

        // ⭐ X360 line 879 -- UpdateAttribSys( lpIO->mpInputBuffer ). REAL as of 2026-08-02,
        // in the console's own position (immediately after PrepareBehaviours). It is INERT on
        // this build by design: its only gate is the controller block's
        // mbGameTalkRefreshRequest byte, which only the live-tuning tool sets. See its body.
        UpdateAttribSys(lpIO->mpInputBuffer);

        // ⭐ THE EVENT-END PUSH, immediately after UpdateAttribSys. One of the four producers of
        // GameState::mEventState: when either end-of-event request in the flag tail is standing
        // the journal goes back to ACTIVE and meEventType is cleared to E_MODE_NONE, which is
        // what takes the arbitrator out of the post-event / race-intro ladders.
        // FLAG: the second half of the first arm is a byte inside the director OUTPUT
        //   INTERFACE, an opaque named span in the output buffer with no recovered field
        //   layout, so it is read at its own documented offset -- the same treatment the span
        //   gets everywhere else. Its ROLE (it qualifies the first request) is the console's.
        if ((maStateFlagTail[E_FLAG_TAIL_EVENT_END_REQUEST] != 0 &&
             lpIO->mpOutputBuffer->GetDirectorOutputIn()[0x0D] != 0) ||
            maStateFlagTail[E_FLAG_TAIL_EVENT_END_FORCED] != 0)
        {
            maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_ACTIVE);
            maGameState.meEventType = BrnGameState::GameStateModuleIO::E_MODE_NONE;
        }

        // ⚠️ GATE (debug display only): 0x8227520C..0x8227528C -- while mbShowCameraStateFlags
        // (+0x3543B) is set, print the thirty CameraState flag names through the main DebugPrinter.
        // Not reproduced (the printer is an opaque region here); it writes nothing but text.

        // ⭐ THE TAIL'S TWO LATCHES (2026-09-25, FX-DIRECTOR2), the console's last statements
        // (r28 == 0 from `li r28, 0` @0x82274F64, r27 == 1 from `li r27, 1` @0x82274F68, neither
        // written again before them):
        //
        //   0x82275290..0x822752B0  if (mbDebugSingleTimestep /*+0x3543D*/)
        //                           { mbDebugSingleTimestep = 0; mbDebugZeroTimestep /*+0x3543C*/ = 1; }
        //       a debug tweakable pair, both seeded 0 by Construct: inert on retail, landed for order.
        //
        //   0x822752B4..0x822752D0  if (mVehicleTracker.mbIsFirstFrameOfCrash /*+0x33C85 = tracker +0x2A5*/)
        //                               mbForceNextWorldCrashToBeFastTopDown /*+0x35437*/ = 0;
        //       ⭐⭐ THIS MAKES THE FLAG A ONE-SHOT -- "force the NEXT world crash": Construct seeds it 1
        //       (0x8225B924) and the first crash after boot consumes it here. It is read in exactly two
        //       places (a scan of every ARTIST export for +0x35437): VehicleTracker::Update's 4th
        //       argument (0x8225BCBC -- a wall hard stop classifies HIGH) and UpdateMoments' shared-info
        //       copy (+0x527 -- MomentHardStop's "fast top-down" A shot). HIGH is what arms
        //       MomentHardStop's ultra slo-mo (every KU_CRASHES_BEFORE_FORCE_ULTRA_SLOMO-th high-energy
        //       world crash, dt x 0.005..0.01). Without this clear the PC forced EVERY wall crash HIGH;
        //       the console forces only the first one after boot.
        if (maStateFlagTail[E_FLAG_TAIL_DEBUG_SINGLE_TIMESTEP])
        {
            maStateFlagTail[E_FLAG_TAIL_DEBUG_SINGLE_TIMESTEP] = 0;   // stb r28 @0x822752A8
            maStateFlagTail[E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP]   = 1;   // stbx r27 @0x822752B0
        }
        if (mVehicleTracker.IsFirstFrameOfCrash())                   // lbzx +0x33C85 @0x822752BC
        {
            maStateFlagTail[E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN] = 0;   // stbx r28 @0x822752D0
        }
    }

    // ------------------------------------------------------------------------
    // PostGuiUpdate  @ 0x82236F88   -- PARTIALLY LIVE (the two intro latches), rest still gated
    //
    // The post-GUI pass DirectorModule::PostGuiUpdate runs when not replaying. The X360 body
    // folds this frame's GUI events -- the ones BrnGameModule::BridgeGuiToDirector @0x823CBF70
    // has just published into the director INPUT buffer -- into the director's GameState, then
    // runs the effect interface + the "prepare for mode" action:
    //
    //     if ( input->GetCarSelectionChangedThisFrame() )     <GameState +211328 = 1>
    //     if ( input->HasGotCrashNavShownEvent() )            <set the crash-nav shown latch>
    //     if ( input->HasGotCrashNavHiddenEvent() )           <clear it>
    //     if ( input->HasGotColourCalibrationShownEvent() )   <set the colour-cal latch>
    //     if ( input->HasGotColourCalibrationHiddenEvent() )  <clear it>
    //     if ( input->HasGotShortcutMenuEvent() )
    //         <GameState shortcut-menu state> = input->GetShortcutMenuState();
    //     if ( input->GetEndOfCarSelect() && <mode> )         <mode = 5; clear a flag>
    //     if ( input->HasGotHookEnumeration() )
    //         EffectInterface::Update( maEffectInterface, *input->GetHookEnumeration(), ... )
    //     if ( input->GetRankUpThisFrame() )                  <rank latch + new rank>
    // ⭐  if ( input->GetStartNewProfileIntro() )    mbNewProfileIntroActive = true;
    // ⭐  if ( input->GetStartGameIntroFlyby()  )    mbGameIntroFlybyActive  = true;
    // ⭐  if ( input->GetStopGameIntroFlyby()   )  { mbGameIntroFlybyActive  = false;
    //                                               mbNewProfileIntroActive = false; }
    //     if ( input->HasNewDirectorProfileData() )           <profile data + a derived bool>
    //     ... ( the online post-event / 100%-sequence / mode-action tail )
    //     MainDirector::HandlePrepareForModeAction( this, maModeActionAndDebugBlock, lpIO )
    //
    // ⭐ NEARLY ALL OF IT IS LIVE NOW (2026-08-01). Naming maGameState turned the "un-homed
    // remainder" into ordinary members, so every leg above whose destination is a GameState
    // field is bodied below -- including the two that FEED the junkyard machine:
    //     GetCarSelectionChangedThisFrame()  -> mbJunkyardSelectionChangedMessageWaiting
    //     GetCarSelectTickerClosedThisFrame()-> mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting
    // (ProcessInputQueue converts both into their one-frame *receivedThisFrame* bits), and
    //     GetEndOfCarSelect() && meJunkyardState != E_JY_INACTIVE -> E_JY_WAITING_FOR_AUDIO.
    // ⚠️ The `<mode>` in the banner's `if ( input->GetEndOfCarSelect() && <mode> ) <mode = 5>`
    //    line is meJunkyardState, and 5 is E_JY_WAITING_FOR_AUDIO -- CORRECTED, it is not a
    //    game mode.
    //
    // ⚠️ STILL GATED (each names a destination this class cannot honestly reach yet):
    //   * the hook-enumeration leg -- BrnDirector::EffectInterface has no reconstructed home;
    //   * HasNewDirectorProfileData's SECOND store, `*(this + 91808) = (data == 1)`, which
    //     lands inside mArbitrator, not the GameState. (Its FIRST store, into
    //     DirectorProfileData +0x08, is an opaque sub-object byte-word -- also left alone.)
    //
    // PostGuiUpdate runs AFTER Update, so every latch here takes effect on the FOLLOWING
    // frame. That is the console's own one-frame delay, not a shortfall.
    // ------------------------------------------------------------------------
    void MainDirector::PostGuiUpdate(const DirectorInputOutput* lpIO)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
        if (lpInput == 0)
            return;

        // GUI command 415 -> the junkyard selection-changed handshake's WAITING bit (+0x1A0).
        if (lpInput->GetCarSelectionChangedThisFrame())
            maGameState.mbJunkyardSelectionChangedMessageWaiting = true;

        if (lpInput->HasGotCrashNavShownEvent())
            maGameState.mbCrashNavShown = true;                       // +0x101
        if (lpInput->HasGotCrashNavHiddenEvent())
            maGameState.mbCrashNavShown = false;

        if (lpInput->HasGotColourCalibrationShownEvent())
            maGameState.mbColourCalibrationShown = true;              // +0x102
        if (lpInput->HasGotColourCalibrationHiddenEvent())
            maGameState.mbColourCalibrationShown = false;

        if (lpInput->HasGotShortcutMenuEvent())
            maGameState.mbShortCutMenuShown = lpInput->GetShortcutMenuState();   // +0x103

        // ⭐ GUI command 192 -- the car-select audio hold. Note the SECOND half of the test:
        // the console only takes this arm while the junkyard is already active, so an
        // end-of-car-select that arrives outside the junkyard is ignored, not a state forge.
        if (lpInput->GetEndOfCarSelect() &&
            maGameState.meJunkyardState != GameState::E_JY_INACTIVE)
        {
            maGameState.meJunkyardState        = GameState::E_JY_WAITING_FOR_AUDIO;   // = 5
            maGameState.mbJunkyardCarModActive = false;
        }

        // @0x82236F88 pseudocode 30..40 -- the GUI's hook enumeration (event 501, copied whole
        // into the input buffer by BridgeGuiToGame) feeds EffectInterface::Update, whose
        // out-flag is the "enumerate again" request; with no enumeration this frame the
        // request is simply "the hooks have not arrived yet".
        if (lpInput->HasGotHookEnumeration())
        {
            const BrnGui::GuiPFXHookEnumeration* lpEnumeration =
                reinterpret_cast<const BrnGui::GuiPFXHookEnumeration*>(lpInput->GetHookEnumeration());
            bool lbRequestEnumeration = false;
            reinterpret_cast<EffectInterface*>(maEffectInterface)->Update(
                lpEnumeration->miHookNameCount, lpEnumeration->mapHookNames, &lbRequestEnumeration);
            maStateFlagTail[E_FLAG_TAIL_REQUEST_HOOK_ENUMERATION] = lbRequestEnumeration ? 1 : 0;
            if (PfxDirDiag())
                *CgsDev::Log::gpDebugPrint << "[pfx-dir] enumeration received: " << lpEnumeration->miHookNameCount
                                           << " hooks, request again " << (lbRequestEnumeration ? 1 : 0) << "\n";
        }
        else
        {
            maStateFlagTail[E_FLAG_TAIL_REQUEST_HOOK_ENUMERATION] =
                reinterpret_cast<const EffectInterface*>(maEffectInterface)->HasGotHooks() ? 0 : 1;
        }

        // GUI command 303 -- the rank-up handshake's WAITING bit + the new rank.
        if (lpInput->GetRankUpThisFrame())
        {
            maGameState.mbRankUpMessageWaiting = true;                // +0x1AA
            maGameState.miRankUpNewRank        = lpInput->GetRankUpNewRank();   // +0x1AC
        }

        if (lpInput->GetStartNewProfileIntro())
            maGameState.mbNewProfileIntroActive = true;               // +0xD8 (GameState +216)

        if (lpInput->GetStartGameIntroFlyby())
            maGameState.mbGameIntroFlybyActive = true;                // +0xD9 (GameState +217)

        if (lpInput->GetStopGameIntroFlyby())
        {
            maGameState.mbGameIntroFlybyActive  = false;
            maGameState.mbNewProfileIntroActive = false;
        }

        // ---- GUI command 475 -- the profile's camera preference. X360 PostGuiUpdate
        // @0x82236F88 pseudocode 64..68: the payload word lands in
        // DirectorProfileData::meCameraMode (GameState +0x1F0) and the shared camera container's
        // mbUseGameplayExternal becomes (payload == 1 == E_CAMERA_MODE_THIRD_PERSON). Landed
        // 2026-09-18 -- until then the director always started in the external view whatever the
        // profile said, and only the in-game view toggle (ArbStateRoaming) ever moved the flag.
        if (lpInput->HasNewDirectorProfileData())
        {
            const s32 liProfileData = lpInput->GetDirectorProfileData();
            maGameState.mDirectorProfileData.meCameraMode =
                static_cast<GameState::ECameraMode>(liProfileData);                        // +0x1F0
            mArbitrator.GetSharedCameras().mbUseGameplayExternal  = (liProfileData == 1);  // +0x166A0
        }

        // ⭐ THE ONLINE POST-EVENT HANDSHAKE, and the fourth producer of the event-state
        // journal. Entering the post-event arms the latch and clears any stale deferral;
        // leaving it either REPLAYS the prepare-for-mode action that arrived while the results
        // were up (the copy HandlePrepareForModeAction parked in maModeActionAndDebugBlock) or,
        // if none did, simply pushes the event state back to ACTIVE.
        if (lpInput->GetEnteredOnlinePostEvent())                     // GUI command 290
        {
            maStateFlagTail[E_FLAG_TAIL_IN_ONLINE_POST_EVENT] = 1;
            maStateFlagTail[E_FLAG_TAIL_MODE_ACTION_DEFERRED] = 0;
        }
        if (lpInput->GetLeftOnlinePostEvent() &&                      // GUI command 294
            maStateFlagTail[E_FLAG_TAIL_IN_ONLINE_POST_EVENT] != 0)
        {
            const bool lbHadDeferredAction = (maStateFlagTail[E_FLAG_TAIL_MODE_ACTION_DEFERRED] != 0);
            maStateFlagTail[E_FLAG_TAIL_IN_ONLINE_POST_EVENT] = 0;
            if (lbHadDeferredAction)
            {
                HandlePrepareForModeAction(
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(maModeActionAndDebugBlock),
                    lpIO);
            }
            else
            {
                maGameState.mEventState.SetCurrent(GameState::E_EVENT_STATE_ACTIVE);
            }
        }

        // GUI command 77 -> the car-unlock-ticker handshake's WAITING bit (+0x1A4).
        if (lpInput->GetCarSelectTickerClosedThisFrame())
            maGameState.mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting = true;

        // GUI commands 480 / 479 -- the online race-intro bar/clamp trio.
        if (lpInput->GetFinishedOnlineEventLoading())
        {
            maGameState.mbOnlineRaceIntroCanUseBars        = true;     // +0x1A9
            maGameState.mbOnlineCarSelectCanStartRaceIntro = true;     // +0x1A7
        }
        if (lpInput->GetStartedOnlineEventLoading())
        {
            maGameState.mbOnlineRaceIntroCanUseBars     = false;
            maGameState.mbOnlineCarSelectMustClampToCar = true;        // +0x1A8
        }

        // GUI commands 469 / 470 -- the 100%-completion sequence latch.
        if (lpInput->GetStarting100PercentSequence())
            maGameState.mbDoing100PercentSequence = true;              // +0x1B0
        if (lpInput->GetFinished100PercentSequence())
            maGameState.mbDoing100PercentSequence = false;
    }
}
