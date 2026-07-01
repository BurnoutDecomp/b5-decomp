#ifndef BRN_DIRECTOR_MODULE_H
#define BRN_DIRECTOR_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"        // CgsModule::ModuleSingleBuffered base
#include "GameSource/Director/BrnDirectorICEWrapper.h"                    // BrnDirector::ICEWrapper (DirectorResourceManager needs the full type)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h" // BrnDirector::DebugComponent (mDebugComponent, real committed type)
#include "GameSource/Director/BrnDirectorResourceManager.h"               // BrnDirector::DirectorResourceManager (mDirectorResourceManager)
#include "GameSource/Director/BrnMainDirector.h"                          // BrnDirector::MainDirector (mMainDirector)
#include "GameSource/Director/Camera/Camera.h"                            // BrnDirector::Camera::Camera (mCamera)
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"                   // BrnReplays::BaseSerialiser (mDirectorSerialiser)

// ============================================================================
// GameSource/Director/BrnDirectorModule.h
//
// BrnDirector::DirectorModule -- the top-level director engine module (a
// CgsModule::ModuleSingleBuffered derivative, as declared at the BrnGameModule member
// site). DWARF home: GameSource/Director/DirectorModule/BrnDirectorModule.h. It owns
// the debug component, the resource manager, the top-level MainDirector, a replay
// serialiser, the active Camera, plus a very large fleet of gameplay/camera/debug
// sub-objects (WorldMap, Arbitrator, MomentController, BehaviourManager, GameState,
// VehicleTracker, ...) that the 3 functions reconstructed in this wave never touch.
//
// LAYOUT MODEL -- NAMED MEMBERS AT ASM-PROVEN OFFSETS, PADDING FOR THE REST.
//
// This class was previously reconstructed (and reverted after fresh-eyes review) as one
// opaque `alignas(16) u8 maStorage[233000];` buffer accessed everywhere through raw
// reinterpret_cast offset pokes -- forbidden by AGENTS.md's "NO RAW OFFSET POINTER
// HACKS" rule for an in-memory engine object. This version instead follows the
// established BrnGuiCache.h / BrnWorldModule.h convention: every field the 3 bodied
// functions (Construct/Destruct/Release) actually touch is a NAMED member (its own real
// committed type where one exists -- DebugComponent, DirectorResourceManager,
// MainDirector, Camera::Camera, BaseSerialiser -- else a scalar/scratch buffer at the
// asm-proven byte offset), and every gap between named members the bodied functions
// never reach is an explicit `u8 mPad_0xNNNN[SIZE]` buffer. Per BrnWorldModule.h's
// precedent for the same "huge object, 3-of-many functions reconstructed" situation,
// this header does NOT attempt to model (or size-pin) the WHOLE ~0x38F00-byte object:
// only the touched members, in ascending asm-proven offset order, with padding filling
// the measured gaps between them. The untouched tail past the last function-touched
// field (miPerfCount_PostGuiUpdate @0x38B0C / meReleaseStage @0x38B14) is intentionally
// ABSENT from this slice -- the object's true total size is not attested by any function
// reconstructed so far, so no static_assert(sizeof(DirectorModule)==...) is made here
// (matching BrnWorldModule.h, which explicitly declines to pin a size it cannot prove).
//
// OFFSETS quoted in comments below are X360 CONSOLE (4-byte-pointer) byte offsets --
// provenance for the reconstructed .cpp bodies, not host layout facts (the real
// sub-objects such as DirectorResourceManager/MainDirector are wider on this 64-bit
// host; access is by name, never by offsetof-against-console-numbers).
//
// TWO UNREPRODUCED SIDE EFFECTS (flagged rather than raw-offset-poked into another
// class's private/opaque storage -- see the per-member comments below):
//   * Construct's inlined DirectorResourceManager::Construct() body (an
//     EventReceiverQueue<512,16>::Construct() per the DecFIGS DWARF dump for
//     BrnDirectorResourceManager.cpp) is called via mDirectorResourceManager.Construct(),
//     but the currently-committed BrnDirectorResourceManager.h version of that method is
//     `inline void Construct() {}` (a stub) -- the queue-init side effect is not
//     reproduced. That gap belongs to the resource-manager TU.
//   * Construct stores &mDebugComponent into a word at this+0xB40 -- inside
//     MainDirector's own placement region (MainDirector spans this+0xB00..+0x35F50,
//     confirmed by BrnMainDirector.h's own static_assert(sizeof(MainDirector)==0x35450)
//     placement-window pin), i.e. MainDirector's own +0x40. MainDirector is modelled as
//     an opaque sized buffer in BrnMainDirector.h with no exposed field for this; raw-
//     poking into ANOTHER class's opaque storage from this TU would repeat exactly the
//     violation this reconstruction is fixing. Left unreproduced and flagged in the .cpp.
//
// NOTE: a SEPARATE, already-landed TU (GameSource/Director/BrnDirectorModule.cpp, the
// real `DirectorModule::DirectorModule()` constructor) defines its OWN small,
// TU-private/local `struct DirectorModule` and does not include this header -- the two
// coexist under this project's per-TU `cl /c` compile gate (no link step), exactly like
// every other declaration-only external stub in this codebase. Do not conflate that
// file's ctor with the Construct()/Destruct()/Release() staged-lifecycle methods
// reconstructed here.
// ----------------------------------------------------------------------------

