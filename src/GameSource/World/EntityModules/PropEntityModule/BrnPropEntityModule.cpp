// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//
// BrnWorld::PropEntityModule -- the LIFECYCLE half of the prop entity module.
// Landed 2026-08-12 (prop-spawn wave, phase 2). Until this TU existed the module's
// Construct was an EMPTY stub in WorldLinkStubs.cpp, which is why the shipped PC build
// had zero props in the world: Construct is the only path to
// PropZoneManager::Construct, and that is the only writer of
// mauStartIndexOfZone[0..499] = KU_UNLOADED_ZONE. With the module zero-initialised
// instead, IsZoneLoaded() answered "true" for every zone, so no zone ever streamed in.
//
// Functions in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::Construct                       @0x822FA068  (308 insns)
//   PropEntityModule::ConstructPreScenePerfMonitors   @0x822A90A0  ( 94 insns)
//   PropEntityModule::ConstructPostPhysicsPerfMonitors@0x822A9218  ( 56 insns)
//   PropEntityModule::Prepare                         @0x82306DB8  (124 insns)
//   PropEntityModule::InitializePropPhysicsData       @0x822DA840  (510 insns)
//   PropEntityModule::Release                         @0x822A92F8  ( 41 insns)
//   PropEntityModule::CachePropGraphicsLists          @0x822DBF28  ( 57 insns)
//   PropEntityModule::_AssertLayout                   (host tripwires; never called)
//
// PARKED (declared in the header, no body here):
//   PropEntityModule::Destruct -- NOT EMITTED in the ARTIST image. There is no
//     `BrnWorld::PropEntityModule::Destruct` symbol in the export at all (the DWARF
//     declares one; the X360 compiler folded or elided it). Inventing a teardown order
//     would be fabrication, so the declaration stands bodiless and the eventual link
//     takes the WorldLinkStubs trap.
//
// LAYOUT DISCIPLINE: not one console byte offset appears in the code below. Everything
// is reached by named member / named accessor. The offset -> member mapping that made
// that possible is documented once, in the header banner, with its asm provenance.
// ============================================================================

#include "BrnPropEntityModule.h"

#include "BrnPropEntityModuleIO.h"                       // PropEntityIO::OutputBuffer_Prepare
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"             // CgsSceneManager::EntityId
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // NULLResourceHandle
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h" // PropInputInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"        // RequestInterface<1024>
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"              // LoadGameDataEvent (the reply)
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"        // PropPhysicsDataHeader
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"          // PropTypeData / PropPartTypeData
#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"             // PropGraphicsList

#include <cstddef>   // offsetof (the _AssertLayout pins)

// includes folded in from the PropEntityModule_w*.cpp partfiles (2026-09-15)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"   // PropEntityIO::OutputBuffer_PreScene
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"  // EPropState
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Graphics/BrnCoronaManager.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"
#include "SharedClasses/Physics/Props/BrnPropConstants.h"        // KVF_PROP_FLOOR
#include "rw/math/vpu/vector3_operation.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([DIAG] BRN_PROP_DIAG)
#include <stdlib.h>                                          // getenv    ([DIAG] BRN_PROP_DIAG)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // PotentialContact
#include "GameSource/World/BrnEntityTypes.h"                            // E_ENTITYTYPE_*
#include "rw/math/vpu/matrix44affine_operation.h"  // rw::math::vpu::OrthoNormalize3x3 @0x82203B28
#include <cmath>                                   // std::floor (the vrfin round-to-nearest)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"   // ResetOnTrackResult
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"          // maTypes / GetPropTransform
#include "GameSource/Replays/BrnReplayArray.h"                                    // BrnReplayArray<T,N>::operator[](u8)
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // CrashIO::RaceCarCrashCompleteEvent
#include "SharedClasses/Physics/Props/PropRespawnTypesEnum.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"
#include "GameSource/GameState/BrnGameEvents.h"                                  // RecordPropHitEvent
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include <math.h>                                                                // fabsf (the vandc sign-mask)

namespace BrnWorld
{
    namespace
    {
        // ------------------------------------------------------------------
        // ⭐ 2026-08-12 (prop-BOOT wave, agent B8): these three used to reinterpret_cast the
        // prepare-phase output buffer's opaque byte spans to the real interface types. The
        // buffer now holds the DWARF types BY NAME (BrnPropEntityModuleIO.h :609/:610/:611),
        // so the casts are gone -- the accessors already return exactly these types. They are
        // kept as one-line spellings only because every call site below reads better with
        // them, and because dropping the cast is the whole point: an undersized span cast to
        // an 11 KB host PropInputInterface is what crashed the boot.
        // ------------------------------------------------------------------
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*
        GetSceneInterface(PropEntityIO::OutputBuffer_Prepare* lpOutput)
        {
            return lpOutput->GetSceneInputInterface();
        }

        BrnResource::GameDataIO::RequestInterface<1024>*
        GetGameDataRequestInterface(PropEntityIO::OutputBuffer_Prepare* lpOutput)
        {
            return lpOutput->GetResourceRequestInterface();
        }

        BrnPhysics::Props::PropInputInterface*
        GetPropInputInterface(PropEntityIO::OutputBuffer_Prepare* lpOutput)
        {
            return lpOutput->GetPropInputInterface();
        }

        // The scene's dynamic-volume producer keys its event off the EntityId word; a
        // PropVolumeID's whole payload (owner byte at bits [16..23], type id at [6..15],
        // volume number at [0..5]) lives inside that low 32-bit word, so re-typing it is
        // lossless. The X360 hands AddDynamicVolume the raw 8-byte VolumeId in r4
        // (`ld r30, var_138; mr r4, r30`) and the producer stores it as a 64-bit id whose
        // top word is zero -- identical value, spelled through the committed signature.
        CgsSceneManager::EntityId ToSceneVolumeKey(const PropVolumeID& lrVolumeId)
        {
            return CgsSceneManager::EntityId(static_cast<u32>(lrVolumeId.mVolumeId.mId));
        }

        // ---- culling groups the prop module publishes (Prepare stage 8) --------------
        // The seven SetCullingGroupPair(a, b, enabled) calls at 0x822DA868..0x822DA8F0, in
        // asm order. The group numbers are the scene's culling-group ids; the module owns
        // groups 7 and 8 (whole props / prop parts) and pairs them against 2 (the car
        // group) and 0 (the world group) while switching prop-vs-prop off.
        const u8 KU8_CULLING_GROUP_WORLD = 0;
        const u8 KU8_CULLING_GROUP_CARS  = 2;
        const u8 KU8_CULLING_GROUP_PROPS = 7;
        const u8 KU8_CULLING_GROUP_PARTS = 8;

        // The volume-type flag AddDynamicVolume tags every prop volume with (r6 == 0x10 at
        // both call sites in InitializePropPhysicsData).
        const u8 KU8_PROP_VOLUME_TYPE_FLAG = 16;

        // CgsDev::PerfMonCpu page + budget baked into both perf-monitor constructors
        // (li r4, 6 ; li r5, 0 ; lfs f1, flt_82014984 == 2.0f ; li r7, 1).
        const s32 KI_PROP_PERFMON_PAGE   = 6;
        const f32 KF_PROP_PERFMON_BUDGET = 2.0f;
    }

    // ========================================================================
    // PropEntityModule::PropEntityModule   @ 0x827E0488
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-18 (wave Q keystone). A real ledger function that had no declaration and
    // no definition; the tree relied on the compiler's implicit constructor instead.
    //
    // The shipped body IS that implicit constructor, and nothing more. In order:
    //   * `*a1 = &off_820CE500`, EA::Thread::RWMutex(this+16, 0, 1),
    //     EA::Thread::RWMutex(this+280, 0, 1), `*a1 = off_820CFF40`
    //       -- the CgsModule::ModuleSingleBuffered base sub-object: its two IOBuffer
    //          read/write locks and the base-then-derived vtable stores.
    //   * `a1[210786..210791]` / `a1[210794..210800]` self-linking node writes
    //       -- CgsResource::BaseResourcePtr's default ctor, twice: mpPropPhysicsDataHeader
    //          and mVFXPropCollection (each intrusive list node points at itself).
    //   * the `v3 += 8` loop, 500 iterations from a1[210889] (== module + 0xCDF24)
    //       -- the same BaseResourcePtr default ctor across mapGraphicsLists[500]
    //          (8 dwords == the console's 32-byte ResourcePtr stride).
    //   * four `-1` stores: module+841872 (mZoneManager.mauTrafficLightsToRestore),
    //     +843428 (maRecentlyRecycledProps), +843552 (maRecentlyRecycledParts) and
    //     +860592 == 0xD21B0 (mVisibleOverheadSigns)
    //       -- ::Array<T,N>'s default ctor writing KI_UNCONSTRUCTED into each count word.
    //     (That last one is also the independent confirmation that mVisibleOverheadSigns is
    //      an ::Array: only ::Array's default ctor writes -1, and only at its count word.)
    //   * `a1[210468] = -1`, `a1[210857] = -1`, `a1[210888] = -1`, `a1[215148] = -1` are those
    //     four; there is no other initialisation. Every scalar flag/enum/counter the class
    //     owns is left UNINITIALISED here -- Construct() is what sets them.
    //
    // So the faithful port is an empty user-provided default constructor: it
    // default-initialises exactly the same set (base + the members with non-trivial default
    // ctors) and leaves the same scalars untouched. It is written out of line only because
    // the X360 emitted it out of line and the ledger tracks it. The class was already
    // non-trivially constructible via its base, so nothing about it changes.
    // ========================================================================
    PropEntityModule::PropEntityModule()
    {
    }

    // ========================================================================
    // PropEntityModule::Construct   @ 0x822FA068   (308 insns)
    // ------------------------------------------------------------------------
    // Store-for-store from the asm. Reading order follows the asm's, which is also the
    // only order that explains the register reuse (r26 = &mabLoadedWorldGraphics is set up
    // early and re-used for the second clear near the end -- both clears are reproduced,
    // see the note at the second one).
    // ========================================================================
    void PropEntityModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();

        mePrepareStage  = E_PREPARESTAGE_START;    // stw  0 @0x228
        meReleaseStage  = E_RELEASESTAGE_DONE;     // stw  2 @0x22C
        meStreamingMode = E_STREAM;                // stwx 0 @0xD3200
        mbStreamingSettled = false;                // stbx 0 @0xD3204

        mDebugComponent.Construct(this);

        mbUseOverrides           = false;          // stbx  0   @0xCD960
        mfOverrideMoveThreshold  = 0.0f;           // stfsx 0.0 @0xCD968
        mfOverrideSmashThreshold = 0.0f;           // stfsx 0.0 @0xCD96C
        mfOverrideLeanThreshold  = 0.0f;           // stfsx 0.0 @0xCD964  (asm stores it last)

        mZoneManager.Construct();

        // Each of these is one count-word store in the asm -- the container's Clear()
        // folded inline (Set/Array both store {elements[N], u32 count}, so the count word
        // is at +128 / +60 / +120 from the container base, which is exactly 0xCDE64 /
        // 0xCDEA4 / 0xCDF20).
        maRecentlyBrokenProps.Clear();
        maRecentlyRecycledProps.Clear();
        maRecentlyRecycledParts.Clear();

        // EventReceiverQueue<1024,16>::Construct: bind the embedded buffer (this+0x18),
        // capacity 1024 (@+0x10), alignment 16 (@+0x14), then Clear().
        mReceiverQueue.Construct();

        // ⭐ UNPARKED 2026-08-18 (wave Q keystone). The X360 clears mVisibleOverheadSigns'
        // live count here (`stwx r29, r31, 0xD21B0`). The park said its DWARF type
        // GuiOverheadSignInfoEvent::VisibleOverheadSignArray "has no committed home" -- it
        // does (DWARF BrnGuiEventTypeDefs.h:1001 == Array<BrnGui::OverheadSignScore,32>, and
        // the element is committed at GameSource/Gui/BrnGuiEventTypeDefs.h:155), so the member
        // is now that array and 0xD21B0 IS its count word. This line is that store, by name.
        mVisibleOverheadSigns.Clear();

        mLoadedZones.Clear();                      // stwx 0 @0xD339C (the Set's count word)
        miFramesUntilUpdateVisibleSigns = 1;       // stwx 1 @0xD21C0
        mabLoadedWorldGraphics.UnSetAll();         // 8 x std 0 @0xD33A0

        mPropEntitySerialiser.Construct();

        mu8PlayerIndex = 0;                        // stbx 0 @0xD3210

        // 500 x CgsResource::BaseResourcePtr::CreateFromHandle(slot, &dword_82FAD960).
        // dword_82FAD960 == &NULLResourcePtr + 0x14, i.e. the sentinel's
        // {mpThis, muThreadId} pair -- which IS a ResourceHandle. Spelled through the
        // committed assign-from-handle overload against CgsResource::NULLResourceHandle
        // (see the note on ResourcePtr::operator=(const BaseResourcePtr&)).
        for (u32 luZone = 0; luZone < KU_MAX_ZONES; ++luZone)
        {
            mapGraphicsLists[luZone] = CgsResource::NULLResourceHandle;
        }

        // 8 x `stvx128 v0(zero)` over 0xD3230..0xD32A0 -- one 16-byte lane per car slot.
        for (u32 luCar = 0; luCar < 8; ++luCar)
        {
            maRaceCarVelocity[luCar].x = 0.0f;
            maRaceCarVelocity[luCar].y = 0.0f;
            maRaceCarVelocity[luCar].z = 0.0f;
            maRaceCarVelocity[luCar].w = 0.0f;   // the console zeroes the whole 16-byte lane
        }

        muNumberOfLoadedZones = 0;                 // stwx 0 @0xD320C

        mabWaitingForGraphics.UnSetAll();          // 8 x std 0 @0xD32B0
        mabWaitingForInstances.UnSetAll();         // 8 x std 0 @0xD32F0
        // NOT a transcription slip: the asm clears 0xD33A0 TWICE -- once through r26 at
        // 0x822FA17C and again through the same register at 0x822FA290. Two separate
        // source statements the compiler did not merge; both are reproduced.
        mabLoadedWorldGraphics.UnSetAll();

        miReplayState          = 0;                // stwx 0 @0xD3330
        mbInReplay             = false;            // stbx 0 @0xD3334
        muReplayPropsInScene   = 0;                // stwx 0 @0xD3338
        muReplayPartsInScene   = 0;                // stwx 0 @0xD333C

        mbEasySmashProps       = false;            // stbx 0 @0xD3341
        mbResetPropPosition    = false;            // stbx 0 @0xD3346
        mbAllowPropProgression = true;             // stbx 1 @0xD3342  (the only `true`)
        mbPlayerCrashing       = false;            // stbx 0 @0xD3343
        mbResourceSystemStalled= false;            // stbx 0 @0xD3345
        mbDrawBoundingSpheres  = false;            // stb  0 @0xD334C
        mbPlayerWasJustReset   = false;            // stbx 0 @0xCDDE0
        mbOverrideLod          = false;            // stb  0 @0xD3347
        miLodOverrideValue     = 0;                // stw  0 @0xD3348
        mbOverrideLodDistances = false;            // stb  0 @0xD334D
        mbPlayerWrecked        = false;            // stbx 0 @0xD3344
        mbCurrentlyOnline      = false;            // stbx 0 @0xD3340

        // `mauOverrideLodDistances[i] = 100 * (i + 1)` -- the loop's address math is
        // `(i + 0x34CD4) * 4`, i.e. 0xD3350 + i*4, and the trip count is the sign-extended
        // byte counter compared against KI_NUM_LODS.
        for (s8 li8Lod = 0; li8Lod < KI_NUM_LODS; ++li8Lod)
        {
            mauOverrideLodDistances[li8Lod] = 100 * (li8Lod + 1);
        }

        // PARK -- the tail of the X360 body (0x822FA38C..0x822FA52C) takes the global
        // CgsDebugManager critical section and registers eight MODULE-level debug
        // variables with it:
        //     "World"      / "Draw prop bounding spheres" -> &mbDrawBoundingSpheres
        //     "World/LODs" / "Override Prop LOD"          -> &mbOverrideLod
        //     "World/LODs" / "Prop LOD number"            -> &miLodOverrideValue, limits 0..15
        //     "World/LODs" / "OverridePropDistances"      -> &mbOverrideLodDistances
        //     "World/LODs" / "PropLOD%dDistance"          -> &mauOverrideLodDistances[0..2],
        //                                                    each with limits 1..10000
        // These go through CgsDebugManager's own RegisterVariable / SetLimits entry points
        // (X360 sub_8282E400 / sub_8282E3B8 / sub_8282F910), which are NOT declared on the
        // committed CgsDebugManager (it exposes only GetInstance()). Adding them is that
        // class's lane, not this one, and the block is dev-menu-only -- it has no gameplay
        // effect and cannot affect prop spawning. Restore it when CgsDebugManager grows
        // the API. (The member INITIALISERS the block is interleaved with are all above;
        // only the registration calls are parked.)

        // ⭐ UNPARKED 2026-08-12 (conductor). `stb r21(1), 4(r31)` -- the one-byte flag at
        // +4, set true right before the perf/debug tail -- is `mbIsNewModule`, and refusing
        // to poke +4 blindly was the right call: the identity is already attested in the
        // tree. CgsGuiViewModule.cpp:137 names it outright ("the guest tail store
        // *(this+4) = 1 is CgsModule::mbIsNewModule = true -- NOT the prepare stage, whose
        // stores go to +8: ModuleSingleBuffered::Prepare @0x8286E824 `li r11,2; stw r11,8(r31)`
        // -- so +4 is the bool right after the vptr"), ModuleSingleBuffered's ctor clears it
        // (CgsModuleSingleBuffered.cpp:10), and WorldEntityModule -- our sibling entity
        // module -- sets it the same way (BrnWorldEntityModule.cpp:202).
        //
        // MEASURED CONSEQUENCE of leaving it unset: the first boot after this wave landed
        // took 629 asserts and never left the BOOT phase. The very first was
        //     "This is a new module type - can't lock/unlock etc etc"
        //     (CgsModuleSingleBuffered.cpp:213)
        //     PropEntityModule::Prepare -> ModuleSingleBuffered::Prepare
        //                               -> CreateInputDataStructure
        // -- exactly the dead-end the ViewModule comment predicts. PropEntityModule is a
        // NEW-module type: its IO buffers arrive as arguments from WorldModule, so the base
        // Prepare must skip every owned-buffer stage (each is gated on !mbIsNewModule) and
        // fall through to DONE. Set BY NAME, not by offset.
        mbIsNewModule = true;

        miUpdatesSinceLastSimPause = 0;            // stwx 0 @0xD335C
    }

    // ========================================================================
    // PropEntityModule::ConstructPreScenePerfMonitors   @ 0x822A90A0   (94 insns)
    // ------------------------------------------------------------------------
    // Four CPU monitors on page 6 with a 2.0 ms budget, lib-perf tagged, each followed by
    // its own `handle >= 0` tripwire. Note the ORDER: the fourth is miSerialisePM (the
    // X360-only replay monitor at +0xD3374), NOT mrTimestep -- the asm stores the handle
    // to 0xD3374 and the assert text is "miSerialisePM >= 0".
    // ========================================================================
    void PropEntityModule::ConstructPreScenePerfMonitors()
    {
        miCollisionStreamingPM = CgsDev::PerfMonCpu::AddMonitor(
            "      Collision streaming",
            static_cast<CgsDev::PerfMonCpuPage>(KI_PROP_PERFMON_PAGE),
            false, KF_PROP_PERFMON_BUDGET, true);
        CGS_ASSERT(miCollisionStreamingPM >= 0, "miCollisionStreamingPM >= 0");

        miLoadingPM = CgsDev::PerfMonCpu::AddMonitor(
            "      Zone loading",
            static_cast<CgsDev::PerfMonCpuPage>(KI_PROP_PERFMON_PAGE),
            false, KF_PROP_PERFMON_BUDGET, true);
        CGS_ASSERT(miLoadingPM >= 0, "miLoadingPM >= 0");

        miUnloadingPM = CgsDev::PerfMonCpu::AddMonitor(
            "      Zone unloading",
            static_cast<CgsDev::PerfMonCpuPage>(KI_PROP_PERFMON_PAGE),
            false, KF_PROP_PERFMON_BUDGET, true);
        CGS_ASSERT(miUnloadingPM >= 0, "miUnloadingPM >= 0");

        miSerialisePM = CgsDev::PerfMonCpu::AddMonitor(
            "      Serialising",
            static_cast<CgsDev::PerfMonCpuPage>(KI_PROP_PERFMON_PAGE),
            false, KF_PROP_PERFMON_BUDGET, true);
        CGS_ASSERT(miSerialisePM >= 0, "miSerialisePM >= 0");
    }

    // ========================================================================
    // PropEntityModule::ConstructPostPhysicsPerfMonitors  @ 0x822A9218  (56 insns)
    // ========================================================================
    void PropEntityModule::ConstructPostPhysicsPerfMonitors()
    {
        miProcessContactsPM = CgsDev::PerfMonCpu::AddMonitor(
            "      Process contacts",
            static_cast<CgsDev::PerfMonCpuPage>(KI_PROP_PERFMON_PAGE),
            false, KF_PROP_PERFMON_BUDGET, true);
        CGS_ASSERT(miProcessContactsPM >= 0, "miProcessContactsPM >= 0");

        miUpdatePropsPM = CgsDev::PerfMonCpu::AddMonitor(
            "      Update props",
            static_cast<CgsDev::PerfMonCpuPage>(KI_PROP_PERFMON_PAGE),
            false, KF_PROP_PERFMON_BUDGET, true);
        CGS_ASSERT(miUpdatePropsPM >= 0, "miUpdatePropsPM >= 0");
    }

    // ========================================================================
    // PropEntityModule::Prepare   @ 0x82306DB8   (124 insns)
    // ------------------------------------------------------------------------
    // The resumable staged prepare. WorldModule::Prepare @0x827D53B0 re-enters it every
    // frame until it returns true; each stage records where it got to in mePrepareStage
    // and either falls through to the next stage or returns false to be resumed.
    //
    // Faithful to the jump table at 0x82306E00: cases 0 and 1 share an entry, 2/3/4/5/8/9
    // each have one, and 6 + 7 (the LOAD/AQUIRE_PROP_INSTANCES stages, which this build
    // never enters) land in the "Invalid Stage\n" default.
    //
    // lpPhysicsAllocator is accepted for the committed call-site signature but is NOT
    // touched by the X360 body (r5 is dead through the whole function).
    // ========================================================================
    bool PropEntityModule::Prepare(PropEntityIO::OutputBuffer_Prepare* lpOutputBuffer,
                                   rw::IResourceAllocator* lpPhysicsAllocator)
    {
        (void)lpPhysicsAllocator;

        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
        case E_PREPARESTAGE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
            {
                return false;
            }
            mDebugComponent.Register();
            // fall through

        case E_PREPARESTAGE_REQUEST_PROP_VFX_DATA:
        {
            mePrepareStage = E_PREPARESTAGE_REQUEST_PROP_VFX_DATA;
            // AcquireResource(&mReceiverQueue, eventId 2, poolId 13, "vfx_props_collection"):
            // the X360 open-codes the 24-byte record {mpUser, miEventId, miPoolId, mId =
            // ID::HashString("vfx_props_collection")} and AddEvent's it with type 4, which
            // is precisely RequestInterface<N>::AcquireResource.
            GetGameDataRequestInterface(lpOutputBuffer)->AcquireResource(
                &mReceiverQueue, 2, 13, "vfx_props_collection");
            mReceiverQueue.Clear();
            // fall through
        }

        case E_PREPARESTAGE_AQUIRE_PROP_VFX_DATA:
        {
            mePrepareStage = E_PREPARESTAGE_AQUIRE_PROP_VFX_DATA;
            if (mReceiverQueue.GetCount() < 1)
            {
                return false;   // reply has not landed yet -- resume next frame
            }
            // PARK -- the X360 then binds mVFXPropCollection from the reply's resource
            // handle (`ld r11, 0x18(event); CreateFromHandle(&mVFXPropCollection, &r11)`).
            // The reply is a pool AcquireResourceResponse, whose struct has NO committed
            // home (CgsResourceIOEvents.h declares the REQUEST and the *List* response, not
            // this one), so the handle cannot be read by name and the console's +0x18 is a
            // 32-bit-target offset that does not survive to x64. Deliberately not poked.
            // Consequence: the VFX prop-effects collection stays unbound -- particle FX on
            // prop impacts, not prop spawning/rendering. Restore as
            // `mVFXPropCollection = lpResponse->mHandle;` when the response type lands.
            // fall through
        }

        case E_PREPARESTAGE_LOAD_PROP_PHYSICS:
            mePrepareStage = E_PREPARESTAGE_LOAD_PROP_PHYSICS;
            GetGameDataRequestInterface(lpOutputBuffer)->LoadPropPhysics(&mReceiverQueue, 0, 1);
            mReceiverQueue.Clear();
            // fall through

        case E_PREPARESTAGE_AQUIRE_PROP_PHYSICS:
        {
            mePrepareStage = E_PREPARESTAGE_AQUIRE_PROP_PHYSICS;
            if (mReceiverQueue.GetCount() < 1)
            {
                return false;   // PROPPHYSICS has not loaded yet -- resume next frame
            }

            const CgsModule::Event* lpEvent = 0;
            s32 liEventSize = 0;
            mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);

            // The reply is a LoadGameDataEvent (GameDataAssetEvent), whose loaded-asset
            // handle is mHandle -- the X360's `event + 0x20`.
            const BrnResource::GameDataIO::LoadGameDataEvent* lpLoaded =
                static_cast<const BrnResource::GameDataIO::LoadGameDataEvent*>(lpEvent);
            mpPropPhysicsDataHeader = lpLoaded->mHandle;
            // fall through
        }

        case E_PREPARESTAGE_INITIALIZE_PHYSICS_DATA:
            mePrepareStage = E_PREPARESTAGE_INITIALIZE_PHYSICS_DATA;
            InitializePropPhysicsData(lpOutputBuffer);
            // fall through

        case E_PREPARESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ========================================================================
    // PropEntityModule::InitializePropPhysicsData   @ 0x822DA840   (510 insns)
    // ------------------------------------------------------------------------
    // Prepare stage 8. Two jobs:
    //   1. publish the prop culling-group matrix on the scene's update interface;
    //   2. register EVERY prop-type collision volume -- the whole-prop volumes and then
    //      each part's volumes -- as a scene dynamic volume keyed by a PropVolumeID whose
    //      volume number runs continuously across both loops (the asm keeps one u8 counter,
    //      r24, live across them; the per-part loop has its OWN index r95 for the part's
    //      volume array but still bumps r24).
    //
    // The bulk of the 510 instructions is inline VMX assert scaffolding:
    // `RwMath::IsValid(*lpVolume->GetRelativeTransform())` (four vcmpeqfp self-compares --
    // a NaN test -- over the four transform rows) and a GetBBox degenerate-extent check.
    // Both are DEV TRIPWIRES with no side effects. They are NOT reproduced here because
    // rw::collision::Volume exposes neither GetRelativeTransform() nor the vtable GetBBox
    // slot (`(*(*(volume + 64) + 4))(volume, volume, 1, &aabb)`) in the committed
    // vendor headers; adding them is rwcollision's lane. Reported, not faked.
    // ========================================================================
    void PropEntityModule::InitializePropPhysicsData(PropEntityIO::OutputBuffer_Prepare* lpOutputBuffer)
    {
        typedef BrnPhysics::Props::PropTypeData     PropTypeData;
        typedef BrnPhysics::Props::PropPartTypeData PropPartTypeData;

        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene =
            GetSceneInterface(lpOutputBuffer);

        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_CARS,  KU8_CULLING_GROUP_PROPS, 1);
        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_CARS,  KU8_CULLING_GROUP_PARTS, 1);
        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_PROPS, KU8_CULLING_GROUP_PROPS, 0);
        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_PROPS, KU8_CULLING_GROUP_PARTS, 0);
        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_PARTS, KU8_CULLING_GROUP_PARTS, 0);
        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_PROPS, KU8_CULLING_GROUP_WORLD, 1);
        lpScene->SetCullingGroupPair(KU8_CULLING_GROUP_PARTS, KU8_CULLING_GROUP_WORLD, 1);

        // The local volume id. Its owner byte is seeded to E_ENTITYTYPE_PROP by
        // PropVolumeID's default constructor (2026-08-12, prop-BOOT wave) -- exactly what the
        // console emits here, `lis r11,3 ; std r11, 0x1A0+var_138(r1)` at 0x822DA8F4/0x822DA908,
        // right after the seven SetCullingGroupPair calls. Set() below reads that
        // PRE-EXISTING owner in its tripwire, so without the seed the first Set asserted.
        PropVolumeID lVolumeId;

        const PropPhysicsDataHeader* lpHeader = mpPropPhysicsDataHeader.GetMemoryResource();

        for (u32 luTypeId = 0; luTypeId < lpHeader->GetNumberOfPropTypes(); ++luTypeId)
        {
            const PropTypeData* lpType = lpHeader->GetType(luTypeId);

            // One running volume number per prop TYPE, shared by the whole-prop volumes
            // and every part volume that follows them.
            u8 lu8VolumeNumber = 0;

            for (u32 luVolume = 0; luVolume < lpType->GetNumberOfVolumes(); ++luVolume)
            {
                lVolumeId.Set(static_cast<u16>(luTypeId), lu8VolumeNumber);

                const ::rw::collision::Volume* lpVolume = lpType->GetCollisionVolume(luVolume);

                lVolumeId.AssertIsProp();
                lpScene->AddDynamicVolume(ToSceneVolumeKey(lVolumeId),
                                          lpVolume,
                                          KU8_PROP_VOLUME_TYPE_FLAG);
                ++lu8VolumeNumber;
            }

            for (u32 luPart = 0; luPart < lpType->GetNumberOfParts(); ++luPart)
            {
                const PropPartTypeData& lrPart = lpType->GetParts()[luPart];

                for (u32 luPartVolume = 0; luPartVolume < lrPart.GetNumberOfVolumes(); ++luPartVolume)
                {
                    lVolumeId.Set(static_cast<u16>(luTypeId), lu8VolumeNumber);

                    const ::rw::collision::Volume* lpVolume = lrPart.GetCollisionVolume(luPartVolume);

                    lVolumeId.AssertIsProp();
                    lpScene->AddDynamicVolume(ToSceneVolumeKey(lVolumeId),
                                              lpVolume,
                                              KU8_PROP_VOLUME_TYPE_FLAG);
                    ++lu8VolumeNumber;
                }
            }
        }

        // Tail (0x822DB00C..0x822DB024): hand the physics side the prop-physics data
        // handle -- `ldx r31, this, 0xCDD9C` (mpPropPhysicsDataHeader + 0x14, i.e. the
        // ResourcePtr's {mpThis, muThreadId} pair, which IS a ResourceHandle) then
        // `std r31, 0x2BF8(propInputInterface)` (PropInputInterface::mpPhysicsData).
        //
        // UNPARKED 2026-08-12 (conductor): PropInputInterface::SetPhysicsData is now an
        // additive inline on that class (see BrnPropInputInterface.h), so the handle is
        // written BY NAME rather than by poking the console's +0x2BF8 -- which would have
        // been wrong on x64 anyway, since ResourceHandle widens on the host.
        GetPropInputInterface(lpOutputBuffer)
            ->SetPhysicsData(mpPropPhysicsDataHeader.GetResourceHandle());
    }

    // ========================================================================
    // PropEntityModule::Release   @ 0x822A92F8   (41 insns)
    // ------------------------------------------------------------------------
    // The release twin of Prepare's ladder. Both exit paths store
    // E_RELEASESTAGE_MANAGER (r30 == 1) to meReleaseStage -- the X360 never writes
    // E_RELEASESTAGE_DONE here; reproduced as shipped rather than "corrected".
    // ========================================================================
    bool PropEntityModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
        case E_RELEASESTAGE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Release())
            {
                return false;
            }
            break;

        case E_RELEASESTAGE_DONE:
            break;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }

        meReleaseStage = E_RELEASESTAGE_MANAGER;
        mePrepareStage = E_PREPARESTAGE_START;
        return true;
    }

    // ========================================================================
    // PropEntityModule::CachePropGraphicsLists   @ 0x822DBF28   (57 insns)
    // ------------------------------------------------------------------------
    // Rebuild the prop-type -> PropGraphics registration table from every loaded per-zone
    // graphics list.
    //
    // ⚠ THE CONSOLE CONSTANTS IN THIS BODY ARE ALL GONE, ON PURPOSE. The asm walks
    // `module + 843556` with a 32-byte stride, compares a 12-byte prefix against
    // dword_82FAD94C, and clears 500 slots at `module + 860612` with an 8-byte stride.
    // Every one of those numbers is a CONSOLE fact:
    //     843556 == mapGraphicsLists     860612 == mPropGraphicsManager
    //     32     == console sizeof(ResourcePtr<T>)   (56 on x64)
    //     8      == console sizeof(PropGraphicsReference) (16 on x64)
    //     12     == the BaseResourcePtr identity prefix IsEqual() compares
    //     dword_82FAD94C == CgsResource::NULLResourcePtr
    // Carrying any of them into the host build is the recurring corruption this wave was
    // called to stop; the loops below index by named member and the sentinel test goes
    // through the committed `operator!=(const BaseResourcePtr&)`, which IS IsEqual().
    // ========================================================================
    void PropEntityModule::CachePropGraphicsLists()
    {
        // 500 x {slot pointer = 0, slot ref count = 0} -- PropGraphicsManager's own table
        // reset, folded inline by the X360 compiler.
        mPropGraphicsManager.Reset();

        for (u32 luZone = 0; luZone < KU_MAX_ZONES; ++luZone)
        {
            if (mapGraphicsLists[luZone] == CgsResource::NULLResourcePtr)
            {
                continue;   // zone not loaded -- the 12-byte sentinel compare
            }

            const PropGraphicsList* lpGraphicsList = mapGraphicsLists[luZone].GetMemoryResource();

            // The asm re-reads the element count every iteration (it calls the accessor
            // twice per pass), so the bound is re-evaluated, not hoisted.
            for (u32 luGraphics = 0; luGraphics < lpGraphicsList->muNumberOfPropModels; ++luGraphics)
            {
                mPropGraphicsManager.Register(
                    const_cast<PropGraphicsList*>(lpGraphicsList)->GetPropGraphics(luGraphics));
            }
        }
    }

    // ========================================================================
    // Host layout tripwires. Never called -- a member function so it may name private
    // members. These pin the FACTS THIS TU RELIES ON, all of them host facts; there is
    // deliberately no offsetof against a console number anywhere.
    // ========================================================================
    void PropEntityModule::_AssertLayout()
    {
        // The module IS the single-buffered module -- Construct/Prepare/Release all
        // delegate to the base, and Release's ladder is meaningless without it.
        static_assert(sizeof(PropEntityModule) > sizeof(CgsModule::ModuleSingleBuffered),
                      "PropEntityModule must derive from CgsModule::ModuleSingleBuffered");

        // Construct's 500-slot loops and CachePropGraphicsLists' walk share one bound.
        static_assert(KU_MAX_ZONES == 500, "mapGraphicsLists / mab*Zones are 500 wide");
        static_assert(sizeof(reinterpret_cast<PropEntityModule*>(0)->mapGraphicsLists) /
                      sizeof(reinterpret_cast<PropEntityModule*>(0)->mapGraphicsLists[0]) == KU_MAX_ZONES,
                      "mapGraphicsLists[KU_MAX_ZONES]");

        // The three LOD-distance slots the debug block and the init loop share.
        static_assert(KI_NUM_LODS == 3, "mauOverrideLodDistances[3]");

        // Both bit arrays are per-zone, one bit per zone -- 500 bits == 8 x u64.
        static_assert(CgsContainers::BitArray<KU_MAX_ZONES>::kuNumberOfBitFields == 8,
                      "BitArray<500> is 8 x u64 (the 8 x std 0 clears in Construct)");

        // The event queue Construct binds: 1024-byte buffer, 16-byte record alignment.
        static_assert(sizeof(CgsModule::EventReceiverQueue<1024, 16>) >
                      sizeof(CgsModule::BaseEventReceiverQueue),
                      "EventReceiverQueue<1024,16> carries its own backing buffer");

        // 8 car velocity lanes (the 8 x stvx128 in Construct), each a 16-byte SIMD lane.
        static_assert(sizeof(reinterpret_cast<PropEntityModule*>(0)->maRaceCarVelocity) == 8 * 16,
                      "maRaceCarVelocity[8] of 16-byte lanes");
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_01.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_01.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q KEYSTONE, GROUP 1: the replay entry/exit ladder.
// (spec: scratchpad/waveQ/PropEntityModule.spec.md, "GROUPS" row 1.)
//
// Ledger TUs covered (ONE class, TWO ledger ids -- the conductor closes them separately):
//   * GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   * class:BrnWorld::PropEntityModule
//
// Functions in this file (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::PrepareForReplay    @0x822A94B0  (55 insns)
//   PropEntityModule::RestoreFromReplay   @0x822A95A8  (58 insns)
//   PropEntityModule::LeaveReplay         @0x822C4810  (66 insns)
//
// COUNTED, not copied: PrepareForReplay's export prints 56 lines of which 1 is the collapsed
// `.long` jump table (0x822A94EC..0x822A9500) -> 55; RestoreFromReplay 59 - 1 -> 58;
// LeaveReplay 66 with no jump table. (The wave spec's section-6 count column said 32/36/45
// and is unreliable -- reported to the conductor, not inherited here.)
//
// All three are X360-ONLY: the DecFIGS (Dec-2007 PS3) DWARF declares none of them, and
// neither does the Feb-2007 leaked source -- both predate the prop replay path. That same
// merge-window delta is why the members they walk (miReplayState, mbStreamingSettled,
// muReplayPropsInScene, muReplayPartsInScene) are the header's X360-only block. Every
// statement below is therefore grounded on the RAW ASSEMBLY of the three per-address
// exports, never on the DWARF and never on Hex-Rays (which renders all three as `int`
// blobs and gives LeaveReplay a phantom `int result` first parameter).
//
// ---- WHAT IS MEASURED vs WHAT IS INFERENCE -------------------------------------------
// MEASURED (read off the asm listing, instruction by instruction):
//   * the whole state ladder: every case value, every store, every return value;
//   * the member offsets touched -- 0xD3330 miReplayState, 0xD3200 meStreamingMode,
//     0xD3204 mbStreamingSettled, 0xD320C muNumberOfLoadedZones, 0xD3338/0xD333C the two
//     replay-in-scene counters. These agree with the block documented in
//     BrnPropEntityModule.h, which the wave-Q spec re-verified against exactly these three
//     bodies (spec section 3.4);
//   * the two assert sites and their console line numbers (0x745 == 1861, 0x774 == 1908);
//   * LeaveReplay's scene-entity id arithmetic and its RemoveEntity(id, 0) flag word.
// INFERENCE (named as such where it appears):
//   * that meStreamingMode's stored `2` is E_RESET_UNLOADING -- the asm stores the raw
//     word 2; the enumerator name comes from the committed EPropStreamingMode in
//     BrnPropEntityModule.h. Value-identical either way.
//   * that the id's part field 0/1 is the Feb-2007 file-local { E_PROP, E_PART } (see the
//     enum below). The asm only shows the literals 0 and 1.
//
// ---- CONSOLE-LITERAL DISCIPLINE (gotcha 1) -------------------------------------------
// Not one console byte offset appears in the code. Every one is reached by named member.
// The only numeric literals that survive are (a) the miReplayState case values, which are
// a state machine's own alphabet and not addresses, and (b) the scene-entity OWNER byte
// 0x22, which is a packed-field VALUE, not an offset -- and it is passed through
// CgsSceneManager::EntityId::Set so the host does the packing (see LeaveReplay).
//
// NaN polarity (gotcha 4): NOT APPLICABLE. There is not one floating-point instruction in
// any of the three functions -- no fcmpu, no fsel, no FPR is touched.
// ============================================================================


namespace BrnWorld
{
    namespace
    {
        // The prop/part discriminator that rides the scene-entity id's 10-bit PART field.
        // GROUND TRUTH: the Feb-2007 leaked source declares exactly this unnamed file-scope
        // enum at references/Feb-2007/BrnEntityModuleUnity/GameSource/World/EntityModules/
        // PropEntityModule/BrnPropEntityModule.cpp:41-44 (`enum { E_PROP = 0, E_PART = 1 };`)
        // and uses E_PART as an EntityId part index at :894. The X360 bodies below emit the
        // folded literals 0 and 1 only, so the *names* are the Feb-2007 source's, the values
        // are the asm's. Kept file-local (anonymous namespace) exactly as the original is
        // file-local -- it must not leak into the class or another TU.
        enum EPropScenePartField
        {
            E_PROP = 0,
            E_PART = 1
        };

        // The scene-entity OWNER byte the replay prop path uses.
        //
        // ⚠️ UNNAMED IN THE TREE, DELIBERATELY LEFT AS A LITERAL. GameSource/World/
        // BrnEntityTypes.h names E_ENTITYTYPE_FIRST_REPLAY_TYPE = 32 and
        // E_ENTITYTYPE_REPLAY_RACECAR = 33, then stops at E_ENTITYTYPE_COUNT = 34 -- so 34
        // (0x22) is a replay-prop owner that the enum home does not name, and it COLLIDES
        // with _COUNT, which is used as a bound elsewhere. BrnEntityTypes.h is outside this
        // lane's ownership, so naming it there is a shared-header request (spec section 8,
        // item 2), not an edit made here. Until that lands, the literal + this citation is
        // the honest spelling.
        //
        // MEASURED at 0x822C4878 (`oris r30, r11, 0x2200` -> owner byte 0x22 in bits 31..24)
        // and at 0x822C48B4/B8 (`lis r11, 0x2200 ; ori r28, r11, 1` -> 0x22000001, i.e. the
        // same owner with part field 1). The same owner appears in ReplayUpdatePropsInScene
        // @0x822DB370 and ReplayUpdatePartsInScene @0x822DB900 (wave-Q group 3).
        const u32 KU_REPLAY_PROP_SCENE_OWNER = 0x22u;
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::PrepareForReplay  @0x822A94B0  (55 insns, counted)
    //
    // Called only from PostPhysicsUpdate @0x823031D8. Drives the module from live play into
    // replay playback: it asks the streaming system to tear the live prop population down
    // (meStreamingMode = E_RESET_UNLOADING) and answers `true` only once the world has
    // actually settled, so the caller can spin on it across frames.
    //
    // ASM (jump table @0x822A94EC, 6 cases, `cmplwi r9,5 ; bgt default`):
    //   r11 = this ; r10 = this + 0xD3330 (miReplayState) ; r9 = *r10
    //   cases 0,1,4,5 (0x822A9504): stwx 2 -> this+0xD3200 (meStreamingMode)
    //                               stw  2 -> 0(r10)       (miReplayState)
    //                               li r3,0                -> return false
    //   case 2 (0x822A952C): lbzx r9, this+0xD3204 (mbStreamingSettled)
    //                        beq -> 0x822A9590 (li r3,0 ; return false)
    //                        lwzx r11, this+0xD320C (muNumberOfLoadedZones)
    //                        cmplwi r11,1 ; ble -> 0x822A9590 (return false)
    //                        stw 3 -> 0(r10) ; fall into case 3
    //   case 3 (0x822A955C): li r3,1 -> return true
    //   default (0x822A9570): assert "Shouldn't get here",
    //                         BrnPropEntityModule.cpp:1861 (r5 = 0x745), then return false.
    //
    // ⚠️ The `cmplwi r11, 1 ; ble` on muNumberOfLoadedZones is an UNSIGNED compare against
    // the immediate 1, i.e. `zones <= 1u` -> bail. Written below as `<= 1u` on the u32
    // member, which is the same test. (No float is involved, so gotcha 4 does not apply.)
    // ------------------------------------------------------------------------------------
    bool PropEntityModule::PrepareForReplay()
    {
        switch (miReplayState)
        {
        case 0:
        case 1:
        case 4:
        case 5:
            // INFERENCE (name only): the asm stores the raw word 2; EPropStreamingMode's
            // committed E_RESET_UNLOADING == 2 (BrnPropEntityModule.h). Value-identical.
            meStreamingMode = E_RESET_UNLOADING;
            miReplayState   = 2;
            return false;

        case 2:
            // Not settled yet, or the world is still down to a single loaded zone: stay in
            // state 2 and answer "not ready". Both legs branch to the SAME `li r3,0` exit.
            if (!mbStreamingSettled || muNumberOfLoadedZones <= 1u)
            {
                return false;
            }
            miReplayState = 3;
            return true;

        case 3:
            // Terminal "ready" state -- no store, just the answer (the asm falls into the
            // case-2 tail's `li r3,1` at 0x822A955C).
            return true;

        default:
            // X360: CgsDev::Assert::FireAssert("Shouldn't get here",
            //   "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/
            //    PropEntityModule/BrnPropEntityModule.cpp", 1861)
            CGS_ASSERT(false, "Shouldn't get here");
            return false;
        }
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::RestoreFromReplay  @0x822A95A8  (58 insns, counted)
    //
    // Called only from PostPhysicsUpdate @0x823031D8. The mirror of the above: it walks the
    // module back out of replay playback and answers `true` when the live world is usable
    // again. NOTE it is NOT a symmetric copy -- the ladder is a different shape (state 0 is
    // the terminal "already live" answer here, and there is no muNumberOfLoadedZones gate).
    //
    // ASM (jump table @0x822A95E4, 6 cases, `cmplwi r9,5 ; bgt default`):
    //   r10 = this ; r11 = this + 0xD3330 (miReplayState) ; r9 = *r11
    //   case 0   (0x822A95FC): li r3,1 -> return true
    //   cases 1-3(0x822A9610): stw 4 -> 0(r11)   ... and FALLS THROUGH into case 4
    //   case 4   (0x822A9618): stwx 2 -> this+0xD3200 (meStreamingMode)
    //                          stw  5 -> 0(r11)  (miReplayState)
    //                          li r3,0 -> return false
    //   case 5   (0x822A9644): lbzx r10, this+0xD3204 (mbStreamingSettled)
    //                          beq -> 0x822A9694 (li r3,0 ; return false)
    //                          stw 0 -> 0(r11) ; li r3,1 -> return true
    //   default  (0x822A9674): assert "Shouldn't get here",
    //                          BrnPropEntityModule.cpp:1908 (r5 = 0x774), then return false.
    //
    // ⚠️ The cases-1-3 -> case-4 FALL-THROUGH is real, not a decompiler artifact: the
    // `stw r9(4), 0(r11)` at 0x822A9614 is immediately followed in address order by case 4's
    // first instruction at 0x822A9618 with no branch between them. The redundant-looking
    // `miReplayState = 4` then `miReplayState = 5` pair is what the shipped code does.
    // ------------------------------------------------------------------------------------
    bool PropEntityModule::RestoreFromReplay()
    {
        switch (miReplayState)
        {
        case 0:
            return true;

        case 1:
        case 2:
        case 3:
            miReplayState = 4;
            // FALL THROUGH -- measured, see the banner above.
            [[fallthrough]];

        case 4:
            // INFERENCE (name only): raw word 2 == E_RESET_UNLOADING, as in PrepareForReplay.
            meStreamingMode = E_RESET_UNLOADING;
            miReplayState   = 5;
            return false;

        case 5:
            if (!mbStreamingSettled)
            {
                return false;
            }
            miReplayState = 0;
            return true;

        default:
            // X360: FireAssert("Shouldn't get here", <same path>, 1908)
            CGS_ASSERT(false, "Shouldn't get here");
            return false;
        }
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::LeaveReplay  @0x822C4810  (66 insns, counted)
    //
    // Called only from ReplayPreSceneUpdate @0x822EF878 (wave-Q group 3). Evicts every scene
    // entity the replay playback path published -- first the props, then the parts -- and
    // zeroes both published counts so the next replay entry starts from an empty population.
    //
    // ASM:
    //   r28 = this ; r26 = r4 (lpOutput) ; r29 = 0 (the constant zero reused as the loop
    //   seed, as the RemoveEntity flags word, and as both final stores)
    //   r25 = this + 0xD3338 (muReplayPropsInScene)
    //   loop 1 (0x822C4850..0x822C4898), entry guard `lwz r11,0(r25) ; cmpwi r11,0 ; ble`:
    //       cmplwi r31, 0x4000 ; blt -> skip assert
    //       assert "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)", console CgsEntityId.h:116
    //       slwi r11, r31, 10 ; oris r30, r11, 0x2200        == (i << 10) | 0x22000000
    //       mr r3, r26 ; bl sub_822B9738                     == OutputBuffer_PreScene::
    //                                                           GetSceneInputInterface()
    //       mr r5, r29(0) ; mr r4, r30 ; bl InSceneUpdateInterface::RemoveEntity
    //       lwz r11, 0(r25) ; addi r31,r31,1 ; cmpw r31,r11 ; blt  (bound RE-READ each pass)
    //   loop 2 (0x822C48BC..0x822C4904): identical, over r27 = this + 0xD333C
    //       (muReplayPartsInScene), with r28 preloaded to 0x22000001 and the id built as
    //       `slwi r11,r31,10 ; or r30,r11,r28`   == (i << 10) | 0x22000001
    //   tail (0x822C4908): stw r29(0), 0(r25) ; stw r29(0), 0(r27)
    //
    // WHY THE ID GOES THROUGH EntityId::Set RATHER THAN THE PACKED LITERAL:
    // CgsSceneManager::EntityId is owner[31..24] | entityIndex[23..10] | partIndex[9..0]
    // (CgsEntityId.h, pinned on EntityId::Set @0x82277330), so
    // `Set(0x22, i, E_PROP)` == `(i << 10) | 0x22000000` and `Set(0x22, i, E_PART)` ==
    // `(i << 10) | 0x22000001` -- exactly the two words the asm builds. Set() also carries
    // the three tripwires; the shipped code emits only the entityIndex one because the other
    // two arguments are compile-time constants there (partIndex 0/1 < 0x400, owner
    // 0x22 < 128), so the compiler folded them. Routing through Set() reproduces that: the
    // same one assert survives, from the same header line (console CgsEntityId.h:116 --
    // the reconstructed header carries that assert at CgsEntityId.h:69-70; :116 there is the
    // unrelated mId assignment inside SetPartIndex).
    //
    // ⚠️ SIGNED LOOP COMPARE, MEASURED. Both the entry guard (`cmpwi r11,0 ; ble`) and both
    // back-edges (`cmpw r31,r11 ; blt`) are SIGNED compares -- an unsigned counter against
    // an unsigned bound would have compiled to cmplwi/cmplw. The two counters are declared
    // u32 in the committed header (this lane may not edit it), so the cast below preserves
    // the shipped comparison exactly rather than silently widening it. It is NOT decoration:
    // drop it and a count with the top bit set would iterate instead of being skipped.
    // (Unreachable in practice -- the entityIndex field is only 14 bits wide -- but the
    // faithful thing is to keep the compare the compiler actually emitted.)
    // ------------------------------------------------------------------------------------
    void PropEntityModule::LeaveReplay(PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        // Pass 1 -- the replay props.
        for (s32 liProp = 0; liProp < static_cast<s32>(muReplayPropsInScene); ++liProp)
        {
            CgsSceneManager::EntityId lPropEntityId;
            lPropEntityId.Set(KU_REPLAY_PROP_SCENE_OWNER, static_cast<u32>(liProp), E_PROP);

            // The X360 re-fetches the write-locked scene interface INSIDE the loop (one
            // `bl sub_822B9738` per iteration, 0x822C487C); kept, because that getter is
            // also the buffer's "locked for writing" tripwire.
            lpOutput->GetSceneInputInterface()->RemoveEntity(lPropEntityId, 0);
        }

        // Pass 2 -- the replay parts. Same shape, different counter and part field.
        for (s32 liPart = 0; liPart < static_cast<s32>(muReplayPartsInScene); ++liPart)
        {
            CgsSceneManager::EntityId lPartEntityId;
            lPartEntityId.Set(KU_REPLAY_PROP_SCENE_OWNER, static_cast<u32>(liPart), E_PART);

            lpOutput->GetSceneInputInterface()->RemoveEntity(lPartEntityId, 0);
        }

        muReplayPropsInScene = 0;
        muReplayPartsInScene = 0;
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_02.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/
//   PropEntityModule_wQ_02.cpp
//
// Wave Q (BREAKABLE PROPS) keystone group 2, implementer 02. Sibling partfile of
// BrnPropEntityModule.cpp / _PreScene.cpp / _Streaming.cpp / _Render.cpp -- all define
// members of the ONE class declared in BrnPropEntityModule.h.
//
// LANDED HERE:
//   BrnWorld::PropEntityModule::GetDesiredState   @ 0x822A93A8  (66 insns, NO CALLS)
//
// PARKED (bodies fully reconstructed, but they cannot compile against the tree as it
// stands; each parked file names the exact declaration that unblocks it):
//   BrnWorld::PropEntityModule::PostSceneUpdate   @ 0x822C4718  (61 insns)
//       -> scratchpad/waveQ/parked/PropEntityModule_02_PostSceneUpdate.cpp
//       ROUND-1 BLOCKER (now RETIRED, see below): PropEntityIO::InputBuffer_PostScene was
//       the placeholder `struct { u8 maDeferredPayload[16]; }` with no member and no
//       accessor for the race-car-crash-complete event queue the console walks at
//       lpInput + 8, and the 16-byte payload was far too small to reinterpret onto.
//   BrnWorld::PropEntityModule::RenderReplayProp  @ 0x822EF968  (77 insns)
//       -> scratchpad/waveQ/parked/PropEntityModule_02_RenderReplayProp.cpp
//       ROUND-1 BLOCKER (now RETIRED, see below): two members of
//       BrnReplays::PropSerialiserFrame -- GetPropTransform @0x822BB920 was declared
//       nowhere in the tree, and the recorded prop-type table at frame +0x25F0 sat inside
//       the opaque pad `maPad25E1`.
//
// ⚠️ ROUND-2 STATUS OF BOTH PARKS (2026-08-18, verified against the committed headers,
// NOT taken from a banner): both blockers have been retired by the round-2 shared-header
// owners, so neither park is blocked any more --
//   * InputBuffer_PostScene now carries the typed
//     EventQueue<CrashIO::RaceCarCrashCompleteEvent,10> plus Construct/Destruct and the
//     accessor `GetCrashEventQueue()` (BrnPropEntityModuleIO.h; see
//     scratchpad/waveQ2/worldio.owner.md §1a -- note the accessor's DWARF name is
//     GetCrashEventQueue, NOT the round-1 request's GetRaceCarCrashCompleteEventQueue);
//   * PropSerialiserFrame's pad ladder is gone: `Matrix44Affine GetPropTransform(u8) const`
//     is declared at BrnReplayPropSerialiserFrame.h:301 and the prop type table is the
//     real member `maTypes` @0x25F0 (see scratchpad/waveQ2/replays.owner.md §2).
// The two parked bodies are landed by the round-2 LANDER for this group, not by this
// file. This partfile still holds GetDesiredState only.
//
// GROUND TRUTH: the RAW assembly of each per-address ARTIST export (the Hex-Rays
// pseudocode for all three renders the parameter lists as `int` blobs and is not used).
// ============================================================================


namespace BrnWorld
{
    namespace
    {
        // ------------------------------------------------------------------------
        // KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET -- MEASURED, value 80.0f.
        //
        // The DecFIGS DWARF places it at BrnPropEntityModule.cpp:1644, i.e. it is a
        // translation-unit-local constant of THIS file's console TU (which is why it has
        // no header home and none is requested). In the shipped X360 image it is the
        // .bss VecFloat `unk_82FAD530`, splat-filled at run time by an unnamed C++
        // dynamic initialiser at 0x82C4C248..0x82C4C26C (`lfs flt_82004A18` -> `vspltw`
        // -> `stvx128`) from the rodata literal flt_82004A18 == 80.0f. That initialiser
        // is why a function-export-only scan finds no writer; the value comes from a
        // headless-IDA dump of the database (wave Q keystone spec, §2.1).
        //
        // GetDesiredState is its ONLY consumer.
        //
        // ⚠️ TYPE NOTE (provenance, not behaviour): the console entity is a 16-byte .bss
        // VecFloat whose four lanes are the same 80.0f -- a plain rodata `const float`
        // would not have needed a dynamic initialiser at all. Modelled here as a scalar
        // f32 because every lane is identical and the only consumer compares one lane.
        // The DWARF also records `rw::math::vpu::operator<` as a callee of
        // GetDesiredState, i.e. the source spelled the test
        // `lfIncomingCarSpeed < KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET`; that operator on a
        // VecFloat is what emits the REVERSED `vcmpgtfp. v0(limit), v13(speed)` the asm
        // shows. The test below is written in that source order. Both spellings are the
        // same predicate including NaN (unordered -> all lanes false -> CR6 all-true bit
        // clear -> falls through, and `NaN < 80.0f` is likewise false). Do NOT change the
        // predicate direction to >= / <=.
        // ------------------------------------------------------------------------
        const f32 KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET = 80.0f;
    }

    // ========================================================================
    // GetDesiredState  @ 0x822A93A8  (66 insns)
    //
    // Classify what state a car arriving at speed `lfIncomingCarSpeed` should put a prop
    // of this type into. No calls at all -- every accessor below is inlined in the
    // console body, so the whole function is one straight read of the asm.
    //
    // ⚠️ THE FLOAT IS A SPEED, NOT AN IMPULSE (round-1 mislabel, corrected). The DWARF
    // names the parameters lpPropType / lfIncomingCarSpeed
    // (references/DecFIGS/dwarfdump/_compile/BrnEntityModuleUnity.cpp:1299), and the sole
    // caller confirms it: ProcessPotentialContactWithProp @0x822DB038 builds the argument
    // from maRaceCarVelocity[lContactEntityId.GetEntityIndex()] and splats LANE 3 --
    // 0x822DB100 `slwi r11,r11,4` / 0x822DB108 `lvx128 v0,r0,r11` / 0x822DB110
    // `vspltw v0,v0,3` / 0x822DB118 `lfs f1` -- i.e. the W lane the module documents as
    // the car speed (BrnPropEntityModule.h GetRaceCarSpeed). That caller DOES also receive
    // a real contact impulse (r6/v1) which this function never reads; do not "fix" the
    // call to pass it. The constant this value is compared against is named
    // KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET for the same reason.
    // The declaration in BrnPropEntityModule.h still spells the old parameter names; that
    // header is outside this lane's write set (reported as a header request).
    //
    // PPC ABI (gotcha 3): the float rides f1 and SKIPS its GPR slot. The body uses r3
    // (this) and r4 (lpPropType) and NEVER TOUCHES r5 -- re-dumped and confirmed. Hex-Rays'
    // phantom leading `int` parameter is that skipped slot and is not a parameter.
    //
    // CONSOLE OFFSET -> MEMBER DECODE (provenance only; nothing below indexes by offset):
    //   PropTypeData +0x4C mfLeanThreshold   -> GetLeanThreshold()
    //   PropTypeData +0x50 mfMoveThreshold   -> GetMoveThreshold()
    //   PropTypeData +0x58 muSceneUriId      -> read inside HACKShouldMoveComOffset()
    //   PropTypeData +0x5F mu8JointType      -> GetLeanState()
    //   module      +0xCD960 mbUseOverrides
    //   module      +0xCD964 mfOverrideLeanThreshold
    //   module      +0xCD968 mfOverrideMoveThreshold
    //
    // NaN POLARITY (gotcha 4): every `bge`/`ble` in this body is the NEGATED ORDERED
    // predicate of the C++ comparison written here (`bge` after `fcmpu a,b` is taken when
    // a >= b OR unordered, i.e. it skips the arm exactly when `a < b` is false in C++).
    // The final 80.0f test is a `vcmpgtfp.` all-lanes-greater test of the limit against a
    // splat of the SPEED, i.e. the ordered `limit > speed` == the ordered `speed < limit`
    // (both false when the speed is NaN). So `<`/`>` as written IS the console behaviour
    // on NaN; do NOT "fix" any of them to `>=`/`<=`.
    // ========================================================================
    EPropState
    PropEntityModule::GetDesiredState( const BrnPhysics::Props::PropTypeData* lpPropType,
                                       f32 lfIncomingCarSpeed )
    {
        // 0x822A93AC / 0x822A93B0 -- the type's own pair of SPEED thresholds.
        f32 lfLeanThreshold = lpPropType->GetLeanThreshold();
        f32 lfMoveThreshold = lpPropType->GetMoveThreshold();

        // 0x822A93B4..0x822A93D8 -- the debug overlay's overrides replace BOTH thresholds
        // together (one branch, two lfsx). The smash-threshold override is not read here.
        if ( mbUseOverrides )
        {
            lfLeanThreshold = mfOverrideLeanThreshold;
            lfMoveThreshold = mfOverrideMoveThreshold;
        }

        // 0x822A93DC..0x822A93F0 -- a type whose move threshold is below 1.0f
        // (flt_82001C98) is never anything but fully physical.
        if ( lfMoveThreshold < 1.0f )
        {
            return E_PHYSICAL;                                  // li r3, 4
        }

        EPropState leDesiredState = E_STATIC;                   // 0x822A93F4  li r3, 1

        // 0x822A93F8..0x822A940C -- leaning needs the car to be above the type's LEAN
        // SPEED threshold and a joint that can lean (the short-circuit is the console's
        // two separate branches).
        if ( lfIncomingCarSpeed > lfLeanThreshold && lpPropType->GetLeanState() != 0 )
        {
            leDesiredState = E_LEANING;                         // li r3, 2
        }

        // 0x822A9410..0x822A941C -- above the type's MOVE SPEED threshold.
        if ( lfIncomingCarSpeed > lfMoveThreshold )
        {
            leDesiredState = E_PHYSICAL;                        // li r3, 4
        }

        // 0x822A9420 `cmpwi r3,4 ; bnelr` -- the extra-centre-of-mass tail is reached
        // either by falling out of the branch above (r3 already 4) or by taking it.
        if ( leDesiredState == E_PHYSICAL )
        {
            // 0x822A9428..0x822A9460 -- two specific prop graphics ids get a shifted
            // centre of mass, but only below the speed limit. That id test is the
            // compiler-inlined PropTypeData::HACKShouldMoveComOffset() (DWARF
            // SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h:110, and the DWARF
            // records the call inside THIS function at _compile/
            // BrnEntityModuleUnity.cpp:1311); it is de-inlined at
            // BrnPhysicsPropTypeData.h:222, so the two content ids no longer appear as
            // bare hex here. The `clrlwi r11,r11,24` at 0x822A9458 -- an 8-bit truncation
            // of the 0/1 the compare chain produced -- is the signature of exactly such an
            // inlined bool-returning accessor.
            if ( lpPropType->HACKShouldMoveComOffset() )
            {
                // 0x822A9464..0x822A94AC -- `vcmpgtfp. v0(80.0f splat), v13(speed splat)`
                // then the CR6 all-true bit. Written in the DWARF's source order (see the
                // constant's banner): rw::math::vpu::operator< on the VecFloat limit is
                // what reverses it into a vcmpgtfp.
                if ( lfIncomingCarSpeed < KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET )
                {
                    leDesiredState = E_PHYSICAL_WITH_EXTRA_COM_OFFSET; // li r3, 3
                }
            }
        }

        return leDesiredState;
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_03.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_03.cpp
//
// WAVE Q (2026-08-18) -- group 3 of the BREAKABLE-PROPS keystone: the replay scene
// reconciliation driver.
//
// LEDGER TUs (one class, two ids, one owner):
//   GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   class:BrnWorld::PropEntityModule
//
// BODIED HERE (1 of the group's 3):
//   BrnWorld::PropEntityModule::ReplayPreSceneUpdate   @0x822EF878  (59 insns, counted:
//        0x822EF878..0x822EF960 at a 4-byte stride, (0xE8/4)+1 == 59, no collapsed
//        jump-table line to discount)
//
// ⚠️ STILL PARKED, STILL BODIED NOWHERE IN b5-decomp/src -- AND THIS FILE CALLS BOTH OF
// THEM (see the block comment at the end of this file):
//   BrnWorld::PropEntityModule::ReplayUpdatePropsInScene @0x822DB370 (355 insns)
//        parked: scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePropsInScene.cpp
//   BrnWorld::PropEntityModule::ReplayUpdatePartsInScene @0x822DB900 (393 insns)
//        parked: scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePartsInScene.cpp
// (Those round-2 copies SUPERSEDE the round-1 ones at scratchpad/waveQ/parked/
// PropEntityModule_03_ReplayUpdate*InScene.cpp, which do not compile against this tree.)
//
// GATE-GREEN != LINK-GREEN (AGENTS.md gotcha 12), re-grepped 2026-08-18: both are
// DECLARED ONLY (BrnPropEntityModule.h:360 / :361); `grep -rn '::ReplayUpdateP.*InScene'
// --include=*.cpp b5-decomp/src` finds no definition, and no stub TU carries an inert gate for
// them. Mounting THIS partfile in
// tools/build/build_game_exe.bat is therefore two unresolved externals (LNK2019 x2) that
// `cl /c` and the compile gate cannot see. DO NOT mount this file, and do not close the
// TU on a compile-gate pass, until those two bodies land (or gates are added) and a real
// LINK proves it.
//
// Round 1 could not compile the two parked bodies because BrnReplays::PropSerialiserFrame
// (a header this lane does NOT own) modelled the per-frame sub-arrays they walk as opaque
// pads. ROUND-2 UPDATE: that HEADER blocker is retired -- the arrays are real and named --
// but ONE header request is still open and still blocks both bodies; see the block at the
// end of this file.
//
// ---- SOURCES ----------------------------------------------------------------------
//  * RAW X360 ASSEMBLY, .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EF878.json, read as
//    `assembly` (the Hex-Rays pseudocode for this function is a 7-`int` blob and is not
//    used for anything below).
//  * scratchpad/waveQ/PropEntityModule.spec.md (the wave keystone) for the include set
//    and for the two residual blocks it names (the serialiser's +0x5B byte, the 0x22
//    scene owner byte).
//
// ---- CONSOLE-OFFSET DISCIPLINE (gotcha 1) -----------------------------------------
// Not one console byte offset appears in the code below. The decode used to READ the asm
// is recorded here as PROVENANCE ONLY:
//     this + 0xD3334  mbInReplay              (lbz/stb 0(r29), r29 = this + 0xD3334)
//     this + 0xD3338  muReplayPropsInScene    (stwx r26 -> 0xD3338)
//     this + 0xD333C  muReplayPartsInScene    (stwx r26 -> 0xD333C)
//     this + 0xD3180  mPropEntitySerialiser   (r28; `lwz r11,0(r28)` == BaseSerialiser::meMode)
//     this + 0xD31DB  mPropEntitySerialiser + 0x5B == mbSkipModuleSerialise (see banner)
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET (spec §5, copied verbatim) ----------------
// ---------------------------------------------------------------------------------

namespace BrnWorld
{
    // ========================================================================
    // PropEntityModule::ReplayPreSceneUpdate @0x822EF878 -- MEASURED INSTRUCTION BY
    // INSTRUCTION against the raw asm. Step 1 of PreSceneUpdate @0x82309AB8.
    //
    // ASM SHAPE (0x822EF878 .. 0x822EF960, in order):
    //   0x822EF888  extrwi r11, r6, 8,16      ; r11 = (lUpdateSet >> 8) & 0xFF
    //   0x822EF890  clrlwi r30, r11, 31       ; r30 = that & 1  -> the "in replay" bit
    //   0x822EF8A4  lbz    r10, 0(r29)        ; r29 = this + 0xD3334 == &mbInReplay
    //   0x822EF8AC  beq    ->0x822EF8E0       ; unchanged: skip the transition work
    //   0x822EF8B4  beq    ->0x822EF8D4       ; new bit == 0  -> LEAVING replay
    //   0x822EF8C8  stwx r26(=0) -> +0xD3338  ; ENTERING replay: zero both scene counters
    //   0x822EF8CC  stwx r26(=0) -> +0xD333C
    //   0x822EF8DC  bl     LeaveReplay        ; (r3 = this, r4 = r27 = lpOutput)
    //   0x822EF8E4  stb    r30, 0(r29)        ; mbInReplay = the new bit (BOTH paths)
    //   0x822EF8EC  lwz    r11, 0(r28)        ; r28 = this + 0xD3180 == &mPropEntitySerialiser
    //   0x822EF8F0..0x822EF90C  r11 in {4,5,6} ? 1 : 0
    //          -- this is BaseSerialiser::IsPlaying() @0x821F3440 INLINED, byte for byte
    //             (E_MODE_PLAYING_PREPARING / E_MODE_PLAYING / E_MODE_PLAYING_STALLED).
    //   0x822EF918  beq    ->epilogue         ; not playing: the whole rest is skipped
    //   0x822EF924  lbzx   r11, this + 0xD31DB ; == mPropEntitySerialiser + 0x5B  (see below)
    //   0x822EF92C  bne    ->0x822EF944       ; flag set -> SKIP the serialiser Read
    //   0x822EF934  bl     PropEntitySerialiser::GetStaticLayout
    //   0x822EF940  bl     PropEntitySerialiser::Read
    //   0x822EF94C  bl     ReplayUpdatePropsInScene(lpOutput)
    //   0x822EF958  bl     ReplayUpdatePartsInScene(lpOutput)
    //
    // ⚠️ MEASURED, AND DELIBERATE: r4 (lpInput) IS NEVER READ. No instruction in the
    // function moves it, loads through it or passes it on. The parameter exists only
    // because the call site PreSceneUpdate @0x82309AB8 passes it. Do not invent a use.
    //
    // ⚠️ THE `+0x5B` GATE -- ROUND-1 GOT THIS EXACTLY BACKWARDS; CORRECTED 2026-08-18.
    // `mPropEntitySerialiser + 0x5B` (the module-relative `0xD31DB` this function loads at
    // 0x822EF924) is now homed as `BrnReplays::BaseSerialiser::mbSkipModuleSerialise`
    // (BrnReplayBaseSerialiser.h:308, accessor `SkipModuleSerialise()` :220).
    //
    // Round 1 landed the Read UNCONDITIONALLY on the spec's claim that the byte has "no
    // writer anywhere in the image". THAT CLAIM WAS FALSE, and the method that produced it
    // is the lesson: the scan covered the per-address function EXPORTS only, and the sole
    // writer's function is not in that export set -- a per-address-export scan cannot prove
    // a negative. `BrnReplays::BaseSerialiser::Construct` @0x8264C280 (no JSON export;
    // disassembled headless by two independent round-2 checkers) does
    // `8264C2C8  stb r9, 0x5B(r31)` where r9 is its 6th post-`this` argument, and
    // `BrnReplays::PropEntitySerialiser::Construct` @0x8264C6C0 passes `li r9, 1`
    // (@0x8264C6D4). Per-serialiser values: PropEntity/RaceCarEntity/TrafficEntity/Sound 1;
    // Director/GameModule/Effects/GuiModule/DirectorModule 0. Nothing clears it.
    //
    // CONSEQUENCE: on the shipped image the byte is 1, `bne cr6 -> 0x822EF944` is ALWAYS
    // taken, and GetStaticLayout()+Read() NEVER run here. That is not cosmetic --
    // BaseSerialiser::Read @0x8264C188 POPS BYTES off the playback stream, so an extra Read
    // would desync every subsequent replay read. The gate below is therefore reproduced,
    // negated, exactly as the console spells it.
    // (The two ReplayUpdate*InScene calls sit at the join 0x822EF944 and are UNGATED.)
    // Honest limit on the negative: the full-image opcode scans behind this cover the
    // direct `stb rX,0x5B(rY)` form and the module-relative indexed form; they do not prove
    // a universal negative for every possible indexed store through every serialiser.
    //
    // BOOT SAFETY (unchanged from park P1's note): on a non-replay frame the update-set
    // bit is 0 and equals mbInReplay, and everything past the flag store is gated on
    // IsPlaying(), so this function is inert outside replay playback.
    // ========================================================================
    void PropEntityModule::ReplayPreSceneUpdate( PropEntityIO::InputBuffer_PreScene* lpInput,
                                                 PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                                 BrnUpdateSet lUpdateSet )
    {
        // lpInput is declared for the call site only -- MEASURED: never read (see banner).
        (void)lpInput;

        const bool lbReplayActive = ( ( lUpdateSet >> 8 ) & 1 ) != 0;

        if ( lbReplayActive != mbInReplay )
        {
            if ( lbReplayActive )
            {
                // Entering replay: the recorded scene population starts empty.
                muReplayPropsInScene = 0;
                muReplayPartsInScene = 0;
            }
            else
            {
                // Leaving replay: drop every replay prop/part entity we put in the scene.
                LeaveReplay( lpOutput );
            }
        }

        mbInReplay = lbReplayActive;

        if ( !mPropEntitySerialiser.IsPlaying() )
        {
            return;
        }

        // 0x822EF924 `lbzx r11, this+0xD31DB` / 0x822EF92C `bne -> 0x822EF944`: non-zero
        // SKIPS the pair. On the shipped image the prop serialiser's flag is 1 (see the
        // banner), so neither call runs -- but the gate is what the console spells, and
        // the polarity is load-bearing (an extra Read desyncs the playback stream).
        if ( !mPropEntitySerialiser.SkipModuleSerialise() )
        {
            mPropEntitySerialiser.Read( mPropEntitySerialiser.GetStaticLayout() );
        }

        ReplayUpdatePropsInScene( lpOutput );
        ReplayUpdatePartsInScene( lpOutput );
    }
}

// ============================================================================
// THE TWO PARKED SIBLINGS -- why they are not in this file, and exactly what unblocks
// them. (Recorded here so the next reader does not re-derive it; the full bodies are in
// the parked files, not summarised away.)
//
//   scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePropsInScene.cpp  @0x822DB370
//   scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePartsInScene.cpp  @0x822DB900
//   (round-2 copies; they supersede scratchpad/waveQ/parked/PropEntityModule_03_*.cpp,
//    which were never compiled and do not build against this tree -- e.g. their
//    `Vector3(0.0f,0.0f,0.0f)` does not compile against this tree's aggregate Vector3.)
//
// ⚠️ ROUND-2 STATUS (2026-08-18, read off the committed header, not off a banner): THE
// FRAME-LAYOUT BLOCKER BELOW IS RETIRED, BUT THE BODIES ARE STILL PARKED -- see the second
// request at the bottom, which is the one thing still holding them.
// BrnReplayPropSerialiserFrame.h's pad ladder is gone; the frame
// is nine real BrnReplayArray members with static_asserted offsets, and the five tables
// these two bodies walk are now named `maPropPositions` (@0x0610, muLength @0x15F0),
// `maTypes` (@0x25F0), `maPartPositions` (@0x27F0), `maPartTypes` (@0x3810) and
// `maPartIds` (@0x3912) -- see scratchpad/waveQ2/replays.owner.md §2 for the full table
// and §7.2 for the call spelling. Read the counts as `<array>.muLength`, NOT as the old
// `mbAddedFlagXXXX` bools. Two arrays the round-1 request missed entirely (the prop and
// part ORIENTATION arrays at +0x1600 / +0x3000) are also now real; their count bytes are
// the placeholders round 1 called mbAddedFlag25E0 / mbAddedFlag3800.
// ⚠️ THE TWO BODIES ARE **NOT** LANDED. Round 2's lander for this group landed
// RenderReplayProp only (PropEntityModule_wQ2_01.cpp); it re-parked both ReplayUpdate*InScene
// bodies on the SECOND request below, which is STILL OPEN --
// CgsSceneManagerIO_SceneUpdate.h:194 remains `SetEntityPosition(u32, const Matrix44Affine&)`
// and is the ONLY declaration of that name on InSceneUpdateInterface (re-grepped 2026-08-18).
// That lander compile-proved the park both ways (verbatim bodies -> exactly two C2664s, one
// per body, on that one call; the same text with a stand-in declaration -> STATUS=pass).
// So: this file's two calls at :175-176 are unresolved externals until that overload and the
// two bodies land together.
//
// ---- THE ROUND-1 BLOCKER, KEPT AS THE DERIVATION THAT PINNED THE LAYOUT ----
// The replay frame's five per-frame sub-arrays had no host home: both bodies read the
// prop/part COUNT, TYPE-ID, PART-INDEX and POSITION tables out of
// BrnReplays::PropSerialiserFrame, and the header modelled every one of them as opaque
// `maPadXXXX[]` bytes -- with each array's u8 muLength byte mis-named as a standalone
// `bool mbAddedFlagXXXX` reset flag. The only alternative was
// `reinterpret_cast<...>(base + <console offset>)`, i.e. exactly the console-literal
// defect gotcha 1 exists to prevent.
//
// The blocker was NOT a guess -- the five arrays are each pinned three ways (base offset,
// element stride, and the count byte's offset) by their own X360 operator[] / PushBack
// instantiations, and every count byte lands exactly on one of the "mbAddedFlag" offsets
// the round-1 header documented:
//
//   accessor            stride  count@   frame base   static base   =>  array
//   0x822AA348           16     +0xFE0   0x0610       0x4030        BrnReplayArray<ReplayPropRecord16,254>  prop positions
//   0x822AA638            2     +0x1FC   0x25F0       0x6010        BrnReplayArray<u16,254>                 prop type ids
//   0x822AA7B0           16     +0x800   0x27F0       0x6210        BrnReplayArray<ReplayPropRecord16,128>  part positions
//   0x822AAAA0            2     +0x100   0x3810       0x7230        BrnReplayArray<u16,128>                 part type ids
//   0x822AAAA0            2     +0x100   0x3912       0x7332        BrnReplayArray<u16,128>                 part indices
//   (frame base == static base - 0x3A20, i.e. the LIVE frame; 0x822AA5B8 is the matching
//    PushBack for the 16-byte 254-element instantiation, already committed as
//    GameSource/Replays/Array_ReplayPropRecord_254_pushback.cpp.)
//
// Consequently:
//   0x0610 + 0xFE0 == 0x15F0  == the committed header's `mbAddedFlag15F0`
//   0x25F0 + 0x1FC == 0x27EC  == `mbAddedFlag27EC`
//   0x27F0 + 0x800 == 0x2FF0  == `mbAddedFlag2FF0`
//   0x3810 + 0x100 == 0x3910  == `mbAddedFlag3910`
//   0x3912 + 0x100 == 0x3A12  == `mbAddedFlag3A12`
// -- so those five "reset flags" were the arrays' muLength counters, and reading them as
// `bool` would have collapsed every count to 0/1 and silently broken the reconciliation.
// That is why the bodies were parked instead of being fitted to the round-1 header. The
// round-2 replay owner landed exactly this layout (plus the two orientation arrays), so
// the derivation above is now confirmed rather than pending.
//
// SECOND, SMALLER REQUEST (same lane, different header): both bodies need
//     void InSceneUpdateInterface::SetEntityPosition( CgsSceneManager::EntityId, Vector3 );
// The console producer @0x822B1398 takes the position in v1 (it stages
// InEventSetEntityPosition{ mPosition = v1, mEntityId = r4 } and appends); the committed
// declaration in GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h is
// `SetEntityPosition( u32, const Matrix44Affine& )` and extracts `.Pos()` itself. The
// replay frame stores a bare position -- there is no transform to hand it.
// ============================================================================

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_04.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_04.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q KEYSTONE, GROUP 4: **THE BREAK**.
// (spec: scratchpad/waveQ/PropEntityModule.spec.md, "GROUPS" row 4.)
//
// Ledger TUs covered (ONE class, TWO ledger ids -- the conductor closes them separately):
//   * GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   * class:BrnWorld::PropEntityModule
//
// Functions in this file (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::ChangePropState     @0x822EF550  (201 insns)
//   PropEntityModule::BreakPropIntoParts  @0x822FB000  (168 insns)
//
// The two share the physical-slot broker (PropZoneManager::GetPhysicalPropSlot /
// IncrementNumberOfPropsInSim) and the contact-generation forwarders the keystone added.
//
// ---- GROUND TRUTH ---------------------------------------------------------------------
// Both bodies are reconstructed from the RAW ASSEMBLY of their per-address ARTIST exports
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EF550.json and 0x822FB000.json), read
// instruction by instruction. The Hex-Rays pseudocode was used only to read the assert
// STRING LITERALS out in full; it is wrong about the shapes -- it renders ChangePropState
// as `_DWORD *(int,int,int,unsigned int,int,int)` and BreakPropIntoParts as `int()`, and it
// drops the second argument of PropZoneManager::GetProp entirely (`sub_822CDA28(a1 + 640)`).
// Declaration shapes are the DecFIGS DWARF ones already committed in
// BrnPropEntityModule.h (:1538 / :1920).
//
// ⚠️ The Feb-2007 leaked source DOES contain a BreakPropIntoParts
// (references/Feb-2007/BrnEntityModuleUnity/GameSource/World/EntityModules/
//  PropEntityModule/BrnPropEntityModule.cpp:835) but it is a DIFFERENT, EARLIER function:
// it takes a PropEntityInstance* rather than a PropEntityID, it walks the scene interface
// itself (RemoveEntity / RemoveForCollision / RemoveVolumeInstance per collision volume)
// instead of calling the cell-manager forwarders, and it builds part ids with
// `lPartId.Set(entityIndex + partIndex + 1, E_PART)` rather than SetPartIndex. NOTHING from
// that revision is used below; the shipped X360 body is the authority. Recorded so a later
// reader does not "restore" the old shape.
//
// ---- MEASURED vs INFERENCE ------------------------------------------------------------
// MEASURED (read off the asm listing):
//   * every assert site, its predicate, its rodata message and its console line number;
//   * the whole control flow of both bodies, including which leg calls what and in which
//     order (the two legs of ChangePropState order SetState differently -- see there);
//   * every member touched, by console offset, each of which maps to a NAMED member that
//     BrnPropEntityInstance.h / BrnPhysicsPropTypeData.h / BrnPropEntityModule.h already
//     pin with static_asserts (0x44 muTypeId, 0x40 muInstanceID, 0x48 mu16PartsIndex,
//     0x4A mu8Flags, 0x4C mu8PhysicsIndex, 0x4D mu8State; type 0x5D muNumberOfParts,
//     0x60 mu8ExtraTypeInfo, 0x40 maParts);
//   * the part-transform maths (0x822FB1E0..0x822FB230) and the 1-based inclusive loop
//     bound (`blt` at 0x822FB23C against a RE-READ `lbz 0x5D`);
//   * KVF_PROP_FLOOR == -1000.0f -- re-dumped for THIS file in round 1; the constant is
//     now consumed from its real home, SharedClasses/Physics/Props/BrnPropConstants.h.
// INFERENCE (each marked where it appears):
//   * that `stb 6, 0x4D(prop)` / `stb 5, 0x4D(prop)` are the compiler's fold of
//     PropEntityInstance::SetState(E_SMASHED) / SetState(E_MOVED). The asm stores the raw
//     bytes 6 and 5; the enumerator names come from the committed EPropState. The folded
//     store carries no range assert, the call does -- with a compile-time-true predicate,
//     so the behaviour is identical.
//   * that `lbz 0x60(type); cmplwi 1` is PropTypeData::IsTrafficLight(). The committed
//     accessor IS `mu8ExtraTypeInfo == 1` (BrnPhysicsPropTypeData.h:140), and the guarded
//     call is RequestTrafficLightKnockDown, so the reading is corroborated by the callee.
//
// ---- CONSOLE-LITERAL DISCIPLINE (gotcha 1) -------------------------------------------
// Not one console byte offset, stride or object size appears in the code below. The two
// traps this pair carries, both defused:
//   (1) BreakPropIntoParts walks the part descriptors with `r29 += 0x30` (0x822FB228).
//       48 is the CONSOLE sizeof(PropPartTypeData); on the host it is 64 (its embedded
//       volume pointer widened 4->8; pinned by PropPartTypeData::_AssertLayout). The loop
//       below subscripts `lpType->GetParts()[luPart - 1]` and lets the host compiler pick
//       the stride. Identical trap, identical fix, as PropCellManager::AddPropPartsToScene.
//   (2) the AddPropPartsToContactGeneration argument at 0x822FB25C..0x822FB288 is
//       `&mZoneManager.maParts[ lpProp->mu16PartsIndex ]`, computed with the CONSOLE
//       80-byte PropPartEntityInstance stride (`rotlwi 2; add; slwi 4`) and the console
//       maParts base (+435968). That whole computation belongs to the de-inlined
//       PropZoneManager::AddPropPartsToContactGeneration forwarder, which subscripts the
//       array itself; the four-argument call below does NOT reintroduce it.
//   The one numeric literal that survives is the `1` in IncrementNumberOfPropsInSim(1) --
//   a population delta, not an address.
//
// ---- NaN POLARITY (gotcha 4) ----------------------------------------------------------
// BreakPropIntoParts: NOT APPLICABLE -- it contains no floating-point COMPARE at all (the
// only FP work is the VMX part-transform multiply-add cascade).
// ChangePropState: exactly one float compare, the prop-floor tripwire at
// 0x822EF5F4 `vcmpgtfp. v13, v0` with v13 == splat(Pos().Y()) and v0 == splat(KVF_PROP_FLOOR).
// This is the VMX ORDERED greater-than (NaN in either operand yields a FALSE lane), and the
// branch tests CR6[0] "all lanes true", so the assert PASSES iff `Y > FLOOR` with NaN
// failing -- which is precisely what C++ `>` does. It is NOT the fcmpu+bge/ble form, so the
// negated-ordered-predicate rewrite does NOT apply here and writing `>` is the faithful
// spelling. (Writing `!(Y <= FLOOR)` would be the WRONG polarity: it would pass on NaN.)
// ============================================================================


namespace BrnWorld
{
    // ------------------------------------------------------------------
    // ROUND-2 UPDATE (2026-08-18): the file-local `KF_PROP_FLOOR` copy that lived here (in
    // an anonymous namespace) is GONE, along with the namespace. `BrnPhysics::Props::
    // KVF_PROP_FLOOR` now has its real DWARF home -- SharedClasses/Physics/Props/
    // BrnPropConstants.h:56 -- landed by the round-2 owner with the same measured value
    // (-1000.0f) and the same headless-idat derivation this file carried (unk_82FAD840 is
    // .bss, splat-filled at run time by the unnamed initialiser at 0x82C4B478 from
    // flt_8200D4F8 == 0xC47A0000 == -1000.0f, which is why a function-export scan finds no
    // writer). ChangePropState's third tripwire now reads the homed constant; nothing in
    // this file re-spells the value.
    // ------------------------------------------------------------------

    // ========================================================================
    // PropEntityModule::ChangePropState   @ 0x822EF550   (201 insns)
    // DWARF BrnPropEntityModule.cpp:1538.
    // ------------------------------------------------------------------------
    // Move ONE prop between simulation states, brokering a physical slot when it needs one.
    // Two legs, chosen on the OLD state (0x822EF6F4..0x822EF718):
    //
    //   * old == E_STATIC or E_LEANING  -> the prop is ALREADY in the sim on a slot it keeps.
    //     Re-issue it to the prop manager in place (Remove then Add on the SAME
    //     mu8PhysicsIndex) and store the new state. NO slot churn, no population change,
    //     no traffic-light event. (0x822EF814..0x822EF868.)
    //
    //   * old == E_NON_PHYSICAL or E_MOVED -> the prop needs a physical slot. Ask the broker;
    //     if it says no, the whole call is a no-op (the prop stays where it was). If the slot
    //     came from an EVICTION, demote the evicted prop to E_MOVED, pull its rigid body and
    //     remember it in maRecentlyRecycledProps (when there is room); otherwise the sim
    //     population grows by one. Then adopt the slot and add the body.
    //     (0x822EF71C..0x822EF80C.)
    //
    // REGISTER -> PARAMETER MAP (prologue 0x822EF554..0x822EF578, all measured):
    //   r3 -> r23 this | r4 -> r28 lpProp | r5 -> r22 lPropEntityId
    //   r6 -> r29 leOldState | r7 -> r25 leNewState | r8 -> r21 lpOutput
    // No float rides in, so gotcha 3's "a float skips its GPR slot" does not bite: the six
    // GPRs are six parameters, in order.
    //
    // E_PHYSICAL_WITH_EXTRA_COM_OFFSET never reaches the prop manager as a state: at
    // 0x822EF6A4 the body rewrites it to E_PHYSICAL and raises the separate
    // lbAddExtraComOffset flag AddPropInstance carries. Both legs pass the rewritten pair.
    void PropEntityModule::ChangePropState( PropEntityInstance* lpProp, PropEntityID lPropEntityId,
                                            EPropState leOldState, EPropState leNewState,
                                            PropEntityIO::OutputBuffer_PrePhysics* lpOutput )
    {
        // 0x822EF578. The write-locked prop-manager input interface this whole body feeds.
        BrnPhysics::Props::PropInputInterface* lpPropManagerInterface =
            lpOutput->GetPropInputInterface();
        CGS_ASSERT(lpPropManagerInterface != NULL, "lpPropManagerInterface");            // :1565

        // 0x822EF5AC. ⚠️ THIS ASSERT IS *NOT* CanChangeState -- corrected 2026-08-18.
        // Round 1 routed it through the member because the committed inline happened to be
        // the same two terms. The round-2 owner corrected CanChangeState to the DWARF's
        // three-term form (BrnPropEntityModule.h:531, with the extra
        // `leOldState == E_MOVED && leNextState == E_PHYSICAL` conjunct that both of ITS
        // folds carry), so routing through it here would assert STRICTER than the console
        // and fire on E_MOVED -> lower transitions the shipped build permits.
        // This assert's own fold is exactly two terms -- `cmpw r29,r25 ; blt ;
        // cmpwi r29,5 ; beq` -- and the full rodata message at 0x8201DFAC is literally the
        // expression spelled below. So it is spelled inline, verbatim.
        CGS_ASSERT((leOldState < leNewState) || (leOldState == E_MOVED),
                   "leOldState < leNewState || (leOldState == E_MOVED)");                // :1567

        // 0x822EF5D8. See the NaN-polarity note in this file's banner: `vcmpgtfp.` is the
        // ordered compare, so plain `>` is the faithful (and NaN-correct) spelling.
        CGS_ASSERT(lpProp->GetWorldTransform().Pos().y > BrnPhysics::Props::KVF_PROP_FLOOR,
                   "lpProp->GetWorldTransform().Pos().Y() > BrnPhysics::Props::KVF_PROP_FLOOR"); // :1569

        // 0x822EF628 / 0x822EF664 -- the legality pair. Each is a chain of `cmpwi/beq` over
        // exactly the enumerators its message names, in the asm's own order.
        CGS_ASSERT(leOldState == E_NON_PHYSICAL || leOldState == E_STATIC ||
                   leOldState == E_LEANING     || leOldState == E_MOVED,
                   "leOldState == E_NON_PHYSICAL || leOldState == E_STATIC || "
                   "leOldState == E_LEANING || leOldState == E_MOVED");                  // :1574
        CGS_ASSERT(leNewState == E_STATIC   || leNewState == E_LEANING ||
                   leNewState == E_PHYSICAL || leNewState == E_PHYSICAL_WITH_EXTRA_COM_OFFSET,
                   "leNewState == E_STATIC || leNewState == E_LEANING || "
                   "leNewState == E_PHYSICAL || leNewState == E_PHYSICAL_WITH_EXTRA_COM_OFFSET"); // :1579

        // 0x822EF6A0..0x822EF6B0. E_PHYSICAL_WITH_EXTRA_COM_OFFSET is folded away here.
        bool lbAddExtraComOffset = false;
        if (leNewState == E_PHYSICAL_WITH_EXTRA_COM_OFFSET)
        {
            lbAddExtraComOffset = true;
            leNewState          = E_PHYSICAL;
        }

        // 0x822EF6B4..0x822EF6C8. `addis r3,r23,0xD; addi r3,r3,-0x2278` == this + 0xCDD88 ==
        // mpPropPhysicsDataHeader; the truncated `BrnPhysics__Props__Prop` symbol is that
        // ResourcePtr's GetMemoryResource. The type id is `lhz 0x44(prop)` == muTypeId.
        const BrnPhysics::Props::PropTypeData* lpPropTypeData =
            mpPropPhysicsDataHeader.GetMemoryResource()->GetType(lpProp->muTypeId);
        CGS_ASSERT(lpPropTypeData != NULL, "lpPropTypeData");                            // :1590

        // ---- [DIAG] NOT IN THE X360 BINARY -- the BUG-WAVE STATE rung --------------------
        // ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen. Set BRN_PROP_DIAG.
        //
        // The two rungs either side of this one (CLASSIFY / COMMIT in PropEntityModule_wQ_05.cpp)
        // can only say that a transition was asked for and whether it stuck. This one names WHICH
        // OF THE TWO LEGS ran, and -- the reason it exists -- catches the ONE silent refusal in
        // the whole promotion chain: leg B's `if (!GetPhysicalPropSlot(...)) return;`. There are
        // only KU_MAX_PHYSICAL_PROPS == 15 physical prop slots, the console's own broker returns
        // false when it cannot free one, and that return is a NO-OP with no assert and no log --
        // a prop that loses that race simply never reacts, at any speed. A `legB-NOSLOT` line is
        // therefore a complete diagnosis on its own.
        // Rate-limited first-N; the getenv latch is a function-local static.
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siStateLinesLeft = 32;

            if ( sbPropDiag && siStateLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siStateLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[prop-diag] STATE prop=" << lPropEntityId.GetValue()
                    << " old="      << static_cast<s32>( leOldState )
                    << " new="      << static_cast<s32>( leNewState )
                    << " extraCom=" << ( lbAddExtraComOffset ? 1 : 0 )
                    << " leg="      << ( ( leOldState == E_STATIC || leOldState == E_LEANING ) ? "A" : "B" )
                    // ⚠️ RENAMED 2026-08-23. This read happens BEFORE leg B brokers a slot,
                    // so on leg B it is the prop's PREVIOUS mu8PhysicsIndex (0 for a prop
                    // that has never been physical) -- NOT the slot it is about to get.
                    // Spelled `physIdx` it read as "the allocator handed out slot 0 again",
                    // and a whole bug-wave finding was raised off four such lines. The slot
                    // actually granted is printed by the legB-SLOT rung below and by the
                    // COMMIT rung (PropEntityModule_wQ_05.cpp); in that same drive they read
                    // 0 / 0 / 1 / 0, i.e. the pool behaved.
                    << " physIdxBefore=" << static_cast<s32>( lpProp->mu8PhysicsIndex )
                    << "\n";
            }
        }

        if (leOldState == E_STATIC || leOldState == E_LEANING)
        {
            // ---- leg A (0x822EF814): already in the sim, keep the slot ----------------
            // Both calls read mu8PhysicsIndex fresh off the prop (`lbz 0x4C` at 0x822EF818
            // and again at 0x822EF828) -- nothing between them can change it, so the two
            // reads are one value.
            lpPropManagerInterface->RemovePropInstance(lPropEntityId, lpProp->mu8PhysicsIndex);
            lpPropManagerInterface->AddPropInstance(lPropEntityId,
                                                    lpProp->muTypeId,
                                                    lpProp->mu8PhysicsIndex,
                                                    lpProp->GetWorldTransform(),
                                                    lbAddExtraComOffset,
                                                    leNewState);
            // 0x822EF840..0x822EF868 -- the compiler INLINED SetState here (its range assert
            // and its `stb 0x4D`), and it lands AFTER the add. The other leg calls SetState
            // BEFORE its add. That ordering difference is real; it is reproduced, not tidied.
            lpProp->SetState(leNewState);
            return;
        }

        // ---- leg B (0x822EF71C): broker a physical slot ------------------------------
        // The X360 hoists this local's zero-init all the way up to 0x822EF5F8 (`stw r11(0),
        // var_68`, immediately before the prop-floor assert). Behaviourally the broker
        // overwrites it on success and nothing reads it on failure, so it is initialised at
        // its declaration here.
        s32          liPhysicsSlot = 0;
        PropEntityID lEvictedId;
        bool         lbEvicted     = false;

        if (!mZoneManager.GetPhysicalPropSlot(lPropEntityId, liPhysicsSlot, lEvictedId, lbEvicted))
        {
            // ---- [DIAG] NOT IN THE X360 BINARY -- see the STATE rung above ---------------
            // ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen.
            // THE SILENT REFUSAL. The console returns here with no assert and no message; a
            // prop that cannot get one of the 15 physical slots never reacts and nothing in
            // the log says so. This line is the only way a boot-drive can tell "the promotion
            // never happened" from "the promotion happened and the sim did nothing".
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                static s32        siNoSlotLinesLeft = 16;

                if ( sbPropDiag && siNoSlotLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siNoSlotLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[prop-diag] STATE legB-NOSLOT prop=" << lPropEntityId.GetValue()
                        << " old=" << static_cast<s32>( leOldState )
                        << " new=" << static_cast<s32>( leNewState )
                        << "\n";
                }
            }

            // 0x822EF73C -- no slot available: the prop keeps its old state entirely.
            return;
        }

        if (lbEvicted)
        {
            // 0x822EF750. The slot was taken from another prop. `sub_822CDA28` is
            // PropZoneManager::GetProp(PropEntityID) -- Hex-Rays drops its second argument;
            // the asm passes r4 = the evicted id.
            PropEntityInstance* lpEvictedProp = mZoneManager.GetProp(lEvictedId);
            // INFERENCE (named in the banner): `stb 5, 0x4D` == SetState(E_MOVED).
            lpEvictedProp->SetState(E_MOVED);

            // The evicted prop's rigid body is pulled off the slot WE are about to take.
            lpPropManagerInterface->RemovePropInstance(lEvictedId, liPhysicsSlot);

            // 0x822EF77C. `addis r30,r23,0xD; addi r30,r30,-0x2198` == this + 0xCDE68 ==
            // maRecentlyRecycledProps (::Array<PropEntityID,15>). A full list simply drops
            // the record -- the asm has no else arm.
            if (!maRecentlyRecycledProps.IsFull())
            {
                maRecentlyRecycledProps.Append(lEvictedId);
            }
        }
        else
        {
            // 0x822EF7A8. `lwz/addi/stw 0x904(this+0x280)` == a bare `++miNumPropsInSim` on
            // the embedded PropCellManager (0x280 + 0x904 == 2948). The de-inlined forwarder
            // takes the DWARF's int32_t delta, so the folded form is the delta == 1 case.
            mZoneManager.IncrementNumberOfPropsInSim(1);
        }

        // 0x822EF7B4. Here the state lands BEFORE the add (contrast leg A).
        lpProp->SetState(leNewState);
        lpProp->SetPhysicsIndex(liPhysicsSlot);

        // ---- [DIAG] NOT IN THE X360 BINARY -- the granted slot, ADDED 2026-08-23 -------
        // ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen.
        // The STATE rung above can only print the slot the prop had BEFORE the broker ran.
        // This one prints what GetPhysicalPropSlot actually granted, plus whether it had to
        // evict a resident, so "slot 0 every time" can be told from "slot 0 because the
        // previous tenant was released" without cross-reading the COMMIT rung.
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siSlotLinesLeft = 32;

            if ( sbPropDiag && siSlotLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siSlotLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[prop-diag] STATE legB-SLOT prop=" << lPropEntityId.GetValue()
                    << " grantedSlot=" << liPhysicsSlot
                    << " evicted="     << ( lbEvicted ? 1 : 0 )
                    << " evictedProp=" << lEvictedId.GetValue()
                    << "\n";
            }
        }

        lpPropManagerInterface->AddPropInstance(lPropEntityId,
                                                lpProp->muTypeId,
                                                liPhysicsSlot,
                                                lpProp->GetWorldTransform(),
                                                lbAddExtraComOffset,
                                                leNewState);

        // 0x822EF7EC. `lbz 0x60(type); cmplwi 1` == PropTypeData::IsTrafficLight(); the
        // knock-down request carries `lwz 0x40(prop)` == muInstanceID. Only the slot-brokering
        // leg raises it -- a prop that was already E_STATIC/E_LEANING has been knocked down
        // once already.
        if (lpPropTypeData->IsTrafficLight())
        {
            lpOutput->GetPropToTrafficInterface()
                ->RequestTrafficLightKnockDown(lpProp->muInstanceID);
        }
    }

    // ========================================================================
    // PropEntityModule::BreakPropIntoParts   @ 0x822FB000   (168 insns)
    // DWARF BrnPropEntityModule.cpp:1920.
    // ------------------------------------------------------------------------
    // THE BREAK. Shatter one whole prop into its part instances: mark it E_SMASHED, pull the
    // whole-prop body out of contact generation and out of the scene, seat every part at the
    // prop's pose offset by that part's design-time offset, then push the parts into the
    // scene and into contact generation.
    //
    // REGISTER -> PARAMETER MAP (prologue 0x822FB014..0x822FB020, measured):
    //   r3 -> r26 this | r4 -> r19 lPropEntityId | r5 -> r28 lpOutput
    // (r19 is fed straight to `sub_822CDA28` == PropZoneManager::GetProp(PropEntityID), and
    //  r28 is the object every `sub_822B9738` == OutputBuffer_PreScene::GetSceneInputInterface
    //  call is made on -- which is what fixes both types.)
    //
    // r27 == this + 0x280 == &mZoneManager throughout; PropCellManager is embedded at
    // offset 0 inside it, which is why the raw asm appears to call PropCellManager methods
    // on the zone manager's address (gotcha 2 -- three classes on one pointer). The calls
    // below go through the de-inlined PropZoneManager forwarders, per the keystone's
    // convention.
    void PropEntityModule::BreakPropIntoParts( PropEntityID lPropEntityId,
                                               PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        PropEntityInstance* lpProp = mZoneManager.GetProp(lPropEntityId);

        // 0x822FB030 `lbz 0x4A(prop); rlwinm r11,r11,0,29,29` -- mask 0x00000004 ==
        // KU_ADDED_TO_CONTACT_GEN_BIT. A prop that never entered contact generation cannot
        // be broken by one, and the whole body is skipped (branch to the epilogue).
        if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) == 0)
        {
            return;
        }

        // 0x822FB040..0x822FB0FC. The first test is GetState() INLINED (its
        // "mu8State < E_STATE_COUNT" assert at BrnPropEntityInstance.h:653, then `cmplwi 4`);
        // the other two are real out-of-line GetState calls. Three calls, three states.
        //
        // The failing arm builds its message with CgsDev::StrStream
        // (`"Can't break prop in state: " << GetState() << "\n"`); collapsed to a plain
        // CGS_ASSERT per the convention this TU already uses for StrStream asserts
        // (BrnPropZoneManager.cpp:1356). The streamed state value is dropped, the literal is
        // verbatim.
        CGS_ASSERT(lpProp->GetState() == E_PHYSICAL ||
                   lpProp->GetState() == E_STATIC   ||
                   lpProp->GetState() == E_LEANING,
                   "Can't break prop in state: ");                                       // :2127

        // 0x822FB100. INFERENCE (named in the banner): `stb 6, 0x4D` == SetState(E_SMASHED).
        lpProp->SetState(E_SMASHED);

        // 0x822FB10C -> r16. The scene-update interface is fetched THREE separate times in
        // the shipped body (0x822FB10C, 0x822FB14C, 0x822FB178); only the first result is
        // kept live across the part loop, for the two Add* calls at the end.
        // ⚠️ WHY THE REPEAT IS KEPT (round-1 wording corrected -- it said the getter "takes
        // the buffer's write lock, so the repeat is observable", which the asm contradicts):
        // sub_822B9738 == OutputBuffer_PreScene::GetSceneInputInterface ASSERTS the buffer is
        // already locked for writing -- `lbz r11,0(buf) ; extrwi r11,r11,1,28`, and on a
        // clear bit it fires the assert whose message is "Not locked for writing\n" (StrStream
        // 0x822B977C-0x822B97B4, FireAssert 0x822B97C8) -- then `addi r3,r28,0x420`. It takes
        // no lock and has no side effect beyond that assert. The three calls are reproduced
        // call for call because that is what the shipped body does, not because a lock is
        // acquired.
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput =
            lpOutput->GetSceneInputInterface();

        // 0x822FB118..0x822FB128. Same mpPropPhysicsDataHeader -> GetType(muTypeId) pair as
        // ChangePropState above.
        const BrnPhysics::Props::PropTypeData* lpType =
            mpPropPhysicsDataHeader.GetMemoryResource()->GetType(lpProp->muTypeId);

        // [DIAG] NOT IN THE X360 BINARY. The middle rung of the wave-Q4 break probe (set
        // BRN_PROP_DIAG): reaching here means the smash test passed and the prop is being
        // taken apart. Placed after lpType is fetched because the part count comes from it,
        // and after SetState(E_SMASHED) so a line here means the state change is committed.
        // The latch is evaluated ONCE (a static bool), not per call.
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[prop-diag] BREAK prop=" << lPropEntityId.GetValue()
                    << " type=" << lpProp->muTypeId
                    << " parts=" << lpType->GetNumberOfParts()
                    << "\n";
            }
        }

        // 0x822FB12C..0x822FB144. `li r11,3; sldi r11,r11,56; std r11, var_E0` is the
        // compiler's fold of PropVolumeInstanceID's default constructor, which seeds the
        // embedded entity word's OWNER byte with E_ENTITYTYPE_PROP (== 3) at bit 56. The
        // committed constructor already does exactly that, so the declaration alone
        // reproduces the store; then the prop's id is installed over it.
        PropVolumeInstanceID lVolumeInstanceID;
        lVolumeInstanceID.SetPropEntityId(lPropEntityId);

        // 0x822FB168 / 0x822FB194. Both take a FRESHLY fetched scene interface (r7 comes from
        // the 0x822FB14C and 0x822FB178 getter calls, not from r16).
        mZoneManager.RemovePropFromContactGeneration(lpProp, lpType, lVolumeInstanceID,
                                                     lpOutput->GetSceneInputInterface());
        mZoneManager.RemovePropFromScene(lpProp, lpType, lVolumeInstanceID,
                                         lpOutput->GetSceneInputInterface(),
                                         &mPropEntitySerialiser);

        // ---- seat every part at the prop's pose --------------------------------------
        // 0x822FB198..0x822FB23C. The asm guards the loop on `lbz 0x5D(type) != 0` and then
        // runs a do-while over 1-BASED part indices, RE-READING the part count at the bottom
        // of every iteration (0x822FB234). The guarded do-while with a re-read bound is
        // exactly the `for` below; the count cannot change inside the body (lpType is a
        // const serialised record), so the two are equivalent as well as same-shaped.
        //
        // ⚠️ CONSOLE STRIDE TRAP: the descriptor walk is `r29 += 0x30` -- see banner note (1).
        const Matrix44Affine& lrPropTransform = lpProp->mWorldTransform;

        for (u32 luPart = 1; luPart <= lpType->GetNumberOfParts(); ++luPart)
        {
            const BrnPhysics::Props::PropPartTypeData& lrPartType =
                lpType->GetParts()[luPart - 1];

            // 0x822FB1D0/0x822FB1DC. A local copy of the prop id with the part field set;
            // the asm stores the incoming word into the stack slot and calls SetPartIndex on
            // it. (The slot is the same one the volume id used -- the compiler reused it
            // after r18 had already taken the volume id's value at 0x822FB150. Two distinct
            // locals here, one stack slot there.)
            PropEntityID lPartEntityId = lPropEntityId;
            lPartEntityId.SetPartIndex(luPart);

            // 0x822FB1E0..0x822FB214. The textbook affine point transform.
            //
            // ⚠️ IDA OPERAND-ORDER TRAP, AND THE TWO FORMS PRINT DIFFERENTLY -- MEASURED,
            // corrected 2026-08-18 (the round-1 wording quoted the base-VMX rule while
            // annotating VMX128 instructions, which would have inverted this maths):
            //   * VMX128 -- `vmaddfp128 vD,vA,vC,vB` prints in ISA MNEMONIC order, so the
            //     ADDEND IS THE LAST OPERAND: vD = vA*vC + vB. That is the form HERE:
            //     0x822FB204 `vmulfp128 v12, v127, v12` then 0x822FB20C
            //     `vmaddfp128 v12, v126, v11, v12` with v11 = `vspltw v0,v0,1` (off.y,
            //     0x822FB1EC) and v12 the running accumulator -- accumulator LAST.
            //   * BASE VMX -- `vmaddfp vD,vA,vB,vC` prints in RAW FIELD order, so the
            //     addend is the THIRD operand: vD = vA*vC + vB. Contrast site in the same
            //     export: 0x822B8188 `vmaddfp v13, v12, v13, v8` inside
            //     PropEntityInstance::InitialiseFromData, where the accumulator v13 is
            //     third and the fresh splat v8 is fourth.
            // Applying the base rule to `vmaddfp128 v12, v126, v11, v12` would read
            // v12 = v126*v12 + v11, i.e. accumulating into a splat -- nonsense, and not
            // what the code below (correctly) does.
            //     partPos = M.Right()*offset.x + M.Up()*offset.y + M.At()*offset.z + M.Pos()
            // Kept as 4-lane Vector3 expressions rather than a TransformPoint call because
            // the console carries the w lane through -- identical to
            // PropCellManager::AddPropPartsToScene, which is the same maths on the same data.
            const Vector3& lrOffset = lrPartType.GetOffset();
            Vector3 lPartPosition = lrPropTransform.xAxis * lrOffset.x;
            lPartPosition = lrPropTransform.yAxis * lrOffset.y + lPartPosition;
            lPartPosition = lrPropTransform.zAxis * lrOffset.z + lPartPosition;
            lPartPosition = lrPropTransform.wAxis + lPartPosition;

            // 0x822FB218 `sub_822CDB90` == PropZoneManager::GetPart(PropEntityID).
            // 0x822FB21C..0x822FB230: the part inherits the prop's ORIENTATION verbatim --
            // the three rotation rows are block-copied (stvx128 into part+0/+0x10/+0x20) and
            // only the translation row differs.
            PropPartEntityInstance* lpPart = mZoneManager.GetPart(lPartEntityId);
            lpPart->mWorldTransform.xAxis = lrPropTransform.xAxis;
            lpPart->mWorldTransform.yAxis = lrPropTransform.yAxis;
            lpPart->mWorldTransform.zAxis = lrPropTransform.zAxis;
            lpPart->mWorldTransform.wAxis = lPartPosition;
        }

        // 0x822FB258 / 0x822FB28C. Both use the FIRST scene interface (r16), and both are
        // handed the same volume id (r18) the two removals used.
        //
        // ⚠️ The contact-generation call is FOUR arguments here; PropCellManager's takes
        // FIVE. The console's EXTRA parameter -- positionally the SECOND, r5 =
        // &maParts[lpProp->mu16PartsIndex], built at 0x822FB264-0x822FB288 with the 80-byte
        // console stride -- is resolved inside the de-inlined forwarder (see banner note
        // (2), and BrnPropCellManager.h:236 for the callee's own parameter order:
        // (lpProp, lpFirstPart, ...)). It is NOT the fifth ARGUMENT of this call.
        mZoneManager.AddPropPartsToScene(lpProp, lpType, lVolumeInstanceID, lpSceneInput,
                                         &mPropEntitySerialiser);
        mZoneManager.AddPropPartsToContactGeneration(lpProp, lpType, lVolumeInstanceID,
                                                     lpSceneInput);
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_05.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_05.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q, GROUP 5: THE CONTACT FAN-OUT.
// One queue decode, one router, two handlers. This partfile lands two of the three:
//
//   PropEntityModule::ProcessPotentialContacts        @0x822FA538  (213 insns)  <- here
//   PropEntityModule::ProcessPotentialContactWithProp @0x822DB038  (206 insns)  <- here
//   PropEntityModule::ProcessPotentialContactWithPart @0x822EEDA8  (126 insns)  <- PARKED
//       in round 1, scratchpad/waveQ/parked/PropEntityModule_05_ProcessPotentialContactWithPart.cpp
//       ROUND-2 UPDATE (2026-08-18): its single blocker is RETIRED. The one declaration it
//       needed -- a reader of mZoneManager.mCellManager.miNumPartsInSim, the physical-PART
//       budget gate at 0x822EEE70..0x822EEE80 -- landed as
//       `PropZoneManager::IsInPhysicalBudgetForParts(s32)` (BrnPropZoneManager.h:488, DWARF
//       :312). The round-2 owner compiled the parked body against it unchanged
//       (STATUS=pass, scratchpad/waveQ2/worldio.owner.md §0). The body is landed by the
//       round-2 LANDER, not by this file.
//
// LEDGER NOTE: ProcessPotentialContactWithProp is ledger-attributed to the
// GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h catch-all TU, not to
// BrnPropEntityModule.cpp -- an inlining artefact. It is a member of PropEntityModule
// (declared BrnPropEntityModule.h:512, DWARF BrnPropEntityModule.cpp:1265) and it is bodied
// here with its sibling. Both ledger TU ids are owned by this lane.
//
// SOURCE-OF-TRUTH. Every line below is read off the X360 ARTIST **assembly** for the two
// addresses above (the Hex-Rays pseudocode is used only as a map). Declaration shape comes
// from the committed BrnPropEntityModule.h, whose entries quote the DecFIGS DWARF.
//
// ---- LAYOUT DISCIPLINE ------------------------------------------------------------------
// Not one console byte offset appears in the code. The offset -> member decode used to READ
// the asm is recorded per site, in comments, and every access goes through a named member or
// accessor. The console strides that DO appear in the asm and are NOT reproduced:
//   * PotentialContact stride 80 (BaseEventQueue<PotentialContact>::GetEvent @0x822AC2D8
//     computes idx*80 with `slwi 2 ; add ; slwi 4`) -- the host `sizeof` is whatever the
//     compiler picks; indexing goes through GetEvent().
//   * maRaceCarVelocity stride 16 (`slwi r11,r11,4` at 0x822DB100) -- read through
//     GetRaceCarSpeed(), the DWARF accessor for the splatted .w lane, instead.
//   * queue length at queue+8 (`lwz r11, 8(r24)`) -- that is the CONSOLE offset of
//     BaseEventQueue<T>::miLength; on the host mpEvents widens 4->8 and it moves. Read
//     through GetLength().
//
// ---- CONSTANTS, ALL VERIFIED IN THIS LANE ----------------------------------------------
//   flt_82001C98 == 1.0f -- re-dumped from the IDB in this lane (`3f800000`), the
//   move/smash-threshold floor ProcessPotentialContactWithProp compares against.
//
// ---- ⚠ DEFECT FOUND IN A COMMITTED HEADER -- NOW FIXED THERE ---------------------------
// Round 1 found that BrnPropEntityModule.h:493 defined
//     bool CanChangeState( EPropState leOldState, EPropState leNewState ) const
//     { return ( leOldState < leNewState ) || ( leOldState == E_MOVED ); }
// which is ChangePropState ASSERT text ("leOldState < leNewState || (leOldState ==
// E_MOVED)", fold at 0x822EF5AC-0x822EF5B8) and NOT the predicate. The two places the
// shipped build folds the PREDICATE ITSELF --
//     ProcessPotentialContacts        @0x822FA760..0x822FA7BC
//     ProcessPotentialContactWithProp @0x822DB2E0..0x822DB33C
// -- are byte-for-byte the same shape:
//     r11 = (new > old) ; r9 = r11
//     if (old == 5) { r11 = (new == 4) } else { r11 = 0 }
//     take the branch when (r9 | r11)
// i.e.  ( leOldState < leNewState ) || ( leOldState == E_MOVED && leNewState == E_PHYSICAL ),
// which is STRICTER than the old inline (that one also allowed E_MOVED -> E_NON_PHYSICAL /
// E_STATIC / E_LEANING / E_PHYSICAL_WITH_EXTRA_COM_OFFSET).
// ROUND-2 UPDATE (2026-08-18): the header owner landed the correction at
// BrnPropEntityModule.h:531, in the DWARF three-term spelling
// (lbHigherState / lbMovedToPhysical / lbMovedToSmashed, DWARF header lines 397/400/403;
// the third term is invisible in both ARTIST folds only because E_MOVED==5 < E_SMASHED==6
// makes it subsumed by lbHigherState), non-const, second parameter leNextState. Both bodies
// below now CALL the member; the round-1 file-local copy is gone.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET (spec §5, verbatim) ----------------------

namespace BrnWorld
{
    // ------------------------------------------------------------------------------------
    // [DIAG] NOT IN THE X360 BINARY -- the two rate limits for this file's BRN_PROP_DIAG
    // rungs. ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen.
    // First-N, not per-frame: a car sliding along a fence produces one contact per prop per
    // frame, so an ungated line here is a log flood that hides its own answer. 24 is enough
    // to cover a teleport-and-throttle run into a run of kerbside props plus one gate.
    // ------------------------------------------------------------------------------------
    static const s32 KI_DIAG_MAX_CLASSIFY_LINES = 24;
    static const s32 KI_DIAG_MAX_COMMIT_LINES   = 24;

    // ROUND-2 UPDATE (2026-08-18): this file no longer has an anonymous namespace at all.
    //   * `KI_MAX_CONTACTED_PROPS` was a file constant; the DWARF makes it FUNCTION-LOCAL to
    //     ProcessPotentialContacts (BrnPropEntityModule.cpp:1179), which is where it now
    //     lives -- with the same grounding it carried here: the tripwire the console bakes
    //     at 0x822FA5D0 / 0x822FA65C (`cmpwi r27, 0x1F4`), whose message names the constant
    //     ("liNumContactedProps < KI_MAX_CONTACTED_PROPS", BrnPropEntityModule.cpp:1224
    //     and :1242).
    //   * The two file-local HELPERS are gone, because both now have their real DWARF homes:
    //       - `IsOverheadSignGraphicsId(u32)` -> `PropTypeData::IsOverheadSign()`
    //         (BrnPhysicsPropTypeData.h:179, DWARF :114). It is grounded on the very fold
    //         this file measured, 0x822DB198..0x822DB1E0, and the DWARF records it as an
    //         INLINED SUBROUTINE three times inside ProcessPotentialContactWithProp
    //         (_compile/BrnEntityModuleUnity.cpp:32038-32039, :32057). Keeping a fourth
    //         hand-spelled copy of the 0x7A4D6/0x7A4C2/0x7B8C2 triple was exactly the
    //         divergence risk gotcha 7 exists to stop.
    //       - `CanChangePropStateMeasured(...)` -> the class own `CanChangeState(...)`
    //         (BrnPropEntityModule.h:531). Round 1 forked it because the committed inline
    //         was then a WEAKER two-term predicate; the round-2 owner corrected it to the
    //         DWARF three-term form, which is the same predicate this file measured at
    //         0x822FA760..0x822FA7BC / 0x822DB2E0..0x822DB33C plus a third conjunct
    //         (E_MOVED -> E_SMASHED) that 5 < 6 already subsumes -- so the fork is now a
    //         pure duplicate and is retired.
    //         NOTE it is NOT the same predicate as ChangePropState's assert: that guard
    //         really is two terms and is spelled inline at PropEntityModule_wQ_04.cpp.
    //         Do not re-merge them.

    // ============================================================================
    // PropEntityModule::ProcessPotentialContacts   @0x822FA538  (213 insns)
    // DWARF BrnPropEntityModule.cpp:1171.
    //
    // Drain the scene's per-frame potential-contact queue. Each record names TWO collision
    // volume instances; for each of them independently, when the volume belongs to a PROP the
    // contact is routed to the part handler (part index non-zero) or to the whole-prop handler
    // (part index zero). Whole props are additionally remembered in a frame-local list, and a
    // second pass then commits each remembered prop's pending state change.
    //
    // ---- ASM -> C++ DECODE (offsets are CONSOLE; every one is a named member here) --------
    //   0x822FA554  bl 0x822B92A0   InputBuffer_PrePhysics::GetPotentialContactQueue() const
    //                               (reads this+0x10 behind the read-lock tripwire)
    //   0x822FA560  lwz r11, 8(r24)         queue->GetLength()   (console offset of miLength;
    //                                       re-read every iteration -- the loop condition)
    //   0x822FA58C  bl 0x822AC2D8   BaseEventQueue<PotentialContact>::GetEvent(int) const
    //   0x822FA594  ld  0x30(r28) ; srdi 32 ; clrlwi 0
    //                               -> PotentialContact::muVolumeInstanceIdA (+48), HIGH dword
    //   0x822FA598  ld  0x38(r28) ; srdi 32 ; clrlwi 0
    //                               -> PotentialContact::muVolumeInstanceIdB (+56), HIGH dword
    //          The 64-bit VolumeInstanceId carries the packed 32-bit scene EntityId word in
    //          its HIGH dword (CgsVolumeInstanceId.h: KU_ENTITY_ID_START_INDEX == 32), which
    //          is exactly what `>> 32` yields, endian-independently.
    //   0x822FA5AC  srwi r11, r31, 24       EntityId::GetOwner()      (KU_OWNER_BASE == 24)
    //   0x822FA5B8  clrlwi r11, r31, 22     EntityId::GetPartIndex()  (low 10 bits)
    //   0x822FA608  lvx128 v1, r28, r26 (r26 == 0x20)
    //                               -> PotentialContact::mNormal (+32), the vector argument
    //   0x822FA6E0  addi r23, r21, 0x280    &mZoneManager
    //   0x822FA70C  bl sub_822CDA28         PropZoneManager::GetProp(PropEntityID)
    //   0x822FA714  lbz 0x4E(r30)           PropEntityInstance::mu8NextState (GetNextState)
    //   0x822FA738  lbz 0x4D(r30)           PropEntityInstance::mu8State     (GetState)
    //   0x822FA874  stb r31, 0x4E(r30)      SetNextState( GetState() )
    // ============================================================================
    void PropEntityModule::ProcessPotentialContacts( const PropEntityIO::InputBuffer_PrePhysics* lpInput,
                                                     PropEntityIO::OutputBuffer_PrePhysics* lpOutput )
    {
        // The frame-local list of whole props that took a contact this frame. Lives on the
        // stack exactly as it does on the console (`r29 = sp + var_840`, 4-byte elements).
        // DWARF names (_compile/BrnEntityModuleUnity.cpp:32124-32130): the bound is a
        // FUNCTION-LOCAL `const int32_t KI_MAX_CONTACTED_PROPS` (BrnPropEntityModule.cpp:1179)
        // and the array is `PropEntityID[500] laContactedProps` (:1180) -- `la`, a LOCAL
        // array, not the `ma` MEMBER prefix round 1 used. It must not persist across frames,
        // which matters here because the 500-entry overrun below is non-gating.
        const s32    KI_MAX_CONTACTED_PROPS = 500;
        PropEntityID laContactedProps[ KI_MAX_CONTACTED_PROPS ];
        s32          liNumContactedProps = 0;

        const PropEntityIO::InputBuffer_PrePhysics::OutPotentialContactQueue* lpContactQueue =
            lpInput->GetPotentialContactQueue();

        // The console re-loads the queue length at the BOTTOM of every iteration
        // (0x822FA6C4), so the bound is re-read, not cached.
        for ( s32 liContact = 0; liContact < lpContactQueue->GetLength(); ++liContact )
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact& lrContact =
                lpContactQueue->GetEvent( liContact );

            const CgsSceneManager::EntityId lEntityIdA(
                static_cast<u32>( lrContact.muVolumeInstanceIdA.muId >> 32 ) );
            const CgsSceneManager::EntityId lEntityIdB(
                static_cast<u32>( lrContact.muVolumeInstanceIdB.muId >> 32 ) );

            // ---- side A --------------------------------------------------------------
            if ( lEntityIdA.GetOwner() == E_ENTITYTYPE_PROP )
            {
                if ( lEntityIdA.GetPartIndex() != 0 )
                {
                    ProcessPotentialContactWithPart( PropEntityID( static_cast<u32>( lEntityIdA ) ),
                                                     lpOutput->GetPropInputInterface() );
                }
                else
                {
                    const PropEntityID lPropEntityId( static_cast<u32>( lEntityIdA ) );

                    // NON-GATING tripwire, exactly as shipped: the console fires the assert
                    // and then stores anyway (0x822FA5D4 `blt` branches only around the assert;
                    // 0x822FA5F4 `stw r31,0(r29)` + 0x822FA5FC `addi r29,r29,4` run on BOTH
                    // paths). THE OVERRUN IS DELIBERATE AND FAITHFUL, AND ITS HOST FAILURE MODE
                    // DIFFERS FROM THE CONSOLE'S: past element 500 this writes off the end of
                    // `laContactedProps` into the caller's frame. The X360 walked its own frame
                    // quietly; on the host MSVC /GS will trip the frame cookie and ASAN/the CRT
                    // will abort here. A crash at this store is THE SHIPPED DEFECT SURFACING,
                    // not a transcription error -- do not "fix" it by gating the store.
                    CGS_ASSERT( liNumContactedProps < KI_MAX_CONTACTED_PROPS,
                                "liNumContactedProps < KI_MAX_CONTACTED_PROPS" );
                    laContactedProps[ liNumContactedProps ] = lPropEntityId;
                    ++liNumContactedProps;

                    // The header spells the third parameter `lContactImpulse`; MEASURED, the
                    // vector the console loads into v1 is the record's +0x20 row, i.e.
                    // PotentialContact::mNormal.
                    ProcessPotentialContactWithProp( lPropEntityId, lEntityIdB, lrContact.mNormal,
                                                     lpOutput->GetPropInputInterface() );
                }
            }

            // ---- side B (the same block with A/B swapped; asserts at :1242) -----------
            if ( lEntityIdB.GetOwner() == E_ENTITYTYPE_PROP )
            {
                if ( lEntityIdB.GetPartIndex() != 0 )
                {
                    ProcessPotentialContactWithPart( PropEntityID( static_cast<u32>( lEntityIdB ) ),
                                                     lpOutput->GetPropInputInterface() );
                }
                else
                {
                    const PropEntityID lPropEntityId( static_cast<u32>( lEntityIdB ) );

                    // Same non-gating overrun as side A, same host caveat: 0x822FA660 `blt`
                    // skips only the assert; 0x822FA680 `stw r30,0(r29)` always runs. See the
                    // note at the side-A store.
                    CGS_ASSERT( liNumContactedProps < KI_MAX_CONTACTED_PROPS,
                                "liNumContactedProps < KI_MAX_CONTACTED_PROPS" );
                    laContactedProps[ liNumContactedProps ] = lPropEntityId;
                    ++liNumContactedProps;

                    ProcessPotentialContactWithProp( lPropEntityId, lEntityIdA, lrContact.mNormal,
                                                     lpOutput->GetPropInputInterface() );
                }
            }
        }

        // ---- second pass: commit each contacted prop's pending state change --------------
        // The console counts liNumContactedProps back down to zero while walking the list
        // (0x822FA870 `addi r27, r27, -1`); a forward walk over the recorded entries is the
        // same traversal.
        for ( s32 liIndex = 0; liIndex < liNumContactedProps; ++liIndex )
        {
            const PropEntityID  lPropEntityId = laContactedProps[ liIndex ];
            PropEntityInstance* lpProp        = mZoneManager.GetProp( lPropEntityId );

            const EPropState leState     = lpProp->GetState();
            const EPropState leNextState = lpProp->GetNextState();

            const bool lbCanChange = CanChangeState( leState, leNextState );

            if ( lbCanChange )
            {
                ChangePropState( lpProp, lPropEntityId, leState, leNextState, lpOutput );
            }

            // ---- [DIAG] NOT IN THE X360 BINARY -- the BUG-WAVE COMMIT rung ---------------
            // ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen.
            // The CLASSIFY rung above says what the contact ASKED for; this one says what the
            // prop actually GOT. `stateAfter != state` is the proof the transition committed;
            // `stateAfter == state` with canChange=1 means ChangePropState refused it, and the
            // [prop-diag] STATE rung in PropEntityModule_wQ_04.cpp names which of its two legs
            // dropped it (the physical-slot broker is the silent one).
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                static s32        siCommitLinesLeft = KI_DIAG_MAX_COMMIT_LINES;

                if ( sbPropDiag && siCommitLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0
                     && leNextState != leState )
                {
                    --siCommitLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[prop-diag] COMMIT prop=" << lPropEntityId.GetValue()
                        << " state="      << static_cast<s32>( leState )
                        << " next="       << static_cast<s32>( leNextState )
                        << " canChange="  << ( lbCanChange ? 1 : 0 )
                        << " stateAfter=" << static_cast<s32>( lpProp->GetState() )
                        << " physIdx="    << static_cast<s32>( lpProp->mu8PhysicsIndex )
                        << "\n";
                }
            }

            // Whatever happened, the pending state is retired to the state the prop is
            // actually in now (`stb GetState(), 0x4E` at 0x822FA874).
            lpProp->SetNextState( lpProp->GetState() );
        }
    }

    // ============================================================================
    // PropEntityModule::ProcessPotentialContactWithProp   @0x822DB038  (206 insns)
    // DWARF BrnPropEntityModule.cpp:1265.
    //
    // Classify what state ONE whole prop should be pushed towards by ONE potential contact,
    // and latch it into the prop's pending (next) state. Nothing is published to physics
    // here -- ProcessPotentialContacts' second pass calls ChangePropState.
    //
    // ---- SIGNATURE (asm-confirmed at the call site, 0x822FA604..0x822FA618) --------------
    //   r4 = the prop's PropEntityID, r5 = the OTHER volume's CgsSceneManager::EntityId,
    //   v1 = the contact vector, r6 = the prop input interface.
    //   The vector argument consumes NO GPR slot here (r6 is the 4th register and the 4th
    //   parameter), which is what makes the committed 4-parameter declaration right.
    // ⚠ MEASURED: the shipped body reads NEITHER v1 NOR r6 -- r6 is copied nowhere and no
    //   vector register is read. Both parameters exist because the caller passes them; the
    //   (void) casts below record that unused-ness explicitly instead of leaving it implicit.
    //
    // ---- ASM -> C++ DECODE ---------------------------------------------------------------
    //   0x822DB04C  addi r28, r31, 0x280   &mZoneManager
    //   0x822DB058  bl sub_822CDA28        PropZoneManager::GetProp(PropEntityID)
    //   0x822DB05C  addis/addi -> +0xCDD88 mpPropPhysicsDataHeader
    //   0x822DB06C  lhz 0x44(r21)          PropEntityInstance::muTypeId
    //   0x822DB070  bl 0x822868E0          ResourcePtr<PropPhysicsDataHeader>::GetMemoryResource
    //   0x822DB078  bl 0x82277C50          PropPhysicsDataHeader::GetType(u32)
    //   0x822DB0B4  lbz 0x4D(r21) == 5     GetState() == E_MOVED     -> E_PHYSICAL, done
    //   0x822DB0C4  srwi r24, r25, 24      lContactEntityId.GetOwner()
    //   0x822DB0D8  lbzx this+0xD3341      mbEasySmashProps          -> E_PHYSICAL, done
    //   0x822DB0EC  extrwi r11, r25, 14,8  lContactEntityId.GetEntityIndex()
    //   0x822DB100  slwi 4 ; add -> +0xD3230 + 16*idx, vspltw lane 3
    //                                      maRaceCarVelocity[idx].w  (the w lane is the
    //                                      speed the setter parks there -- see the
    //                                      GetRaceCarVelocity/GetRaceCarSpeed note at
    //                                      BrnPropEntityModule.h:533)
    //   0x822DB11C  bl 0x822A93A8          GetDesiredState(lpType, speed)
    //   0x822DB128  r24 == 2               E_ENTITYTYPE_TRAFFIC_VEHICLE
    //   0x822DB13C  bl 0x822BC4D0          PropZoneManager::GetRespawnType(PropEntityID)
    //   0x822DB148  lfs 0x50(r30)          PropTypeData::mfMoveThreshold  (GetMoveThreshold)
    //   0x822DB168  lbz 0x5D(r30)          PropTypeData::muNumberOfParts  (IsSmashable)
    //   0x822DB180  lfs 0x54(r30)          PropTypeData::mfSmashThreshold (GetSmashThreshold)
    //   0x822DB198  lwz 0x58(r30)          PropTypeData::muSceneUriId     (GetGraphicsId)
    //   0x822DB238  bl 0x822B7888          PropEntityID::PropEntityID(u32)
    //   0x822DB2B4  addi r31, r11, 1       (predicate ? E_PHYSICAL(4) : E_STATIC(1))
    //   0x822DB364  stb r31, 0x4E(r21)     SetNextState(...)
    //
    // ---- ⚠ NaN POLARITY (gotcha 4) --------------------------------------------------------
    // Both float compares are `fcmpu` + `blt`, i.e. the ORDERED less-than, which is exactly
    // C++ `<` (false when unordered). The first is used positively
    // (`GetMoveThreshold() < 1.0f`); the second is used NEGATED
    // (`!( GetSmashThreshold() < 1.0f )`) because the console falls through to the
    // "true" store only when the blt is NOT taken. Neither is rewritten as `>=`.
    // ============================================================================
    void PropEntityModule::ProcessPotentialContactWithProp( PropEntityID lPropEntityId,
                                                            CgsSceneManager::EntityId lContactEntityId,
                                                            Vector3 lContactImpulse,
                                                            BrnPhysics::Props::PropInputInterface* lpPropInput )
    {
        // MEASURED: the shipped body reads neither of these. Kept in the signature because
        // the call site fills them (0x822FA604 r6 / 0x822FA608 v1).
        (void)lContactImpulse;
        (void)lpPropInput;

        PropEntityInstance* lpProp = mZoneManager.GetProp( lPropEntityId );

        const BrnPhysics::Props::PropTypeData* lpType =
            mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lpProp->GetTypeId() );

        EPropState leDesiredState;

        if ( lpProp->GetState() == E_MOVED )
        {
            // Already moving: a further contact can only keep it physical.
            leDesiredState = E_PHYSICAL;
        }
        else if ( lContactEntityId.GetOwner() == E_ENTITYTYPE_RACECAR )
        {
            if ( mbEasySmashProps )
            {
                // The debug "everything gives way" switch (+0xD3341).
                leDesiredState = E_PHYSICAL;
            }
            else
            {
                // GetRaceCarSpeed is the DWARF own accessor for this read
                // (BrnPropEntityModule.h:605, DWARF BrnPropEntityModule.h:345); it returns
                // the .w lane of the same 16-byte slot, i.e. the `vspltw v0,v0,3` at
                // 0x822DB110. Reading `.w` directly was structurally fragile as well as
                // unfaithful -- the SDK reconstruction documents Vector3 w as unused.
                leDesiredState = GetDesiredState(
                    lpType,
                    GetRaceCarSpeed( lContactEntityId.GetEntityIndex() ) );
            }
        }
        else
        {
            // Anything that is not the player's car: no impulse classification at all, just a
            // small set of "does this prop give way to this kind of contact" rules.
            const bool lbHitByTrafficVehicle =
                ( lContactEntityId.GetOwner() == E_ENTITYTYPE_TRAFFIC_VEHICLE );

            const bool lbPropRespawns =
                ( mZoneManager.GetRespawnType( lPropEntityId ) == BrnPhysics::Props::E_RESPAWN );

            // "This prop has no threshold worth beating": its move threshold is below the
            // 1.0f floor (flt_82001C98 -- re-dumped in this lane as 3f800000), and it is
            // either not smashable at all or its smash threshold is NOT below that floor
            // (i.e. it would be moved, not smashed). That whole expression is the DWARF
            // PropTypeData::ShouldActivateOnNonRaceCarContact(), an INLINED SUBROUTINE of
            // this function (_compile/BrnEntityModuleUnity.cpp:32037) reading only that
            // class own fields; de-inlined at BrnPhysicsPropTypeData.h:161, including the
            // deliberately negated ordered second compare (gotcha 4).
            const bool lbMovesOnAnyContact = lpType->ShouldActivateOnNonRaceCarContact();

            const bool lbOverheadSign = lpType->IsOverheadSign();

            bool lbMakePhysical = ( !lbOverheadSign && lbPropRespawns && lbMovesOnAnyContact );

            // ...except that an overhead sign IS knocked physical when the thing that hit it
            // is another WHOLE prop that is itself an overhead sign (sign-on-sign collapse).
            if ( lbOverheadSign
                 && ( lContactEntityId.GetOwner() == E_ENTITYTYPE_PROP )
                 && ( lContactEntityId.GetPartIndex() == 0 ) )
            {
                const PropEntityID  lContactPropId( static_cast<u32>( lContactEntityId ) );
                PropEntityInstance* lpContactProp = mZoneManager.GetProp( lContactPropId );

                const BrnPhysics::Props::PropTypeData* lpContactType =
                    mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lpContactProp->GetTypeId() );

                if ( lpContactType->IsOverheadSign() )
                {
                    lbMakePhysical = true;
                }
            }

            leDesiredState = ( lbMakePhysical || lbHitByTrafficVehicle ) ? E_PHYSICAL : E_STATIC;
        }

        // ---- latch it into the pending state ---------------------------------------------
        // The committed CanChangeState() (BrnPropEntityModule.h:531) IS this fold now -- see
        // the round-2 note where the file-local copy used to be.
        const EPropState leNextState = lpProp->GetNextState();

        const bool lbLatched = CanChangeState( leNextState, leDesiredState );

        // ---- [DIAG] NOT IN THE X360 BINARY -- the BUG-WAVE CLASSIFY rung -----------------
        // ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen. Set BRN_PROP_DIAG.
        //
        // WHY IT EXISTS. This is the ONE place in the world module where the car's SPEED picks
        // which response a prop will get, and the whole "reacts slow, does nothing fast" report
        // turns on that pick:
        //     carSpeedMph <= mfMoveThreshold  -> E_LEANING (2)  -> the prop enters the sim as a
        //         jointed STATIC_BODY and PropManager::HandleContactWithLeanProp /
        //         HandleContactWithTiltProp move it DIRECTLY (lerp + AddWorldSpaceImpulse +
        //         their own UpdatePropEvent tail).
        //     carSpeedMph >  mfMoveThreshold  -> E_PHYSICAL (4) -> the prop enters the sim as a
        //         free ACTIVE_BODY and gets NO direct response at all: every millimetre of its
        //         motion has to come back through sim -> PropManager::ReadUpdatedBodies ->
        //         OutputUpdatedProps -> PropEntityModule::UpdateProps ->
        //         PropZoneManager::UpdateInstance.
        // So one line here plus the existing [Q6-world] line in UpdateProps
        // (PropEntityModule_wQ_06.cpp) brackets the entire question: this line says which arm
        // the contact took and at what speed; that one says whether the ACTIVE_BODY arm ever
        // delivered a pose back.
        //
        // SCOPE + COST. RACE-CAR contacts only (the ones the player makes) and the first
        // KI_DIAG_MAX_CLASSIFY_LINES of the run -- never an unbounded per-frame line. The getenv
        // latch is a function-local static, so the shipped path pays one predicted branch. Every
        // value printed is a member/accessor read with no side effect.
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siClassifyLinesLeft = KI_DIAG_MAX_CLASSIFY_LINES;

            if ( sbPropDiag && siClassifyLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0
                 && lContactEntityId.GetOwner() == E_ENTITYTYPE_RACECAR )
            {
                --siClassifyLinesLeft;

                const s32 liCarIndex = static_cast<s32>( lContactEntityId.GetEntityIndex() );

                *CgsDev::Log::gpDebugPrint
                    << "[prop-diag] CLASSIFY prop="  << lPropEntityId.GetValue()
                    << " type="        << static_cast<u32>( lpProp->GetTypeId() )
                    << " car="         << liCarIndex
                    << " carSpeedMph=" << GetRaceCarSpeed( liCarIndex )
                    << " lean="        << lpType->GetLeanThreshold()
                    << " move="        << lpType->GetMoveThreshold()
                    << " smash="       << lpType->GetSmashThreshold()
                    << " jointType="   << static_cast<s32>( lpType->GetJointType() )
                    << " state="       << static_cast<s32>( lpProp->GetState() )
                    << " next="        << static_cast<s32>( leNextState )
                    << " desired="     << static_cast<s32>( leDesiredState )
                    << " latched="     << ( lbLatched ? 1 : 0 )
                    << " easySmash="   << ( mbEasySmashProps ? 1 : 0 )
                    << "\n";
            }
        }

        if ( lbLatched )
        {
            lpProp->SetNextState( leDesiredState );
        }
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_06.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_06.cpp
//
// WAVE Q (breakable props) -- group 6, "the animated-prop leg". ONE chain, three
// bodies, all reconstructed from the BURNOUT_X360_ARTIST.XEX per-address exports:
//
//   BrnWorld::PropEntityModule::UpdateProps      @ 0x822FB2A0  (148 insns)  [ledger]
//   BrnWorld::PropZoneManager::UpdateAnimation   @ 0x822DF3A0  (202 insns)  [non-ledger;
//                                                   declared BrnPropZoneManager.h:351,
//                                                   the link between the other two]
//   BrnWorld::PropEntityInstance::Update         @ 0x822C5608  (398 insns)  [ledger, filed
//                                                   under the altivec.h catch-all]
//
// Authority: the RAW assembly of those three addresses (Hex-Rays mislabels all three --
// it renders the PPC float-argument slot as a phantom leading `int` parameter on every
// one of them). Member names / offsets / enum values come from the committed headers,
// which were themselves DWARF-derived; the assert texts are the verbatim rodata strings
// read out of the image.
//
// ⚠️ NOTE ON FILE PLACEMENT: the wave brief's group guidance asked for
// PropEntityInstance::Update in its own PropEntityInstance_wQ_06.cpp, while the
// ownership rule for this lane says "exactly one new file". The ownership rule wins, so
// all three bodies land here; the two ledger TUs are unaffected (attribution is by
// function, not by file) and splitting the last function out later is a pure file move.
//
// ⚠️ CONSOLE-CONSTANT DISCIPLINE (the recurring killer). Three console strides appear in
// this group's asm and NONE of them is written here:
//   * `idx*5 << 4` == 80  -- the console sizeof(PropEntityInstance); subscripted instead.
//   * `6 * (idx + 0x22314)` -- 6 is the console sizeof(PropEntityRotationParams) and
//     0x22314 (140052) is `offsetof(PropZoneManager, maRotationParams) / 6` on the
//     console; subscripted instead.
//   * `this + 0xC46` in UpdateProps == mZoneManager(0x280) + maProps(0x980) +
//     mu16ZoneIndex(70) -- reached here through the accessor, not through arithmetic.
// ============================================================================

// ---- the wave Q keystone include set (spec §5, verbatim) -------------------------

namespace BrnWorld
{
    namespace vpu = rw::math::vpu;

    // ========================================================================
    // PropEntityModule::UpdateProps @ 0x822FB2A0
    // ------------------------------------------------------------------------
    // Drain this frame's physics-result queue into the zone manager, then advance every
    // animated (swinging) prop. Called only by PostPhysicsUpdate @0x823031D8, inside its
    // miUpdatePropsPM perfmon bracket.
    //
    // Shape, instruction for instruction:
    //   * `lwz r11, 8(r23)` == BaseEventQueue::miLength, re-read at the head AND the foot
    //     of the loop (0x822FB2C0 and 0x822FB4C4) -- so the bound is the live GetLength(),
    //     not a hoisted copy.
    //   * `bl BrnPhysics__Props__Upd` (an IDA-TRUNCATED symbol, gotcha 6) is
    //     BaseEventQueue<UpdatePropEvent>::GetEvent(i) -- it takes (queue, index) and
    //     returns the element address, which the body then reads at +0x60 (mEntityId),
    //     +0x68 (mbFrozen), +0x40 / +0x50 (the two velocity vectors) and +0x00 (the
    //     transform). That is UpdatePropEvent's layout exactly.
    //   * the owner tripwire at 0x822FB358..0x822FB37C is PropEntityID::AssertIsProp()
    //     folded in ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278);
    //     it is what PropZoneManager::GetZone() runs before the zone read, which is why
    //     the tripwire below is spelled as GetZone() rather than as a raw slot read.
    //   * the unloaded-prop tripwire compares mu16ZoneIndex against 0xFFFF
    //     (KU_UNLOADED_ZONE). GetZone() hands the same halfword back SIGNED, so 0xFFFF
    //     arrives as -1 -- the sentinel survives the narrowing, which is the whole point
    //     of that accessor's signature.
    //
    // ⚠️ NOT A MISSING STEP: at 0x822FB458..0x822FB490 the console builds a LOCAL
    // ResourcePtr on the stack (three self-pointers + BaseResourcePtr::CreateFromHandle
    // @0x822FB490, fed from `mpPropPhysicsDataHeader + 20`, the handle field) and passes
    // `&local` in r7, because the DWARF's UpdateInstance takes a PropPhysicsResourcePtr
    // BY VALUE. The committed UpdateInstance takes the raw `const PropPhysicsDataHeader*`
    // instead; GetPropPhysicsDataHeader() (== mpPropPhysicsDataHeader.GetMemoryResource())
    // is the identical pointer, so the two forms are semantically equivalent and the
    // CreateFromHandle round trip has no observable effect here. (Spec §10 defect 6.)
    //
    // ⚠️ ARGUMENT ORDER IS ABI-PINNED, NOT GUESSED (gotcha 3): the UpdateInstance call at
    // 0x822FB4C0 loads r3=&mZoneManager, r4=mEntityId, r5=&event (the by-value
    // Matrix44Affine's hidden pointer; mTransform is at event+0), r6=mbFrozen,
    // r7=the resource ptr, **r8 is never written**, r9=lpOutput, r10=&mPropEntitySerialiser,
    // f1=mrTimestep, v1=mLinearVelocity(+0x40), v2=mAngularVelocity(+0x50). The skipped r8
    // is the float's consumed-but-unused GPR slot, which is what makes the committed
    // nine-parameter signature the right one.
    void PropEntityModule::UpdateProps( PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                                        const UpdatePropEventQueue* lpUpdatePropEventQueue )
    {
        // ---- [DIAG] NOT IN THE X360 BINARY -- wave Q6 world-side arrival witness ---------
        // ⛔ DELETE-WHEN smashed parts are confirmed moving on screen. Set BRN_PROP_DIAG.
        // This is the first place in the world module that can see a physics pose, so it is
        // the proof that PropManager::OutputUpdatedProps -> the post-physics bridge leg ->
        // AppendUpdatedPropQueue actually delivered. Two independent one-shots rather than
        // one line: the first non-empty batch can legitimately be all whole-prop events, and
        // the part line is the one the wave is actually about.
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
            {
                const s32 liQueueLength = lpUpdatePropEventQueue->GetLength();

                static bool sbLoggedFirstBatch = false;
                if ( !sbLoggedFirstBatch && liQueueLength > 0 )
                {
                    sbLoggedFirstBatch = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[Q6-world] first " << liQueueLength << " updated props\n";
                }

                static bool sbLoggedFirstPart = false;
                if ( !sbLoggedFirstPart )
                {
                    for ( s32 liScan = 0; liScan < liQueueLength; ++liScan )
                    {
                        const BrnPhysics::Props::UpdatePropEvent& lrScanEvent =
                            lpUpdatePropEventQueue->GetEvent( liScan );
                        if ( lrScanEvent.mEntityId.GetPartIndex() != 0 )
                        {
                            const Vector3& lScanPosition = lrScanEvent.mTransform.Pos();
                            sbLoggedFirstPart = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[Q6-world] first part " << lrScanEvent.mEntityId.GetValue()
                                << " pos (" << lScanPosition.x
                                << ", "     << lScanPosition.y
                                << ", "     << lScanPosition.z << ")\n";
                            break;
                        }
                    }
                }

                // ---- ADDED 2026-09-06 (props lane, bug #2): the PART arm ----------------
                // The one-shot above proves a part ARRIVED once; it says nothing about where
                // a shed part goes. Bug #2 ("props sent flying way too much at medium/high
                // speed") is a report about what leaves the scene, and the debris of a
                // smashed gate/fence is most of what a player sees leave -- so it has to be
                // measurable too. Same shape as the whole-prop rung below: budgeted first-N,
                // opt-in, one line per part per update. DELETE-WHEN bug #2 is closed.
                static s32 siPartLinesLeft = 6000;
                for ( s32 liScan = 0; siPartLinesLeft > 0 && liScan < liQueueLength; ++liScan )
                {
                    const BrnPhysics::Props::UpdatePropEvent& lrScanEvent =
                        lpUpdatePropEventQueue->GetEvent( liScan );
                    if ( lrScanEvent.mEntityId.GetPartIndex() != 0 )
                    {
                        --siPartLinesLeft;
                        const Vector3& lScanPosition = lrScanEvent.mTransform.Pos();
                        *CgsDev::Log::gpDebugPrint
                            << "[Q6-world] part " << lrScanEvent.mEntityId.GetValue()
                            << " pos (" << lScanPosition.x
                            << ", "     << lScanPosition.y
                            << ", "     << lScanPosition.z
                            << ") |linVel|="
                            << rw::math::vpu::Magnitude( lrScanEvent.mLinearVelocity )
                            << " frozen=" << ( lrScanEvent.mbFrozen ? 1 : 0 )
                            << "\n";
                    }
                }

                // ---- ADDED 2026-08-23 (bug-wave round 2): the WHOLE-PROP arm -------------
                // The two rungs above witness the batch and the first PART. Neither can
                // answer the question the "reacts slow, nothing fast" report actually turns
                // on: a prop promoted to E_PHYSICAL gets NO direct response -- every
                // millimetre of its motion has to arrive here as a whole-prop
                // UpdatePropEvent. `[Q6-read] props=N` upstream only proves the event was
                // QUEUED; this proves a POSE arrived and, across consecutive lines, whether
                // it is actually changing. Budgeted, opt-in, whole props only (part index 0).
                static s32 siWholePropLinesLeft = 6000;  // 24 -> 200, 2026-09-02 (props-at-speed);
                                                         // 200 -> 6000, 2026-09-06 (props lane,
                                                         // bug #2): 200 lines is <1 s of updates
                                                         // with five props live, so a prop's whole
                                                         // flight fell outside the window.
                for ( s32 liScan = 0; siWholePropLinesLeft > 0 && liScan < liQueueLength;
                      ++liScan )
                {
                    const BrnPhysics::Props::UpdatePropEvent& lrScanEvent =
                        lpUpdatePropEventQueue->GetEvent( liScan );
                    if ( lrScanEvent.mEntityId.GetPartIndex() == 0 )
                    {
                        --siWholePropLinesLeft;
                        const Vector3& lScanPosition = lrScanEvent.mTransform.Pos();
                        *CgsDev::Log::gpDebugPrint
                            << "[Q6-world] whole prop " << lrScanEvent.mEntityId.GetValue()
                            << " pos (" << lScanPosition.x
                            << ", "     << lScanPosition.y
                            << ", "     << lScanPosition.z
                            << ") |linVel|="
                            << rw::math::vpu::Magnitude( lrScanEvent.mLinearVelocity )
                            << " v=(" << lrScanEvent.mLinearVelocity.x << ","
                            << lrScanEvent.mLinearVelocity.y << ","
                            << lrScanEvent.mLinearVelocity.z << ")"
                            << " frozen=" << ( lrScanEvent.mbFrozen ? 1 : 0 )
                            << "\n";
                    }
                }
            }
        }

        for ( s32 liEvent = 0; liEvent < lpUpdatePropEventQueue->GetLength(); ++liEvent )
        {
            const BrnPhysics::Props::UpdatePropEvent& lrEvent =
                lpUpdatePropEventQueue->GetEvent( liEvent );

            // "Trying to update unloaded prop: " -- BrnPropEntityModule.cpp:2251. The console
            // streams the packed id after the prefix (StrStream AppendFormat "0x%X"); the
            // project's CGS_ASSERT takes a plain string, so only the literal prefix survives.
            CGS_ASSERT( mZoneManager.GetZone( lrEvent.mEntityId ) != -1,
                        "Trying to update unloaded prop: " );

            mZoneManager.UpdateInstance( lrEvent.mEntityId,
                                         lrEvent.mTransform,
                                         lrEvent.mLinearVelocity,
                                         lrEvent.mAngularVelocity,
                                         lrEvent.mbFrozen,
                                         GetPropPhysicsDataHeader(),
                                         mrTimestep,
                                         lpOutput,
                                         &mPropEntitySerialiser );
        }

        // 0x822FB4D4..0x822FB4E4: `lfsx f1, r29, 0xD3378` == mrTimestep, r3 == this + 0x280
        // == &mZoneManager. (Again the float rides f1 and skips r4.)
        mZoneManager.UpdateAnimation( mrTimestep );
    }

    // ========================================================================
    // PropZoneManager::UpdateAnimation @ 0x822DF3A0 (202 insns)
    // ------------------------------------------------------------------------
    // Step every animated prop by one timestep. NOT a ledger function of either wave-Q TU
    // (DWARF BrnPropZoneManager.h:172, declared in the committed header this wave), but
    // UpdateProps' only unresolved callee, so it is bodied with its group.
    //
    // The whole body is a set-bit walk over mUsedRotationParams (a BitArray<100>, two u64
    // fields). The console INLINES both halves of that walk, which is why the asm looks
    // bigger than the source:
    //   * 0x822DF3CC..0x822DF414 -- scan the two fields for a non-zero one, then isolate its
    //     lowest set bit with the `(x & -x)` / `cntlzd` / `+63` idiom. That is exactly
    //     CgsContainers::BitArray::GetFirstNonZeroBit() (its GetZeroBitInInt helper carries
    //     that same idiom in its comment).
    //   * 0x822DF4EC..0x822DF6B8 -- walk to the end of the current 64-bit field one bit at a
    //     time through IsBitSet (whose bounds assert, "invalid index : <n> < 100"
    //     @CgsBitArray.h:203, is emitted at THIS call site because the container header is
    //     deliberately assert-free), then jump to the next non-zero field. That is
    //     GetNextNonZeroBit(). The committed helper cannot run past tuNumBits, so the
    //     inlined assert is unreachable in the port and is not reproduced.
    //   * `cmpwi r29, 0x64` at 0x822DF418 pins the bound at 100 == KU_MAX_ROTATION_PARAMS,
    //     and `cmpwi r29, -1` is the KI_INVALID_BITINDEX exit.
    //
    // Per entry: read the params record's miPropIndex (`lhz`/`extsh` at 0x822DF498 -- a
    // SIGNED halfword), resolve that prop slot, tripwire, step it.
    //
    // ⚠️ ODR NOTE (2026-08-18). This is a PropZoneManager member bodied in a PropEntityModule
    // partfile, not at its mirrored home BrnPropZoneManager.cpp. It is the ONLY definition in
    // the tree -- but `cl /c` cannot see a cross-TU duplicate, so whoever fills out
    // BrnPropZoneManager.cpp must NOT mint a second one. The class's own header now records
    // that (BrnPropZoneManager.h:377-383, corrected in round 2 -- it previously claimed the
    // method was "currently unbodied in the tree"). Left here rather than moved: this fixer's
    // write set is the PropEntityModule_wQ_0*.cpp partfiles only, and moving a body across
    // that boundary is a conductor action.
    void PropZoneManager::UpdateAnimation( f32 lfTimeStep )
    {
        for ( s32 liAnimationIndex = mUsedRotationParams.GetFirstNonZeroBit();
              liAnimationIndex != CgsContainers::BitArray<KU_MAX_ROTATION_PARAMS>::KI_INVALID_BITINDEX
                  && static_cast<u32>( liAnimationIndex ) < KU_MAX_ROTATION_PARAMS;
              liAnimationIndex = mUsedRotationParams.GetNextNonZeroBit( liAnimationIndex ) )
        {
            const PropEntityRotationParams& lrRotationParams =
                maRotationParams[liAnimationIndex];

            PropEntityInstance* lpPropToUpdate = &maProps[lrRotationParams.miPropIndex];

            // BrnPropZoneManager.cpp:693, verbatim rodata text. The console reads the byte at
            // prop+0x4B (mi8RotationParamsIndex) and sign-extends it before the compare.
            CGS_ASSERT( lpPropToUpdate->mi8RotationParamsIndex == liAnimationIndex,
                        "lpPropToUpdate->GetRotationParamsIndex() == liAnimationIndex" );

            lpPropToUpdate->Update( lfTimeStep, &lrRotationParams );
        }
    }

    // ========================================================================
    // PropEntityInstance::Update @ 0x822C5608 (398 insns) -- ONE animated prop's swing step
    // ------------------------------------------------------------------------
    // The console inlines an entire VMX sin/cos kernel into this function, which is where
    // most of those 398 instructions go. The kernel is the SAME one four other reconstructed
    // subsystems already identified (range-reduce by 2*pi via vrfin, then a 12-term Taylor
    // pair); its coefficient rows were re-dumped for this body out of the shipped image and
    // agree value-for-value with the bank already committed in
    // GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp.
    // It is de-SIMD'd here TU-locally for the same reason it is there: the committed
    // rw::math::vpu tree flags SinCos as not-committed.
    // ========================================================================

    // Re-dumped 2026-08-18 from BURNOUT_X360_ARTIST.XEX (headless IDA on a DB copy):
    //   0x82000BD0 = { 1, -1/3!, 1/5!, -1/7! }        0x82000C00 = { 1, -1/2!, 1/4!, -1/6! }
    //   0x82000BE0 = { 1/9! .. -1/15! }               0x82000C10 = { 1/8! .. -1/14! }
    //   0x82000BF0 = { 1/17! .. -1/23! }              0x82000C20 = { 1/16! .. -1/22! }
    //   0x82000C60 = { pi, 2pi, 1/pi, 1/(2pi) }  (lanes 1 and 3 are the ones this uses)
    // Which lane feeds which accumulator is MEASURED, not assumed: the odd-power chain seeded
    // by `x + BD0[1]*x^3` (0x822C5784) is SIN and the chain seeded by `1 + C00[1]*x^2`
    // (0x822C57F4, whose seed 1.0f comes from the vspltisw/vupkd3d128 pair) is COS.
    static const f32 KAF_PEI_SIN_COEFFS[12] =
    {
        1.0f,            -1.66666672e-1f,  8.33333377e-3f,  -1.98412701e-4f,
        2.75573188e-6f,  -2.50521080e-8f,  1.60590438e-10f, -7.64716373e-13f,
        // ⚠️ 2026-08-18: this first entry was 2.81145725e-15f, which encodes 0x274A963B --
        // ONE ULP BELOW the shipped image. The image word at 0x82000BF0 is 0x274A963C, and
        // that is also the correct round-to-nearest float32 of 1/17!
        // (1/17! == 2.8114572543455206e-15 -> 0x274A963C, checked by round-trip, and the
        // image byte was dumped independently with headless IDA). Corrected. The same wrong
        // word is still the committed value in the sibling bank at
        // GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp:571,
        // which is NOT this lane's file -- reported to the conductor.
        2.8114573e-15f,  -8.22063525e-18f, 1.95729411e-20f, -3.86817017e-23f
    };
    static const f32 KAF_PEI_COS_COEFFS[12] =
    {
        1.0f,            -5.0e-1f,         4.16666679e-2f,  -1.38888892e-3f,
        2.48015876e-5f,  -2.75573188e-7f,  2.08767570e-9f,  -1.14707456e-11f,
        4.77947733e-14f, -1.56192070e-16f, 4.11031762e-19f, -8.89679139e-22f
    };
    static const f32 KF_PEI_TWO_PI     = 6.28318548f;   // 0x40C90FDB @0x82000C64 (lane 1)
    static const f32 KF_PEI_INV_TWO_PI = 0.159154937f;  // 0x3E22F983 @0x82000C6C (lane 3)

    // r = angle - 2pi*round(angle/2pi) (the console's vrfin round-to-nearest; floor(x+0.5)
    // here differs only at an exact .5 tie on an already-inexact angle), then the two 12-term
    // series over r^2.
    //
    // ⚠️ ROUNDING, STATED HONESTLY (round-1 wording was too strong): the console does NOT
    // evaluate a Horner form. It materialises the explicit odd/even powers r^3, r^5 ... r^23
    // through the vmulfp128 lattice at 0x822C5774..0x822C57CC and accumulates c_k*r^(2k+1)
    // with a chain of FUSED vmaddfp -- one rounding per term. The Horner recurrence below is
    // the SAME TERMS but a DIFFERENT rounding sequence (unfused multiply-add over r^2), so it
    // is a deliberate de-optimisation that differs by a few ULP in the tail terms, not a
    // "float-rounding-equivalent order". The same unfused-vs-fused delta applies to the
    // range reduction (the console uses `vnmsubfp v8, v13, v12, v0` at 0x822C5934) and to the
    // three per-arm row combines (e.g. `vmaddfp` at 0x822C58B4), alongside the fmsubs note
    // already recorded elsewhere in this file.
    static void PeiSinCos( f32 lfAngle, f32& lrfSin, f32& lrfCos )
    {
        const f32 lfR  = lfAngle - KF_PEI_TWO_PI * std::floor( lfAngle * KF_PEI_INV_TWO_PI + 0.5f );
        const f32 lfR2 = lfR * lfR;

        f32 lfSinPoly = KAF_PEI_SIN_COEFFS[11];
        f32 lfCosPoly = KAF_PEI_COS_COEFFS[11];
        for ( s32 li = 10; li >= 0; --li )
        {
            lfSinPoly = lfSinPoly * lfR2 + KAF_PEI_SIN_COEFFS[li];
            lfCosPoly = lfCosPoly * lfR2 + KAF_PEI_COS_COEFFS[li];
        }
        lrfSin = lfR * lfSinPoly;
        lrfCos = lfCosPoly;
    }

    // ------------------------------------------------------------------------
    // The per-frame swing step for ONE animated prop (a gate, a barrier arm, a swinging
    // sign). Its only caller is PropZoneManager::UpdateAnimation @0x822DF4E8, which sets
    // r3 / f1 / r5 only -- r4 is dead, being the float's consumed GPR slot (gotcha 3).
    //
    // ⭐ THE ANGULAR-SPEED CONSTANTS -- and a CORRECTION to the wave spec.
    // 0x822C5658..0x822C56A0 computes, exactly:
    //     speed6 = (u8)mnRotSpeed & ~0xC0        (`lbz` + `rlwinm ...,0,26,23`, mask
    //                                             0xFFFFFF3F; the `lbz` proves the field is
    //                                             read UNSIGNED before the mask -- a signed
    //                                             read would not fold to `& 0x3F`)
    //     rate   = fmsubs( (f32)speed6 * flt_82014AB0, flt_82CDB6AC, flt_82FAD61C )
    //     angle  = rate * lfTimeStep
    //   flt_82014AB0 = 0.015625f == 1/64   (rodata, dumped)
    //   flt_82CDB6AC = 12.568f             (dumped; ~4*pi, a hand-typed constant)
    //   flt_82FAD61C -- the spec records this as 0.0f. That is the value in the STATIC
    //     image, and it is WRONG for the running game: the address has a dynamic
    //     initialiser at 0x82C4C298 (`lfs flt_82CDB6AC ; lfs flt_820147FC ; fmuls ; stfs
    //     flt_82FAD61C`), i.e. it is filled at run time with 12.568f * 0.5f == 6.284f.
    //     flt_820147FC == 0.5f was read out of rodata alongside. This is the same
    //     dynamic-initialiser class of constant the spec itself recovered for
    //     KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET (80.0f) and KVF_PROP_FLOOR (-1000.0f); the
    //     bias was simply read statically and missed. WITH the bias the 6-bit field is a
    //     SIGNED speed spanning about +-1 revolution/second and 32 means "stopped"; without
    //     it every animated prop in the game would spin one way only, at up to 2 rev/s.
    //
    // ⚠️ `fmsubs` is a FUSED multiply-subtract (one rounding). The host expression below is
    // two roundings unless the compiler contracts it -- a sub-ULP difference on a value that
    // is immediately scaled by a timestep, recorded rather than papered over.
    void PropEntityInstance::Update( f32 lfTimeStep, const PropEntityRotationParams* lpRotationParams )
    {
        // 0x822C562C: `lbz r11, 0x4B(r31) ; cmplwi r11, 0xFF` -- the byte form of
        // IsAnimated(), i.e. mi8RotationParamsIndex != -1. BrnPropEntityInstance.cpp:254.
        CGS_ASSERT( mi8RotationParamsIndex != -1, "IsAnimated()" );

        const f32 KF_PEI_ROT_SPEED_STEP  = 0.015625f;         // flt_82014AB0 == 1/64
        const f32 KF_PEI_ROT_SPEED_SCALE = 12.568f;           // flt_82CDB6AC
        const f32 KF_PEI_ROT_SPEED_BIAS  = 12.568f * 0.5f;    // flt_82FAD61C, dynamic init

        // mnRotSpeed packs { axis selector : 2 } into the top bits and { rate : 6 } below.
        const u8  lu8RotSpeed  = static_cast<u8>( lpRotationParams->mnRotSpeed );
        const u32 luRotAxis    = lu8RotSpeed & 0xC0u;
        const u32 luRotSpeed6  = lu8RotSpeed & 0x3Fu;

        const f32 lfAngularSpeed =
            static_cast<f32>( luRotSpeed6 ) * KF_PEI_ROT_SPEED_STEP * KF_PEI_ROT_SPEED_SCALE
            - KF_PEI_ROT_SPEED_BIAS;

        f32 lfRotationAngle = lfAngularSpeed * lfTimeStep;

        // A prop whose two limit bytes are equal has no end stops: it free-runs, and the
        // constraint / ease-in pair is skipped entirely (`beq` at 0x822C56A8).
        if ( lpRotationParams->muMinAngle != lpRotationParams->muMaxAngle )
        {
            // 0x822C56B4: `fcmpu ; bge` past the `fneg`. bge after fcmpu is `bc 4,LT`, which
            // is TAKEN when the compare is unordered -- so this is the NEGATED ORDERED
            // predicate and `x < 0.0f` is the NaN-correct spelling (gotcha 4). Written as the
            // compare rather than as std::fabs on purpose: fabs would clear a NaN's sign bit
            // where the console leaves it alone.
            f32 lfConstraintInput = lfRotationAngle;
            if ( lfConstraintInput < 0.0f )
            {
                lfConstraintInput = -lfConstraintInput;
            }

            // The three out-params come back through r5/r6/r7 (the console reuses the
            // rotation angle's own stack slot for lfMaxAngle, which is why the asm looks like
            // it aliases them). The two calls are strictly sequenced: UpdateConstraints fills
            // the three floats, THEN CalculateEaseInSpeedModulation reads them, and only then
            // are the two results multiplied (`fmuls f0, f1, f11` at 0x822C56F0).
            f32 lfTrueAngle = 0.0f;
            f32 lfMinAngle  = 0.0f;
            f32 lfMaxAngle  = 0.0f;

            const f32 lfConstrainedSpeed = UpdateConstraints( lfConstraintInput,
                                                              &lfTrueAngle,
                                                              &lfMinAngle,
                                                              &lfMaxAngle,
                                                              lpRotationParams );
            const f32 lfEaseIn = CalculateEaseInSpeedModulation( lfTrueAngle,
                                                                 lfMinAngle,
                                                                 lfMaxAngle );
            lfRotationAngle = lfConstrainedSpeed * lfEaseIn;
        }

        // 0x822C570C: the translation row is captured BEFORE anything else touches the
        // matrix, and restored as the very last store of the function (0x822C5C28), after
        // the orthonormalise has written all four rows back.
        const Vector3 lSavedTranslation = mWorldTransform.wAxis;

        f32 lfSin;
        f32 lfCos;
        PeiSinCos( lfRotationAngle, lfSin, lfCos );

        // Rotate the 3x3 about the local axis mnRotSpeed selects. Each arm loads BOTH source
        // rows before storing either (the console holds them in vector registers), and the
        // untouched row is the rotation axis itself. The sign convention is measured, not
        // assumed -- in every arm the FIRST row written is `u*cos + v*sin` and the second is
        // `v*cos - u*sin`, with (u,v) the two rows following the axis cyclically:
        //   axis 0x00 (X): 0x822C58B4 `v0 = row2*sin + row1*cos` -> +0x10,
        //                  0x822C58B8 `v13 = row2*cos - row1*sin` -> +0x20
        //   axis 0x40 (Y): 0x822C5A70 -> +0x20,  0x822C5A74 -> +0x00
        //   axis 0x80/0xC0 (Z): 0x822C5BE0 -> +0x00, 0x822C5BE4 -> +0x10
        const Vector3 lXAxis = mWorldTransform.xAxis;
        const Vector3 lYAxis = mWorldTransform.yAxis;
        const Vector3 lZAxis = mWorldTransform.zAxis;

        if ( luRotAxis == 0x00u )
        {
            mWorldTransform.yAxis = lYAxis * lfCos + lZAxis * lfSin;
            mWorldTransform.zAxis = lZAxis * lfCos - lYAxis * lfSin;
        }
        else if ( luRotAxis == 0x40u )
        {
            mWorldTransform.zAxis = lZAxis * lfCos + lXAxis * lfSin;
            mWorldTransform.xAxis = lXAxis * lfCos - lZAxis * lfSin;
        }
        else
        {
            mWorldTransform.xAxis = lXAxis * lfCos + lYAxis * lfSin;
            mWorldTransform.yAxis = lYAxis * lfCos - lXAxis * lfSin;
        }

        // 0x822C5BF8: `bl rw__math__vpu__OrthoNormalize3x3(&temp, this)`, then four
        // lvx128/stvx128 pairs copying temp's rows 0/0x10/0x20/0x30 back over this. The
        // committed OrthoNormalize3x3 already passes wAxis through untouched, so the explicit
        // restore below is a no-op in the port -- it is kept because the console performs it
        // and because it pins the intent (the swing must never translate the prop).
        mWorldTransform = vpu::OrthoNormalize3x3( mWorldTransform );
        mWorldTransform.wAxis = lSavedTranslation;
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ_07.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_07.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q KEYSTONE, GROUP 7: THE PRE-PHYSICS TICK and
// THE BROKEN-PROP RETIREMENT. (spec: scratchpad/waveQ/PropEntityModule.spec.md,
// "GROUPS" row 7.)
//
// Ledger TUs covered (ONE class, TWO ledger ids -- the conductor closes them separately):
//   * GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   * class:BrnWorld::PropEntityModule
//
// Functions in this file (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::ProcessBrokenProps        @0x822EEFA0  (363 insns)   [ledger]
//   PropEntityModule::PrePhysicsUpdate          @0x82303048  (100 insns)   [ledger]
//   PropCellManager::GetPhysicalPropSlot        @0x822E11C0  (NON-ledger; declared in
//                                                BrnPropCellManager.h since the keystone,
//                                                never bodied -> the group link-fails
//                                                without it. Spec section 1, the
//                                                "ledger-reviewed != implemented" table.)
//   PropCellManager::GetPhysicalPartSlot        @0x822E0DB0  (same, and it is the callee
//                                                ProcessBrokenProps' inner loop turns on)
//
// PrePhysicsUpdate is the DRIVER of ProcessBrokenProps (and of group 5's
// ProcessPotentialContacts); both decode InputBuffer_PrePhysics, which is why they pair.
//
// ---- GROUND TRUTH ---------------------------------------------------------------------
// All four bodies are reconstructed from the RAW ASSEMBLY of their per-address ARTIST
// exports, read instruction by instruction:
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EEFA0.json
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82303048.json
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822E11C0.json
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822E0DB0.json
// The Hex-Rays pseudocode was used ONLY to read the assert string literals out in full and
// to cross-check the branch structure. It is wrong about the shapes -- it renders
// ProcessBrokenProps as `_DWORD *(_DWORD *, int, int)` and PrePhysicsUpdate as
// `int(int,int,int,_DWORD*,int,char)`, and it drops the second argument of
// PropZoneManager::GetProp/GetPart entirely. Declaration shapes are the DecFIGS DWARF ones
// already committed in BrnPropEntityModule.h (:1430 for ProcessBrokenProps) and
// BrnPropCellManager.h.
//
// ---- MEASURED vs INFERENCE ------------------------------------------------------------
// MEASURED (read off the asm listing, per function):
//   * ProcessBrokenProps: the whole control flow -- the Set walk over maRecentlyBrokenProps
//     (asserts at CgsSet.h:227/:274/:275, count word at this+0xCDE64), the `state == 5`
//     early-continue at 0x822EF184, the two smashable/smashed tripwires at :1470/:1471, the
//     slot release + RemovePropInstance + `--miNumPropsInSim` triple, the 1-based part-id
//     seed (`insrwi r27, 1, 10, 22` at 0x822EF2CC), the eviction leg, the VMX part-transform
//     cascade at 0x822EF3F4..0x822EF454, and the terminal `maRecentlyBrokenProps.Clear()`
//     (`stwx 0, this, 0xCDE64` at 0x822EF540).
//   * PrePhysicsUpdate: the two locks, the `lUpdateSet & 1` split at 0x82303074, the
//     paused leg's `miUpdatesSinceLastSimPause = 0` + potential-contact tripwire, the
//     running leg's SendTrafficLightRestoreEvents / reset-queue drain / ProcessBrokenProps /
//     ProcessPotentialContacts / `++miUpdatesSinceLastSimPause`, and the two unlocks.
//   * Both slot brokers: the find-first-clear-bit search, the >= capacity and == -1 escapes
//     into the eviction scan, the "largest mfTimeInSim wins" selection with its exact
//     `fcmpu` + `blt` polarity, and every store on all four exit paths.
// INFERENCE (each marked where it appears):
//   * the NAME KU_UPDATESET_SIM_PAUSED for update-set bit 0 (the bit itself is measured).
//   * that `if (state == 5) continue;` is `GetState() == E_MOVED` -- the asm compares the
//     raw byte 5 against a value that has just passed GetState()'s own
//     "mu8State < E_STATE_COUNT" tripwire; E_MOVED == 5 in the committed EPropState.
//   * that `lbz 0x4A; rlwinm 0,28,28` is PropEntityInstance::IsSmashable(). The assert
//     STRING is "lpInstance->IsSmashable()" verbatim, and mask 0x8 == KU_SMASHABLE_BIT,
//     but this class has no IsSmashable() member in the committed header, so the predicate
//     is spelled as the flag test -- exactly the idiom BrnPropCellManager.cpp:512/:558
//     already uses for the sibling IsAddedToScene().
//
// ---- CONSOLE-LITERAL DISCIPLINE (gotcha 1) -------------------------------------------
// Not one console byte offset, stride or object size appears in the code below. The traps
// this group carries, all defused:
//   (1) ProcessBrokenProps walks the part descriptors with `r24 += 0x30` (0x822EF4F4).
//       48 is the CONSOLE sizeof(PropPartTypeData); on the host it is 64. The loop below
//       subscripts `lpType->GetParts()[luPart]` and lets the host compiler pick the stride.
//   (2) Every queue length in the asm is `lwz r11, 8(queue)`. 8 is the CONSOLE offset of
//       BaseEventQueue<T>::miLength (mpEvents 4 + miMaxLength 4); on the host mpEvents
//       widens to 8 bytes and miLength moves to +0x10. Every length below goes through
//       GetLength().
//   (3) The two slot brokers address maPhysicalPropParams / maPhysicalPartParams as
//       `this + 8*(i + 0xF1)` / `this + 8*(i + 0x100)` and their time fields as
//       `+0x78C` / `+0x804`. Those are the CONSOLE offsets of members that sit AFTER the
//       two widening member pointers mpaProps/mpaPropParts (BrnPropCellManager.h's banner
//       says so explicitly), so they do not hold on the host at all. Both bodies subscript
//       the named arrays.
//   (4) The bit-array words are read with `ld`/`std` at `this + 0x778` / `+0x780` and the
//       shift is `1 << (i & 0x3F)`. Reached below through CgsContainers::BitArray's own
//       GetFirstClearBit / SetBit / IsBitSet, whose committed bodies are that same bit math
//       (GetFirstClearBit's banner cites the identical `~field` + lowest-set-bit idiom).
//   (5) The eviction miss writes the literal word 0x03000000 into the out-parameter. That
//       is a PropEntityID whose OWNER byte is E_ENTITYTYPE_PROP and whose index/part fields
//       are zero -- the compiler's fold of a default-constructed prop handle. Spelled below
//       through PropEntityID::KU_OWNER_BASE + E_ENTITYTYPE_PROP, the same way
//       BrnPropZoneManager.cpp:1092 and BrnPropEntityDebugComponent.cpp:446 already spell it.
//   The only numeric literals that survive are population deltas (the `1` in
//   Increment/DecrementNumberOf*InSim) and the part-index seed `1`.
//
// ---- POOL-CAPACITY SPELLING (changed 2026-08-18, round 2) ------------------------------
// The two slot brokers are PropCellManager members, and they now bound their loops and
// qualify their BitArrays with the CLASS'S OWN constants -- PropCellManager::
// KU_MAX_PHYSICAL_PROPS (15) and ::KU_MAX_PHYSICAL_PARTS (30), BrnPropCellManager.h:140-141,
// the same symbols that size `BitArray<15u> mPhysicalProps` / `maPhysicalPropParams[15]` /
// `BitArray<30u> mPhysicalParts` / `maPhysicalPartParams[30]`. Round 1 used
// BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS / KU_MAX_PHYSICAL_PROP_PARTS
// (BrnPropInputInterface.h:52/:61) instead: all three spellings agree at 15/30 today, but
// that pair is explicitly marked there as temporary squatters scheduled to MOVE to
// BrnPropConstants.h, so a loop bound and an array extent were coupled through different
// headers -- a pool resized through one spelling only is a silent out-of-bounds walk over
// maPhysicalPartParams. The round-2 owner added the four static_asserts that tie the three
// spellings together (BrnPropCellManager.cpp:139-147), so a drift now fails to compile.
//
// ---- NaN POLARITY (gotcha 4) ----------------------------------------------------------
// ProcessBrokenProps and PrePhysicsUpdate: NOT APPLICABLE -- neither contains a single
// floating-point compare (ProcessBrokenProps' only FP work is the VMX multiply-add cascade).
// The two slot brokers: exactly one compare each, the recycle scan's
//     0x822E1524  fcmpu cr6, f0, f31        (f0 == maPhysical*Params[i].mfTimeInSim,
//     0x822E1528  blt   cr6, <skip update>   f31 == the running best, seeded 0.01f)
// `blt` after fcmpu is bc 12,LT -- taken ONLY on an ordered less-than, so an UNORDERED
// (NaN) compare falls through and TAKES the update. The faithful C++ is therefore
// `if ( !(lfTimeInSim < lfBest) )`, which is likewise true for NaN. Writing
// `if (lfTimeInSim >= lfBest)` would be WRONG: C++ `>=` is false for NaN and would skip a
// slot the console selects. (This is the mirror image of the bge/ble case in the gotcha
// list -- there the negation is needed because the branch IS taken on unordered; here it is
// needed because the branch is NOT.)
// ============================================================================


namespace BrnWorld
{
    namespace
    {
        // ------------------------------------------------------------------
        // Update-set bit 0.
        //
        // MEASURED: PrePhysicsUpdate splits on it at 0x82303074 with
        //     clrlwi r11, r30, 31 ; cmplwi cr6, r11, 0 ; bne cr6, <bit-set leg>
        // and the BIT-SET leg is the one that resets miUpdatesSinceLastSimPause and fires
        // "Received potential contacts in a paused update". So bit 0 SET == the simulation
        // did not step this frame.
        //
        // CORROBORATED, not invented: the committed WorldModule::Update already tests the
        // same bit and skips SceneModule::UpdateContactGeneration when it is set
        // (BrnWorldModule.cpp:2610, `if ( ( lUpdateSet & 1 ) == 0 )`) -- i.e. with bit 0 set
        // no contact generation runs, which is exactly why receiving potential contacts is
        // an assertable condition here. BrnPropEntityModule_PreScene.cpp's park P6 quotes a
        // third consumer of the same bit in this very class.
        //
        // ⚠️ INFERENCE: only the NAME. No enumerator for this bit exists in any committed
        // header and no assert string spells one; the name below describes the measured
        // role and nothing more. The DWARF-attested spellings that DO exist for other bits
        // live at their own call sites (KU_UPDATESET_RESOURCE_SYSTEM_STALLED == 0x400 in
        // BrnPropEntityModule_PreScene.cpp:289, KU_UPDATESET_USE_REPLAY_INTERFACE == 0x100
        // in WorldBridgeRaceCarToPropModule.cpp:96), and this follows their convention.
        const BrnUpdateSet KU_UPDATESET_SIM_PAUSED = 0x1;

        // ------------------------------------------------------------------
        // The seed of the recycle scan's running maximum in BOTH slot brokers
        // (0x822E0E14 and 0x822E1224, `lfs f31, flt_82002138`).
        //
        // VALUE MEASURED: flt_82002138 == 0.01f. It is a shared rodata float, decoded
        // identically (0.0099999998) in eight other ARTIST exports' pseudocode -- e.g.
        // BrnDirector::Camera::Utils::Looker::Parameters::Construct @0x821F8D80 and
        // CollisionPolicyAttachedToVehicle::UpdateRadius @0x8220E4D0 -- so the reading does
        // not rest on this one function's decompilation.
        //
        // ROLE MEASURED: the scan keeps the slot with the LARGEST mfTimeInSim, and this is
        // the initial best. A pool whose every resident has been in the sim for less than
        // 0.01 s therefore yields NO recycle candidate and the broker fails -- i.e. a prop
        // or part cannot be evicted in the same ~frame it was admitted.
        //
        // ⚠️ INFERENCE: only the NAME. No assert string or symbol attests one.
        const f32 KF_MINIMUM_TIME_IN_SIM_BEFORE_RECYCLE = 0.01f;

        // ------------------------------------------------------------------
        // The seed value the two slot brokers write into their `evicted id` out-parameter
        // when nothing was evicted: `lis rN, 0x300` == 0x03000000.
        //
        // That is a PropEntityID with owner E_ENTITYTYPE_PROP (== 3) at KU_OWNER_BASE and a
        // zero entity/part payload -- the compiler's fold of a default-constructed prop
        // handle, the same fold BrnPropZoneManager.cpp:1025 documents for the sibling
        // PropVolumeInstanceID. PropEntityID's committed default constructor is trivial (it
        // does NOT seed the owner byte), so the seed is written explicitly here rather than
        // relying on the declaration.
        //
        // NOTE the value is deliberately NOT built with the explicit PropEntityID(u32)
        // constructor: that one is a real out-of-line X360 symbol (0x822B7888) with an
        // owner tripwire, and the asm calls no such thing here -- it materialises a
        // constant.
        PropEntityID MakeDefaultPropEntityId()
        {
            PropEntityID lId;
            lId.mEntityId.muValue =
                static_cast<u32>( E_ENTITYTYPE_PROP ) << PropEntityID::KU_OWNER_BASE;
            return lId;
        }
    }

    // ========================================================================
    // PropCellManager::GetPhysicalPropSlot   @ 0x822E11C0
    // ------------------------------------------------------------------------
    // ⭐ NON-LEDGER, NEW 2026-08-18 (wave Q keystone, group 7). Declared in
    // BrnPropCellManager.h, bodied nowhere in the tree until now; ChangePropState (group 4,
    // via the PropZoneManager forwarder) and every future physical-prop promotion is an
    // unresolved external without it. `cl /c` cannot see that, which is exactly why the
    // spec lists it under "ledger-`reviewed` != implemented".
    //
    // Claim a slot in the 15-entry physical-prop pool for lEntityId:
    //   * if a slot is free, take it, seed its bookkeeping, and report "no eviction";
    //   * otherwise EVICT the prop that has been resident LONGEST (largest mfTimeInSim),
    //     hand its id back to the caller and re-seat the slot on lEntityId;
    //   * if the pool is full AND no resident qualifies, fail with a -1 slot.
    //
    // SIGNATURE (prologue 0x822E11D0..0x822E11E8, measured): r3 this, r4 lEntityId,
    // r5 -> lriSlot, r6 -> lrEvictedId, r7 -> lrbEvicted. That is the committed
    // declaration exactly.
    //
    // ⚠️ MEASURED ODDITY, reproduced: the console does NOT clear the pool bit on the
    // eviction path (it is already set) and it does NOT touch mfTimeInSim on the failure
    // path. Both are as-shipped.
    // ========================================================================
    bool PropCellManager::GetPhysicalPropSlot( PropEntityID lEntityId, s32& lriSlot,
                                               PropEntityID& lrEvictedId, bool& lrbEvicted )
    {
        // 0x822E11EC..0x822E1328. The console open-codes find-first-clear-bit over
        // mPhysicalProps' single 64-bit field (`ld` / `cmpdi -1` to skip an all-ones field,
        // then `not` + isolate-lowest-set-bit + `cntlzd`), and treats BOTH "ran past the
        // capacity" (`cmpwi r31, 0xF ; bge`) and "came back -1" (`cmpwi r31, -1 ; beq`) as
        // "no free slot". BitArray<15>::GetFirstClearBit is that same idiom and collapses
        // both escapes into its KI_INVALID_BITINDEX return.
        const s32 liFreeSlot = mPhysicalProps.GetFirstClearBit();

        if ( liFreeSlot != CgsContainers::BitArray<KU_MAX_PHYSICAL_PROPS>::KI_INVALID_BITINDEX )
        {
            // 0x822E132C -- SetBit's own bounds tripwire (CgsBitArray.h:222). Its X360
            // message is StrStream-built ("Index: " << i << ", Number of bits: " << 15);
            // collapsed to a plain CGS_ASSERT per this TU's convention, literal verbatim.
            CGS_ASSERT( static_cast<u32>( liFreeSlot ) < KU_MAX_PHYSICAL_PROPS,
                        "Index: " );

            // 0x822E13DC..0x822E1438, in asm store order.
            mPhysicalProps.SetBit( static_cast<u32>( liFreeSlot ) );
            maPhysicalPropParams[liFreeSlot].mfTimeInSim = 0.0f;
            maPhysicalPropParams[liFreeSlot].mEntityId   = lEntityId;

            lriSlot     = liFreeSlot;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return true;
        }

        // ---- the pool is full: pick the longest-resident slot ------------------------
        // 0x822E1210..0x822E1544. f31 is seeded from flt_82002138 == 0.01f, so a slot whose
        // mfTimeInSim has not yet reached 0.01 s is never recycled.
        f32 lfLongestTimeInSim = KF_MINIMUM_TIME_IN_SIM_BEFORE_RECYCLE;
        s32 liRecycleIndex     = -1;

        for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_PROPS; ++luSlot )
        {
            // 0x822E1274 -- IsBitSet's bounds tripwire (CgsBitArray.h:203), StrStream-built
            // ("invalid index : " << i << " < " << 15); collapsed, literal verbatim. It is
            // unreachable with this loop bound, exactly as in the console.
            CGS_ASSERT( luSlot < KU_MAX_PHYSICAL_PROPS, "invalid index : " );

            // 0x822E14CC..0x822E1518 (BrnPropCellManager.cpp:1091). Every occupied-pool slot
            // must be marked in the bit array; the whole pool is occupied on this path.
            CGS_ASSERT( mPhysicalProps.IsBitSet( luSlot ),
                        "mPhysicalProps.IsBitSet( liRecycleIndex )" );

            // 0x822E1520..0x822E1530. See the NaN-polarity note in the banner: the console's
            // `blt` skips the update ONLY on an ordered less-than, so the negated ordered
            // predicate is the faithful spelling.
            const f32 lfTimeInSim = maPhysicalPropParams[luSlot].mfTimeInSim;
            if ( !( lfTimeInSim < lfLongestTimeInSim ) )
            {
                lfLongestTimeInSim = lfTimeInSim;
                liRecycleIndex     = static_cast<s32>( luSlot );
            }
        }

        if ( liRecycleIndex == -1 )
        {
            // 0x822E1550..0x822E1570 -- nothing recyclable.
            lriSlot     = -1;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return false;
        }

        // 0x822E1580..0x822E15EC. The console reads the resident id's OWNER byte and fires
        // the prop tripwire before handing it back (`lbzx r11, r31, r22 ; cmplwi r11, 3`);
        // that is PropEntityID::AssertIsProp folded at the point of the read. (Its part-slot
        // twin at 0x822E1170 does NOT carry this tripwire -- a real asymmetry between the
        // two shipped bodies, reproduced.)
        maPhysicalPropParams[liRecycleIndex].mEntityId.AssertIsProp();

        lrEvictedId = maPhysicalPropParams[liRecycleIndex].mEntityId;
        lrbEvicted  = true;

        maPhysicalPropParams[liRecycleIndex].mfTimeInSim = 0.0f;
        maPhysicalPropParams[liRecycleIndex].mEntityId   = lEntityId;

        lriSlot = liRecycleIndex;
        return true;
    }

    // ========================================================================
    // PropCellManager::GetPhysicalPartSlot   @ 0x822E0DB0
    // ------------------------------------------------------------------------
    // ⭐ NON-LEDGER, NEW 2026-08-18 (wave Q keystone, group 7). Same story as its whole-prop
    // twin above: declared, never bodied, and ProcessBrokenProps' inner loop turns on it.
    //
    // Structurally IDENTICAL to GetPhysicalPropSlot with three differences, all measured:
    //   * the pool is mPhysicalParts / maPhysicalPartParams, capacity 30 not 15;
    //   * the assert texts name mPhysicalParts (BrnPropCellManager.cpp:1014, not :1091);
    //   * the eviction leg does NOT run the owner tripwire on the resident id.
    // ========================================================================
    bool PropCellManager::GetPhysicalPartSlot( PropEntityID lEntityId, s32& lriSlot,
                                               PropEntityID& lrEvictedId, bool& lrbEvicted )
    {
        // 0x822E0DDC..0x822E0F18 -- find-first-clear-bit, both escapes folded (see the twin).
        const s32 liFreeSlot = mPhysicalParts.GetFirstClearBit();

        if ( liFreeSlot != CgsContainers::BitArray<KU_MAX_PHYSICAL_PARTS>::KI_INVALID_BITINDEX )
        {
            // 0x822E0F1C -- SetBit bounds tripwire (CgsBitArray.h:222).
            CGS_ASSERT( static_cast<u32>( liFreeSlot ) < KU_MAX_PHYSICAL_PARTS,
                        "Index: " );

            // 0x822E0FCC..0x822E1028, in asm store order.
            mPhysicalParts.SetBit( static_cast<u32>( liFreeSlot ) );
            maPhysicalPartParams[liFreeSlot].mfTimeInSim = 0.0f;
            maPhysicalPartParams[liFreeSlot].mEntityId   = lEntityId;

            lriSlot     = liFreeSlot;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return true;
        }

        // ---- the pool is full: pick the longest-resident slot ------------------------
        f32 lfLongestTimeInSim = KF_MINIMUM_TIME_IN_SIM_BEFORE_RECYCLE;   // flt_82002138
        s32 liRecycleIndex     = -1;

        for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_PARTS; ++luSlot )
        {
            // 0x822E0E64 -- IsBitSet bounds tripwire (CgsBitArray.h:203).
            CGS_ASSERT( luSlot < KU_MAX_PHYSICAL_PARTS, "invalid index : " );

            // 0x822E10BC..0x822E1108 (BrnPropCellManager.cpp:1014).
            CGS_ASSERT( mPhysicalParts.IsBitSet( luSlot ),
                        "mPhysicalParts.IsBitSet( liRecycleIndex )" );

            // 0x822E110C..0x822E1120 -- negated ordered predicate; see the banner.
            const f32 lfTimeInSim = maPhysicalPartParams[luSlot].mfTimeInSim;
            if ( !( lfTimeInSim < lfLongestTimeInSim ) )
            {
                lfLongestTimeInSim = lfTimeInSim;
                liRecycleIndex     = static_cast<s32>( luSlot );
            }
        }

        if ( liRecycleIndex == -1 )
        {
            // 0x822E1140..0x822E1160.
            lriSlot     = -1;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return false;
        }

        // 0x822E1170..0x822E11B0. No owner tripwire here (contrast the whole-prop twin).
        lrEvictedId = maPhysicalPartParams[liRecycleIndex].mEntityId;
        lrbEvicted  = true;

        maPhysicalPartParams[liRecycleIndex].mfTimeInSim = 0.0f;
        maPhysicalPartParams[liRecycleIndex].mEntityId   = lEntityId;

        lriSlot = liRecycleIndex;
        return true;
    }

    // ========================================================================
    // PropEntityModule::ProcessBrokenProps   @ 0x822EEFA0   (363 insns)
    // DWARF BrnPropEntityModule.cpp:1430.
    // ------------------------------------------------------------------------
    // Retire the props the PHYSICS side reported broken last frame, and re-issue them as
    // PARTS. For each id in maRecentlyBrokenProps:
    //   * skip it entirely if it is already E_MOVED;
    //   * release the whole-prop physics slot, tell the prop manager to drop the whole-prop
    //     rigid body, and decrement the in-sim prop population;
    //   * then, for every part of the prop type in turn, broker a PART slot -- evicting the
    //     longest-resident part if the 30-slot pool is full (and appending the evicted id to
    //     maRecentlyRecycledParts so PreSceneUpdate can pull it out of the scene next frame)
    //     -- mark the part physical, seat it at the prop's pose offset by that part's
    //     design-time offset, and enqueue an AddPartInstance.
    // Finally clear the broken list.
    //
    // ⚠️ MEASURED: lpInput (r4) is NEVER READ. The prologue moves r3 -> r29 and r5 -> r22 and
    // leaves r4 untouched; no later instruction reads it before the first call clobbers it.
    // The parameter is declared because the CALLER passes it (PrePhysicsUpdate @0x82303100
    // sets r4 = lpInput, r5 = lpOutput) and because the DWARF declares it. Left unnamed.
    //
    // r23 == this + 0x280 == &mZoneManager throughout, and PropCellManager is embedded at
    // offset 0 inside it -- which is why the raw asm appears to call PropCellManager methods
    // on the zone manager's address (gotcha 2: three classes on one pointer). The calls below
    // go through the de-inlined PropZoneManager forwarders, per the keystone's convention.
    // ========================================================================
    void PropEntityModule::ProcessBrokenProps( const PropEntityIO::InputBuffer_PrePhysics* /*lpInput*/,
                                               PropEntityIO::OutputBuffer_PrePhysics* lpOutput )
    {
        // 0x822EF0C0..0x822EF530. A `while (true)` in the asm whose exit test is
        // `if (i >= GetLength()) break` -- i.e. this for. The bound is RE-READ every
        // iteration (0x822EF0C0 and 0x822EF0E4 both `lwz 0x80(r30)`); nothing in the body
        // adds to the set, so the re-read and the for are equivalent as well as same-shaped.
        // The three asserts the console fires per iteration are CgsSet.h:227 ("Set used
        // before Construct/Clear was called", from GetLength), :274 (the same, from GetItem)
        // and :275 ("Set index out of bounds"). GetLength()/operator[] carry the first two
        // verbatim. ⚠️ The THIRD differs in the committed container and is NOT introduced by
        // this body: the console bounds the index against the LIVE COUNT (`lwz 0x80(set)`,
        // `cmplw ; blt`) while CgsSet.h:97-100's GetItem bounds it against the CAPACITY N.
        // Same message, weaker predicate. Recorded, not worked around -- the loop bound above
        // is the live count, so neither form can fire here.
        for ( u32 luBroken = 0; luBroken < maRecentlyBrokenProps.GetLength(); ++luBroken )
        {
            const PropEntityID lPropEntityId = maRecentlyBrokenProps[luBroken];

            // [DIAG] NOT IN THE X360 BINARY. The last rung of the wave-Q4 break probe (set
            // BRN_PROP_DIAG): the prop reached maRecentlyBrokenProps and is being retired
            // from the simulation. Emitted at the TOP of the iteration, before the E_MOVED
            // early-out, so an entry that is skipped still shows up -- "the event exists" and
            // "the event was acted on" are different failures and the log must separate them.
            // The latch is evaluated ONCE (a static bool), not per iteration.
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[prop-diag] broken-event prop=" << lPropEntityId.GetValue()
                        << "\n";
                }
            }

            // 0x822EF150 `sub_822CDA28` == PropZoneManager::GetProp(PropEntityID) -- the
            // GLOBAL-index overload. Hex-Rays drops its second argument; the asm passes
            // r4 = the id.
            PropEntityInstance* lpInstance = mZoneManager.GetProp( lPropEntityId );

            // 0x822EF158..0x822EF184. GetState() folded (its "mu8State < E_STATE_COUNT"
            // tripwire at BrnPropEntityInstance.h:653), then `cmplwi 5 ; beq <loop tail>`.
            // INFERENCE (banner): 5 == E_MOVED. A prop that has already been shoved out of
            // its slot by an eviction is not retired again.
            if ( lpInstance->GetState() == E_MOVED )
            {
                continue;
            }

            // 0x822EF188 `lbz 0x4A ; rlwinm 0,28,28` == mu8Flags & KU_SMASHABLE_BIT.
            // INFERENCE (banner): this is IsSmashable(), which this class does not declare.
            CGS_ASSERT( ( lpInstance->mu8Flags & KU_SMASHABLE_BIT ) != 0,
                        "lpInstance->IsSmashable()" );                                    // :1470
            // 0x822EF1B0..0x822EF208. The second "mu8State < E_STATE_COUNT" plus the
            // branchless `state >= 6` carry trick IS PropEntityInstance::IsSmashed().
            CGS_ASSERT( lpInstance->IsSmashed(), "lpInstance->IsSmashed()" );              // :1471

            // 0x822EF20C..0x822EF274. `addis r3,r29,0xD ; addi r3,r3,-0x2278` == this +
            // 0xCDD88 == mpPropPhysicsDataHeader; the truncated `BrnPhysics__Props__Prop`
            // symbol is that ResourcePtr's GetMemoryResource. GetType's two bounds
            // tripwires (BrnPropPhysicsDataHeader.h:173/:174) are inlined at 0x822EF220 and
            // 0x822EF240 and come with the call.
            const BrnPhysics::Props::PropTypeData* lpType =
                mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lpInstance->muTypeId );

            // 0x822EF268..0x822EF2A4. Release the whole-prop slot, drop the whole-prop rigid
            // body, and decrement the in-sim population. mu8PhysicsIndex is re-read between
            // the two calls (`lbz 0x4C` at 0x822EF268 and 0x822EF280); nothing in between can
            // change it, so the two reads are one value.
            mZoneManager.FreePhysicalPropSlot( lpInstance->mu8PhysicsIndex );
            lpOutput->GetPropInputInterface()
                    ->RemovePropInstance( lPropEntityId, lpInstance->mu8PhysicsIndex );
            // `lwz/addi -1/stw 0x904(this+0x280)` == a bare `--miNumPropsInSim` on the
            // embedded PropCellManager. The de-inlined forwarder takes the DWARF's int32_t
            // delta, so the folded form is the delta == 1 case.
            mZoneManager.DecrementNumberOfPropsInSim( 1 );

            // 0x822EF294..0x822EF2CC. `insrwi r27, 1, 10, 22` splices the literal 1 into the
            // low 10-bit part field, preceded by the hoisted owner tripwire -- i.e.
            // SetPartIndex(1). Part ids are 1-BASED (part 0 is the whole prop).
            PropEntityID lPartEntityId = lPropEntityId;
            lPartEntityId.SetPartIndex( 1 );

            // 0x822EF2C4..0x822EF4FC. The console guards the loop on `lbz 0x5D(type) != 0`
            // and then runs a do-while over 0-based part indices, RE-READING the part count
            // at the bottom of every iteration (0x822EF4EC). lpType is a const serialised
            // record, so the guarded do-while with a re-read bound is exactly this for.
            //
            // ⚠️ CONSOLE STRIDE TRAP: the descriptor walk is `r24 += 0x30` -- banner note (1).
            const Matrix44Affine& lrPropTransform = lpInstance->mWorldTransform;

            for ( s32 liPart = 0; liPart < static_cast<s32>( lpType->GetNumberOfParts() ); ++liPart )
            {
                // 0x822EF2E4 `sub_822CDB90` == PropZoneManager::GetPart(PropEntityID).
                PropPartEntityInstance* lpPart = mZoneManager.GetPart( lPartEntityId );
                CGS_ASSERT( lpPart != NULL, "lpPart != NULL" );                           // :1495

                // 0x822EF30C..0x822EF32C. Broker a part slot. The three out-parameters are
                // the stack slots var_12C / var_134 / var_140 (r5 / r6 / r7).
                s32          liSlot    = 0;
                PropEntityID lEvictedId;
                bool         lbEvicted = false;

                if ( !mZoneManager.GetPhysicalPartSlot( lPartEntityId, liSlot,
                                                        lEvictedId, lbEvicted ) )
                {
                    // 0x822EF32C `beq cr6, loc_822EF4EC` -- straight to the LOOP TAIL, which
                    // skips the part-index advance at 0x822EF474..0x822EF4E8. So a failed
                    // slot request leaves lPartEntityId pointing at the SAME part for the
                    // next iteration. That is as-shipped, not a transcription slip.
                    continue;
                }

                if ( lbEvicted )
                {
                    // 0x822EF340..0x822EF3B4. The slot was taken from another part: clear
                    // that part's physical flag (`stb 0, 0x47`), pull its rigid body, and
                    // record it so PreSceneUpdate can take it out of the scene next frame.
                    mZoneManager.GetPart( lEvictedId )->mbPhysical = false;
                    lpOutput->GetPropInputInterface()
                            ->RemovePartInstance( lEvictedId, liSlot );

                    // 0x822EF370..0x822EF3B0. `this + 0xCDEA8` == maRecentlyRecycledParts
                    // (::Array<PropEntityID,30>), count word at +0x78. A full list simply
                    // drops the record -- the asm has no else arm.
                    if ( !maRecentlyRecycledParts.IsFull() )
                    {
                        maRecentlyRecycledParts.Append( lEvictedId );
                    }
                }
                else
                {
                    // 0x822EF3B8. `lwz/addi 1/stw 0x908(this+0x280)` == `++miNumPartsInSim`.
                    mZoneManager.IncrementNumberOfPartsInSim( 1 );
                }

                // 0x822EF3C4..0x822EF3F0, in asm store order (flag first, index second).
                CGS_ASSERT( lpPart->mbPhysical == false, "lpPart->mbPhysical == false" );  // :1523
                lpPart->mbPhysical      = true;
                lpPart->mu8PhysicsIndex = static_cast<u8>( liSlot );

                // 0x822EF3F4..0x822EF454. The part inherits the prop's ORIENTATION verbatim
                // (three `lvx128`/`stvx128` row copies) and only its translation differs:
                //     partPos = M.Right()*off.x + M.Up()*off.y + M.At()*off.z + M.Pos()
                // The console stores the prop's own wAxis into the local first and then
                // overwrites it with the computed row; both stores are reproduced.
                //
                // IDA operand-order trap. The site HERE is the BASE-VMX form -- 0x822EF448
                // `vmaddfp v13, v12, v13, v8` / 0x822EF44C `vmaddfp v0, v11, v13, v0` (no
                // `128` suffix) -- which IDA prints in RAW FIELD order `vD,vA,vB,vC`
                // meaning vD = vA*vC + vB, i.e. the ADDEND (the running accumulator v13) is
                // the THIRD operand. Verified by decode, not assumed.
                // ⚠️ The VMX128 form prints DIFFERENTLY: `vmaddfp128 vD,vA,vC,vB` is ISA
                // mnemonic order and its addend is the LAST operand (see
                // PropEntityModule_wQ_04.cpp's BreakPropIntoParts, 0x822FB20C, which is the
                // same maths written with vmaddfp128). Do not carry either rule across.
                const BrnPhysics::Props::PropPartTypeData& lrPartType =
                    lpType->GetParts()[liPart];
                const Vector3& lrOffset = lrPartType.GetOffset();

                Matrix44Affine lPartTransform;
                lPartTransform.xAxis = lrPropTransform.xAxis;
                lPartTransform.yAxis = lrPropTransform.yAxis;
                lPartTransform.zAxis = lrPropTransform.zAxis;
                lPartTransform.wAxis = lrPropTransform.wAxis;

                Vector3 lPartPosition = lrPropTransform.xAxis * lrOffset.x;
                lPartPosition = lrPropTransform.yAxis * lrOffset.y + lPartPosition;
                lPartPosition = lrPropTransform.zAxis * lrOffset.z + lPartPosition;
                lPartPosition = lrPropTransform.wAxis + lPartPosition;

                lPartTransform.wAxis = lPartPosition;

                // 0x822EF458..0x822EF470. r5 is a FRESH `lhz 0x44(prop)` -- the PROP's type
                // id, not the part's; r6 is the 0-based loop counter; r8 is the brokered slot.
                lpOutput->GetPropInputInterface()
                        ->AddPartInstance( lPartEntityId,
                                           static_cast<s32>( lpInstance->muTypeId ),
                                           liPart,
                                           lPartTransform,
                                           liSlot );

                // 0x822EF474..0x822EF4E8. GetPartIndex() (owner tripwire + `clrlwi 22`),
                // +1, then SetPartIndex (owner tripwire + the `< 1<<10` bound at
                // CgsEntityId.h:167 + `clrrwi 10 ; or`). Advance to the next part id.
                lPartEntityId.SetPartIndex( lPartEntityId.GetPartIndex() + 1 );
            }
        }

        // 0x822EF534..0x822EF540. `stwx 0, r29, 0xCDE64` == maRecentlyBrokenProps' count
        // word == Set<PropEntityID,32>::Clear(). Unconditional, outside the loop.
        maRecentlyBrokenProps.Clear();
    }

    // ========================================================================
    // PropEntityModule::PrePhysicsUpdate   @ 0x82303048   (100 insns)
    // ------------------------------------------------------------------------
    // The prop module's pre-physics tick, and the DRIVER of both broken-prop retirement and
    // potential-contact routing. WorldModule::EntityModulePrePhysicsUpdate
    // (BrnWorldModule.cpp:1688) calls it once per frame inside miPhysicsPropPrePhysicsUpdatePM.
    //
    // PrePhysicsUpdate once had an inert boot gate in WorldLinkStubs.cpp whose parameter list was
    // TOKEN-IDENTICAL to the definition below (`BrnUpdateSet` is `typedef u16`), i.e. the same
    // mangled symbol twice and an LNK2005 that `cl /c` could not see and coverage_check did not
    // scan for. That gate was deleted in the same change that mounted this file, so the body
    // here is its only definition.
    //
    // REGISTER -> PARAMETER MAP (prologue 0x82303054..0x82303064, measured):
    //   r3 -> r31 this | r4 UNUSED | r5 UNUSED | r6 -> r24 lpInput | r7 -> r23 lpOutput
    //   | r8 -> r30 lUpdateSet
    // r4/r5 are the two IOBufferStacks; this body never touches them (it neither creates nor
    // destroys a buffer -- WorldModule does that around the call). They are declared because
    // the committed signature and every sibling entity module's PrePhysicsUpdate carry them,
    // and because the arguments after them depend on their positions. Left unnamed.
    // NOTE this is NOT the float-skips-its-GPR-slot case (gotcha 3): both dead registers hold
    // real pointer parameters that the body simply does not read.
    // ========================================================================
    void PropEntityModule::PrePhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                             CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                             PropEntityIO::InputBuffer_PrePhysics* lpInput,
                                             PropEntityIO::OutputBuffer_PrePhysics* lpOutput,
                                             BrnUpdateSet lUpdateSet )
    {
        // 0x82303068 / 0x82303070 -- write lock on the OUTPUT buffer, read lock on the INPUT
        // buffer, in that order.
        lpOutput->LockForWrite();
        lpInput->LockForRead();

        if ( ( lUpdateSet & KU_UPDATESET_SIM_PAUSED ) != 0 )
        {
            // ---- 0x82303134: the simulation did not step this frame -------------------
            // Reset the "frames since the sim last paused" counter, then assert that nobody
            // handed us contacts anyway (WorldModule skips UpdateContactGeneration under this
            // same bit -- BrnWorldModule.cpp:2610).
            miUpdatesSinceLastSimPause = 0;

            CGS_ASSERT( lpInput->GetPotentialContactQueue()->GetLength() == 0,
                        "Received potential contacts in a paused update" );               // :1171
        }
        else
        {
            // ---- 0x82303080: the normal frame -----------------------------------------
            // 0x82303084 `addi r3, r31, 0x280` == &mZoneManager. Traffic lights that were
            // knocked down and have served their time are queued for restoration.
            mZoneManager.SendTrafficLightRestoreEvents( lpOutput );

            // 0x82303090 (`BrnWorld::PropEnti`, truncated by IDA) == 0x822B9348 ==
            // InputBuffer_PrePhysics::GetResetOnTrackResultQueue.
            const PropEntityIO::InputBuffer_PrePhysics::ResetOnTrackResultQueue* lpResetQueue =
                lpInput->GetResetOnTrackResultQueue();

            // 0x8230309C..0x823030F8. `lwz r11, 8(queue)` is the CONSOLE miLength offset --
            // banner note (2); GetLength() below. The callee at 0x822AC500 (IDA
            // `BrnAI::AIModuleIO::`, truncated) is
            // BaseEventQueue<ResetOnTrackResult>::GetEvent(int).
            for ( s32 liResult = 0; liResult < lpResetQueue->GetLength(); ++liResult )
            {
                const BrnAI::AIModuleIO::ResetOnTrackResult& lrResult =
                    lpResetQueue->GetEvent( liResult );

                // 0x823030D0..0x823030E8. `lbz 0(r25)` with r25 == this + 0xD3210 ==
                // mu8PlayerIndex; `lwz 0x24(event)` == meGlobalRaceCarIndex; the position is
                // `lvx128 v0, r0, r3` == the event's +0 mResetPosition, loaded before the
                // compare and stored only on a match.
                if ( static_cast<s32>( lrResult.GetGlobalRaceCarIndex() )
                     == static_cast<s32>( mu8PlayerIndex ) )
                {
                    // `stbx 1, r31, 0xCDDE0` / `stvx128 v0, r31, 0xCDDD0`.
                    mbPlayerWasJustReset     = true;
                    mLastPlayerResetPosition = lrResult.GetResetPosition();
                }
            }

            // 0x823030FC..0x82303118. Both take (lpInput, lpOutput) in that order.
            ProcessBrokenProps( lpInput, lpOutput );
            ProcessPotentialContacts( lpInput, lpOutput );

            // 0x8230311C..0x8230312C. `++*(this + 0xD335C)`.
            ++miUpdatesSinceLastSimPause;
        }

        // 0x823031C0..0x823031CC -- released in the MIRROR order of the acquire: read lock
        // first, then the write lock.
        lpInput->UnlockForRead();
        lpOutput->UnlockForWrite();
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ2_01.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ2_01.cpp
//
// WAVE Q ROUND 2 (2026-08-18) -- the replay half of the BREAKABLE-PROPS keystone that
// round 1 reconstructed completely but could not compile.  Round 1 parked three bodies on
// BrnReplays::PropSerialiserFrame; the round-2 replay-frame owner has now landed the
// frame's nine real sub-arrays and its Get{Prop,Part}Transform accessors
// (scratchpad/waveQ2/replays.owner.md), so the prop-render body lands here.
//
// LEDGER TUs (one class, two ids):
//   GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   class:BrnWorld::PropEntityModule
//
// BODIED HERE (1 of 3):
//   BrnWorld::PropEntityModule::RenderReplayProp          @0x822EF968  (77 insns)
//
// STILL PARKED (2 of 3) -- ONE remaining blocker, and it is NOT the frame:
//   BrnWorld::PropEntityModule::ReplayUpdatePropsInScene  @0x822DB370  (355 insns)
//   BrnWorld::PropEntityModule::ReplayUpdatePartsInScene  @0x822DB900  (393 insns)
//     -> scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePropsInScene.cpp
//     -> scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePartsInScene.cpp
//   Their frame reads are now fully expressible (maPropPositions / maTypes /
//   maPartPositions / maPartTypes / maPartIds all exist).  What they still cannot say is
//   the scene write: both push a BARE POSITION through
//   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::SetEntityPosition, and this
//   tree declares only `SetEntityPosition(u32, const Matrix44Affine&)`.  The DecFIGS DWARF
//   declares exactly one form and it is the other one --
//   references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/
//   CgsSceneManagerIO_SceneUpdate.h:446 (source line :346)
//   `void SetEntityPosition(EntityId, Vector3);`.  See the parked banners.
//
// ---- SOURCES ---------------------------------------------------------------------
//  * RAW X360 ASSEMBLY, .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EF968.json, read as
//    `assembly` and decoded instruction by instruction.  (The Hex-Rays pseudocode types
//    this function `_DWORD *(int, unsigned int, int, int, double)` and mislabels the
//    variadic tail -- it is not used for anything below.)  77 instructions COUNTED:
//    0x822EF968..0x822EFA98 inclusive, /4, +1.
//  * scratchpad/waveQ2/replays.owner.md sections 6 and 7 for the landed member spelling.
//  * scratchpad/waveQ/round1/PropEntityModule_G2.md for the round-1 verifier's
//    instruction-by-instruction confirmation of this body (no MUST_FIX was filed against
//    RenderReplayProp itself; the two MUST_FIXes in that report are GetDesiredState's,
//    already committed in PropEntityModule_wQ_02.cpp).
//
// ---- CALLEES (the export's own `xrefs_from`, MEASURED -- every one has a body) -----
//   0x822A4090  BrnReplays::PropEntitySerialiser::GetStaticLayout
//                 -> BrnReplayPropEntitySerialiser.cpp:43
//   0x822AA638  BrnReplayArray<u16,254>::operator[](u8)   [IDA prints this symbol
//                 TRUNCATED as "BrnReplays::DefaultAreDiffere" -- gotcha 6; the real
//                 identity is fixed by its stride-2 / count-at-+0x1FC guard]
//                 -> generic inline in BrnReplayArray.h + the committed explicit
//                    instantiation GameSource/Replays/Array_u16_254_operator_index.cpp
//   0x822BB920  BrnReplays::PropSerialiserFrame::GetPropTransform
//                 -> BrnReplayPropSerialiserFrame_wQ2_owner.cpp:225 (landed this round)
//   0x822DC010  BrnWorld::PropEntityModule::RenderPropAndCoronas
//                 -> BrnPropEntityModule_Render.cpp:429
//   plus CgsDev::Assert::Begin/Fire/EndAssert (CGS_ASSERT) and __savegprlr_24 (helper).
// `xrefs_to` has EXACTLY ONE entry: PropEntityModule::GenerateDispatchLists @0x822FB4F0.
// Note what is NOT in xrefs_from: PropGraphicsManager::GetPropGraphics -- confirming from
// the export itself that the console inlined it (see note 3 below).
//
// ---- WHAT CHANGED SINCE THE PARKED COPY (re-derived, per the round-2 brief) --------
//  1. `maPropTypes` -> `maTypes`.  The round-1 park banner guessed the name; the landed
//     one is ATTESTED VERBATIM by the streamed assert literal " maTypes.GetLength(): "
//     that PropSerialiserFrame::KeyFrameRead/Read/Write build
//     (BrnReplayPropEntitySerialiser.h:420/:421).  The park banner's pad-arithmetic recipe
//     ("shrink maPad27ED by one byte") is moot -- the whole pad ladder is gone.
//  2. The park banner's `Matrix44Affine GetPropTransform(u8) const` request landed exactly
//     as written, at BrnReplayPropSerialiserFrame.h:301.
//  3. THE EXPLICIT `CGS_ASSERT( luType < KU_MAX_PROP_TYPES )` IS DELETED -- it was a
//     double-fire.  MEASURED: the assert the console fires at 0x822EF9F0..0x822EFA0C cites
//     BrnPropZoneManager.cpp:1034 with the literal "luType < BrnPhysics::Props::KU_MAX_PROP"
//     -- i.e. it is PropGraphicsManager::GetPropGraphics' OWN assert, inlined here together
//     with its table read.  That read is FIVE instructions, transcribed verbatim (an
//     earlier banner quoted a three-instruction `lwzx r4,r11,0xD21C4` form that does not
//     exist -- `lwzx` takes two register operands):
//         0x822EFA10  slwi  r11, r31, 3
//         0x822EFA14  lis   r10, 0xD
//         0x822EFA18  add   r11, r11, r29
//         0x822EFA1C  ori   r10, r10, 0x21C4      # 0xD21C4
//         0x822EFA20  lwzx  r4, r11, r10
//     -- in effect `r4 = *(this + 0xD21C4 + 8*type)`; the module-relative 0xD21C4 is
//     mPropGraphicsManager and the ×8 stride is that manager's PropGraphicsReference.
//     The committed de-inlined accessor already carries that identical CGS_ASSERT
//     (BrnPropZoneManager.cpp:203), so restating it here would fire it TWICE where the
//     console fires it once.  Calling GetPropGraphics at this point in the sequence
//     reproduces the console firing exactly (AGENTS.md "undo compiler optimizations").
//
// ---- CONSOLE-OFFSET DISCIPLINE (gotcha 1) -----------------------------------------
// Not one console offset, stride or size appears in the code below.  The decode used to
// READ the asm is recorded here as PROVENANCE ONLY:
//     this + 0x0D3180   mPropEntitySerialiser  (0x822EF990 `addis r31,r29,0xD ; addi 0x3180`)
//     this + 0x0D21C4   mPropGraphicsManager   (0x822EFA1C, indexed `slwi r11,type,3`)
//     staticLayout + 0x3A20  == mLiveFrame     (0x822EF9B4 / 0x822EF9DC)
//     liveFrame    + 0x25F0  == maTypes        (0x822EF9BC)
// Every one of them is reached BY NAME below.
//
// ---- gotchas 3 and 4, checked -----------------------------------------------------
//  * gotcha 3 (a float rides an FPR and SKIPS its GPR slot): this is precisely why the
//    parameter list below has no parameter in the r7 slot -- `f1` (lfLodDistanceZoomScale)
//    consumes it.  The same rule one slot later is why RenderPropAndCoronas' `r9` is empty.
//    Both halves of the map are proved by the forwarding stores at 0x822EFA2C..0x822EFA84,
//    which copy each incoming stack slot to the outgoing one 0x10 bytes higher.
//  * gotcha 4 (PPC NaN polarity): NOT APPLICABLE -- this function performs no floating
//    point compare.  Its only float, f1, is moved (`fmr f1,f31`) and never tested.  The
//    two integer branches are `blt cr6` on an unsigned compare and `beq cr6` on a null
//    test; neither has a NaN reading.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET (spec section 5) --------------------------
// ---------------------------------------------------------------------------------

namespace BrnWorld
{
    // ---- SIGNATURE PIN (gotcha 11) -------------------------------------------------
    // RenderReplayProp is NOT virtual: it is a plain public member of PropEntityModule
    // (BrnPropEntityModule.h -- the `public:` at :264 is the last access specifier before
    // :378), and the export's `xrefs_to` list has EXACTLY ONE entry,
    // PropEntityModule::GenerateDispatchLists @0x822FB4F0 -- a direct `bl`, no vtable slot.
    // So there is no override to bind.  What IS worth pinning is that the definition binds
    // to the DECLARATION
    // at BrnPropEntityModule.h:378 and not to some future overload of the same name: taking
    // a member pointer at the declared type makes a parameter-type drift a compile error
    // here rather than an unbodied declaration at link time.
    namespace
    {
        typedef void ( PropEntityModule::*RenderReplayPropFn )(
            PropEntityID, DispatchFrame*, Matrix44::InParam, Vector3::InParam, f32,
            s32, s32, s32, bool, const ShaderLodInfo*, bool, const void*, bool,
            BrnCoronaManager::BrnSubmissionInterface* );
        const RenderReplayPropFn KP_RENDER_REPLAY_PROP = &PropEntityModule::RenderReplayProp;
    }

    // ========================================================================
    // PropEntityModule::RenderReplayProp  @ 0x822EF968  (77 insns, counted)
    //
    // The playback-time draw of ONE recorded prop: resolve its recorded type and world
    // transform out of the replay frame, look the graphics record up in
    // mPropGraphicsManager, and forward the entire render tail to RenderPropAndCoronas.
    // There is no logic of its own beyond the two lookups and the null early-out.
    //
    // PARAMETER MAP -- this is a MEASURED PER-PARAMETER MAP, not a rule to re-derive from.
    // Every entry below is read straight off the forwarding stores at 0x822EFA2C..0x822EFA84.
    // Stack slots are named by their SLOT BASE; on this big-endian target a 4-byte value
    // lands at slot+4 and a bool at slot+7, which is the address the cited instruction uses.
    //
    // ⚠️ THE ROUND-1/2 "SAVE-AREA RULE" THAT USED TO SIT HERE WAS WRONG AND IS DELETED.
    // It said "the save area starts at incoming-SP + 0x14, 8 bytes per parameter".  0x14 is
    // not 8-byte aligned, and every slot measured below IS (0x60/0x68/0x70/0x78/0x80/0x88
    // incoming), so no 8-byte ladder can start there; 0x14 + 8*9 == 0x5C, one slot short of
    // the measured 0x60.  The rule that DOES reproduce all six incoming and all eight
    // outgoing offsets, checked arithmetically below, is: save area at incoming-SP + 0x08
    // (X360 linkage area = back chain @0x00 + saved LR @0x04); 8 bytes per scalar in
    // DECLARATION order; a __vector4 rides v1, consumes NO GPR, but DOES take a 16-byte,
    // 16-byte-ALIGNED save-area slot; a float rides f1, BURNS its GPR (gotcha 3) and takes
    // its own 8-byte slot.  Worked through for this function:
    //     this@0x08  id@0x10  dispatch@0x18  viewproj@0x20  [align 0x28->0x30]
    //     vector@0x30..0x40  f1@0x40  modelOnly@0x48  opaque@0x50  transparent@0x58
    //     envMap@0x60(byte 0x67)  shaderLod@0x68(word 0x6C)  coronas@0x70(byte 0x77)
    //     vfx@0x78(word 0x7C)  zOnly@0x80(byte 0x87)  iface@0x88(word 0x8C)   -- 6/6 exact.
    // Note the GPR ladder and the save-area ladder are SEPARATE counters here: the vector
    // advances only the save-area ladder, which is why the last three GPR arguments are
    // r8/r9/r10 while their slots are 0x48/0x50/0x58.
    // THE SAME WRONG SENTENCE STILL SITS IN BrnPropEntityModule.h:365-366 (and that header's
    // table at :372-375 uses the same slot+4 labelling as the old table below) -- that file
    // is not this partfile's to edit; the header's owner should apply the identical
    // correction so the wrong rule stops propagating.
    //
    //     incoming                    ->  outgoing (RenderPropAndCoronas)
    //     r4  lPropEntityId           ->  (consumed: GetEntityIndex)
    //     r5  dispatch frame          ->  r7          (0x822EFA44  mr r7, r28)
    //     r6  view-projection         ->  r8          (0x822EFA3C  mr r8, r27)
    //     v1  camera position         ->  v1          (0x822EFA30  vmr128 v1, v127)
    //     f1  lod zoom scale          ->  f1          (0x822EFA48  fmr f1, f31)
    //     r8  model-only list         ->  r10         (0x822EFA34  mr r10, r26)
    //     r9  opaque list             ->  slot 0x60   (0x822EFA40  stw r25, var_DC == +0x64)
    //     r10 transparent list        ->  slot 0x68   (0x822EFA38  stw r24, var_D4 == +0x6C)
    //     slot 0x60 envMap  (bool)    ->  slot 0x70   (0x822EFA7C lbz arg_67 / 0x822EFA80
    //                                                  stb var_C9 == +0x77)
    //     slot 0x68 shaderLodInfo     ->  slot 0x78   (0x822EFA74 lwz arg_6C / 0x822EFA78
    //                                                  stw var_C4 == +0x7C)
    //     slot 0x70 renderCoronas     ->  slot 0x80   (0x822EFA6C lbz arg_77 / 0x822EFA70
    //                                                  stb var_B9 == +0x87)
    //     slot 0x78 VFX prop table    ->  slot 0x88   (0x822EFA64 lwz arg_7C / 0x822EFA68
    //                                                  stw var_B4 == +0x8C)
    //     slot 0x80 zOnly   (bool)    ->  slot 0x90   (0x822EFA58 lbz arg_87 / 0x822EFA60
    //                                                  stb var_A9 == +0x97)
    //     slot 0x88 corona iface      ->  slot 0x98   (0x822EFA2C lwz arg_8C / 0x822EFA50
    //                                                  stw var_A4 == +0x9C)
    //   (var_X displacements are 0x140 - X, the frame being `stwu r1,-0x140`; incoming
    //    arg_X displacements are 0x140 + X off the post-prologue r1, i.e. X off entry SP.)
    //   and the three RenderPropAndCoronas parameters this function SUPPLIES itself:
    //     r4 lpPropGraphics (the manager lookup)
    //     r5 lpTransform    (0x822EFA54 `addi r5, r1, var_A0` -- the 64-byte sret slot
    //                        GetPropTransform filled at 0x822EF9D8/0x822EF9E4)
    //     r6 luPropTypeId   (0x822EFA4C `mr r6, r31`)
    //
    // Return type is void: the epilogue sets no result and the `beq loc_822EFA88`
    // early-out leaves r3 holding a leftover.
    // ========================================================================
    void
    PropEntityModule::RenderReplayProp(
        PropEntityID lPropEntityId,
        DispatchFrame* lpDispatchFrame,
        Matrix44::InParam lCameraViewProjection,
        Vector3::InParam lCameraPosition,
        f32 lfLodDistanceZoomScale,
        s32 liModelOnlyDisplayList,
        s32 liOpaqueList, s32 liTransparentList,
        bool lbRenderingEnvironmentMap,
        const ShaderLodInfo* lpShaderLodInfo,
        bool lbRenderCoronas,
        const void* lpVFXPropTable,
        bool lbUseZOnlyRendering,
        BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface )
    {
        // 0x822EF9AC `extrwi r30, r4, 14, 8` -- PropEntityID::GetEntityIndex() inlined.
        //
        // ASSERT PARITY, MEASURED so note 3's "one firing, not two" claim is not overstated:
        // the 77-instruction body contains EXACTLY ONE assert triple (BeginAssert /
        // FireAssert / EndAssert occur once each, at 0x822EF9F0 / 0x822EFA08 / 0x822EFA0C),
        // and that one is the luType/KU_MAX_PROP_TYPES assert de-inlined below.  The console's
        // index extraction here is a bare branch-free `extrwi` with no compare and no call,
        // whereas the tree's PropEntityID::GetEntityIndex (BrnPropEntityID.cpp:35-39) opens
        // with AssertIsProp().  That tripwire is therefore an ACCEPTED TREE-CONVENTION
        // ADDITION, not a console firing -- the same convention as the committed sites
        // BrnPropZoneManager.cpp:426/:630 and BrnPropCellManager.cpp:823, and it cannot fire
        // spuriously on this path because the sole caller, GenerateDispatchLists @0x822FB4F0,
        // iterates prop entities (owner byte always 3).  If exact firing parity is ever
        // wanted, read the field without the tripwire instead of changing the console shape.
        const u32 luPropIndex = lPropEntityId.GetEntityIndex();

        // 0x822EF9B0..0x822EF9CC.  The recorded type, out of the LIVE frame's type table.
        // The index is truncated to a byte on the way in (0x822EF9B8 `clrlwi r4,r30,24`) --
        // that is BrnReplayArray<T,N>::operator[](u8)'s own parameter type, and its bounds
        // assert ("Array index out of bounds: ", BrnReplayArray.h) is the one the console
        // fires.  The element load is `lhz` (0x822EF9CC), a zero-extended u16 that the rest
        // of the function carries in a 32-bit register and hands on as the u32
        // luPropTypeId, so the local is a u32 here too.
        const u32 luType =
            mPropEntitySerialiser.GetStaticLayout()
                ->mLiveFrame.maTypes[ static_cast<u8>( luPropIndex ) ];

        // 0x822EF9D0..0x822EF9E4.  GetStaticLayout() is called a SECOND time here in the
        // shipped code (it is a checked accessor, so both calls carry its "Static buffer
        // size is too small" assert); reproduced rather than folded, so no assert firing is
        // lost.  The transform comes back BY VALUE into the 64-byte stack slot the call
        // below then points at (r3 is the hidden sret pointer, r4 is &mLiveFrame, r5 the
        // index -- GetPropTransform does its own `clrlwi r29,r5,24` truncation).
        const Matrix44Affine lPropTransform =
            mPropEntitySerialiser.GetStaticLayout()
                ->mLiveFrame.GetPropTransform( static_cast<u8>( luPropIndex ) );

        // 0x822EF9E8..0x822EFA28.  The console has PropGraphicsManager::GetPropGraphics
        // INLINED here -- its bounds assert (0x822EF9E8 `cmplwi cr6,r31,0x1F4`, fired at
        // 0x822EF9F0 against BrnPropZoneManager.cpp:1034) and then its one-line table read.
        // De-inlined back to the accessor: see the banner note 3 -- restating the assert
        // here would fire it twice.  It really does sit AFTER the transform fetch, and it
        // is NON-GATING: the console asserts and carries straight on into the lookup.
        //
        // No assert on the RESULT -- an unregistered type is simply not drawn.
        const BrnPhysics::Props::PropGraphics* lpPropGraphics =
            mPropGraphicsManager.GetPropGraphics( luType );
        if ( lpPropGraphics == 0 )
        {
            return;                                        // 0x822EFA28 beq loc_822EFA88
        }

        // 0x822EFA2C..0x822EFA84 -- the straight forward.
        RenderPropAndCoronas( lpPropGraphics,
                              &lPropTransform,
                              luType,
                              lpDispatchFrame,
                              lCameraViewProjection,
                              lCameraPosition,
                              lfLodDistanceZoomScale,
                              liModelOnlyDisplayList,
                              liOpaqueList,
                              liTransparentList,
                              lbRenderingEnvironmentMap,
                              lpShaderLodInfo,
                              lbRenderCoronas,
                              lpVFXPropTable,
                              lbUseZOnlyRendering,
                              lpCoronaSubmissionInterface );
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ2_02.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ2_02.cpp
//
// WAVE Q ROUND 2 -- LANDER 02. Two bodies that round 1 reconstructed in full and parked
// out-of-tree on missing declarations; the round-2 shared-header owners have since landed
// every one of those declarations, so both bodies come in-tree here.
//
//   BrnWorld::PropEntityModule::PostPhysicsUpdate   @0x823031D8  (254 insns -- COUNTED from
//        .ida-exports/BURNOUT_X360_ARTIST.XEX/0x823031D8.json's `assembly`, 254 lines)
//   BrnWorld::PropCellManager::RecordPropPositions  @0x822E1988  (197 insns -- COUNTED the
//        same way from 0x822E1988.json)
//
// Round-1 material: scratchpad/waveQ/parked/PropEntityModule_08_PostPhysicsUpdate.cpp and
// .../PropEntityModule_08_RecordPropPositions.cpp (complete bodies + grounding), and the
// verifier's MUST_FIX/NIT list in scratchpad/waveQ/round1/PropEntityModule_G8.md.
// Round-2 owner specs consumed: scratchpad/waveQ2/worldio.owner.md (the IO buffer cluster)
// and scratchpad/waveQ2/replays.owner.md (PropSerialiserFrame + BaseSerialiser).
//
// Ledger TUs: GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//             and class:BrnWorld::PropEntityModule. (RecordPropPositions is a PropCellManager
//             member the ledger files `reviewed` with no body anywhere; it is landed here
//             because PostPhysicsUpdate link-fails without it, exactly as round 1 reported.)
//
// ============================================================================
// WHAT CHANGED FROM THE PARKED TEXT, AND WHY (every item re-derived, not taken on trust)
// ============================================================================
// (1) THE MEASURED 87-BYTE HEAP OVERRUN IS GONE. The parked body reached the replay request
//     interface as `reinterpret_cast<ReplayIO::RequestInterface*>(GetPropInputInterface())`.
//     BrnPropEntityModuleIO.h had three OutputBuffer_PostPhysics accessors bound one member
//     out of step, so that getter returned the buffer's LAST byte and RegisterSerialiser
//     indexed an 88-byte host object starting there. The header now names the members
//     correctly and the call is `lpOutput->GetReplayRequestInterface()` -- the accessor whose
//     X360 symbol is 0x822B9FC0 (IDA's truncated "…::GetRep", which is the callee the asm
//     shows at 0x823032A0).
//
// (2) THE PROGRESSION BYTE HAS A REAL MEMBER + THE DWARF'S OWN SETTER NAME.
//     `SetShouldRequestProgression(true)` -> `RequestPropProgression()`
//     (BrnPropEntityModuleIO.h:466, DWARF :745). The round-1 request's proposed
//     CGS_ASSERT(IsBufferLockedForWriting()) was an INVENTED tripwire -- the console's store
//     at 0x8230321C..0x82303228 is a bare `lis/ori/stbx` with no lock-bit test inlined,
//     unlike the sibling out-of-line accessors. The landed setter carries no assert; that is
//     the faithful shape and this file does not add one.
//
// (3) THE +0x5B GATE IS REAL AND IT IS NEGATED. Round 1 landed the Write() UNCONDITIONALLY
//     on the premise that nothing in the image writes BaseSerialiser+0x5B. That premise is
//     the exact inverse of the truth: BaseSerialiser::Construct @0x8264C280 does
//     `stb r9, 0x5B(r31)` @0x8264C2C8 with its 6th argument, and PropEntitySerialiser passes
//     1. Construct has no per-address JSON export, which is why an export-set scan saw no
//     writer. The byte is homed as `mbSkipModuleSerialise` with the reader
//     `SkipModuleSerialise()`, and the asm here is unambiguous:
//         0x823034A8  ori   r11, r11, 0x31DB     ; module+0xD31DB == serialiser+0x5B
//         0x823034B0  lbzx  r11, r28, r11
//         0x823034B4  stbx  r17, r28, 0xD31D9    ; mbDataRestored = true  (UNCONDITIONAL)
//         0x823034B8  cmplwi cr6, r11, 0
//         0x823034BC  bne   cr6, loc_823035C8    ; non-zero -> return, skipping the Write
//     So SetDataRestored(true) runs either way and the GetStaticLayout()/Write() pair is
//     gated. This is the same MUST_FIX the group-3 verifier filed against the twin call in
//     ReplayPreSceneUpdate; landing the Write unconditionally is NOT behaviour-identical,
//     because BaseSerialiser::Read/Write move bytes on the replay stream.
//
// (4) THE PROPSERIALISERFRAME PAD LADDER IS GONE -- and round 1's unblock recipe for it was
//     wrong twice over. The frame is NINE BrnReplayArray members (round 1 asked for five and
//     missed both orientation arrays), and the two count bytes round 1 wanted as bare
//     `u8 mu8Recorded{Props,Parts}Length` are `maPropPositions.muLength` /
//     `maPartPositions.muLength`. All eight clears and both count reads below use the landed
//     spelling (paste table: scratchpad/waveQ2/replays.owner.md §6).
//
// (5) WriteProp/WritePart TAKE THE TRANSFORM, NOT THE INSTANCE. Round 1 asked for
//     `WriteProp(const PropEntityInstance*, u16)`. What landed is
//     `WriteProp(const Matrix44Affine&, u16)` -- the console reads exactly the four 16-byte
//     rows at r4+0/+0x10/+0x20/+0x30 and nothing else, and the caller passes the instance
//     pointer only because mWorldTransform is that class's first member. Call sites below
//     pass the transform explicitly.
//
// (6) THE 254 / 128 THRESHOLD COMMENT WAS WRONG (gotcha 9). Round 1 wrote that 254 "is the u8
//     count domain BrnReplayArray::KU_MAX_LENGTH names" and that 128 "is a soft warning line,
//     not the array bound". Both halves are false: 254 and 128 are the CAPACITIES N of
//     BrnReplayArray<Vector3,254> (frame +0x610, count +0x15F0) and
//     BrnReplayArray<Vector3,128> (frame +0x27F0, count +0x2FF0). "Replay full" means
//     literally full. Rewritten at the constants below.
//
// ============================================================================
// LINK-LEVEL FACTS THE CONDUCTOR NEEDS (gate-green != link-green, gotcha 12)
// ============================================================================
//  * PostPhysicsUpdate once had an inert boot gate in WorldLinkStubs.cpp; that gate was deleted
//    in the same change that mounted this file, so the body here is its only definition.
//    RecordPropPositions never had a gate anywhere (grepped the stub TUs).
//  * MOUNTED, together with the set this file needs: wQ2_03 (ProcessContacts), wQ_06
//    (UpdateProps) and wQ_01 (PrepareForReplay / RestoreFromReplay).
//  * CALLEES: EVERY ONE IS BODY-PRESENT IN THE TREE -- re-grepped 2026-08-18, definition by
//    definition (an earlier version of this block named two phantom blockers):
//      ProcessContacts                     PropEntityModule_wQ2_03.cpp:169
//      UpdateProps                         PropEntityModule_wQ_06.cpp:117
//      PrepareForReplay / RestoreFromReplay PropEntityModule_wQ_01.cpp:126 / :190
//      PropZoneManager::RecordPropPositions BrnPropZoneManager.cpp:889
//      PropZoneManager::GetHitPropsFromZone BrnPropZoneManager.cpp:665
//      PropSerialiserFrame::WriteProp/WritePart
//                                          BrnReplayPropSerialiserFrame_wQ2_owner.cpp:165/:185
//      PropSerialiserFrame::GetZone        BrnReplayPropSerialiserFrame.cpp:80
//      PropEntitySerialiser::GetStaticLayout / Write
//                                          BrnReplayPropEntitySerialiser.cpp:43 / :119
//      BaseSerialiser::IsPlaying / IsRecording
//                                          BrnReplayBaseSerialiser.cpp:30 / :39
//      RequestInterface::RegisterSerialiser BrnReplayRequestInterface.cpp:57
//      OutputBuffer_PostPhysics::GetReplayRequestInterface
//                                          BrnPropEntityModuleIO_OutputBuffer_PostPhysics.cpp:219
//      InputBuffer_PostPhysics::GetUpdatedPropQueue
//                                          BrnPropEntityModuleIO_InputBuffer_PostPhysics.cpp:87
//    (SetDataReady / SetDataRestored / GetMode / SkipModuleSerialise / RequestPropProgression /
//     PropPhysicsDataHeader::GetType / PropTypeData::GetNumberOfParts are header inlines.)
//    The ONLY link obstacle is the WorldLinkStubs gate above, plus the mount set.
//  * GetStaticLayout() RETURNS NULL ON THIS BUILD, and both bodies here dereference it
//    UNCONDITIONALLY -- console-faithfully, so the fix is NOT to add a guard. BaseSerialiser
//    ::Construct sets `mpStaticBuffer = 0` (BrnReplayBaseSerialiser.cpp:196, the only
//    assignment in the tree) and nothing ever fills it; BrnReplayPropEntitySerialiser.cpp:52-89
//    documents this as a [FLAG PC boot gate] and records the 2026-08-12 access violation when
//    the identical pattern read frame+0x5E8 off a null base (== 0x4008), which is why the four
//    record-side serialiser ENTRY POINTS there carry `if (!HasStaticLayout()) return;`.
//    NOT REACHABLE TODAY: every dereference here sits behind IsRecording()/IsPlaying()/GetMode()
//    and nothing in the tree calls BaseSerialiser::SetMode, so meMode stays E_MODE_IDLE and
//    all those legs are dead. WHEN the replay manager's buffer hand-off lands, or when anything
//    starts driving the prop serialiser's mode, RE-CHECK THIS TU FIRST -- if a guard is ever
//    needed it belongs at the serialiser entry points, where the tree already puts it.
//
// ============================================================================
// CONSOLE-LITERAL DISCIPLINE (gotcha 1)
// ============================================================================
// No console offset, stride or size appears in the code below. The asm walks props with the
// console stride 80 (`slwi 2; add; slwi 4` at 0x822E1AB4 and 0x822E1BE8) -- that is
// sizeof(PropEntityInstance) ON THE CONSOLE; here every access is `mpaProps[i]` /
// `mpaPropParts[i]` and the host compiler picks the stride. The frame's eight count bytes are
// reached through their owning array's `.muLength`, never through 0x15F0-style displacements.
// The only literals are state-machine enumerators, the two array capacities (spelled from the
// frame's own KU_MAX_RECORDED_* constants), and the hit-props run length (spelled from the
// BitArray's own kuNumberOfBitFields, not as a bare 10).
//
// NaN POLARITY (gotcha 4): NOT APPLICABLE to either function -- neither touches an FPR. I
// re-read both `assembly` arrays for fcmpu/fsel/fmr/vcmp: zero hits.
// PPC FLOAT-ARG ABI (gotcha 3): NOT APPLICABLE -- no float parameter in either signature.
// EMBEDDED SUB-OBJECT (gotcha 2): 0x82303308 `addi r23, r28, 0x280` builds r23 (NOT r3 -- it
// becomes r3 only at 0x82303314 `mr r3, r23`; the intervening 0x8230330C `mr r5, r3` captures
// sub_822CA1E8's RETURN value, the type header). r23 == module + 0x280 is
// &mZoneManager, whose PropCellManager sits at its offset 0 -- which is why the call goes
// through PropZoneManager::RecordPropPositions (the committed forwarder,
// BrnPropZoneManager.cpp:888) and not through a raw cast.
// ============================================================================


namespace BrnWorld
{
    namespace
    {
        // The two "the replay frame is full, stop recording" warning thresholds, read straight
        // off the compares at 0x822E1B44 (`cmplwi r11, 0xFE`) and 0x822E1C24
        // (`cmplwi r11, 0x80`).
        //
        // CORRECTED (round-1 comment was wrong, gotcha 9): BOTH numbers are the CAPACITY N of
        // the array whose count byte is being tested -- BrnReplayArray<Vector3,254> at frame
        // +0x610 (count +0x15F0) and BrnReplayArray<Vector3,128> at frame +0x27F0
        // (count +0x2FF0). Neither is "the u8 count domain" and neither is a soft line below
        // the real bound: "replay full" means literally full, and the console emits the
        // warning on the record that fills the array. Spelled from the frame's own constants
        // so the two can never drift apart.
        // (the two constants live at BrnReplays namespace scope, beside the frame's other
        //  capacities -- BrnReplayPropSerialiserFrame.h:145-146.)
        const u8 KU_PROP_REPLAY_FULL_WARNING      = BrnReplays::KU_MAX_RECORDED_PROPS;   // 254
        const u8 KU_PROP_PART_REPLAY_FULL_WARNING = BrnReplays::KU_MAX_RECORDED_PARTS;   // 128
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::PostPhysicsUpdate  @0x823031D8  (254 insns)
    //
    // The prop module's post-physics tick. Four phases, in order:
    //   1. lock the pair of IO buffers and, if the streaming machine is waiting to ask the
    //      profile for this player's prop progression, raise the request on the output buffer
    //      and advance the machine;
    //   2. unless this update set is the light one, run ProcessContacts (the smash recorder)
    //      and UpdateProps (apply the physics results), each inside its own CPU monitor;
    //   3. publish the prop serialiser to the frame's replay request interface and unlock;
    //   4. the replay ladder -- record a frame while recording, or step the
    //      prepare/restore handshake while entering or leaving playback.
    //
    // MEASURED from the 0x823031D8 `assembly`: the parameter map (r3 this, r4/r5 the two
    // buffer stacks -- both DEAD, no instruction reads them, r6 lpInput, r7 lpOutput,
    // r8 lUpdateSet); the lock order (LockForWrite on the OUTPUT before LockForRead on the
    // input, unlocked in the mirrored order); meStreamingMode 4 -> byte, then 5; all three
    // perf-monitor handles (module +0xD336C / +0xD3370 / +0xD3374); the whole mode ladder and
    // which flag byte each leg writes; the per-zone loop.
    // INFERENCE, marked in place: that the open-coded mode sets {1,2,3} and {4,5,6} are
    // IsRecording() / IsPlaying(); the enumerator NAMES for the raw streaming words 4 and 5;
    // that the zone record's +0x58 run is maPropsPreviouslyHit.
    // ------------------------------------------------------------------------------------
    void PropEntityModule::PostPhysicsUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                                              CgsModule::IOBufferStack* lpOutputBufferStack,
                                              PropEntityIO::InputBuffer_PostPhysics* lpInput,
                                              PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                                              BrnUpdateSet lUpdateSet )
    {
        // MEASURED: r4 and r5 are moved nowhere and never read. They are declared because the
        // caller (WorldModule::EntityModulePostPhysicsUpdate) passes them and because every
        // sibling tick on this class takes the same five parameters.
        (void)lpInputBufferStack;
        (void)lpOutputBufferStack;

        lpOutput->LockForWrite();
        lpInput->LockForRead();

        if ( meStreamingMode == E_REQUESTING_PROFILE_DATA )
        {
            // 0x8230321C..0x8230322C: a bare `lis/ori/stbx` of 1 into the buffer's progression
            // byte, then `stw 5` into meStreamingMode. No lock-bit test is inlined here, so the
            // setter this calls carries none either.
            lpOutput->RequestPropProgression();
            meStreamingMode = E_WAITING_FOR_PROFILE_DATA;
        }

        // [pausebit] witness. NOT X360. Prints ONLY when the value CHANGES, so a whole run costs
        // a handful of lines. Pairs with the identical probe in PhysicsModule::Update: the two
        // read the SAME bit of the SAME set and must agree on every frame.
        {
            static u32 suLastSet = 0xFFFFFFFFu;
            const u32 luSet = static_cast<u32>( lUpdateSet );
            if ( luSet != suLastSet && CgsDev::Log::gpDebugPrint != 0 )
            {
                suLastSet = luSet;
                *CgsDev::Log::gpDebugPrint
                    << "[pausebit] PropEntityModule::PostPhysicsUpdate updateSet="
                    << CgsDev::E_PRINTMODE_HEXONCE << luSet
                    << " bit0=" << static_cast<s32>( luSet & 1 )
                    << " -> ProcessContacts " << ( ( luSet & 1 ) == 0 ? "RUNS" : "skipped" ) << "\n";
            }
        }

        // MEASURED: bit 0 of the update set, not bit 8 (`clrlwi r29, r8, 31` at 0x823031F4,
        // then `bne -> skip` at 0x82303238). The heavy half of the tick is skipped on the
        // update sets that do not carry a physics result.
        if ( ( lUpdateSet & 1 ) == 0 )
        {
            CgsDev::PerfMonCpu::StartMonitor( miProcessContactsPM );
            ProcessContacts( lpInput, lpOutput );
            CgsDev::PerfMonCpu::StopMonitor( miProcessContactsPM );

            CgsDev::PerfMonCpu::StartMonitor( miUpdatePropsPM );
            UpdateProps( lpOutput, lpInput->GetUpdatedPropQueue() );
            CgsDev::PerfMonCpu::StopMonitor( miUpdatePropsPM );
        }

        // Publish this module's serialiser so the replay manager picks it up when it walks the
        // frame's request interface. 0x823032A0 calls OutputBuffer_PostPhysics's +0xCB5F0
        // accessor (IDA's truncated "…::GetRep" == GetReplayRequestInterface @0x822B9FC0) and
        // feeds the result straight to RegisterSerialiser with r4 == module+0xD3180.
        lpOutput->GetReplayRequestInterface()->RegisterSerialiser( &mPropEntitySerialiser );

        lpInput->UnlockForRead();
        lpOutput->UnlockForWrite();

        if ( mPropEntitySerialiser.IsRecording() )
        {
            // ---- record one prop frame -------------------------------------------------
            CgsDev::PerfMonCpu::StartMonitor( miSerialisePM );

            // Snapshot every active cell's props/parts into the live frame. 0x82303308 builds
            // r23 = module + 0x280 == &mZoneManager (PropCellManager is at its offset 0); it
            // becomes r3 at 0x82303314, after 0x8230330C `mr r5,r3` has captured the type
            // header returned by the call at 0x82303304. So this goes through the committed
            // PropZoneManager forwarder.
            mZoneManager.RecordPropPositions( &mPropEntitySerialiser,
                                              GetPropPhysicsDataHeader() );

            // Then copy each loaded zone's persistent "already hit" run into that zone's
            // record in the frame, so playback respawns exactly what the live run did.
            for ( u32 luZone = 0; luZone < mLoadedZones.GetLength(); ++luZone )
            {
                // FOREIGN-HEADER DIVERGENCE, assert-only, reported not worked around: the
                // console's inlined Set::GetItem bounds-checks against muLength --
                // 0x823033B0 `lwz r11, 0x20(r30)` (the set base is module+0xD337C and its
                // count word is +0xD339C, i.e. set+0x20), then `cmplw r22,r11 ; blt`, else the
                // "Set index out of bounds" assert with baked line 0x113 == 275. The committed
                // template asserts against CAPACITY instead (CgsSet.h:99 and the non-const
                // twin at :103, `CGS_ASSERT(luIndex < N, ...)`, N == 15 here). Equivalent for
                // THIS loop -- luZone is already bounded by GetLength() -- so nothing is
                // corrupted; but the committed guard is up to N-muLength slots too permissive
                // for every Set<T,N> user, and it belongs with the containers owner alongside
                // the BrnReplayArray KU_MAX_LENGTH==254-should-be-N request. The COMPANION
                // assert from the same inline does NOT diverge: 0x82303390 `cmpwi r11,-1 ;
                // bne` fires baked line 0x112 == 274 exactly when muLength == KU_INVALID,
                // which is what CgsSet.h:98 spells.
                const u16 lu16ZoneId = mLoadedZones.GetItem( luZone );

                u64 lau64HitProps[ CgsContainers::BitArray<BrnReplays::KU_PROPS_PER_ZONE>::kuNumberOfBitFields ];
                mZoneManager.GetHitPropsFromZone( lau64HitProps, lu16ZoneId );

                // Baked file is a GameSource/Replays/Serialisers one (line 0x2EA == 746), i.e.
                // this tripwire was folded out of a serialiser-side inline; reproduced at the
                // call site because that inline has no recovered name.
                CGS_ASSERT( !mPropEntitySerialiser.IsPlaying(), "!IsPlaying()" );

                BrnReplays::PropLoadedZoneRecord* lpZone =
                    mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.GetZone( lu16ZoneId );
                CGS_ASSERT( lpZone != 0, "lpZone != NULL" );

                // asm 0x82303468..0x82303488: a bare `mtctr 0xA` / `ld` / `std` copy of the ten
                // 64-bit fields into the record's +0x58 run. Spelled from the BitArray's own
                // field count so no console literal survives.
                for ( u32 luField = 0;
                      luField < CgsContainers::BitArray<BrnReplays::KU_PROPS_PER_ZONE>::kuNumberOfBitFields;
                      ++luField )
                {
                    lpZone->maPropsPreviouslyHit.SetBitField( luField, lau64HitProps[ luField ] );
                }
            }

            CgsDev::PerfMonCpu::StopMonitor( miSerialisePM );

            // 0x823034B4: mbDataRestored = true, stored BEFORE the gate is tested, so it runs
            // on both sides of the branch.
            mPropEntitySerialiser.SetDataRestored( true );

            // 0x823034A8..0x823034BC: the serialiser's +0x5B byte. NON-ZERO SKIPS the
            // GetStaticLayout()/Write() pair. BaseSerialiser::Construct @0x8264C280 stores its
            // 6th argument there (`stb r9, 0x5B(r31)` @0x8264C2C8) and PropEntitySerialiser
            // passes 1, so on the shipped image this Write never runs -- which is exactly why
            // it must be gated rather than landed unconditionally.
            if ( !mPropEntitySerialiser.SkipModuleSerialise() )
            {
                mPropEntitySerialiser.Write( mPropEntitySerialiser.GetStaticLayout() );
            }
        }
        else if ( mPropEntitySerialiser.GetMode() == BrnReplays::BaseSerialiser::E_MODE_RECORDING_PREPARING
                  || mPropEntitySerialiser.GetMode() == BrnReplays::BaseSerialiser::E_MODE_PLAYING_PREPARING )
        {
            // E_MODE_RECORDING_PREPARING is unreachable here (IsRecording() already claimed
            // modes 1..3 above), but the console really does test both -- 0x823034E0 compares
            // against 1 and 0x823034E8 against 4. Kept, rather than reduced to the one live
            // case, because reducing it would be an edit the binary does not attest.
            CGS_ASSERT( mbInReplay, "mbInReplay" );                 // baked line 0x70A == 1802

            mPropEntitySerialiser.SetDataReady( true );             // 0x82303548, serialiser +0x58
            miReplayState = 0;                                      // 0x82303550, module +0xD3330
        }
        else if ( mPropEntitySerialiser.GetMode() == BrnReplays::BaseSerialiser::E_MODE_RESTORING )
        {
            mPropEntitySerialiser.SetDataRestored( RestoreFromReplay() );   // 0x82303578, +0x59
        }
        else if ( mPropEntitySerialiser.IsPlaying() )
        {
            mPropEntitySerialiser.SetDataReady( PrepareForReplay() );       // 0x823035C4, +0x58
        }
    }

    // ------------------------------------------------------------------------------------
    // PropCellManager::RecordPropPositions  @0x822E1988  (197 insns)
    //
    // The replay RECORD-side per-frame snapshot, driven once per frame by
    // PropEntityModule::PostPhysicsUpdate while the prop serialiser is recording. It empties
    // the live frame's record arrays and re-fills them from the cells that are currently
    // active: the cell ids themselves, then, for every prop in each cell's [start,end) run
    // that is actually in the scene, either the whole prop (still standing) or every one of
    // its parts (already smashed).
    //
    // MEASURED from the 0x822E1988 `assembly`: the prologue map (r3 this == PropCellManager,
    // r4 the serialiser, r5 the type table); the eight clears and their order (the cell-array
    // one is LAST, 0x822E19C8); the outer loop over maActiveCells (base +0x70C, stride 12,
    // bound miNumActiveCells at +0x76C re-read every iteration); the cell-id round trip; the
    // per-prop `mu8Flags & 2` gate; the smashed/not-smashed split; both "replay full" strings
    // and their thresholds; the part loop's bound re-read (`lbz r11, 0x5D(r25)` at 0x822E1C54).
    // INFERENCE, marked in place: that the `>= 6` carry fold is `GetState() >= E_SMASHED`;
    // the enumerator name KU_ADDED_TO_SCENE_BIT for the `rlwinm ...,0,30,30` mask (== 2).
    // ------------------------------------------------------------------------------------
    void PropCellManager::RecordPropPositions( BrnReplays::PropEntitySerialiser* lpSerialiser,
                                               const PropPhysicsDataHeader* lpTypes )
    {
        // ---- reset the frame's record arrays ------------------------------------------
        // Eight count bytes, in the console's own order (the cell-array count last). Each is
        // the muLength of one of the frame's nine BrnReplayArray members. MEASURED: the ninth
        // (maLoadedZones, count byte at frame +0x5E8 == static +0x4008) is NOT among the eight
        // stores at 0x822E19AC..0x822E19C8, so this function leaves it alone. [INFERENCE for
        // the reason only: the loaded-zone set is session state, not per-frame state.]
        {
            BrnReplays::PropSerialiserFrame& lrFrame =
                lpSerialiser->GetStaticLayout()->mLiveFrame;

            lrFrame.maPropPositions.muLength    = 0;   // static 0x5010 -> frame 0x15F0
            lrFrame.maPropOrientations.muLength = 0;   // static 0x6000 -> frame 0x25E0
            lrFrame.maTypes.muLength            = 0;   // static 0x620C -> frame 0x27EC
            lrFrame.maPartPositions.muLength    = 0;   // static 0x6A10 -> frame 0x2FF0
            lrFrame.maPartOrientations.muLength = 0;   // static 0x7220 -> frame 0x3800
            lrFrame.maPartTypes.muLength        = 0;   // static 0x7330 -> frame 0x3910
            lrFrame.maPartIds.muLength          = 0;   // static 0x7432 -> frame 0x3A12
            lrFrame.maRecordedCells.muLength    = 0;   // static 0x4020 -> frame 0x0600
        }

        for ( s32 liCell = 0; liCell < miNumActiveCells; ++liCell )
        {
            const PropCellRecord& lrCell = maActiveCells[ liCell ];

            // ⚠️ THE ONE PLACE THE CONSOLE PACKS A CELL ID INTO A WORD. BrnPhysicsPropZoneData.h
            // rightly warns never to view PropCellId as a u32 -- but here the record array's
            // element IS a u32, and the console builds it from the two halves by hand:
            //   0x822E1A54  lhz r11, temp+0    ; the X half of the big-endian word
            //   0x822E1A58  lhz r30, temp+2    ; the Z half
            //   0x822E1A5C  insrwi r30, r11, 16, 0   ; (X << 16) | Z
            // Rebuilt from GetX()/GetZ() so the host produces the same VALUE regardless of
            // endianness (a `*(u32*)&id` view would give (Z<<16)|X here). The consumer,
            // PropSerialiserFrame::IsCellActive, computes the word the same arithmetic way.
            const u32 luPackedCellId =
                ( static_cast<u32>( lrCell.GetId().GetX() ) << 16 )
                | static_cast<u32>( lrCell.GetId().GetZ() );

            // 0x822E1A64..0x822E1AA0 is BrnReplayArray<u32,4>::PushBack folded inline.
            // ⚠️ FLAGGED, not fixed (foreign header): the console's guard here is
            // `cmplwi r11, 4` -- i.e. MaxLength is the per-instantiation N, not a fixed 254.
            // BrnReplayArray.h:56 declares `KU_MAX_LENGTH = 254` for every instantiation, so
            // the committed PushBack's tripwire is 250 elements too permissive for <u32,4>
            // (and 126 too permissive for every N == 128 array). Non-gating assert, so nothing
            // breaks today; the fix belongs in BrnReplayArray.h (`= N`).
            lpSerialiser->GetStaticLayout()->mLiveFrame.maRecordedCells.PushBack( luPackedCellId );

            for ( u16 lu16Prop = lrCell.GetStartIndex(); lu16Prop < lrCell.GetEndIndex(); ++lu16Prop )
            {
                PropEntityInstance* lpProp = &mpaProps[ lu16Prop ];

                // Only props that are actually published to the scene are recorded.
                if ( ( lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT ) == 0 )
                {
                    continue;
                }

                // GetState() carries its own "mu8State < E_STATE_COUNT" tripwire (baked line
                // 0x28D == 653) -- the assert the console emits at 0x822E1AE4. The split below
                // is the `subfc/subfe/addi` carry trick at 0x822E1B08..0x822E1B1C, which
                // evaluates to (mu8State >= 6); written as its negation.
                if ( lpProp->GetState() < E_SMASHED )
                {
                    // Still standing: one whole-prop record. The console passes the instance
                    // pointer, but WriteProp reads only the four 16-byte transform rows at
                    // r4+0/+0x10/+0x20/+0x30 -- and mWorldTransform is the instance's first
                    // member -- so the landed signature takes the transform by reference.
                    lpSerialiser->GetStaticLayout()->mLiveFrame.WriteProp(
                        lpProp->GetWorldTransform(), lpProp->muTypeId );

                    if ( lpSerialiser->GetStaticLayout()->mLiveFrame.maPropPositions.muLength
                         == KU_PROP_REPLAY_FULL_WARNING )
                    {
                        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                        {
                            *CgsDev::Log::gpDebugPrint << "Prop replay full\n";
                        }
                    }
                    continue;
                }

                // Already smashed: record every part instead. GetType() carries the two bounds
                // tripwires the console emits inline here ("liTypeId < KU_MAX_PROP_TYPES" /
                // "liTypeId < muNumberOfPropTypes", baked lines 0xAD == 173 / 0xAE == 174).
                const PropTypeData* lpType = lpTypes->GetType( lpProp->muTypeId );

                // MEASURED: the part count is re-read from the type every iteration
                // (0x822E1C54), so it stays a live bound here rather than a hoisted local. The
                // console's zero-part early-out at 0x822E1BD4 is this loop's entry test.
                for ( u32 luPart = 0; luPart < lpType->GetNumberOfParts(); ++luPart )
                {
                    PropPartEntityInstance* lpPart =
                        &mpaPropParts[ lpProp->mu16PartsIndex + luPart ];

                    // Same transform-by-reference rule as WriteProp; r5 is `lhz 0x40` ==
                    // muTypeId (loaded at 0x822E1BFC into r29, moved to r5 at 0x822E1C0C) and
                    // r6 is `lhz 0x42` == muPartId (loaded at 0x822E1BF8 into r30, moved to r6
                    // at 0x822E1C10) -- the two loads are emitted in the opposite order to the
                    // arguments they feed.
                    // PropPartEntityInstance has no GetWorldTransform accessor, so the public
                    // member is named directly.
                    lpSerialiser->GetStaticLayout()->mLiveFrame.WritePart(
                        lpPart->mWorldTransform, lpPart->muTypeId, lpPart->muPartId );

                    if ( lpSerialiser->GetStaticLayout()->mLiveFrame.maPartPositions.muLength
                         == KU_PROP_PART_REPLAY_FULL_WARNING )
                    {
                        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                        {
                            *CgsDev::Log::gpDebugPrint << "Prop part replay full\n";
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ2_03.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ2_03.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q ROUND 2, LANDER 03. Three bodies that round 1
// reconstructed COMPLETELY but had to park out-of-tree for want of declarations. Those
// declarations landed in round 2 (shared-header owner, scratchpad/waveQ2/worldio.owner.md),
// so the bodies come in-tree here:
//
//   PropEntityModule::ProcessContacts                 @0x822FA890  (476 insns)  <- THE GATE
//   PropEntityModule::ProcessPotentialContactWithPart @0x822EEDA8  (126 insns)
//   PropEntityModule::PostSceneUpdate                 @0x822C4718  ( 61 insns)
//
// Instruction counts above are COUNTED, in this pass, off the non-empty lines of each
// address's `assembly` array in .ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json.
//
// SOURCE OF TRUTH. Every line below is read off the X360 ARTIST **assembly** arrays for the
// three addresses. The Hex-Rays pseudocode is a map only -- it renders all three functions
// with `int` parameters and inlines the vector math as opaque blocks.
//
// ---- LAYOUT DISCIPLINE (AGENTS.md gotcha 1) ---------------------------------------------
// Not one console byte offset appears in the code. Every member is reached by name or
// accessor. The console values that DO appear in the asm and are deliberately NOT
// reproduced:
//   * the prop-contact queue at contactSpyData + 0x167E0 -> GetPropContacts()
//   * maRaceCarVelocity at module + 0xD3230 stride 16     -> GetRaceCarVelocity(idx)
//   * mauStartIndexOfZone at `2*(zone + 0x662A1) + module`-> GetStartIndexOfZone(zone)
//   * the queue length at queue + 8                       -> GetLength()  (mpEvents widens
//     4 -> 8 on the host, so that offset moves)
//   * maRecentlyRecycledParts at module + 0xCDEA8         -> the named member
//   * the physical-part budget bound 30                   -> KU_MAX_PHYSICAL_PROP_PARTS
//   * the crash-complete queue at lpInput + 8             -> GetCrashEventQueue()
//   * PropVFXLocatorEvent's +0x00/+0x10/+0x20/+0x30/+0x40/+0x44 stores -> Construct()
//
// ---- ROUND-1 VERIFY ISSUES CLOSED HERE ---------------------------------------------------
//   [MUST_FIX, G8] "the three queue re-types write through a NEVER-CONSTRUCTED EventQueue".
//     Both halves are gone: OutputBuffer_PostPhysics now HAS a Construct()
//     (BrnPropEntityModuleIO_OutputBuffer_PostPhysics.cpp:136, X360 @0x822EFE08) that points
//     every embedded queue's mpEvents at its inline maEvents[], and the four queues are real
//     typed CgsModule::EventQueue<T,N> members, so all three reinterpret_cast re-types are
//     DELETED below in favour of the typed accessors.
//   [NIT, G8] the stale BrnPropZoneManager.h addresses for HasPropBeenHit(PropEntityID) /
//     RecordHitProp(PropEntityID): re-checked this pass -- the header now reads 0x822CDD48 /
//     0x822CDCD0 (:353 / :359), which are the two addresses ProcessContacts actually calls at
//     0x822FAD40 / 0x822FAD5C. Closed, nothing to do here.
//   [G5 park] the miNumPartsInSim reader: PropZoneManager::IsInPhysicalBudgetForParts(s32)
//     landed at BrnPropZoneManager.h:488 with exactly the requested spelling.
//   [G2 park] the InputBuffer_PostScene 16-byte placeholder: the buffer now carries the real
//     RaceCarCrashCompleteEventQueue member plus Construct/Destruct/GetCrashEventQueue.
//
// ---- STALE PARK-BANNER CLAIMS RE-DERIVED AND CORRECTED (AGENTS.md gotchas 9 + 10) --------
//   1. The parked ProcessContacts banner's "SECOND FINDING" (three OutputBuffer_PostPhysics
//      accessors mislabelled) and its ⚠️ note at the Clear() call site are now HISTORY: the
//      header was retyped in round 2 and every accessor binds the member its X360 body
//      returns. The Clear() below goes through GetPropInputInterface(), which is both the
//      DWARF name and the +0xC86B0 member -- no deliberate misnaming remains.
//   2. That banner's "THIRD FINDING" opaque-storage re-type idiom no longer applies; the
//      queues are typed.
//   3. The parked ProcessPotentialContactWithPart banner said KVF_PROP_FLOOR "has NO header
//      home anywhere in the tree" and spelled it file-local. FALSE as of round 2: it is
//      homed at SharedClasses/Physics/Props/BrnPropConstants.h:56 as
//      BrnPhysics::Props::KVF_PROP_FLOOR = -1000.0f. The file-local copy is DELETED and the
//      header constant used, so there is one spelling of the value.
//   4. That same banner reported BrnPropZoneManager.cpp's -15000.0f / "not in .ida-exports"
//      comment defect. Re-checked: the round-2 owner already fixed it (that file now uses
//      KVF_PROP_FLOOR). The note is dropped rather than repeated.
//   5. The parked PostSceneUpdate banner's whole "WHY IT IS PARKED / THE EXACT LINES THAT
//      UNBLOCK IT" section described a placeholder that no longer exists. Dropped.
//
// ---- ⚠️ REPORTED, NOT FIXED (foreign headers this lane does not own) ---------------------
//   a. ⭐ CLOSED 2026-08-18 (round-3 fix pass) -- kept as the record, since this file's
//      consumer is what surfaced it. The old BrnPropConstants.h comment claimed all three
//      KVF_PROP_FLOOR consumers compared with `vcmpgtfp.` and that "the predicate is
//      (KVF_PROP_FLOOR > pos.y)". MEASURED at the consumer in THIS file: 0x822EEE50 is
//      `vcmpgefp. v0, v0, v13`, an ORDERED >=, not >. The header now carries a PER-CONSUMER
//      list instead (BrnPropConstants.h:31-61, re-read there this pass), and it was wrong for
//      TWO of the three, not one:
//        ProcessPotentialContactWithPart @0x822EEE50 `vcmpgefp.`  -> KVF_PROP_FLOOR >= pos.y
//        ChangePropState                 @0x822EF5F4 `vcmpgtfp.`  -> pos.y > KVF_PROP_FLOOR
//                                        (operands REVERSED; corroborated by the console's own
//                                         baked assert string, PropEntityModule_wQ_04.cpp:182)
//        PropZoneManager::UpdateInstance @0x822F0B9C `vcmpgtfp.`  -> KVF_PROP_FLOOR > pos.y
//                                        (the only one the old blanket claim described)
//      The >= site and the > sites differ at exactly pos.y == -1000.0f; all three are ORDERED,
//      so a NaN Y is false everywhere. DO NOT normalise them to one spelling.
//   b. BrnPropEntityModuleIO.h:141-146 says BrokenPropEvent's single byte is "the broken
//      prop's index/id". MEASURED: ProcessContacts stores the STRIKING CAR's entity index
//      there (0x822FAF5C `lwz r11, 4(r25)` == mEntityIdB, the car; `srwi r11,r11,10` then
//      `stb`). Written faithfully below and flagged at the site.
//
// ---- LINK-LEVEL (AGENTS.md gotcha 7 + 12) ------------------------------------------------
//   PostSceneUpdate once had an inert boot gate in WorldLinkStubs.cpp with the identical
//   5-parameter signature (BrnUpdateSet is `typedef u16`, so the two mangled the same). Mount and
//   retirement happened in one change -- the duplicate would have been invisible to `cl /c` and to
//   coverage_check, which globs only this directory and so reports DUPLICATE(ODR) = 0. This file
//   and the partfiles it links with are mounted.
//   Neither ProcessContacts nor ProcessPotentialContactWithPart ever had a gate in either stub
//   TU (re-grepped this pass).
// ============================================================================


namespace BrnWorld
{
    namespace
    {
        // flt_82014648, the literal-pool float ProcessContacts' smash test multiplies the dot
        // product by. 1 / 0.44704 == metres-per-second -> miles-per-hour. The project already
        // homes the same value under this spelling in VehiclePhysics.cpp:2582 and
        // BrnVehicleManager_ValidateRaceCarWorldContact.cpp:57 (there from the NAMED X360
        // symbol KF_MPS_TO_MPH @0x8208F820, which is what fixes the name); this TU's copy is
        // an unnamed literal-pool entry, so it is spelled locally in the same style rather
        // than a new shared header being minted for it.
        const f32 KF_MPS_TO_MPH = 2.2369363f;

        // flt_820147E0, the epsilon the "this prop smashes as soon as it moves" test adds to
        // the move threshold before comparing it with the smash threshold. Loaded once into
        // f31 at 0x822FA9DC and used at 0x822FAE98 (`fadds f0, f0, f31`).
        const f32 KF_EASY_SMASH_THRESHOLD_EPSILON = 0.1f;

        // [DIAG] NOT IN THE X360 BINARY. Line budget for the LEG-0 per-contact rung below.
        // ADDED 2026-08-23 (bug-wave round 2): that rung shipped with NO budget at all and
        // printed one line per qualifying car-vs-prop contact per frame -- an unbounded
        // per-frame log line, which the wave's own probe rules forbid. Budgeted like every
        // sibling rung in this file set.
        const s32 KI_DIAG_MAX_CONTACT_LINES = 64;
    }

    // ========================================================================================
    // PropEntityModule::ProcessContacts   @0x822FA890   (476 insns)
    //
    // THE SMASH RECORDER -- the gate function of wave Q. Runs inside PostPhysicsUpdate's
    // miProcessContactsPM monitor, once per frame, over the physics side's resolved
    // prop-contact list. For every contact between a WHOLE prop (part index 0) and a race car
    // it does two independent things:
    //
    //   1. PROGRESSION (player car only): if the prop respawns in a non-default way, publish a
    //      RecordPropHitEvent (position + zone + index-in-zone + "had it been hit before") for
    //      GameState's progression/StuntManager store, and mark the prop hit in the persistent
    //      bit array so it never respawns intact again.
    //   2. SMASH (any car): if the prop is smashable, not already smashed, and the impact
    //      along the contact normal beat its smash threshold (or the type is an "easy smash"),
    //      record it in maRecentlyBrokenProps and emit the outbound BrokenPropEvent +
    //      PropVFXLocatorEvent that the pre-scene pass turns into flying parts and VFX.
    //
    // Leg 2 runs for EVERY qualifying contact, not just player ones -- it is reached by the
    // fall-through at loc_822FAD60, outside leg 1's player/progression gate.
    //
    // CONSOLE OFFSET -> MEMBER DECODE (provenance only; nothing below indexes by offset):
    //   module +0xD3210 mu8PlayerIndex          module +0xD3342 mbAllowPropProgression
    //   module +0xD3343 mbPlayerCrashing        module +0xCD960 mbUseOverrides
    //   module +0xCD96C mfOverrideSmashThreshold module +0xD3230 maRaceCarVelocity
    //   module +0xCDDE4 maRecentlyBrokenProps    module +0x280   mZoneManager
    //   instance +0x4A  mu8Flags   +0x4D mu8State   +0x44 muTypeId   +0x40 muInstanceID
    //   instance +0x46  mu16ZoneIndex            +0x30 mWorldTransform.wAxis
    //   type     +0x50  mfMoveThreshold  +0x54 mfSmashThreshold  +0x5D muNumberOfParts
    // ========================================================================================
    void PropEntityModule::ProcessContacts( const PropEntityIO::InputBuffer_PostPhysics* lpInput,
                                            PropEntityIO::OutputBuffer_PostPhysics* lpOutput )
    {
        CGS_ASSERT( lpInput  != 0, "lpInput != NULL" );    // BrnPropEntityModule.cpp:1923 (li r5,0x783)
        CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // BrnPropEntityModule.cpp:1924 (li r5,0x784)

        // The scene-entity id of the player's own car. Built through EntityId::Set so the host
        // does the packing; the console folded two of Set()'s three tripwires away because
        // their arguments are compile-time constants there, leaving only the entity-index one
        // (CgsEntityId.h:116, `cmplwi r31, 0x4000` at 0x822FA90C) -- which is exactly what
        // Set() still emits. (asm 0x822FA934: `slwi r11, mu8PlayerIndex, 10 ; oris r11,0x100`.)
        CgsSceneManager::EntityId lPlayerCarEntityId;
        lPlayerCarEntityId.Set( E_ENTITYTYPE_RACECAR, mu8PlayerIndex, 0 );

        // Reset this frame's outbound physical-prop request bundle (the four add/remove queue
        // lengths + the remove-all flag). asm 0x822FA944..0x822FA964 -- a call to the buffer's
        // +0xC86B0 accessor followed by PropInputInterface::Clear() inlined (`stw 0` at +8 /
        // +0xFB8 / +0x1F68 / +0x28D4 and `stb 0` at +0x2C00, which is Clear()'s exact store
        // set, BrnPropInputInterface.h).
        lpOutput->GetPropInputInterface()->Clear();

        // The physics side's resolved prop-contact list for this frame. The console folds
        // ContactSpyInterface::GetPropContacts + ContactSpyData::GetPropContacts inline at
        // 0x822FA970..0x822FA9A8: the "mpData != NULL" tripwire (BrnContactSpyInterface.h:211)
        // then mpData + 0x167E0 == &mPropContactQueue. Both are header inlines in the tree now.
        const BrnPhysics::ContactSpy::ContactSpyData::PropContactQueue* lpPropContacts =
            lpInput->GetContactSpyInterface()->GetPropContacts();

        // The bound is RE-READ every iteration (asm 0x822FAFE4 `lwz r10, 8(r3)`), so a callee
        // that appended would be seen -- kept as a live-bound loop, not a hoisted length.
        for ( s32 liContact = 0; liContact < lpPropContacts->GetLength(); ++liContact )
        {
            const BrnPhysics::ContactSpy::PropContact& lrContact =
                lpPropContacts->GetEvent( liContact );

            // The OTHER entity must be a race car. asm 0x822FAA90: `lbz r11, 4(r25)` reads the
            // BIG-ENDIAN top byte of mEntityIdB, i.e. its owner field, and compares == 1.
            const CgsSceneManager::EntityId lOtherEntityId( lrContact.mEntityIdB.muValue );
            if ( lOtherEntityId.GetOwner() != E_ENTITYTYPE_RACECAR )
            {
                continue;
            }

            // The prop side. The constructor carries the owner tripwire the console emits
            // inline here ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278,
            // asm 0x822FAAA0 `srwi r11, r31, 24 ; cmplwi r11, 3`).
            const PropEntityID lPropEntityId( lrContact.mEntityIdA.muValue );

            // Whole props only -- a contact against a broken-off PART is handled by the
            // pre-physics path. asm 0x822FAAC4 `clrlwi r11, r31, 22` then the subfic/subfe
            // "is non-zero" fold.
            if ( lPropEntityId.GetPartIndex() != 0 )
            {
                continue;
            }

            PropEntityInstance* lpInstance = mZoneManager.GetProp( lPropEntityId );

            // asm 0x822FAAF8 `lbz r11, 0x4A(r30) ; rlwinm r11, r11, 0,29,29` -- the accessor is
            // FOLDED to the bare mask (bit 29 from the MSB == 4 == KU_ADDED_TO_CONTACT_GEN_BIT),
            // there is no call. Spelled as the mask for two reasons, both of which the tree
            // already settled: it is what the console executes, and it is the idiom the four
            // committed sibling sites use (BrnPropCellManager.cpp:534/580/607/685, same assert
            // text, same open-coded mask). PropEntityInstance::IsAddedToContactGen() is declared
            // at BrnPropEntityInstance.h:125 and DEFINED NOWHERE in the tree, so calling it here
            // would add an unresolved external to the wave's gate function for no fidelity gain.
            CGS_ASSERT( ( lpInstance->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT ) != 0,
                        "lpInstance->IsAddedToContactGen()" );                  // :1963 (0x7AB)
            // GetState() carries its own "mu8State < E_STATE_COUNT" tripwire
            // (BrnPropEntityInstance.h:653, asm 0x822FAB20..0x822FAB40, li r5,0x28D) -- that is
            // the second assert the console emits here, and it is NOT written out again.
            CGS_ASSERT( lpInstance->GetState() > E_NON_PHYSICAL,
                        "lpInstance->GetState() > E_NON_PHYSICAL" );            // :1964 (0x7AC)

            // [DIAG] NOT IN THE X360 BINARY. Opt-in break-pipeline probe for the wave-Q4
            // bring-up: set BRN_PROP_DIAG to answer "did a car-vs-prop contact even reach
            // the world module?" -- the first question every parked bridge in
            // WorldBridgePropModule.cpp changes the answer to. Placed here, after the
            // race-car/whole-prop filters and before LEG 1, so it reports EVERY qualifying
            // contact, not just the ones that go on to smash. The impact speed is recomputed
            // inside the gate (same dot/abs/MPH the smash test uses further down) so the
            // shipped path pays nothing when the probe is off.
            // The latch is evaluated ONCE: getenv per contact would be a per-frame syscall.
            //
            // ⚠️ ROUND-2 CORRECTION 2026-08-23 -- READ THIS BEFORE FILING `speed=` AS A BUG.
            // `speed=` is |dot(carVelocity, contactNormal)| in MPH: the closing speed ALONG
            // THE CONTACT NORMAL. It is NOT the car's speed, and the two coincide only for a
            // dead-on hit. A car riding OVER a prop has a near-vertical normal, so `speed=`
            // collapses to the car's VERTICAL velocity (a few cm/s == ~0.1 mph) while the car
            // is doing 74 mph -- that is geometry, not a mis-read slot. The 2026-08-23 drive
            // (scratch/flow_run/bugwave_verify1/BrnGame.log) carries BOTH readings out of this
            // ONE expression -- 74.15 / 73.39 / 55.44 / 32.77 mph on head-on contacts and
            // 0.13-0.27 mph on the ride-over frames -- which is what rules out "the wrong
            // velocity slot is being read". Both operands are printed now so nobody has to
            // re-litigate it from the scalar alone.
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                static s32        siContactDiagLinesLeft = KI_DIAG_MAX_CONTACT_LINES;

                if ( sbPropDiag && siContactDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siContactDiagLinesLeft;

                    const s32     liStrikingCar =
                        static_cast<s32>( lOtherEntityId.GetEntityIndex() );
                    const Vector3 lDiagCarVelocity = GetRaceCarVelocity( liStrikingCar );
                    const Vector3 lDiagNormal      = lrContact.mNormal;
                    const f32     lfDiagDotMps     =
                        rw::math::vpu::Dot( lDiagCarVelocity, lDiagNormal );

                    *CgsDev::Log::gpDebugPrint
                        << "[prop-diag] contact prop=" << lPropEntityId.GetValue()
                        << " type=" << lpInstance->muTypeId
                        << " car=" << liStrikingCar
                        << " speed=" << ( fabsf( lfDiagDotMps ) * KF_MPS_TO_MPH )
                        << " carSpeedMph=" << GetRaceCarSpeed( liStrikingCar )
                        << " dotMps=" << lfDiagDotMps
                        << " n=(" << lDiagNormal.x << "," << lDiagNormal.y
                        << "," << lDiagNormal.z << ")"
                        << " |n|=" << rw::math::vpu::Magnitude( lDiagNormal )
                        << " v=(" << lDiagCarVelocity.x << "," << lDiagCarVelocity.y
                        << "," << lDiagCarVelocity.z << ")"
                        << "\n";
                }
            }

            // -------------------------------------------------------------------------------
            // LEG 1 -- PROGRESSION. Player car only, prop not already moved, progression
            // enabled and the player not mid-crash. Four separate `bne/beq -> loc_822FAD60`
            // at 0x822FAB68 / 0x822FAB78 / 0x822FAB88 / 0x822FAB9C, in this order.
            // -------------------------------------------------------------------------------
            // The four clauses are named so the [DIAG] rung below can report WHICH ONE rejected a
            // contact. Each is a plain member/word read with NO side effect, so computing all
            // four ahead of the `&&` chain (which still short-circuits) is observationally
            // identical to the console's four separate branches.
            const bool lbPlayerCarStruckIt =
                ( static_cast<u32>( lPlayerCarEntityId ) == lrContact.mEntityIdB.muValue );
            const bool lbPropNotMoved    = ( ( lpInstance->mu8Flags & KU_MOVED_BIT ) == 0 );
            const bool lbProgressionAllowed = mbAllowPropProgression;
            const bool lbPlayerNotCrashing  = !mbPlayerCrashing;

            // [DIAG] NOT IN THE X360 BINARY. ROUND-8 LEG-1 CLAUSE PROBE -- the cheapest probe the
            // round-7 verifier named, and the one rung that can redirect round 9.
            //
            // WHY IT EXISTS. Run 9 (scratch\flow_run\20260820_200848\BrnGame.log:4720-4745) shows
            // the first smashed gate printing "[prop-diag] contact" (LEG 0, above) and
            // "[prop-diag] BREAK" (LEG 2, below) but NEITHER "Hit dont respawn prop:" NOR
            // "Hit respawn changed prop:" -- so the contact reached this function, qualified as a
            // whole-prop-vs-race-car contact, went on to smash, and was rejected somewhere inside
            // LEG 1. A later gate on the same drive (:5226-5232) passed LEG 1 and produced the
            // whole [UI-gate] ladder, so nothing about the leg is structurally missing. The five
            // candidate rejectors are the four clauses above plus the E_RESPAWN early-out, and
            // the existing "[prop-diag] contact" line cannot separate them: it prints
            // `car=GetEntityIndex()`, which hides the part-index and owner bits that the clause-1
            // compare (a WHOLE-word compare against the player car's packed entity id) actually
            // uses. Hence: both ids IN FULL, plus each clause, plus GetRespawnType().
            //
            // COST WHEN OFF: one already-evaluated bool test. GetRespawnType() is called only
            // inside the guard -- it is const and side-effect free (BrnPropZoneManager.cpp's
            // body is two BitArray::IsBitSet reads plus non-gating tripwires), so the shipped
            // path is byte-for-byte the console's.
            //
            // SCOPE: smashable props only (KU_SMASHABLE_BIT, the same mask LEG 2 gates on at
            // asm 0x822FAD60). Gates and billboards are smashable; kerbside clutter is not, and
            // letting clutter spend the first-16 budget before the junkyard exit is exactly how
            // this rung would fail to answer the question it was added for. The full mu8Flags
            // decode is not printed -- the two bits that matter are printed by name.
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                static s32 siLeg1DiagLinesLeft = 16;

                if ( sbPropDiag && siLeg1DiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0
                     && ( lpInstance->mu8Flags & KU_SMASHABLE_BIT ) != 0 )
                {
                    const BrnPhysics::Props::eRespawnType leDiagRespawnType =
                        mZoneManager.GetRespawnType( PropEntityID( lrContact.mEntityIdA.muValue ) );

                    const bool lbLeg1Records =
                        lbPlayerCarStruckIt && lbPropNotMoved && lbProgressionAllowed
                        && lbPlayerNotCrashing
                        && leDiagRespawnType != BrnPhysics::Props::E_RESPAWN;

                    if ( !lbLeg1Records )
                    {
                        --siLeg1DiagLinesLeft;
                        *CgsDev::Log::gpDebugPrint
                            << "[prop-diag] LEG1 REJECT prop=" << lPropEntityId.GetValue()
                            << " playerCarId="     << static_cast<u32>( lPlayerCarEntityId )
                            << " contactEntityB="  << lrContact.mEntityIdB.muValue
                            << " isPlayerCar="     << ( lbPlayerCarStruckIt ? 1 : 0 )
                            << " movedBit="        << static_cast<s32>( lpInstance->mu8Flags
                                                                       & KU_MOVED_BIT )
                            << " allowProgression="<< ( lbProgressionAllowed ? 1 : 0 )
                            << " playerCrashing="  << ( mbPlayerCrashing ? 1 : 0 )
                            << " respawnType="     << static_cast<s32>( leDiagRespawnType )
                            << "\n";
                    }
                }
            }

            if ( lbPlayerCarStruckIt
                 && lbPropNotMoved
                 && lbProgressionAllowed
                 && lbPlayerNotCrashing )
            {
                const BrnPhysics::Props::eRespawnType leRespawnType =
                    mZoneManager.GetRespawnType( PropEntityID( lrContact.mEntityIdA.muValue ) );

                // E_RESPAWN (0, the ordinary "it comes back" class) records nothing --
                // asm 0x822FABCC `cmplwi r3,1 ; blt -> loc_822FAD60`.
                if ( leRespawnType != BrnPhysics::Props::E_RESPAWN )
                {
                    bool lbRecord = true;
                    PropEntityID lHitPropEntityId( lrContact.mEntityIdA.muValue );

                    if ( leRespawnType == BrnPhysics::Props::E_DONT_RESPAWN )
                    {
                        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                        {
                            // ⚠️ MEASURED ASYMMETRY, reproduced verbatim: only THIS branch sets
                            // the hex-once print mode (asm 0x822FACB0/B8 `li r11,2 ;
                            // stw r11, 4(r31)` == StrStreamBase::mePrintMode). The
                            // E_RESPAWN_CHANGED branch below has no such store, so its id
                            // prints in decimal. Do not "fix" the inconsistency.
                            *CgsDev::Log::gpDebugPrint
                                << "Hit dont respawn prop: "
                                << CgsDev::E_PRINTMODE_HEXONCE
                                << lHitPropEntityId.GetValue()
                                << " Instance id: "
                                << lpInstance->muInstanceID
                                << "\n";
                        }
                    }
                    else if ( leRespawnType == BrnPhysics::Props::E_RESPAWN_CHANGED )
                    {
                        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "Hit respawn changed prop: "
                                << lHitPropEntityId.GetValue()
                                << " Instance id: "
                                << lpInstance->muInstanceID
                                << "\n";
                        }
                    }
                    else
                    {
                        // leRespawnType >= E_RESPAWN_TYPE_COUNT. asm 0x822FABE0: the assert
                        // then branches STRAIGHT to loc_822FAD60, skipping the record.
                        CGS_ASSERT( false, "Shouldn't get here" );              // :2021 (0x7E5)
                        lbRecord = false;
                    }

                    if ( lbRecord )
                    {
                        // ⭐ ROUND 2: this is the leg the wave-Q spec called "genuinely
                        // unrecovered" and round 1 landed through a reinterpret_cast. Both are
                        // gone -- mRecordHitPropQueue is a real
                        // EventQueue<RecordPropHitEvent,50> and the accessor is typed. The
                        // no-arg AddEvent() reserves the tail slot and returns a reference to
                        // it, which is exactly the console shape (`bl GetRecordHitPropQueue ;
                        // bl AddEvent` at 0x822FAC58/0x822FAC5C, result in r3 -> r31, then six
                        // stores through r31).
                        BrnGameState::GameStateModuleIO::RecordPropHitEvent& lrHitEvent =
                            lpOutput->GetRecordHitPropQueue()->AddEvent();

                        const s16 li16Zone = mZoneManager.GetZone( lHitPropEntityId );

                        // asm 0x822FAD10/14: `lvx128 v0, r30, 0x30 ; stvx128 v0, r0, r31` --
                        // the instance transform's translation row straight into the event's
                        // leading Vector3. The stores are in this order in the console too
                        // (position, zone, index-in-zone, hit-before).
                        lrHitEvent.mPosition = lpInstance->mWorldTransform.wAxis;
                        lrHitEvent.muZoneId  = static_cast<u16>( li16Zone );
                        // The prop's index WITHIN its zone. asm 0x822FAD18..0x822FAD3C does the
                        // `lhzx` from mauStartIndexOfZone by hand (`2*(zone + 0x662A1)`);
                        // routed through the named accessor instead (gotcha 1).
                        lrHitEvent.muPropId  = static_cast<u16>(
                            lHitPropEntityId.GetEntityIndex()
                            - mZoneManager.GetStartIndexOfZone( static_cast<u16>( li16Zone ) ) );
                        lrHitEvent.mbHitBefore = mZoneManager.HasPropBeenHit( lHitPropEntityId );

                        if ( !lrHitEvent.mbHitBefore )
                        {
                            mZoneManager.RecordHitProp( lHitPropEntityId );
                        }
                    }
                }
            }

            // -------------------------------------------------------------------------------
            // LEG 2 -- SMASH. Reached for every qualifying contact (loc_822FAD60), whoever hit
            // the prop.
            // -------------------------------------------------------------------------------
            // asm 0x822FAD60 `rlwinm r11, r11, 0,28,28` == mask 8 == KU_SMASHABLE_BIT.
            if ( ( lpInstance->mu8Flags & KU_SMASHABLE_BIT ) == 0 )
            {
                continue;
            }
            if ( lpInstance->IsSmashed() )
            {
                continue;
            }

            CGS_ASSERT( lpInstance == mZoneManager.GetProp( lPropEntityId ),
                        "lpInstance == mZoneManager.GetProp( lPropEntityId )" );  // :2035 (0x7F3)

            const BrnPhysics::Props::PropTypeData* lpType =
                GetPropPhysicsDataHeader()->GetType( lpInstance->muTypeId );

            // The debug menu can override the whole-prop smash threshold globally
            // (asm 0x822FADD8 `lbzx r11, r26, 0xCD960` -> 0x822FADF4 `lfsx f0, r26, 0xCD96C`).
            f32 lfSmashThreshold = lpType->GetSmashThreshold();
            if ( mbUseOverrides )
            {
                lfSmashThreshold = mfOverrideSmashThreshold;
            }

            // Impact speed along the contact normal, in MPH. asm 0x822FAE18..0x822FAE6C:
            // `vmsum3fp128` (3-lane dot of the striking car's cached velocity with the contact
            // normal), `vandc` against a splat 0x80000000 (absolute value), `vmulfp128` by
            // 2.2369363f, `vcmpgtfp.` against the splat threshold. The car slot is the OTHER
            // entity's index (0x822FAE14 `lwz r10, 4(r25) ; extrwi r10,r10,14,8`), not the
            // player's -- so a traffic-car or rival smash is scored off that car's velocity.
            // ⚠️ NaN (gotcha 4): `vcmpgtfp.` is an ORDERED greater-than -- unordered lanes
            // compare false -- and the branch tests CR6 bit 0 ("all lanes true"). That is
            // exactly C++ `>`. Do NOT rewrite as `!(<=)`.
            const f32 lfImpactSpeedMph =
                fabsf( rw::math::vpu::Dot( GetRaceCarVelocity( lOtherEntityId.GetEntityIndex() ),
                                           lrContact.mNormal ) ) * KF_MPS_TO_MPH;
            const bool lbHitHardEnough = lfImpactSpeedMph > lfSmashThreshold;

            // "Easy smash" types: a prop that has parts AND whose smash threshold is no more
            // than a hair above its move threshold breaks on any contact that would have moved
            // it. ⚠️ MEASURED: this test reads the TYPE's smash threshold (0x822FAE9C
            // `lfs f13, 0(r9)` with r9 == &lpType->mfSmashThreshold, loaded at 0x822FADCC
            // BEFORE the override branch), NOT lfSmashThreshold -- mbUseOverrides does not
            // reach it.
            // ⚠️ NaN (gotcha 4): the console's `fcmpu ; blt` at 0x822FAEA0/A4 takes the branch
            // ONLY when the compare is ORDERED-less-than; unordered falls through to
            // `mr r29, r18` == 0. `<` is therefore exact -- do NOT rewrite as `!(>=)`.
            const bool lbEasySmashType =
                ( lpType->GetNumberOfParts() != 0 )
                && ( lpType->GetSmashThreshold()
                     < lpType->GetMoveThreshold() + KF_EASY_SMASH_THRESHOLD_EPSILON );

            // ---- [DIAG] NOT IN THE X360 BINARY -- the BUG-WAVE LEG2 rung -----------------
            // ⛔ DELETE-WHEN the high-speed prop reaction is confirmed on screen. BRN_PROP_DIAG.
            //
            // The bug report is "props react slow, do nothing fast", and the smash gate is the
            // one place in the POST-physics half where a SPEED decides. This rung names the
            // rejecting clause instead of leaving the caller to guess between four of them:
            //   full=1        -> maRecentlyBrokenProps (32) never drained this frame;
            //   hard=0        -> the NORMAL-COMPONENT impact speed lost to the type threshold
            //                    (note that is |dot(carVel, contactNormal)|, NOT the car speed
            //                    GetDesiredState is fed -- a glancing 130 mph pass reads ~0);
            //                    MEASURED 2026-08-23: this is the ONLY clause that ever fired
            //                    in the bug-wave drive, and it was CORRECT every time -- the
            //                    three rejects were ride-over contacts with a near-vertical
            //                    normal, while head-on contacts in the same run read 74.15 /
            //                    73.39 / 55.44 / 32.77 mph out of this same expression and
            //                    smashed. Do NOT "fix" the dot;
            //   easy=0        -> the type is not an easy-smash type, so `hard` had to carry it;
            //   dup=1         -> already recorded this frame, which is not a failure.
            // Rate-limited first-N and printed only for a REJECT, so a working smash is silent.
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                static s32        siLeg2DiagLinesLeft = 16;

                // Computed INSIDE the latch so the shipped path never pays for the Contains
                // scan; the real gate below still evaluates them in the console's own order.
                if ( sbPropDiag && siLeg2DiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    const bool lbFull = maRecentlyBrokenProps.IsFull();
                    const bool lbDup  = maRecentlyBrokenProps.Contains( lPropEntityId );
                    const bool lbLeg2Breaks =
                        !lbFull && ( lbHitHardEnough || lbEasySmashType ) && !lbDup;

                    if ( !lbLeg2Breaks )
                    {
                        --siLeg2DiagLinesLeft;

                        // ADDED 2026-08-23: BOTH OPERANDS of the impact-speed dot, plus the
                        // car's scalar speed, on the SAME line. `impactMph` is |v.n| and
                        // `carSpeedMph` is |v|; printing only the first is what let a
                        // near-vertical contact normal be read as "the wrong quantity is
                        // being read" instead of "the car is riding over the prop".
                        const s32     liDiagCar =
                            static_cast<s32>( lOtherEntityId.GetEntityIndex() );
                        const Vector3 lDiagCarVelocity = GetRaceCarVelocity( liDiagCar );
                        const Vector3 lDiagNormal      = lrContact.mNormal;

                        *CgsDev::Log::gpDebugPrint
                            << "[prop-diag] LEG2 REJECT prop=" << lPropEntityId.GetValue()
                            << " type="       << lpInstance->muTypeId
                            << " state="      << static_cast<s32>( lpInstance->GetState() )
                            << " impactMph="  << lfImpactSpeedMph
                            << " carSpeedMph="<< GetRaceCarSpeed( liDiagCar )
                            << " car="        << liDiagCar
                            << " n=("         << lDiagNormal.x << "," << lDiagNormal.y
                            << "," << lDiagNormal.z << ")"
                            << " |n|="        << rw::math::vpu::Magnitude( lDiagNormal )
                            << " v=("         << lDiagCarVelocity.x << ","
                            << lDiagCarVelocity.y << "," << lDiagCarVelocity.z << ")"
                            << " smashThr="   << lfSmashThreshold
                            << " typeSmash="  << lpType->GetSmashThreshold()
                            << " typeMove="   << lpType->GetMoveThreshold()
                            << " hard="       << ( lbHitHardEnough ? 1 : 0 )
                            << " easy="       << ( lbEasySmashType ? 1 : 0 )
                            << " full="       << ( lbFull ? 1 : 0 )
                            << " dup="        << ( lbDup ? 1 : 0 )
                            << "\n";
                    }
                }
            }

            // Evaluation ORDER is the console's: IsFull first (0x822FAEB8), then the OR of the
            // two smash predicates (0x822FAECC), then Contains (0x822FAEE4).
            if ( !maRecentlyBrokenProps.IsFull()
                 && ( lbHitHardEnough || lbEasySmashType )
                 && !maRecentlyBrokenProps.Contains( lPropEntityId ) )
            {
                if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "PROPS Adding prop to Recently Broken Props: "
                        << CgsDev::E_PRINTMODE_HEXONCE
                        << lPropEntityId.GetValue()
                        << " Zone: "
                        << static_cast<s32>( lpInstance->mu16ZoneIndex )
                        << "\n";
                }

                maRecentlyBrokenProps.Insert( lPropEntityId );

                // ⚠️ MEASURED, and it contradicts the field's committed NAME AND its header
                // comment (BrnPropEntityModuleIO.h:141-146, "the broken prop's index/id"): the
                // byte the console stores is the STRIKING CAR's entity index, not the prop's
                // (asm 0x822FAF5C `lwz r11, 4(r25) ; srwi r11, r11, 10 ; stb r11, var_170` --
                // r25+4 is mEntityIdB, the car; the `stb` keeps the low 8 bits, which is the
                // low 8 bits of the 14-bit entity index, i.e. `(u8)GetEntityIndex()`).
                // Written faithfully here and REPORTED rather than renamed -- the field is
                // outside this lane's ownership.
                PropEntityIO::BrokenPropEvent lBrokenPropEvent;
                lBrokenPropEvent.muPropIndex =
                    static_cast<u8>( lOtherEntityId.GetEntityIndex() );

                lpOutput->GetBrokenPropQueue()->AddEvent( lBrokenPropEvent );

                // The VFX locator the particle system hangs the smash effect off. The console
                // open-codes the constructor (four `stvx128` transform rows at
                // 0x822FAF80..0x822FAFC8, the type id at +0x40 and the event type 1 at +0x44);
                // de-inlined here into the event's own Construct, which is now a header inline
                // in the tree (BrnPropEntityModuleIO.h:113).
                PropEntityIO::PropVFXLocatorEvent lVfxLocatorEvent;
                lVfxLocatorEvent.Construct( lpInstance->mWorldTransform,
                                            lpInstance->muTypeId,
                                            PropEntityIO::PropVFXLocatorEvent::E_EVENTTYPE_PROPSMASH );

                // AddEventSafe, not AddEvent: this queue holds only 10, and the console's
                // bounds-gated append returns false instead of appending when it is full
                // (`bge -> li r3,0` at 0x822C99C4/0x822C9A28). MEASURED, not a choice.
                // Note both appends take ONE argument (r3 = queue, r4 = &event); there is no
                // size argument in this family, so nothing here can carry a console sizeof.
                lpOutput->GetPropVFXLocatorQueue()->AddEventSafe( lVfxLocatorEvent );
            }
        }
    }

    // ========================================================================================
    // PropEntityModule::ProcessPotentialContactWithPart   @0x822EEDA8   (126 insns)
    //                   (DWARF BrnPropEntityModule.cpp:1351; declared BrnPropEntityModule.h:517)
    //
    // Make ONE prop PART physical because something touched it: claim a physical part slot
    // (evicting the oldest occupant if the pool is full) and hand the part to the physics side.
    // Early-outs, in the console's order: below the world floor, already physical, or the
    // physical-part budget is exhausted.
    //
    // GROUND TRUTH (X360 ARTIST assembly @0x822EEDA8, read instruction by instruction):
    //   0x822EEDBC  addi r28, r26, 0x280      &mZoneManager
    //   0x822EEDC8  bl sub_822CDB90           PropZoneManager::GetPart(PropEntityID)
    //   0x822EEDE8  assert "lpPartInstance != NULL"              BrnPropEntityModule.cpp:1382
    //   0x822EEDFC  clrlwi r11, r25, 22       lEntityId.GetPartIndex() != 0
    //   0x822EEE1C  assert "lEntityId.IsPart()"                  BrnPropEntityModule.cpp:1383
    //   0x822EEE30  addi r11, r29, 0x30 ; lvx128 ; vspltw v13, v0, 1
    //                                         PropPartEntityInstance::mWorldTransform.Pos().y
    //                                         (Matrix44Affine's Pos row is +0x30; lane 1 == Y)
    //   0x822EEE3C  unk_82FAD840              BrnPhysics::Props::KVF_PROP_FLOOR
    //   0x822EEE50  vcmpgefp. v0, v0, v13     KVF_PROP_FLOOR >= y  -> RETURN
    //   0x822EEE64  lbz 0x47(r29)             PropPartEntityInstance::mbPhysical -> RETURN if set
    //   0x822EEE70  lwz 0xB88(module) ; addi 1 ; cmpwi 0x1E ; ble
    //                                         the physical-PART budget gate. module+0xB88 ==
    //                                         (module+0x280)+0x908 ==
    //                                         mZoneManager.mCellManager.miNumPartsInSim -- the
    //                                         SAME member this function later read-modify-writes
    //                                         as `0x908(r28)` at 0x822EEF6C with r28 ==
    //                                         module+0x280, which is what proves the identity.
    //   0x822EEEA8  bl 0x822E0DB0             PropCellManager::GetPhysicalPartSlot, reached with
    //                                         r3 == module+0x280, i.e. through PropZoneManager's
    //                                         committed forwarder
    //   0x822EEEC4  stb 1, 0x47(r29)          mbPhysical = true
    //   0x822EEED8  assert "liPhysicalPartIndex >= 0"            BrnPropEntityModule.cpp:1409
    //   0x822EEEFC  assert "liPhysicalPartIndex < static_cast< int32_t > (
    //                       BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS )"  ...cpp:1410
    //   0x822EEF24  bl sub_822CDB90           GetPart(evicted id)
    //   0x822EEF38  stb 0, 0x47(r11)          evicted->mbPhysical = false
    //   0x822EEF3C  bl 0x822CCE20             PropInputInterface::RemovePartInstance
    //   0x822EEF40  addis/addi -> +0xCDEA8    maRecentlyRecycledParts (Array<PropEntityID,30>)
    //   0x822EEF4C  bl 0x822CA3F8             Array<PropEntityID,30>::IsFull
    //   0x822EEF64  bl 0x822ADDC0             Array<PropEntityID,30>::Append
    //   0x822EEF6C  lwz/addi/stw 0x908(r28)   ++mCellManager.miNumPartsInSim
    //                                         == mZoneManager.IncrementNumberOfPartsInSim(1)
    //   0x822EEF7C  lhz 0x42(r29)             PropPartEntityInstance::muPartId
    //   0x822EEF84  lhz 0x40(r29)             PropPartEntityInstance::muTypeId
    //   0x822EEF8C  stb r30, 0x46(r29)        PropPartEntityInstance::mu8PhysicsIndex
    //   0x822EEF94  bl 0x822CCCA0             PropInputInterface::AddPartInstance (r7 ==
    //                                         lpPartInstance, and mWorldTransform is at offset 0
    //                                         of that object, so the 4th by-value
    //                                         Matrix44Affine argument IS its world transform)
    //
    // ⚠️ NaN POLARITY (gotcha 4), RE-MEASURED THIS PASS: 0x822EEE50 is `vcmpgefp.` -- the
    // ORDERED >= (false for unordered lanes) -- and 0x822EEE54..0x822EEE60
    // (`mfocrf r11,2 ; extrwi r11,r11,1,24 ; bne`) tests CR6 bit 0, "all lanes true". So
    // `KVF_PROP_FLOOR >= lfPositionY` maps to C++ `>=` EXACTLY: both are false when the Y lane
    // is NaN, and the console then falls through into the body. It is NOT rewritten as
    // `!(a < b)`; that would invert the NaN case. (BrnPropConstants.h used to claim this
    // consumer spelled `vcmpgtfp.`/`>`; that comment defect is now FIXED -- :31-61 there
    // carries the per-consumer list, with this site listed as the `vcmpgefp.` >= one.)
    // ========================================================================================
    void PropEntityModule::ProcessPotentialContactWithPart( PropEntityID lPropEntityId,
                                                            BrnPhysics::Props::PropInputInterface* lpPropInput )
    {
        PropPartEntityInstance* lpPartInstance = mZoneManager.GetPart( lPropEntityId );

        // Both tripwires are NON-GATING, exactly as shipped (the console branches only around
        // the assert bodies and carries on either way).
        CGS_ASSERT( lpPartInstance != nullptr, "lpPartInstance != NULL" );          // ...cpp:1382
        CGS_ASSERT( lPropEntityId.GetPartIndex() != 0, "lEntityId.IsPart()" );      // ...cpp:1383

        // ⭐ ROUND 2: KVF_PROP_FLOOR is now homed at BrnPropConstants.h:56; the parked body's
        // file-local copy (and its "no header home exists" claim) are gone.
        const f32 lfPositionY = lpPartInstance->mWorldTransform.Pos().y;
        if ( BrnPhysics::Props::KVF_PROP_FLOOR >= lfPositionY )
        {
            return;
        }

        if ( lpPartInstance->mbPhysical )
        {
            return;
        }

        // ⭐ ROUND 2: the miNumPartsInSim reader this body was parked on.
        if ( !mZoneManager.IsInPhysicalBudgetForParts( 1 ) )
        {
            return;
        }

        s32          liPhysicalPartIndex = 0;
        PropEntityID lEvictedPartId;
        bool         lbEvicted = false;

        if ( !mZoneManager.GetPhysicalPartSlot( lPropEntityId, liPhysicalPartIndex,
                                                lEvictedPartId, lbEvicted ) )
        {
            return;
        }

        lpPartInstance->mbPhysical = true;

        CGS_ASSERT( liPhysicalPartIndex >= 0, "liPhysicalPartIndex >= 0" );          // ...cpp:1409
        CGS_ASSERT( liPhysicalPartIndex < static_cast<s32>( BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS ),
                    "liPhysicalPartIndex < static_cast< int32_t > ( BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS )" );
                                                                                     // ...cpp:1410
        if ( lbEvicted )
        {
            // The slot was taken from another part: retire that one first. Its slot index is
            // the one we are about to occupy, which is why RemovePartInstance is passed
            // liPhysicalPartIndex and not a second lookup.
            mZoneManager.GetPart( lEvictedPartId )->mbPhysical = false;
            lpPropInput->RemovePartInstance( lEvictedPartId, liPhysicalPartIndex );

            // Remember it so the streaming side can put it back. IsFull is a gate here, not a
            // tripwire (0x822EEF58 `bne` skips the Append).
            if ( !maRecentlyRecycledParts.IsFull() )
            {
                maRecentlyRecycledParts.Append( lEvictedPartId );
            }
        }
        else
        {
            // A genuinely new occupant, so the sim population grows.
            mZoneManager.IncrementNumberOfPartsInSim( 1 );
        }

        lpPartInstance->mu8PhysicsIndex = static_cast<u8>( liPhysicalPartIndex );

        lpPropInput->AddPartInstance( lPropEntityId,
                                      lpPartInstance->muTypeId,
                                      lpPartInstance->muPartId,
                                      lpPartInstance->mWorldTransform,
                                      liPhysicalPartIndex );
    }

    // ========================================================================================
    // PropEntityModule::PostSceneUpdate   @0x822C4718   (61 insns)
    //
    // A lock/route shell over the crash module's "this race car has finished crashing" queue:
    // while the player is wrecked in an OFFLINE session, a crash-complete event for the
    // PLAYER's own car pushes the prop streaming machine into E_RESET_UNLOADING (the whole
    // prop world is dropped and re-streamed around the reset point).
    //
    // PARAMETER REGISTERS (from the prologue, not from Hex-Rays, which renders this as five
    // `int`s): r3 this, r4 lpInputBufferStack, r5 lpOutputBufferStack, r6 lpInput (-> r24),
    // r7 lpOutput (-> r23), r8 lUpdateSet. r4, r5 and r8 are NEVER READ by the body -- they
    // are declared because the caller (WorldModule::EntityModulePostSceneUpdate @0x827C3C58,
    // mirrored at BrnWorldModule.cpp:1881) passes them and the positions after them depend on
    // them.
    //
    // CONSOLE OFFSET -> MEMBER DECODE (provenance only; nothing below indexes by offset):
    //   module +0xD3340 mbCurrentlyOnline     module +0xD3344 mbPlayerWrecked
    //   module +0xD3210 mu8PlayerIndex        module +0xD3200 meStreamingMode
    //   lpInput +0x08   mRaceCarCrashCompleteEventQueue  (-> GetCrashEventQueue())
    //
    // ⚠️ LINK: WorldLinkStubs.cpp:1163 still carries the inert boot gate for this exact
    // signature (re-confirmed 2026-08-18; block 1158-1172). Reported as a link duplicate; NOT
    // retired by this lane (project rule: the conductor mounts this file and retires the gate
    // together, then re-links). See the LINK-LEVEL block in this file's banner for the exact
    // line range to delete and the marker to leave behind.
    // ========================================================================================
    void
    PropEntityModule::PostSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                       CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                       PropEntityIO::InputBuffer_PostScene* lpInput,
                                       PropEntityIO::OutputBuffer_PostScene* lpOutput,
                                       BrnUpdateSet /*lUpdateSet*/ )
    {
        lpInput->LockForRead();      // 0x822C4734
        lpOutput->LockForWrite();    // 0x822C473C

        // 0x822C4740..0x822C4764. Two independent early-outs to the unlock tail:
        //   `bne loc_822C47F4` on mbCurrentlyOnline, then `beq loc_822C47F4` on
        //   mbPlayerWrecked. Online sessions never reset the prop world this way.
        if ( !mbCurrentlyOnline && mbPlayerWrecked )
        {
            const PropEntityIO::InputBuffer_PostScene::RaceCarCrashCompleteEventQueue*
                lpCrashCompleteQueue = lpInput->GetCrashEventQueue();

            // 0x822C4774 `lwz r25, 8(r28)` -- the length is read ONCE, before the loop (unlike
            // ProcessContacts, which re-reads it every iteration).
            const s32 liNumEvents = lpCrashCompleteQueue->GetLength();

            // 0x822C4778..0x822C47AC. The player's own race-car scene entity word, built ONCE
            // outside the loop: owner E_ENTITYTYPE_RACECAR (1), entity index mu8PlayerIndex,
            // part index 0 -- `slwi r11,r31,10 ; oris r30,r11,0x100`. EntityId::Set is inlined
            // here with two CONSTANT arguments, which is why only its entity-index tripwire
            // survives in the shipped code ("luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)"
            // @0x822C477C, `cmplwi r31, 0x4000`). Calling Set reproduces it exactly.
            CgsSceneManager::EntityId lPlayerEntityId;
            lPlayerEntityId.Set( E_ENTITYTYPE_RACECAR, mu8PlayerIndex, 0 );

            // 0x822C47B0 `cmpwi r25,0 ; ble` -- a zero-length queue skips the loop.
            for ( s32 liEvent = 0; liEvent < liNumEvents; ++liEvent )   // 0x822C47C4..0x822C47F0
            {
                const CrashIO::RaceCarCrashCompleteEvent& lrEvent =
                    lpCrashCompleteQueue->GetEvent( liEvent );          // 0x822C47CC

                // 0x822C47D0..D8 `ld r11,0(r3) ; srdi r11,r11,32 ; clrlwi r11,r11,0`.
                // CgsSceneManager::VolumeInstanceId is a PACKED 64-bit word whose embedded
                // 32-bit scene EntityId lives in the HIGH dword (bits 32..63) -- see
                // CgsVolumeInstanceId.h. This is the PACKING, not a console pointer width, so
                // the shift is correct on the host too (gotcha 1 does not bite here).
                const u32 luEventEntityWord =
                    static_cast<u32>( lrEvent.mRaceCarVolumeInstanceId.muId >> 32 );

                // 0x822C47DC..E4. NOTE: the comparison is over the WHOLE entity word, part
                // index included -- the console does not mask, and the built id has part index
                // 0, so only the car body itself matches.
                if ( luEventEntityWord == static_cast<u32>( lPlayerEntityId ) )
                {
                    meStreamingMode = E_RESET_UNLOADING;   // 0x822C47E4  `stwx r26(==2), ...`
                }
            }
        }

        lpInput->UnlockForRead();      // 0x822C47F8
        lpOutput->UnlockForWrite();    // 0x822C4800
    }
}

// ============================================================================
// FOLDED FROM PropEntityModule_wQ3_01.cpp (wave Q3) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// LANDED 2026-08-18 (wave Q round 3, conductor): the two round-2 PARKED bodies
// scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdate{Props,Parts}InScene.cpp merged
// verbatim into this partfile. The ONE blocker both named -- InSceneUpdateInterface::
// SetEntityPosition(EntityId, Vector3), the DWARF-spelled overload -- now exists in
// GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h/.cpp. The two park
// banners below are kept as the derivation record; their "THE EXACT LINE THAT UNBLOCKS"
// sections are historical.
// ============================================================================
// ============================================================================
// PARKED (round 2) -- scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePropsInScene.cpp
//
//   BrnWorld::PropEntityModule::ReplayUpdatePropsInScene @0x822DB370  (355 insns, counted:
//   0x822DB370..0x822DB8F8 inclusive, /4, +1)
//   ledger TUs: GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//               class:BrnWorld::PropEntityModule
//   Sibling: PropEntityModule_01_ReplayUpdatePartsInScene.cpp @0x822DB900
//   Supersedes scratchpad/waveQ/parked/PropEntityModule_03_ReplayUpdatePropsInScene.cpp
//   (whose banner is now STALE -- see "WHAT CHANGED" below).
//
// ============================================================================
// THE EXACT LINE THAT UNBLOCKS THIS FILE  (ONE line; everything else is landed)
// ============================================================================
// File: b5-decomp/src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h
// In `struct InSceneUpdateInterface`, beside the committed
//   `void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);`  (:194)
// add:
//
//     void SetEntityPosition( CgsSceneManager::EntityId lEntityId, Vector3 lPosition );
//
// GROUND (round-2 re-derivation; the round-1 request was asm-only, this is DWARF-backed):
//  * DWARF, and it is DECISIVE -- the DecFIGS dump declares exactly ONE SetEntityPosition
//    on this struct and it is this one:
//      references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/
//      CgsSceneManagerIO_SceneUpdate.h:446  ->  `void SetEntityPosition(EntityId, Vector3);`
//      (the DWARF's own recorded source line is CgsSceneManagerIO_SceneUpdate.h:346).
//    It sits between `void RemoveEntity(EntityId, bool);` (:340) and
//    `void SetEntityRadius(EntityId, float32_t);` (:352), both of which this tree already
//    carries in the EntityId form. The committed `(u32, const Matrix44Affine&)` spelling is
//    a MODEL the tree fitted to its matrix-holding callers, not a second overload the
//    original had; its own banner in CgsSceneManagerIO_SceneUpdate.cpp:41-43 says as much
//    ("the X360 receives the position pre-extracted in the v1 vmx arg; the committed
//    (u32, const Matrix44Affine&) signature the consumers call extracts it here via Pos()").
//  * X360: the producer @0x822B1398 takes the position in v1 and stages
//    InEventSetEntityPosition{ mPosition = v1, mEntityId = r4 } -- no instruction in it
//    reads a matrix. Both call sites here hand it a BARE recorded position straight out of
//    the frame (0x822DB894 `lvx128 v0,[element]` -> 0x822DB8B0 `lvx128 v1`), so there is no
//    transform in scope to give the committed form.
//  * ADDITIVE: no committed call site changes meaning (EntityId has an `operator u32`, so
//    the existing `(u32, const Matrix44Affine&)` calls still bind to the matrix overload;
//    the new one differs in the second parameter, which no existing caller supplies).
//
// COMPILE-PROVEN, not asserted. `scratchpad/waveQ2/probe_wq2lander/probe_replayupdate.cpp`
// is this body plus its sibling verbatim -> STATUS=fail with EXACTLY TWO diagnostics, one
// per body, both C2664 on that one call. `probe_replayupdate_stub.cpp` is the same text
// with a probe-local `ProbeSetEntityPosition(InSceneUpdateInterface*, EntityId, Vector3)`
// substituted for those two calls and nothing else changed -> STATUS=pass. So this ONE
// declaration is the whole remaining blocker.
//
// ============================================================================
// WHAT CHANGED SINCE THE ROUND-1 PARK (re-derived per the round-2 brief -- the old banner
// was wrong or stale in FOUR places)
// ============================================================================
//  1. THE FRAME BLOCKER IS GONE. BrnReplays::PropSerialiserFrame now carries nine real
//     BrnReplayArray members (BrnReplayPropSerialiserFrame.h:195-222) with all nineteen
//     offsets pinned by static_assert. No pad, no `bool mbAddedFlagNNNN`.
//  2. `maPropTypeIds` IS NOT THE NAME -- it is `maTypes`, ATTESTED VERBATIM by the streamed
//     assert literal " maTypes.GetLength(): " that PropSerialiserFrame::Read/Write/
//     KeyFrameRead build (BrnReplayPropEntitySerialiser.h:420/:421). The round-1 banner
//     guessed it.
//  3. THE ROUND-1 BANNER ASKED FOR FIVE ARRAYS; THERE ARE NINE. It missed both ORIENTATION
//     arrays (frame +0x1600 and +0x3000), whose muLength bytes are the `mbAddedFlag25E0` /
//     `mbAddedFlag3800` it left sitting in the pad ladder -- so its recipe could not have
//     produced the right sizeof. (Its own list of "the other three flags" does name those
//     two offsets as further 16-byte-lane arrays, so the evidence was there; the REQUEST
//     was short.) Not a defect in this body: it reads neither.
//  4. ⚠️ `Vector3( 0.0f, 0.0f, 0.0f )` IN THE ROUND-1 PARKED TEXT DOES NOT COMPILE, and no
//     round-1 verifier caught it because the parked file was never put through cl. This
//     tree's `Vector3` (b5-decomp/vendor/renderware/include/rw/math/vpu/types.h:24) is the
//     aggregate `struct alignas(16) Vector3 { float x, y, z, w; void SetZero(); }` -- it has
//     no three-float constructor. Fixed below to a `SetZero()`d local, which is also the
//     exact console value: `vspltisw128 v126, 0` zeroes ALL FOUR lanes (0x822DB560), and
//     SetZero() zeroes all four. A three-float ctor would have left w undefined.
//
// ============================================================================
// EVERYTHING BELOW IS DECODED FROM THE RAW ASM
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822DB370.json (`assembly`), instruction by
// instruction. Hex-Rays renders this function as an int blob and is not used.
// ============================================================================
//
// ASM SHAPE, in order (prologue r25 = this, r17 = lpOutput -- TWO parameters; 0x822DB390/94
// move exactly r3 and r4 and nothing else):
//
//  1  0x822DB3A8  GetStaticLayout(); `lbz r11, 0x5010(r3)` == the live frame's prop
//                 POSITION-array muLength (static 0x5010 == 0x3A20 mLiveFrame + 0x15F0).
//     0x822DB3C0  `subf r30, r10, r11` == frameCount - muReplayPropsInScene.
//     0x822DB3CC  if negative it becomes the REMOVE count (`neg r26`) and the ADD count
//                 clamps to 0; otherwise REMOVE is 0.
//     0x822DB3F8  assert "liNumPropsToAddToScene == 0 || liNumPropsToRemoveFromScene == 0"
//                 (module .cpp line 0x914 == 2324). Unreachable-true by construction -- one
//                 of the two is always 0 -- but it is in the shipped source.
//  2  0x822DB414  assert `inScene - remove + add == frameCount` (line 0x919 == 2329), whose
//                 failure path streams "Num already in scene: " (aNumAlreadyInSc, NO leading
//                 space -- verified against the parts sibling's distinct aNumAlreadyInSc_0
//                 which DOES have one) / " Num to add: " / " NumToRemove: " / " Num wanted: ".
//                 Per project convention the streamed message collapses to one CGS_ASSERT on
//                 the leading literal.
//  3  0x822DB558  ADD loop: for each new prop, build the scene id from the CURRENT
//                 muReplayPropsInScene + i (re-read from the member every iteration,
//                 0x822DB574 `lwz r11,0(r14)`) and AddEntity it with a ZERO centre
//                 (0x822DB560 `vspltisw128 v126,0`) and a ZERO radius (flt_82001CC0; its raw
//                 image bytes were dumped == 0x00000000 == 0.0f by the ROUND-1 G3 VERIFIER
//                 in headless IDA -- scratchpad/waveQ/probe_PropEntityModule_3/ -- not by
//                 me this round). The flag word is `li r28, 0x490` ==
//                 PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG, the same one
//                 PropCellManager::AddPropToScene pushes.
//  4  0x822DB5E0  REMOVE loop: peel from the TOP of the range,
//                 `muReplayPropsInScene - i - 1` (0x822DB5EC..0x822DB5F4), RemoveEntity(id, 0).
//  5  0x822DB644  latch muReplayPropsInScene = frameCount (`stw r11, 0(r14)`), then for every
//                 prop in the frame push its recorded position and its type's bounding
//                 radius. Order inside the loop, MEASURED: type id (0x822DB744, frame
//                 +0x25F0) -> ResourcePtr null check + GetType's two asserts + the type
//                 lookup (0x822DB758..0x822DB810) -> position (0x822DB874, frame +0x610) ->
//                 SetEntityPosition -> SetEntityRadius. The loop bound is re-read from the
//                 member every iteration (0x822DB8D0).
//
// ⚠️ FAITHFUL RE-EVALUATION: the console calls GetStaticLayout() again at every use rather
// than caching it, and GetStaticLayout() carries the "Static buffer size is too small"
// assert (its `lwz 0x24(this)` vs 0x7480 test is inlined at each in-loop site, 0x822DB6E4
// and 0x822DB814). The repeated calls below are deliberate -- caching would drop firings.
//
// ⚠️ The scene interface is likewise re-fetched per call (`bl sub_822B9738` ==
// OutputBuffer_PreScene::GetSceneInputInterface, the WRITE-lock handle whose "Not locked for
// writing" tripwire is part of the path).
//
// ---------------------------------------------------------------------------
// RESIDUAL, NOT A BLOCKER: the scene owner byte 0x22 (34) has no enumerator
// ---------------------------------------------------------------------------
// GameSource/World/BrnEntityTypes.h names E_ENTITYTYPE_FIRST_REPLAY_TYPE = 32 and
// E_ENTITYTYPE_REPLAY_RACECAR = 33, and 34 collides with E_ENTITYTYPE_COUNT = 34. The
// literal stands with its citation, in the SAME spelling round-1 group 1 already committed
// at PropEntityModule_wQ_01.cpp:90-91. The part-field values 0 (prop) / 1 (part) are the
// Feb-2007 file-local `{ E_PROP = 0, E_PART = 1 }`.
//
// ---------------------------------------------------------------------------
// CONSOLE-OFFSET DISCIPLINE (gotcha 1)
// ---------------------------------------------------------------------------
// No console offset, stride or size appears in the code. The decode used to READ the asm:
//     this + 0xD3180  mPropEntitySerialiser    (r18)
//     this + 0xD3338  muReplayPropsInScene     (r14; re-loaded every loop iteration)
//     this + 0xCDD88  mpPropPhysicsDataHeader  (r15 = this + 0xD0000 - 0x2278)
//     PropTypeData + 0x44  mfSphereRadius      -> GetBoundingRadius()  (host +0x50)
//     serialiser + 0x20 / +0x24  static buffer / its size (GetStaticLayout's own assert)
//
// ---------------------------------------------------------------------------
// gotchas 3 and 4, checked
// ---------------------------------------------------------------------------
// gotcha 3 (a float rides an FPR and SKIPS its GPR slot): not applicable -- this function
// takes no float parameter, and the two floats it PASSES (AddEntity's 0.0f radius in f1,
// SetEntityRadius's f1) are the last argument of an already-committed signature.
// gotcha 4 (PPC NaN polarity): not applicable -- there is no floating-point compare in the
// function. Every branch is an integer `cmpwi`/`cmplwi`/`cmpw`. The only float values are
// loaded and passed, never tested.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET ------------------------------------------
// ---------------------------------------------------------------------------------

namespace BrnWorld
{
    namespace
    {
// (fold: an identical definition of KU_REPLAY_PROP_SCENE_OWNER was dropped here -- this TU defines it once, above)

        // The scene entity id's part field: 0 selects the whole prop, 1 selects a shed
        // part (the Feb-2007 file-local `{ E_PROP = 0, E_PART = 1 }`). This body only ever
        // builds prop ids; the part sibling uses 1.
        const u32 KU_REPLAY_SCENE_PART_FIELD_PROP = 0u;
        // The part sibling builds PART ids: 1 (0x822DBAD4 `ori r15, r11, 1` on 0x22000000
        // -> 0x22000001). Feb-2007 file-local { E_PROP = 0, E_PART = 1 }.
        const u32 KU_REPLAY_SCENE_PART_FIELD_PART = 1u;
    }

    // ========================================================================
    // PropEntityModule::ReplayUpdatePropsInScene @0x822DB370
    //
    // Reconcile the scene's replay-prop population against the playback frame the
    // serialiser just Read. Called only from ReplayPreSceneUpdate @0x822EF878, and only
    // while the serialiser IsPlaying().
    // ========================================================================
    void PropEntityModule::ReplayUpdatePropsInScene( PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        // ---- 1. how many props does the playback frame want in the scene? -----------
        s32 liNumPropsToAddToScene =
            static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPropPositions.muLength ) -
            static_cast<s32>( muReplayPropsInScene );
        s32 liNumPropsToRemoveFromScene = 0;

        if ( liNumPropsToAddToScene < 0 )
        {
            liNumPropsToRemoveFromScene = -liNumPropsToAddToScene;
            liNumPropsToAddToScene      = 0;
        }

        CGS_ASSERT( liNumPropsToAddToScene == 0 || liNumPropsToRemoveFromScene == 0,
                    "liNumPropsToAddToScene == 0 || liNumPropsToRemoveFromScene == 0" );

        // ---- 2. the two must reconcile exactly ---------------------------------------
        CGS_ASSERT( static_cast<s32>( muReplayPropsInScene )
                        - liNumPropsToRemoveFromScene
                        + liNumPropsToAddToScene
                    == static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()
                                             ->mLiveFrame.maPropPositions.muLength ),
                    "Num already in scene: " );

        // ---- 3. grow the scene population -------------------------------------------
        for ( s32 liEntity = 0; liEntity < liNumPropsToAddToScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPropsInScene + static_cast<u32>( liEntity ),
                           KU_REPLAY_SCENE_PART_FIELD_PROP );

            // 0x822DB560 `vspltisw128 v126, 0` -- ALL FOUR lanes zero. SetZero() is the
            // tree's Vector3 spelling for that; the type has no 3-float constructor.
            Vector3 lCentre;
            lCentre.SetZero();

            lpOutput->GetSceneInputInterface()->AddEntity(
                lEntityId,
                PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG,   // li r28, 0x490
                lCentre,
                0.0f );                                            // flt_82001CC0 == 0.0f
        }

        // ---- 4. shrink it, peeling from the top of the range -------------------------
        for ( s32 liEntity = 0; liEntity < liNumPropsToRemoveFromScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPropsInScene - static_cast<u32>( liEntity ) - 1u,
                           KU_REPLAY_SCENE_PART_FIELD_PROP );

            lpOutput->GetSceneInputInterface()->RemoveEntity( lEntityId, 0 );
        }

        // ---- 5. push every recorded prop's position and radius ------------------------
        muReplayPropsInScene = mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPropPositions.muLength;

        for ( u32 luProp = 0; luProp < muReplayPropsInScene; ++luProp )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER, luProp, KU_REPLAY_SCENE_PART_FIELD_PROP );

            // The frame indexes its arrays with a u8 (BrnReplayArray<T,N>::operator[](u8)
            // -- `clrlwi r28, r26, 24` at 0x822DB748).
            const u16 lu16TypeId =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maTypes[ static_cast<u8>( luProp ) ];

            const BrnPhysics::Props::PropTypeData* lpPropTypeData =
                mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lu16TypeId );

            const Vector3& lrPosition =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPropPositions[ static_cast<u8>( luProp ) ];

            lpOutput->GetSceneInputInterface()->SetEntityPosition( lEntityId, lrPosition );
            // PropTypeData console +0x44 == mfSphereRadius (host +0x50) -- by accessor.
            lpOutput->GetSceneInputInterface()->SetEntityRadius( lEntityId,
                                                                 lpPropTypeData->GetBoundingRadius() );
        }
    }


// ---- (from the parts park file's banner) ----------------------------------------
// ============================================================================
// PARKED (round 2) -- scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePartsInScene.cpp
//
//   BrnWorld::PropEntityModule::ReplayUpdatePartsInScene @0x822DB900  (393 insns, counted:
//   0x822DB900..0x822DBF20 inclusive, /4, +1)
//   ledger TUs: GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//               class:BrnWorld::PropEntityModule
//   Sibling: PropEntityModule_01_ReplayUpdatePropsInScene.cpp @0x822DB370
//   Supersedes scratchpad/waveQ/parked/PropEntityModule_03_ReplayUpdatePartsInScene.cpp.
//
// ============================================================================
// THE EXACT LINE THAT UNBLOCKS THIS FILE  (the SAME single line as the sibling)
// ============================================================================
// File: b5-decomp/src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h
// In `struct InSceneUpdateInterface`, beside the committed
//   `void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);`  (:194)
// add:
//
//     void SetEntityPosition( CgsSceneManager::EntityId lEntityId, Vector3 lPosition );
//
// The full grounding (DWARF CgsSceneManagerIO_SceneUpdate.h:446 declaring exactly this one
// form, source line :346; the v1-only X360 producer @0x822B1398; why it is additive; and the
// two-probe compile proof) is in the SIBLING file's banner -- ONE declaration unblocks both.
// This body's own two call sites are 0x822DBE98..0x822DBED8.
//
// ============================================================================
// WHAT CHANGED SINCE THE ROUND-1 PARK (re-derived per the round-2 brief)
// ============================================================================
//  1. THE FRAME BLOCKER IS GONE -- the nine real BrnReplayArray members landed at
//     BrnReplayPropSerialiserFrame.h:195-222.
//  2. RENAMES APPLIED from the owner spec's paste table (scratchpad/waveQ2/replays.owner.md
//     section 6): `maPartTypeIds` -> `maPartTypes`, `maPartIndices` -> `maPartIds`.
//     Neither of those two names is attested; they are the owner's descriptive spelling and
//     are what the header carries. (Only maPropPositions / maPropOrientations / maTypes are
//     assert-attested.)
//  3. ⚠️ MUST_FIX FROM ROUND 1, APPLIED: the streamed leading fragment of the second assert
//     is " Num already in scene: " WITH A LEADING SPACE for the PARTS function -- a
//     genuinely different rodata literal from the props function's "Num already in scene: ".
//     RE-MEASURED here, not taken on trust: this body loads `aNumAlreadyInSc_0` at
//     0x822DBA10/0x822DBA18 (the export's own inline comment renders it as
//     " Num already in scene: "), where the props sibling loads `aNumAlreadyInSc` at
//     0x822DB47C/0x822DB484 ("Num already in scene: "). IDA's `_0` suffix here is a name
//     collision between two DISTINCT literals, not an alias. Same for the third fragment:
//     this body uses `aNumToRemove` == " Num to remove: " (0x822DBA6C), the props sibling
//     uses `aNumtoremove` == " NumToRemove: " (0x822DB4D8).
//  4. ⚠️ `Vector3( 0.0f, 0.0f, 0.0f )` IN THE ROUND-1 PARKED TEXT DOES NOT COMPILE -- this
//     tree's Vector3 (b5-decomp/vendor/renderware/include/rw/math/vpu/types.h:24) is the
//     aggregate `{ float x, y, z, w; void SetZero(); }` with no three-float constructor.
//     Fixed to a SetZero()d local, which is also the exact console value (`vspltisw128
//     v126, 0` at 0x822DBAFC zeroes all four lanes; a 3-float ctor would leave w undefined).
//
// ============================================================================
// DECODED FROM THE RAW ASM .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822DB900.json
// ============================================================================
//
// Structurally identical to ReplayUpdatePropsInScene, with FOUR differences, all measured:
//   * the frame count is the PART position array's muLength (`lbz 0x6A10` at 0x822DB940 /
//     0x822DB9B4 / 0x822DBBE8, static 0x6A10 == 0x3A20 mLiveFrame + 0x2FF0) vs `0x5010`;
//   * the scene id carries part field 1, not 0 (0x822DBAD4 `ori r15, r11, 1` on the
//     0x22000000 word) -- the export's own comment renders the result as 0x22000001;
//   * the assert literals are the "Parts" spellings, at module .cpp lines 0x95C == 2396
//     (0x822DB994) and 0x961 == 2401 (0x822DBAB8), vs 2324 / 2329, plus the two rodata
//     differences in note 3 above;
//   * the radius comes from the PART descriptor, reached through the frame's separate
//     part-INDEX array -- the prop version reads the prop type's own radius.
//
// Prologue: r25 = this, r22 = lpOutput. TWO parameters -- 0x822DB920/24 move exactly r3 and
// r4 and nothing else.
//
// Order inside loop 5, MEASURED (0x822DBCE0..0x822DBEF0): part TYPE id (frame +0x3810,
// static 0x7230) -> ResourcePtr null check + GetType's two asserts + the type lookup ->
// part INDEX (frame +0x3912, static 0x7332) -> the part descriptor -> position (frame
// +0x27F0, static 0x6210) -> SetEntityPosition -> SetEntityRadius.
//
// ⚠️ FAITHFUL RE-EVALUATION: like the sibling, the console re-calls GetStaticLayout() at
// every use (its "Static buffer size is too small" assert is inlined at each site,
// 0x822DBDB0 / 0x822DBE20) and re-fetches the write-locked scene interface per call
// (`bl sub_822B9738` == OutputBuffer_PreScene::GetSceneInputInterface). Both are reproduced
// rather than cached, so no assert firing is lost. The loop bound is likewise re-read from
// the member every iteration (0x822DBEF4..0x822DBF00).
//
// ---------------------------------------------------------------------------
// CONSOLE-OFFSET AND STRIDE DISCIPLINE (gotcha 1) -- READ THIS BEFORE EDITING
// ---------------------------------------------------------------------------
// ⚠️ THE PART-DESCRIPTOR STRIDE IS A CONSOLE VALUE. At 0x822DBE30..0x822DBE3C the asm
// resolves the part descriptor as `maParts + 48 * partIndex`
// (`rotlwi r9,r11,1 ; add r11,r11,r9 ; slwi r11,r11,4` == index*3*16 == index*48).
// 48 is the CONSOLE sizeof(BrnPhysics::Props::PropPartTypeData); on this x64 host it is 64
// (pinned by that class's own _AssertLayout). The body below subscripts
// `lpPropTypeData->GetParts()[ lu16PartIndex ]` and NEVER computes an offset.
//
// Everything else, provenance only -- no console offset appears in the code:
//     this + 0xD3180  mPropEntitySerialiser    (r23, 0x822DB928/2C)
//     this + 0xD333C  muReplayPartsInScene     (r17, 0x822DB93C/48; re-loaded every loop
//                                               iteration -- 0x822DBB10, 0x822DBEFC)
//     this + 0xCDD88  mpPropPhysicsDataHeader
//     PropTypeData     + 0x40  maParts         -> GetParts()          (host +0x48)
//     PropPartTypeData + 0x28  mfSphereRadius  -> GetBoundingRadius() (host +0x30)
//
// ---------------------------------------------------------------------------
// gotchas 3 and 4, checked
// ---------------------------------------------------------------------------
// Neither applies: the function takes no float parameter, and it contains no
// floating-point compare -- every branch is an integer cmpwi/cmplwi/cmpw. Its two floats
// (AddEntity's 0.0f radius, SetEntityRadius's f1) are passed, never tested.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET ------------------------------------------

    // ========================================================================
    // PropEntityModule::ReplayUpdatePartsInScene @0x822DB900
    // The part half of the replay scene reconciliation.
    // ========================================================================
    void PropEntityModule::ReplayUpdatePartsInScene( PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        // ---- 1. how many parts does the playback frame want in the scene? -----------
        s32 liNumPartsToAddToScene =
            static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPartPositions.muLength ) -
            static_cast<s32>( muReplayPartsInScene );
        s32 liNumPartsToRemoveFromScene = 0;

        if ( liNumPartsToAddToScene < 0 )
        {
            liNumPartsToRemoveFromScene = -liNumPartsToAddToScene;
            liNumPartsToAddToScene      = 0;
        }

        CGS_ASSERT( liNumPartsToAddToScene == 0 || liNumPartsToRemoveFromScene == 0,
                    "liNumPartsToAddToScene == 0 || liNumPartsToRemoveFromScene == 0" );

        // ---- 2. the two must reconcile exactly ---------------------------------------
        // ⚠️ LEADING SPACE IS DELIBERATE -- aNumAlreadyInSc_0 @0x822DBA18 is a DIFFERENT
        // literal from the props sibling's aNumAlreadyInSc. See banner note 3.
        CGS_ASSERT( static_cast<s32>( muReplayPartsInScene )
                        - liNumPartsToRemoveFromScene
                        + liNumPartsToAddToScene
                    == static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()
                                             ->mLiveFrame.maPartPositions.muLength ),
                    " Num already in scene: " );

        // ---- 3. grow the scene population -------------------------------------------
        for ( s32 liEntity = 0; liEntity < liNumPartsToAddToScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPartsInScene + static_cast<u32>( liEntity ),
                           KU_REPLAY_SCENE_PART_FIELD_PART );

            // 0x822DBAFC `vspltisw128 v126, 0` -- ALL FOUR lanes zero.
            Vector3 lCentre;
            lCentre.SetZero();

            lpOutput->GetSceneInputInterface()->AddEntity(
                lEntityId,
                PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG,   // li r27, 0x490
                lCentre,
                // 0x822DBB04 `lfs f31, flt_82001CC0`. That constant's raw image bytes were
                // dumped == 0x00000000 == 0.0f by the ROUND-1 G3 VERIFIER in headless IDA
                // (scratchpad/waveQ/probe_PropEntityModule_3/), not re-dumped this round.
                0.0f );
        }

        // ---- 4. shrink it, peeling from the top of the range -------------------------
        for ( s32 liEntity = 0; liEntity < liNumPartsToRemoveFromScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPartsInScene - static_cast<u32>( liEntity ) - 1u,
                           KU_REPLAY_SCENE_PART_FIELD_PART );

            lpOutput->GetSceneInputInterface()->RemoveEntity( lEntityId, 0 );
        }

        // ---- 5. push every recorded part's position and radius ------------------------
        muReplayPartsInScene = mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPartPositions.muLength;

        for ( u32 luPart = 0; luPart < muReplayPartsInScene; ++luPart )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER, luPart, KU_REPLAY_SCENE_PART_FIELD_PART );

            // BrnReplayArray<T,N>::operator[] takes a u8 (`clrlwi r27, r22, 24` @0x822DBCE4).
            const u16 lu16TypeId =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPartTypes[ static_cast<u8>( luPart ) ];

            const BrnPhysics::Props::PropTypeData* lpPropTypeData =
                mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lu16TypeId );

            const u16 lu16PartIndex =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPartIds[ static_cast<u8>( luPart ) ];

            // ⚠️ CONSOLE STRIDE TRAP: the asm walks `maParts + 48*index`; 48 is the CONSOLE
            // PropPartTypeData size (host 64). Subscript, never base + 48*i.
            const BrnPhysics::Props::PropPartTypeData& lrPartTypeData =
                lpPropTypeData->GetParts()[ lu16PartIndex ];

            const Vector3& lrPosition =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPartPositions[ static_cast<u8>( luPart ) ];

            lpOutput->GetSceneInputInterface()->SetEntityPosition( lEntityId, lrPosition );
            // PropPartTypeData console +0x28 == mfSphereRadius (host +0x30) -- by accessor.
            lpOutput->GetSceneInputInterface()->SetEntityRadius( lEntityId,
                                                                 lrPartTypeData.GetBoundingRadius() );
        }
    }

}
