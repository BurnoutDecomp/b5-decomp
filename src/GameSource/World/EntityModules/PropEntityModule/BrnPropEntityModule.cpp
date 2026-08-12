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

        // PARK -- the X360 clears mVisibleOverheadSigns' live count here
        // (`stwx r29, r31, 0xD21B0`). Its DWARF type,
        // GuiOverheadSignInfoEvent::VisibleOverheadSignArray, has no committed home, so
        // the member is opaque storage in the header and there is no named count to
        // reset. Deliberately NOT expressed as a raw offset poke. Restore this line to
        // `mVisibleOverheadSigns.Clear();` when the GUI overhead-sign event group lands.

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