namespace BrnDirector
{

class DirectorModule : public CgsModule::ModuleSingleBuffered
{
public:
    // ---- staged Prepare/Release stage enums (DWARF BrnDirectorModule.h:136 / :149) ----
    enum EPrepareStage
    {
        E_PREPARESTAGE_START                = 0,
        E_PREPARESTAGE_RESOURCES            = 1,
        E_PREPARESTAGE_ICE                  = 2,
        E_PREPARESTAGE_WORLDMAP             = 3,
        E_PREPARESTAGE_MANAGER              = 4,
        E_PREPARESTAGE_BEHAVIOUR_CONTROLLER = 5,
        E_PREPARESTAGE_MOMENT_CONTROLLER    = 6,
        E_PREPARESTAGE_ARBITRATOR           = 7,
        E_PREPARESTAGE_DONE                 = 8,
    };

    enum EReleaseStage
    {
        E_RELEASESTAGE_START                = 0,
        E_RELEASESTAGE_ARBITRATOR           = 1,
        E_RELEASESTAGE_MOMENT_CONTROLLER    = 2,
        E_RELEASESTAGE_BEHAVIOUR_CONTROLLER = 3,
        E_RELEASESTAGE_MANAGER              = 4,
        E_RELEASESTAGE_WORLDMAP             = 5,
        E_RELEASESTAGE_DONE                 = 6,
    };

    // ---- staged runtime lifecycle (NOT the C++ ctor -- see NOTE above) --------------

    // X360 0x8225C590 (EXECUTED in goal trace). Chains to the ModuleSingleBuffered base
    // Construct, builds the debug component, resource manager and camera, seeds the
    // prepare/release stage words + five handle/index sentinels, constructs the
    // top-level MainDirector and the director replay serialiser, and registers three
    // CgsDev::PerfMonCpu monitors (Pre SQ Update / Main Update / Post Gui Update).
    // Reconstructed in the .cpp (BODIED).
    void Construct(f32 lfTime);

    // X360 0x82239198 (recovered; not executed in goal trace). Staged RELEASE state
    // machine mirroring MainDirector::Release's shape: release the owned MainDirector,
    // then the ModuleSingleBuffered base, advancing the release-stage word at each step.
    // Reconstructed in the .cpp (BODIED) -- both callees (MainDirector::Release,
    // ModuleSingleBuffered::Release) are landed.
    bool Release();

    // X360 0x82250D98 (recovered; not executed in goal trace). Destructs the owned
    // MainDirector, then the ModuleSingleBuffered base. Reconstructed in the .cpp
    // (BODIED) -- both callees are landed.
    void Destruct();

    // ---- staged Prepare / per-frame Update spine (DECLARATION-ONLY + FLAGGED) -------
    // All four remaining recovered functions reach un-landed callees (MainDirector::
    // Prepare/Update/PreSceneQueryUpdate are themselves declaration-only in
    // BrnMainDirector.h; DirectorResourceManager::Prepare's body isn't linked yet,
    // BrnDirector::WorldMap has no homed type at all, BrnReplays::DirectorSerialiser /
    // BrnDirector::ReplayDirector / BrnDirector::InertiaController /
    // BrnDirector::KeyAnimShakeController are all [todo]) -- per AGENTS.md the rules
    // forbid bodying a function whose callee graph isn't resolved. Declaration-only,
    // matching the sibling DebugComponent TU's convention for its own blocked functions.

    // X360 0x822712D8 (recovered; not executed in goal trace). Staged PREPARE state
    // machine (register debug component, base Prepare, resource-manager Prepare,
    // WorldMap::LoadData, MainDirector::Prepare). BLOCKED: WorldMap has no homed type.
    bool Prepare(s32 liIOArg, s32 liUnused);

    // X360 0x82275300 (recovered; not executed in goal trace). Per-frame Update spine
    // (ProcessSceneQueryResults, MainDirector::Update / ReplayDirector::Update,
    // replay-serialiser bookkeeping, CgsGraphics::Camera::operator=). BLOCKED:
    // ReplayDirector / BrnReplays::DirectorSerialiser are not homed.
    s32 Update(s32 liIOArg1, s32 liIOArg2, s32* lpIOArg3, s32 liIOArg4, s32* lpIOArg5, s32 liIOArg6);

