#ifndef GAMESOURCE_DIRECTOR_BRN_MAIN_DIRECTOR_H
#define GAMESOURCE_DIRECTOR_BRN_MAIN_DIRECTOR_H

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameShared/GameClasses/Graphics/CgsCamera.h"   // CgsGraphics::Camera (the embedded +0x349D0 camera)

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
// LAYOUT MODEL -- OPAQUE SIZED BUFFER + ATTESTED OFFSET CONSTANTS.
//
// MainDirector is a ~0x35450-byte aggregate (BrnDirector::DirectorModule constructs it
// in place at module +0xB00 and the next sub-object, the ReplayDirector, at +0x35F50 --
// so the MainDirector body spans exactly 0x35F50 - 0xB00 = 0x35450 bytes). Its interior
// is dominated by heavy embedded sub-objects whose FULL layouts are NOT homed here
// (DirectorDevTools, AllVehicleData, the embedded BehaviourManager aggregate, the Arbitrator
// region, the GameState snapshot, a CgsGraphics::Camera / collision generator, the
// debug-snapshot scratch region). Per the project's host-native-pointer-width convention,
// reproducing the byte-exact console layout member-for-member on the x64 host is neither
// possible nor required (the embedded pointers widen); instead the type is modelled as a
// sized opaque buffer and the handful of fields the RECONSTRUCTED bodies touch are reached
// at their X360-attested CONSOLE byte offsets through a char*/typed view of `this`, exactly
// the established convention in the committed BrnDirectorModule.cpp. The offset CONSTANTS
// below are the X360 4-byte-pointer-build offsets the asm proves; they are provenance, used
// only by this TU's own reconstructed bodies.
//
// FLAG: the embedded BehaviourManager aggregate (CONSOLE +0x1CB10 ctor region / +0x1CB90
//   Construct region) has NO homed layout (the committed BrnBehaviourManager.h is forward-
//   decl-only). Every MainDirector method that indexes the behaviour-manager aggregate is
//   DECLARATION-ONLY + FLAGGED below -- never raw-offset-poked.
//
// FLAG: this class is NON-POLYMORPHIC -- the X360 ctor installs the embedded sub-objects'
//   vtables but stores NO vtable pointer at MainDirector +0x00 (DirectorDevTools occupies
//   the head; the lifecycle methods are called directly by DirectorModule, not virtually).
//   No base pure virtuals to override; the type is trivially instantiable.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    class ICEWrapper;
    class Arbitrator;
    struct DirectorInputOutput;   // GameSource/Director/DirectorModule/BrnDirectorInputOutput.h
    namespace DirectorIO { struct OutputBuffer; }   // the director output buffer Prepare threads through

    class MainDirector
    {
    public:
        // ---- X360-attested CONSOLE byte offsets (provenance for this TU's bodies) -------
        // Offsets into the MainDirector aggregate the reconstructed Release / Destruct / ctor
        // bodies address. CONSOLE (4-byte-pointer) offsets; used as documented byte offsets
        // through a char*/typed view of `this` (the BrnDirectorModule.cpp convention), never
        // as a host-layout cast.
        enum : u32
        {
            // The embedded ICE wrapper (BrnDirector::ICEWrapper) -- ctor's first sub-build,
            // Prepare/Destruct dispatch into it. CONSOLE +0x50.
            KU_OFF_ICE_WRAPPER          = 0x50,
            // The embedded CgsGraphics::Camera / collision-generator object Construct builds
            // (sub_827F94E8 + SetFovHorizontal) and Destruct/Prepare tear down via
            // BaseCollisionGenerator::Destruct. CONSOLE +0x349D0 (= 215504).
            KU_OFF_COLLISION_GENERATOR  = 0x349D0,
            // The staged-init RELEASE-stage counter Release/Prepare read (the +0x35420 word).
            // CONSOLE +0x35420 (= 218144).
            KU_OFF_STAGE_COUNTER        = 0x35420,
            // The staged-init STAGE word Release writes 1/2/3/5 into / Construct sets to 5 /
            // Prepare clears to 0. CONSOLE +0x35424 (= 218148).
            KU_OFF_STAGE                = 0x35424,
            // The construct-timestamp double Construct stores the incoming time into.
            // CONSOLE +0x35428 (= 218152).
            KU_OFF_CONSTRUCT_TIME       = 0x35428,
            // Four pointer-list/tree heads Destruct nulls (64-bit zero stores into the
            // per-frame bookkeeping list heads inside the embedded sub-objects). CONSOLE byte
            // offsets, asm-attested by the Destruct std stores.
            KU_OFF_LISTHEAD_A           = 0x1CAB8,   // (= 117432)
            KU_OFF_LISTHEAD_B           = 0x24848,   // (= 149576)
            KU_OFF_LISTHEAD_C           = 0x2C5B8,   // (= 181688)
            KU_OFF_LISTHEAD_D           = 0x2F038,   // (= 192568)
        };

        // ---- ctor (X360 0x827E4AB8, EXECUTED in goal trace) ----------------------------
        // Build the director: in-place-construct the owned sub-objects (ICEWrapper, ICETake,
        // ArbitratorStateContainer, BehaviourManager, CarScoreData) and seed the handful of
        // -1 sentinel index fields the asm stores. Members are reached at their attested
        // offsets through a typed view of `this` (BrnDirectorModule.cpp convention). The
        // embedded sub-object constructors run as direct calls at their attested regions.
        MainDirector();

        // ---- staged-init lifecycle -----------------------------------------------------

        // X360 0x8225B448 (EXECUTED in goal trace). Construct the runtime: build the
        // dev-tools, the CgsGraphics::Camera, the director Camera, the AllVehicleData, the
        // BehaviourManager, the MomentParameterBank and the Arbitrator, seed the camera-shake
        // RNG slots, and Clear the GameState.
        //
        // RECONSTRUCTED (director wave) for the staged-init words + BOTH CAMERAS -- notably
        // Camera::Camera::Construct on mLastCamera (+0x32F10), without which Update would
        // publish uninitialised storage on the first frame. The rest is a documented quiet
        // gate: the body also constructs / indexes the NOT-HOMED BehaviourManager,
        // AllVehicleData, DirectorDevTools, MomentParameterBank, KeyAnimShakeController,
        // ShotSelector, GameState and DebugPrinter aggregates and runs a multi-stage VMX + LCG
        // camera-shake-seed pipeline (vspltisw128 / the 1284865837-multiplier LCG poking 4
        // float slots), which the rules forbid paraphrasing to scalar. See the .cpp banner --
        // it also records why ICEWrapper::Construct and Arbitrator::Construct are held back
        // for a HOST-SIZE reason rather than a homing one.
        void Construct(const class DirectorResourceManager* lpResourceManager, f32 lfTime);

        // X360 0x8224FB38. Staged Prepare state machine (the per-stage init the boot loop
        // pumps): tear the collision generator down, register the dev-tools GameTalk handler,
        // Prepare the ICE wrapper, Prepare the behaviour manager, seed the 20-entry
        // behaviour-helper index table, then mark the runtime ready (stage word -> 0).
        // Returns true once the last stage completes. RECONSTRUCTED in the .cpp with its two
        // un-homed stages (GameTalk registration, BehaviourManager) individually gated.
        //
        // Signature recovered from the only call site, DirectorModule::Prepare @0x822712D8:
        //     MainDirector::Prepare( this+2816, <director OUTPUT buffer>, <s32 forwarded>,
        //                            <the module's DirectorResourceManager> )
        // -- the last two are handed straight on to ICEWrapper::Prepare.
        bool Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg,
                     const class DirectorResourceManager* lpResourceManager);

        // ADDITIVE (director wave). The embedded CgsGraphics::Camera at CONSOLE +0x349D0 --
        // the camera MainDirector::Update fills (via Camera::Camera::CopyToCgsCamera) and
        // publishes with DirectorIO::OutputBuffer::SetCgsCamera, and which
        // DirectorModule::Update then copies into the module's own mCgsCamera
        // (`CgsGraphics::Camera::operator=(module+231824, module+218320)` -- module+218320 is
        // exactly this+0x349D0). Exposed by NAME so DirectorModule never reaches into this
        // class's opaque storage by offset. Body in the .cpp (an attested-offset typed view,
        // the same idiom as GetICEWrapper / GetArbitrator above).
        //
        // NOTE the offset does double duty and that is the binary's doing, not a slip: the
        // same region is torn down through CgsCollision::BaseCollisionGenerator::Destruct by
        // Destruct/Prepare (see KU_OFF_COLLISION_GENERATOR) -- the graphics camera and the
        // collision generator are one embedded aggregate on this build.
        CgsGraphics::Camera&       GetCgsCamera();
        const CgsGraphics::Camera& GetCgsCamera() const;

        // X360 0x82236EB0. Staged Release state machine: walk the release stages, tearing the
        // collision generator down on the first stage and clearing the stage counter on the
        // last. Returns true once the final stage completes. RECONSTRUCTED in the .cpp (it
        // touches only the attested stage word / counter and one committed destructor call).
        bool Release();

        // X360 0x8224FCC0. Tear the director down: null the four per-frame bookkeeping list
        // heads, destruct the embedded CgsGraphics::Camera / collision generator, then destruct
        // the ICE wrapper. RECONSTRUCTED in the .cpp.
        void Destruct();

        // ---- per-frame Update spine ----------------------------------------------------

        // X360 0x82274070. The director's per-frame tick: snapshot the debug camera info,
        // pump the AttribSys-driven behaviour parameters, run the arbitrator, advance the ICE
        // wrapper, update the moment controller, and post the debug printers / behaviour
        // updates. Calls (in asm order): UpdateDebugInfo, UpdateAttribSys, UpdateArbitrator
        // (x2 branches), UpdateICE, UpdateMoments, UpdateCameraBehavioursPostScene,
        // DebugDisplayCurrentCamera, UpdateDebugPrinters.
        //
        // DECLARATION-ONLY + FLAG: the spine is a deep VMX-dominated dispatch over the
        //   NOT-HOMED behaviour-manager / camera-effect aggregates and the decl-only sub-updates
        //   below; the rules forbid paraphrasing it. Declaration-only.
        void Update(const DirectorInputOutput* lpIO);

        // X360 0x8225BA00. Pre-scene-query update: fill in the shared info, process the input
        // queue, then run the pre-scene camera-behaviour pass. RECONSTRUCTED in the .cpp down
        // to (and including) its whole-body player-car guard; the guarded steps are a
        // documented quiet gate -- see the .cpp banner.
        void PreSceneQueryUpdate(const DirectorInputOutput* lpIO);

        // X360 0x82236F88. Post-GUI update. Reconstructed in the .cpp as a DOCUMENTED QUIET
        // GATE (un-homed EffectInterface + the un-named GameState latch region).
        void PostGuiUpdate(const DirectorInputOutput* lpIO);

        // The shared "is there a live player car this frame" predicate both Update and
        // PreSceneQueryUpdate open with (the X360 inlines the same sequence into both): the
        // input buffer's player-car index, overridden by this director's own forced-camera-car
        // index when that one is a used race car, then tested against the used-race-car mask.
        // De-inlined to one named helper so neither body re-derives it. Body in the .cpp.
        bool IsPlayerCarLive(const DirectorInputOutput* lpIO) const;

        // ---- Update sub-steps (all DECLARATION-ONLY + FLAGGED) -------------------------

        // X360 0x82271120. Build the per-frame ArbStateSharedInfo from the director's members
        // and drive Arbitrator::Update with it.
        // DECLARATION-ONLY + FLAG: builds the ~0x60-byte shared-info from raw member offsets and
        //   calls the committed-but-DECLARATION-ONLY Arbitrator::Update (its body is itself
        //   declaration-only in BrnDirectorArbitrator.h until the camera/effect TUs land).
        void UpdateArbitrator(const DirectorInputOutput* lpIO, bool lbCycleCamera,
                              bool lbCycleCameraHeld);

        // X360 0x82238FC0. Advance the embedded ICE wrapper for this frame.
        // DECLARATION-ONLY + FLAG (ICE wrapper Update cone + per-frame reference-space build).
        void UpdateICE(const DirectorInputOutput* lpIO);

        // X360 0x82250268. Pump the moment controller.
        // DECLARATION-ONLY + FLAG (moment-controller / behaviour-manager dependent).
        void UpdateMoments(const DirectorInputOutput* lpIO, s32 liArg);

        // X360 0x8221AFD0. Pull the per-frame camera-behaviour parameters out of the AttribSys
        // (camerabumperbehaviour / cameraexternalbehaviour) and Set them on the gameplay
        // behaviours.
        // DECLARATION-ONLY + FLAG: depends on the [todo] Attrib::Gen generated behaviour
        //   parameter types and raw-offset-pokes the un-homed behaviour-parameter aggregates.
        void UpdateAttribSys(const DirectorInputOutput* lpIO);

        // X360 0x82255318 / 0x8224FD30. The pre/post-scene camera-behaviour passes.
        // DECLARATION-ONLY + FLAG (behaviour-manager-dependent; VMX-dominated).
        void UpdateCameraBehavioursPreScene(const DirectorInputOutput* lpIO, s32 liArg);
        void UpdateCameraBehavioursPostScene(const DirectorInputOutput* lpIO, s32 liArg);

        // X360 0x822372F8. Drain the director input event queue and apply each action to the
        // GameState snapshot (intro/flyby/drive-thru/take-down/mode actions).
        // DECLARATION-ONLY + FLAG: dispatches a deep per-event switch over the un-homed
        //   GameState action region + BrnGameState IO accessors + behaviour-manager state.
        void ProcessInputQueue();

        // X360 0x8221A6B0. Apply the per-frame new-vehicle events to the AllVehicleData tracker.
        // DECLARATION-ONLY + FLAG (AllVehicleData-dependent; not homed).
        void ProcessNewVehicleEvents(const DirectorInputOutput* lpIO);

        // X360 0x8221B0B0. Handle a "prepare for mode" director action.
        // DECLARATION-ONLY + FLAG (GameState action region + behaviour-manager dependent).
        void HandlePrepareForModeAction(s32 liArg1, s32 liArg2, s32 liArg3, s32 liArg4,
                                        s32 liArg5, s32 liArg6);

        // X360 0x8221A3A8. Compute the traffic-light reference space for the current event.
        // DECLARATION-ONLY + FLAG: a multi-stage VMX pipeline (lvlx/vspltw128/lvx128 across
        //   player transform + traffic data) the rules forbid paraphrasing to scalar.
        void CalcTrafficLightSpace(s32 liArg2, s32 liArg3, s32 liArg4, s32 liArg5, s32 liArg6,
                                   s32 liArg7, s32 liArg8, s32 liArg9, s32 liArg10, s32 liArg11,
                                   s32 liArg12, s32 liArg13, s32 liArg14);

        // ---- debug helpers (all DECLARATION-ONLY + FLAGGED) ----------------------------

        // X360 0x82209128. Snapshot the current camera's transform/FOV/near-clip into the debug
        // scratch region.
        // DECLARATION-ONLY + FLAG: copies via a VMX move and selects the near-clip from the
        //   un-dumped rodata floats flt_82CDA55C / flt_82CDA560 (VALUES not recovered) -- the
        //   same un-recovered near-clip constants the committed Camera.h FLAGS.
        void UpdateDebugInfo(const DirectorInputOutput* lpIO);

        // X360 0x82208FC8. Drive the debug printers.
        // DECLARATION-ONLY + FLAG (debug-printer / behaviour-manager dependent).
        void UpdateDebugPrinters();

        // X360 0x821F6740. Render the current-camera debug overlay.
        // DECLARATION-ONLY + FLAG (debug-render + behaviour-manager dependent).
        void DebugDisplayCurrentCamera(s32 liArg);

        // ---- committed sub-object accessors --------------------------------------------

        // The embedded ICE wrapper (the GetICEWrapper the committed dev-tools consumer reaches
        // by name). Returns the sub-object at the attested CONSOLE +0x50 region.
        ICEWrapper& GetICEWrapper();

        // The embedded camera arbitrator (CONSOLE +0x12DC0 region). Reached by name by the
        // Update spine; the body lives in the .cpp (an attested-offset view of `this`).
        Arbitrator& GetArbitrator();

    private:
        // ---- opaque sized storage (X360 0x35450 bytes) ---------------------------------
        // The whole director aggregate as an opaque buffer. The reconstructed bodies address
        // their fields at the attested offsets above through a typed view of this buffer; the
        // remaining interior (the heavy un-homed sub-objects) is preserved as raw storage so
        // the type stays the right size and instantiable. alignas(16) for the SIMD members the
        // interior carries (transforms / camera blocks).
        alignas(16) u8 maStorage[0x35450];
    };

    // Pin the one pointer-invariant fact this TU stands on: the total size matches the
    // DirectorModule placement window (next sub-object at module +0x35F50, this one at +0xB00).
    // The interior member offsets quoted above are the CONSOLE offsets (pointer-invariant for
    // these particular fields -- all stage scalars / list heads, no pointer-width-sensitive
    // member straddles them); parity for the un-homed interior is by role, not by byte.
    static_assert(sizeof(MainDirector) == 0x35450,
                  "BrnDirector::MainDirector must span the DirectorModule placement window");
}

#endif // GAMESOURCE_DIRECTOR_BRN_MAIN_DIRECTOR_H