    // X360 0x8225C768 (recovered; not executed in goal trace). Pre-scene-query update
    // (SceneQueryInterface::Clear, MainDirector::PreSceneQueryUpdate /
    // ReplayDirector::PreSceneQueryUpdate). BLOCKED: same un-homed dependents as Update.
    s32 PreSceneQueryUpdate(s32 liIOArg1, s32 liIOArg2, s32 liIOArg3, s32 liIOArg4, s32 liIOArg5, s32 liIOArg6);

private:
    // ====================================================================
    //  DATA LAYOUT -- named members at asm-proven `this+offset` (X360 CONSOLE bytes),
    //  gaps reserved with explicit padding (AGENTS.md "LAYOUT RECOVERY WITH PADDING",
    //  the BrnGuiCache.h / BrnWorldModule.h convention). Only members the reconstructed
    //  Construct/Destruct/Release bodies touch are named; this is an intentionally
    //  MINIMAL SLICE of the real ~0x38F00-byte object (BrnWorldModule.h precedent) --
    //  the untouched fleet of gameplay sub-objects (WorldMap, Arbitrator,
    //  MomentController, BehaviourManager, GameState, VehicleTracker, the ~40 debug
    //  toggles, ...) is NOT modelled here and the object's true total size is NOT
    //  pinned by a static_assert.
    //
    //  this+0x000 / this+0x004: CgsModule::Module's own vptr + `mbIsNewModule` live in
    //  the inherited base -- NOT redeclared here. Construct's asm sets mbIsNewModule =
    //  true as its very last store; reached through the inherited protected member by
    //  name (no local field needed).
    // ====================================================================

    // NOTE ON PADDING SIZES BELOW: this class embeds several already-homed sub-object
    // types (DebugComponent/DirectorResourceManager/MainDirector/Camera::Camera/
    // BaseSerialiser) whose HOST (x64) sizeof is wider than their X360 CONSOLE size (the
    // widened embedded pointers -- exactly the situation AGENTS.md's x64-gate convention
    // expects: parity is by name, not by byte, and the compiler places each subsequent
    // named member sequentially regardless of the true host/console gap). The u8[N]
    // padding buffers below are sized from the CONSOLE byte deltas between asm-proven
    // offsets purely as ORGANISATIONAL documentation of "how much untouched material sits
    // here on the original console layout" -- they are not, and cannot be, claimed to
    // reproduce the true host byte gap (which is unrecoverable/inapplicable on x64).

    u8 mPad_0x008[0x228 - 0x008];                           // +0x008..+0x227 (untouched)

    // this+0x228 (552). DebugComponent::Construct(this+0x228, this) -- the module's
    // in-game debug menu component (registered as "Camera"). Real committed type.
    DebugComponent mDebugComponent;

    // this+0x248 (584). DirectorResourceManager region. MainDirector::Construct's own
    // declared signature (BrnMainDirector.h) takes `const DirectorResourceManager*`, and
    // the DirectorModule ctor (separate TU) places the resource manager at this same
    // console offset -- both independently corroborate it. See the header-level FLAG
    // above: the queue-init side effect the X360 binary performs here (inlined from
    // DirectorResourceManager::Construct) is not reproduced by the currently-committed
    // (stub) DirectorResourceManager::Construct().
    DirectorResourceManager mDirectorResourceManager;

    u8 mPad_0x8A8[0xB00 - 0x8A8];                           // +0x8A8..+0xAFF (console gap:
    // ShotSelector / CrashAnalyser / CameraFinaliser / the 6 SceneQueryInterface post-
    // office members / WorldMap / AllVehicleData, plus a handful of scattered words
    // Construct zeroes inside this span that are not attributable to a specific
    // DWARF-named/homed member -- none of the touched material here has a real homed
    // type to declare by name, so it is not modelled individually. NOTE: Construct's
    // asm also calls two behavioural helpers whose target sub-objects live in this span
    // -- CgsModule::BaseEventReceiverQueue::Clear(this+0x908) and a CgsArray-style
    // Clear helper (sub_8221CC98, this+0x9A4) -- these are real side effects, not just
    // zero-stores, and are NOT reproduced here (same "no real homed type yet" reason).)

    // this+0xB00 (2816). BrnDirector::MainDirector::Construct(&mDirectorResourceManager,
    // lfTime) / ::Destruct() / ::Release() -- the top-level cinematic camera director.
    // Real committed type (BrnMainDirector.h/.cpp); MainDirector's own static_assert
    // pins it at exactly 0x35450 CONSOLE bytes (the DirectorModule placement window
    // this+0xB00 .. this+0x35F50), which is the evidence mCamera below sits at CONSOLE
    // +0x36380 overall. See the header-level FLAG above: the this+0xB40 back-pointer
    // store (inside MainDirector's own opaque storage) is not reproduced here.
    MainDirector mMainDirector;

    u8 mPad_0x35F50[0x36380 - 0x35F50];                     // CONSOLE +0x35F50..+0x3637F --
    // the Arbitrator / MomentController / BehaviourManager / Random / mLastCamera /
    // rotation controllers span; none of these are touched by Construct/Destruct/Release.

    // this+0x36380 (222080). BrnDirector::Camera::Camera::Construct() -- the module's
    // active/current camera. Real committed type (Camera.h/.cpp).
    Camera::Camera mCamera;

    u8 mPad_0x36380_tail[0x38924 - 0x36380];                // CONSOLE tail after the Camera span
    // -- CameraDebugInfo / debug-toggle bytes / DebugLog + 3x DebugPrinter / GameState /
    // VehicleTracker / EffectInterface -- untouched by Construct/Destruct/Release.

    // this+0x38924 (231716): one zeroed byte Construct stores just ahead of the replay
    // serialiser (asm: this+0x35F50+0x29D4, i.e. this+0x36380 region's own +0x29D4 from
    // the r11 base used by that store -- see the .cpp for the exact asm derivation). Not
    // a DWARF-named/confidently-typed member (candidates per role are
    // mbShowDebugCameraNames / mbDebugAssertNoIllegalSlomo, but DWARF's declared relative
    // order places both of those BEFORE mCamera, which contradicts this offset being
    // AFTER mCamera -- so neither name is asserted here).
    u8 mZeroedByte_0x38924;                                  // +0x38924

    u8 mPad_0x38925[0x38930 - 0x38925];                      // +0x38925..+0x3892F (untouched)

    // this+0x38930 (231728). BrnReplays::BaseSerialiser::Construct(this, 4, 0, 1024, 384,
    // "DirectorModule", 0) -- the director's own replay serialiser record (DWARF calls
    // the derived type BrnReplays::DirectorSerialiser; that derived type is not homed
    // yet, so this is declared at its real touched base type). Real committed base type.
    // Prepare/Update (declaration-only) reach the same field via
    // BrnReplays::DirectorSerialiser::GetStaticLayout(this+231728) -- same offset,
    // corroborating this placement.
    BrnReplays::BaseSerialiser mDirectorSerialiser;

    // this+0x3898C (231820): one zeroed byte immediately after mDirectorSerialiser's
    // BaseSerialiser-sized (0x5C CONSOLE bytes) tail -- a DirectorSerialiser-added field;
    // that derived type is not homed, so this is named as an explicit scratch byte rather
    // than folded into BaseSerialiser itself.
    u8 mDirectorSerialiserExtra_0x3898C;                     // +0x3898C

    u8 mPad_0x3898D[0x38B00 - 0x3898D];                      // +0x3898D..+0x38AFF (untouched)

    // this+0x38B00 / +0x38B01 (232192 / 232193): two adjoining zeroed bytes right before
    // the perf-monitor handles. Not confidently attributable to a specific DWARF bool
    // (many `bool mbXxx` candidates exist in the DWARF list but none is asm-proven at
    // this exact pair of offsets).
    u8 mZeroedByte_0x38B00;                                  // +0x38B00
    u8 mZeroedByte_0x38B01;                                  // +0x38B01

    u8 mPad_0x38B02[0x38B04 - 0x38B02];                      // +0x38B02..+0x38B03 (untouched)

    // this+0x38B04 / +0x38B08 / +0x38B0C (232196 / 232200 / 232204). The three
    // CgsDev::PerfMonCpu monitor handles Construct registers (DWARF-named, adjacent,
    // ascending order -- BrnDirectorModule.h:329-331).
    s32 miPerfCount_PreSQUpdate;                             // +0x38B04
    s32 miPerfCount_MainUpdate;                              // +0x38B08
    s32 miPerfCount_PostGuiUpdate;                            // +0x38B0C

    // this+0x38B10 / +0x38B14 (232208 / 232212). The staged prepare/release stage words
    // (DWARF-named, adjacent, ascending order -- BrnDirectorModule.h:291-292). Construct
    // seeds mePrepareStage=E_PREPARESTAGE_START and meReleaseStage to the literal value 4
    // (asm-proven; NOT E_RELEASESTAGE_DONE=6 -- see the .cpp comment on Release for the
    // same literal-vs-enum-value note).
    EPrepareStage mePrepareStage;                            // +0x38B10
    EReleaseStage meReleaseStage;                             // +0x38B14

    // Everything past this point (the ~0x400-byte-per-instance debug/perf tail plus the
    // untouched gameplay-object fleet noted above) is intentionally ABSENT from this
    // minimal slice -- see the header-level note on why no total-size static_assert is
    // made.
};

}

#endif
