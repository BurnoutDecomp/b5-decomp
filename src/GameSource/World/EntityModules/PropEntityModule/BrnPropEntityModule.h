#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h
//
// BrnWorld::PropEntityModule -- the world entity module that owns every loaded
// destructible prop: its PropZoneManager (instance pools + per-zone slots + cell grid),
// the loaded prop-physics data header, the per-zone graphics-list resource table, the
// prop streaming state machine, and the runtime tuning flags the debug menu exposes.
//
// ============================================================================
// 2026-08-12 (prop-spawn wave, phase 2) -- FULL MEMBER RECOVERY. This header used to be a
// PARTIAL: a handful of named members separated by `u8 mPadN[..]` blocks sized from the
// CONSOLE sizeof(PropZoneManager) (841312) so the named members landed at their X360
// byte offsets. That model was doubly broken:
//
//   (1) It cannot hold on x64. PropZoneManager is wider on the host (PropCellManager's
//       pointers widened; PropGraphicsManager::PropGraphicsReference is 16 bytes here,
//       not 8), so every console-derived pad placed its member somewhere else entirely.
//       The module is embedded BY VALUE in WorldModule (BrnWorldModule.h:586), so a
//       mis-sized pad corrupts the world module, not just this class.
//   (2) Two of the transcribed offsets were WRONG even for the console, because they were
//       copied from Hex-Rays pseudocode instead of the asm:
//         * mbUseOverrides / the three thresholds were documented at +0xCDA20..+0xCDA2C.
//           The OnActivate ASM (0x822C53D0: `addis r4,r10,0xD; addi r4,r4,-0x26A0`)
//           registers "Use Overrides" at module + 0xCD960 -- and Construct @0x822FA0D4
//           zeroes exactly 0xCD960 / 0xCD964 / 0xCD968 / 0xCD96C. 0xCDA20 is inside the
//           event-receiver queue's 1024-byte backing buffer; it was never a member.
//         * mpPropPhysicsDataHeader was documented at +0xCDD48. Prepare's asm
//           (0x82306F68: `addis r3,r31,0xD; addi r3,r3,-0x2278`) binds it at 0xCDD88,
//           which is also exactly mReceiverQueue(0xCD970) + 0x18 + 1024.
//
// The pads are therefore GONE. Every member is now named and typed, in DWARF declaration
// order, and the host compiler lays them out. Nothing in this port may key on a console
// offset again: index by named member.
//
// ---- SOURCES -------------------------------------------------------------------------
//  * DecFIGS DWARF (GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h)
//    -- member NAMES, TYPES and ORDER (rung 2), gated on X360 attestation.
//  * BURNOUT_X360_ARTIST.XEX ASSEMBLY (rung 1) -- which members actually exist in the
//    shipped build, and their initial values. Every offset quoted below was read off the
//    asm, never off the pseudocode.
//
// ---- HOW THE CONSOLE LAYOUT WAS RE-DERIVED (provenance only; NOT host offsets) --------
// Construct @0x822FA068, Prepare @0x82306DB8, CachePropGraphicsLists @0x822DBF28,
// ConstructPreScenePerfMonitors @0x822A90A0, ConstructPostPhysicsPerfMonitors @0x822A9218,
// PrepareForReplay @0x822A94B0, RestoreFromReplay @0x822A95A8, LeaveReplay @0x822C4810 and
// PropEntityDebugComponent::OnActivate @0x822C52C8 between them touch a contiguous chain
// with NO unexplained gaps, which is what makes the mapping safe:
//
//   +0x00228 mePrepareStage            stw 0            (Construct; Release reads 0x228)
//   +0x0022C meReleaseStage            stw 2
//   +0x00280 mZoneManager              PropZoneManager::Construct(this+0x280)
//   +0x0CD900 mDebugComponent          PropEntityDebugComponent::Construct(this+0xCD900)
//   +0x0CD960 mbUseOverrides           stbx 0           (OnActivate "Use Overrides")
//   +0x0CD964 mfOverrideLeanThreshold  stfsx 0.0        (OnActivate "...Lean Threshold")
//   +0x0CD968 mfOverrideMoveThreshold  stfsx 0.0
//   +0x0CD96C mfOverrideSmashThreshold stfsx 0.0
//   +0x0CD970 mReceiverQueue           mpBuffer=+0x18, cap=0x400@+0x10, align=16@+0x14
//   +0x0CDD88 mpPropPhysicsDataHeader  = 0xCD970 + 0x18 + 1024   (Prepare CreateFromHandle)
//   +0x0CDDA8 mVFXPropCollection       = +0x20 past it -> ResourcePtr stride 32 on console
//   +0x0CDDD0 mLastPlayerResetPosition (16-aligned Vector3)
//   +0x0CDDE0 mbPlayerWasJustReset     stbx 0
//   +0x0CDDE4 maRecentlyBrokenProps    Set<PropEntityID,32>:  count word @0xCDE64 = +128
//   +0x0CDE68 maRecentlyRecycledProps  Array<PropEntityID,15>: count word @0xCDEA4 = +60
//   +0x0CDEA8 maRecentlyRecycledParts  Array<PropEntityID,30>: count word @0xCDF20 = +120
//   +0x0CDF24 mapGraphicsLists[500]    500 x CreateFromHandle, stride 0x20 -> ends 0xD1DA4
//   +0x0D1DA4 mVisibleOverheadSigns    (live-count reset @0xD21B0)
//   +0x0D21C0 miFramesUntilUpdateVisibleSigns  stwx 1
//   +0x0D21C4 mPropGraphicsManager     CachePropGraphicsLists passes this+0xD21C4 as `this`
//   +0x0D3180 mPropEntitySerialiser    PropEntitySerialiser::Construct(this+0xD3180)
//   +0x0D3200 meStreamingMode          stwx 0           (ResetProps writes 2 here)
//   +0x0D3204 mbStreamingSettled       stbx 0  (X360-only; see the member's comment)
//   +0x0D3208 muMaxLoadedZones         (not written by Construct)
//   +0x0D320C muNumberOfLoadedZones    stwx 0  (RenderModuleStats " Zones loaded: ")
//   +0x0D3210 mu8PlayerIndex           stbx 0
//   +0x0D3220 mPlayerPosition          (16-aligned Vector3)
//   +0x0D3230 maRaceCarVelocity[8]     8 x stvx128 0 at 0xD3230..0xD32A0
//   +0x0D32B0 mabWaitingForGraphics    8 x std 0  (BitArray<500> == 8 x u64 == 64 bytes)
//   +0x0D32F0 mabWaitingForInstances   8 x std 0
//   +0x0D3330 miReplayState            (X360-only; PrepareForReplay/RestoreFromReplay)
//   +0x0D3334 mbInReplay               stbx 0
//   +0x0D3338 muReplayPropsInScene     (X360-only; LeaveReplay RemoveEntity loop bound)
//   +0x0D333C muReplayPartsInScene     (X360-only; second LeaveReplay loop bound)
//   +0x0D3340 mbCurrentlyOnline        stbx 0  (OnActivate "Currently online")
//   +0x0D3341 mbEasySmashProps         stbx 0
//   +0x0D3342 mbAllowPropProgression   stbx 1  (OnActivate "Allow prop progression")
//   +0x0D3343 mbPlayerCrashing         stbx 0
//   +0x0D3344 mbPlayerWrecked          stbx 0
//   +0x0D3345 mbResourceSystemStalled  stbx 0
//   +0x0D3346 mbResetPropPosition      stbx 0
//   +0x0D3347 mbOverrideLod            stb  0  (debug "Override Prop LOD")
//   +0x0D3348 miLodOverrideValue       stw  0  (debug "Prop LOD number", limits 0..15)
//   +0x0D334C mbDrawBoundingSpheres    stb  0  (debug "Draw prop bounding spheres")
//   +0x0D334D mbOverrideLodDistances   stb  0  (debug "OverridePropDistances")
//   +0x0D3350 mauOverrideLodDistances[3]  = 100*(i+1); the debug registrations use the
//             LITERAL addresses 0xD3350 / 0xD3354 / 0xD3358 and the init loop's index math
//             is `(i + 0x34CD4) * 4` == 0xD3350 + i*4 -- the tightest pin in the class.
//   +0x0D335C miUpdatesSinceLastSimPause  stwx 0
//   +0x0D3360 miCollisionStreamingPM   ConstructPreScenePerfMonitors (asserts name it)
//   +0x0D3364 miLoadingPM
//   +0x0D3368 miUnloadingPM
//   +0x0D336C miProcessContactsPM      ConstructPostPhysicsPerfMonitors
//   +0x0D3370 miUpdatePropsPM
//   +0x0D3374 miSerialisePM            (X360-only; assert text "miSerialisePM >= 0")
//   +0x0D3378 mrTimestep
//   +0x0D337C mLoadedZones             Set<u16,15>: count word @0xD339C = +32 (30+2 pad)
//   +0x0D33A0 mabLoadedWorldGraphics   8 x std 0 -> ends 0xD33E0
//
// DWARF-vs-X360 DELTA (the Dec-2007 PS3 build predates these; all five are attested by
// X360 asm and by X360-only methods the DWARF does not declare -- PrepareForReplay,
// RestoreFromReplay, LeaveReplay, ReplayPreSceneUpdate, ReplayUpdateProps/PartsInScene,
// RenderReplayProp): mPropEntitySerialiser, miSerialisePM, miReplayState,
// muReplayPropsInScene, muReplayPartsInScene, mbStreamingSettled.
//
// NAMING NOTE: five members keep their pre-existing PC spellings rather than the DWARF's
// because BrnPropEntityDebugComponent.cpp (another file's lane) already calls them by
// name: mbUseOverrides (DWARF `mbUseOverides`, a typo in the original source),
// mfOverrideLeanThreshold / mfOverrideMoveThreshold / mfOverrideSmashThreshold (DWARF
// `mfLeanThresholdOverride` / `mfMoveThresholdOverride` / `mfSmashThresholdOverride`) and
// muNumberOfLoadedZones (DWARF `muZonesLoaded`). Flagged for a later rename sweep that can
// touch both files at once. `mbPropsEnabled`, which the previous partial invented ("the
// overlay registers 'Enable props'"), is DELETED -- OnActivate registers no such variable
// and no member exists at that offset.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "SharedClasses/BrnSharedConstants.h"  // BrnUpdateSet (typedef u16)

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // EventReceiverQueue<1024,16>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // ResourcePtr<T>
#include "GameShared/GameClasses/Containers/CgsSet.h"                  // ::Set<T,N>
#include "GameShared/GameClasses/Containers/CgsArray.h"                // ::Array<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"             // BitArray<N>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // CgsModule::EventQueue<T,N>
#include "SharedIO/BrnPropGraphicsAndZoneEvents.h"                     // PropInstancesNeededForZoneEvent
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"               // PropEntityID
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h" // PropEntitySerialiser
#include "BrnPropZoneManager.h"              // PropZoneManager, PropGraphicsManager (by value)
#include "BrnPropEntityDebugComponent.h"     // PropEntityDebugComponent (by value)

namespace BrnPhysics { namespace Props { class PropPhysicsDataHeader; class PropGraphicsList; } }
namespace BrnParticle { class VFXPropCollection; }
namespace CgsGraphics { class Model; }
namespace rw { class IResourceAllocator; }
namespace CgsGraphics { class DispatchFrame; }

namespace CgsModule { struct IOBufferStack; }
namespace BrnWorld { namespace PropEntityIO { class InputBuffer_PrePhysics; class OutputBuffer_PrePhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; struct InputBuffer_Dispatch; class OutputBuffer_Prepare; class InputBuffer_PreScene; class OutputBuffer_PreScene; class InputBuffer_PostPhysics; class OutputBuffer_PostPhysics; } }

namespace BrnWorld
{
    // The dispatch frame every render entry point below takes is CgsGraphics::DispatchFrame
    // (GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h) -- the header used to
    // forward-declare a distinct GLOBAL `class DispatchFrame;` that no definition anywhere
    // ever matched, so the prop draw path could not reach the real dispatcher without a
    // cross-type reinterpret_cast. (Same defect BrnPropGraphicsList.h had with `class Model;`;
    // both fixed together this wave.) Aliased rather than renamed so existing spellings in
    // the declarations below stay as they are.
    typedef CgsGraphics::DispatchFrame DispatchFrame;

    struct ShaderLodInfo;

    // ⭐ RESOLVED 2026-08-12 (prop-spawn wave, agent B5). This used to be
    //     namespace PropEntityIO { class PropInstancesNeededForZoneQueue; }
    // -- a placeholder forward declaration standing in for the DWARF's nested
    // `InputBuffer_PreScene::PropInstancesNeededForZoneQueue`. It could never be
    // completed: the real type is a TYPEDEF to a template instantiation
    // (BrnPropEntityModuleIO.h:81, and the producer spells the same one at
    // BrnWorldEntityModuleIO.h:243)
    //     typedef CgsModule::EventQueue<PropEntityIO::PropInstancesNeededForZoneEvent,30>
    //             PropInstancesNeededForZoneQueue;
    // and a forward-declared CLASS cannot be re-pointed at a typedef. So the two
    // consumers below now name the instantiation directly. Attested by
    // GenerateTargetList @0x822DA730 (`lwz r29, 8(r30)` == BaseEventQueue::miLength;
    // `sub_822AD708` == BaseEventQueue<T>::GetEvent(int), whose "mpEvents != NULL"
    // tripwire is CgsBaseEventQueue.h:272; `lhz 0(result)` reads the event's single u16)
    // and by InputBuffer_PreScene::Construct @0x822EFAA0 (maxLen 30 at this+4).

    // ========================================================================
    // BrnWorld::PropEntityModule (DWARF BrnPropEntityModule.h:69).
    // ========================================================================
    class PropEntityModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        typedef BrnPhysics::Props::PropPhysicsDataHeader PropPhysicsDataHeader;
        typedef BrnPhysics::Props::PropGraphicsList      PropGraphicsList;
        typedef BrnPhysics::Props::PropZoneData          PropZoneData;

        // BrnPropEntityModule.h:40 / :48 / :185 (DWARF constants).
        static const s32 KI_ZONE_SET_SIZE                 = 15;
        static const u16 KU_CREATE_RW_VOLUME_BUFFER_SIZE  = 256;
        static const s8  KI_NUM_LODS                      = 3;

        // BrnPropEntityModule.h:41. The per-frame "zones we want loaded" working set.
        typedef ::Set<u16, KI_ZONE_SET_SIZE>       PropZonesSet;
        // BrnPropCellManager.h:39/40/41 (DWARF names the typedefs there).
        typedef ::Set<PropEntityID, 32u>           RecentlyBrokenPropsArray;
        typedef ::Array<PropEntityID, 15u>         RecentlyRecycledPropsArray;
        typedef ::Array<PropEntityID, 30u>         RecentlyRecycledPartsArray;
        // The DWARF's InputBuffer_PreScene::PropInstancesNeededForZoneQueue
        // (BrnPropEntityModuleIO.h:81), spelled as the instantiation it is -- see the
        // RESOLVED note above the class.
        typedef CgsModule::EventQueue<PropEntityIO::PropInstancesNeededForZoneEvent, 30>
                PropInstancesNeededForZoneQueue;

        // BrnPropEntityModule.h:72 -- the Prepare() resume ladder. Prepare @0x82306DB8 is a
        // `switch (mePrepareStage)` with fallthrough over cases 0,1,2,3,4,5,8,9; 6 and 7
        // land in the "Invalid Stage\n" default (the two AQUIRE_PROP_INSTANCES /
        // LOAD_PROP_INSTANCES stages are never entered by the shipped build).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START                 = 0,
            E_PREPARESTAGE_MANAGER               = 1,
            E_PREPARESTAGE_REQUEST_PROP_VFX_DATA = 2,
            E_PREPARESTAGE_AQUIRE_PROP_VFX_DATA  = 3,
            E_PREPARESTAGE_LOAD_PROP_PHYSICS     = 4,
            E_PREPARESTAGE_AQUIRE_PROP_PHYSICS   = 5,
            E_PREPARESTAGE_LOAD_PROP_INSTANCES   = 6,
            E_PREPARESTAGE_AQUIRE_PROP_INSTANCES = 7,
            E_PREPARESTAGE_INITIALIZE_PHYSICS_DATA = 8,
            E_PREPARESTAGE_DONE                  = 9,
        };

        // BrnPropEntityModule.h:86.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        // BrnPropEntityModule.h:93. The prop streaming / reset state machine the module runs
        // each frame and the debug overlay's "Reset props" action pokes.
        enum EPropStreamingMode
        {
            E_STREAM                      = 0,
            E_DONT_STREAM                 = 1,
            E_RESET_UNLOADING             = 2,
            E_RESET_UNLOADING_FOR_PROFILE = 3,
            E_REQUESTING_PROFILE_DATA     = 4,
            E_WAITING_FOR_PROFILE_DATA    = 5,
        };

    public:
        // ---- lifecycle (this TU: BrnPropEntityModule.cpp) ----
        // @0x822FA068. THE root of the prop chain: it is the only writer of
        // PropZoneManager::mauStartIndexOfZone[] = KU_UNLOADED_ZONE, so until it runs every
        // zone reads as already-loaded and nothing can ever stream in.
        void Construct() override;
        // @0x82306DB8. Resumable staged prepare (see EPrepareStage).
        bool Prepare(PropEntityIO::OutputBuffer_Prepare* lpOutputBuffer,
                     rw::IResourceAllocator* lpPhysicsAllocator);
        // @0x822A92F8.
        bool Release() override;
        // Declaration-only -- NOT EMITTED in the ARTIST image (no `PropEntityModule::Destruct`
        // in the export; the compiler folded/elided it). Body parked rather than invented.
        void Destruct() override;

        // @0x822A90A0 / @0x822A9218. WorldModule::Construct registers the module's nested
        // pre-scene / post-physics CPU perf monitors through these.
        void ConstructPreScenePerfMonitors();
        void ConstructPostPhysicsPerfMonitors();

        // @0x822DBF28. Rebuild the type -> PropGraphics registration table from every
        // loaded per-zone graphics list.
        void CachePropGraphicsLists();

        // ---- per-frame ticks (declaration-only here; see the per-phase TUs) ----
        void PrePhysicsUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                               CgsModule::IOBufferStack* lpOutputBufferStack,
                               PropEntityIO::InputBuffer_PrePhysics* lpInput,
                               PropEntityIO::OutputBuffer_PrePhysics* lpOutput,
                               BrnUpdateSet lUpdateSet );

        void PostSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                              CgsModule::IOBufferStack* lpOutputBufferStack,
                              PropEntityIO::InputBuffer_PostScene* lpInput,
                              PropEntityIO::OutputBuffer_PostScene* lpOutput,
                              BrnUpdateSet lUpdateSet );

        // @0x82309A40 (2289 insns) -- the streaming driver. AGENT B2 OWNS THE BODY.
        void PreSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                             CgsModule::IOBufferStack* lpOutputBufferStack,
                             PropEntityIO::InputBuffer_PreScene* lpInput,
                             PropEntityIO::OutputBuffer_PreScene* lpOutput,
                             BrnUpdateSet lUpdateSet );

        void PostPhysicsUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                                CgsModule::IOBufferStack* lpOutputBufferStack,
                                PropEntityIO::InputBuffer_PostPhysics* lpInput,
                                PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                                BrnUpdateSet lUpdateSet );

        // @0x822FB4F0 (774 insns) -- AGENT B3 OWNS THE BODY. Signature unchanged from the
        // committed one (WorldModule::GenerateDispatchLists @0x827D1CE8 calls it); the DWARF
        // declares two trailing bools after liSortKey which the committed 9-arg form omits.
        // B3: verify the trailing pair against the asm before adding them -- doing so is an
        // ADDITIVE change to this declaration and must be mirrored in BrnWorldModule.cpp.
        // ⭐ RESOLVED 2026-08-12 (agent B3): the two trailing bools ARE real. DWARF
        // BrnEntityModuleUnity.cpp:33003 names them lbRenderingEnvironmentMap / lbRenderCoronas,
        // and BOTH call sites in WorldModule::GenerateDispatchLists @0x827D1CE8 write them --
        // main view @0x827D294C..58 (@0x6F = 0, @0x77 = 1, @0x64 = 15), env-map face
        // @0x827D2C58..70 (@0x6F = 1, @0x77 = 0, @0x64 = r28). They are the ONLY thing that
        // distinguishes the two passes, which is why the 9-arg form could not have worked.
        // Mirrored in BrnWorldModule.cpp.
        void GenerateDispatchLists( PropEntityIO::InputBuffer_Dispatch* lpInput,
                                    const Array<CgsSceneManager::EntityId, 5400u>& lrVisibleEntities,
                                    Matrix44::InParam lCameraViewProjection,
                                    Vector3::InParam lCameraPosition,
                                    f32 lfMainCameraZoomFactor,
                                    const ShaderLodInfo* lpShaderLodInfo,
                                    s32 liModelOnlyDisplayList, s32 liOpaqueList, s32 liTransparentList,
                                    bool lbRenderingEnvironmentMap, bool lbRenderCoronas );

        // ---- replay hooks (X360-only; not in the DecFIGS DWARF) ----
        bool PrepareForReplay();     // @0x822A94B0
        bool RestoreFromReplay();    // @0x822A95A8
        void LeaveReplay( PropEntityIO::OutputBuffer_PreScene* lpOutput );  // @0x822C4810

        const PropPhysicsDataHeader* GetPropPhysicsDataHeader() const
        {
            return mpPropPhysicsDataHeader.GetMemoryResource();
        }

        // Layout tripwires (never called; see the definition in the .cpp).
        static void _AssertLayout();

    private:
        // ---- prepare-stage helper (this TU) ----
        // @0x822DA840 (510 insns). Stage 8 of Prepare: publish the prop culling-group
        // matrix and register every prop-type / part-type collision volume as a dynamic
        // volume with the scene manager, then hand the physics side the data handle.
        void InitializePropPhysicsData( PropEntityIO::OutputBuffer_Prepare* lpOutputBuffer );

        // ====================================================================
        // AGENT B2 (streaming) -- bodies land in BrnPropEntityModule.cpp.
        // Signatures are the DecFIGS DWARF declarations (rung 2), which are the only
        // attested shapes: Hex-Rays renders every one of these as a 14-plus `int` blob.
        // ====================================================================
        // @0x82308330 (1007 insns).  DWARF BrnPropEntityModule.cpp:760.
        void UpdateInstanceStreaming( const PropInstancesNeededForZoneQueue* lpNeeded,
                                      s32 liZoneIndex,
                                      PropEntityIO::OutputBuffer_PreScene* lpOutput );
        // @0x822DA730 (63 insns).   DWARF BrnPropEntityModule.cpp:882.
        void GenerateTargetList( const PropInstancesNeededForZoneQueue* lpNeeded,
                                 PropZonesSet& lrTargetZones );
        // @0x823035D0 (112 insns).  DWARF BrnPropEntityModule.cpp:2001.
        void LoadZone( PropZoneData* lpZoneData,
                       PropEntityIO::OutputBuffer_PreScene* lpOutput );
        // @0x82306FD0 (99 insns).   DWARF BrnPropEntityModule.cpp:2025.
        void UnloadZone( u16 lu16ZoneId,
                         PropEntityIO::OutputBuffer_PreScene* lpOutput );
        // @0x822FB000 (168 insns).  DWARF BrnPropEntityModule.cpp:1920.
        void BreakPropIntoParts( PropEntityID lPropEntityId,
                                 PropEntityIO::OutputBuffer_PreScene* lpOutput );
        // @0x822FB2A0 (148 insns).  DWARF BrnPropEntityModule.cpp:2045.
        void UpdateProps( PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                          const void* lpUpdatePropEventQueue );
        // BrnPropEntityModule.h:386 -- the debug overlay's "Reset props" action target.
        void ResetProps();

        // ====================================================================
        // AGENT B3 (render) -- bodies land in BrnPropEntityModule.cpp.
        // ====================================================================
        // @0x822C4918 (620 insns). DWARF BrnPropEntityModule.cpp:2425 -- verbatim shape.
        bool RenderModel( CgsGraphics::Model* lpModel,
                          const Matrix44Affine* lpTransform,
                          DispatchFrame* lpDispatchFrame,
                          Matrix44::InParam lViewProjection,
                          Vector3::InParam lCameraPosition,
                          f32 lfLodZoomFactor,
                          s32 liList, s32 liSortLayer, s32 liSortKey,
                          // ⭐ RENAMED 2026-08-12 (agent B3, from the 0x822C4918 body): these
                          // two were guessed as lbIsPart / lbIsSmashed. They are actually
                          // lbRenderingEnvironmentMap / lbUseZOnlyRendering -- same positions
                          // and types, so binary-compatible, but the old names described the
                          // wrong thing entirely (the part-vs-whole split happens in the
                          // CALLER, via PropEntityID::GetPartIndex()).
                          bool lbRenderingEnvironmentMap, bool lbUseZOnlyRendering,
                          const ShaderLodInfo* lpShaderLodInfo );
        // @0x822DC010 (187 insns). X360-ONLY -- the DecFIGS DWARF does not declare it and
        // Hex-Rays gives a 21-`int` blob, so this was the one UNATTESTED declaration in the
        // file. ⭐ CORRECTED 2026-08-12 (agent B3) off the 0x822DC030..0x822DC074 prologue.
        // The placeholder was wrong in four ways: r4 is a PropGraphics*, NOT a Model* (the
        // body's first act is `lwz r4,4(r4)` == mpPropModel); r6 is the prop TYPE ID (bounds-
        // checked against the VFX table, `slwi r11,r30,4` == 16-byte VFXProp stride); the
        // return type is void (the epilogue never sets r3, and both early-outs branch straight
        // to it); and it takes 16 parameters, not 10.
        //
        // The parameter map was recovered via the save-area rule that also reconciles
        // GenerateDispatchLists, RenderModel and DrawRenderable::AddToBin's arg_57/5F/64/6F
        // simultaneously: the save area starts at incoming-SP + 0x14 at 8 bytes per parameter;
        // a __vector4 reserves 16 bytes of save area but consumes NO GPR; a float DOES consume
        // its GPR slot (the recurring PPC ABI trap); GPRs are then assigned r3..r10 by counting
        // non-vector parameters, and the rest go on the stack.
        void RenderPropAndCoronas( const BrnPhysics::Props::PropGraphics* lpPropGraphics, // r4
                                   const Matrix44Affine* lpTransform,                     // r5
                                   u32 luPropTypeId,                                      // r6
                                   DispatchFrame* lpDispatchFrame,                        // r7
                                   Matrix44::InParam lCameraViewProjection,               // r8
                                   Vector3::InParam lCameraPosition,                      // v1
                                   f32 lfLodDistanceZoomScale,                            // f1 (skips r9)
                                   s32 liModelOnlyDisplayList,                            // r10
                                   s32 liOpaqueList, s32 liTransparentList,               // @0x64 @0x6C
                                   bool lbRenderingEnvironmentMap,                        // @0x74
                                   const ShaderLodInfo* lpShaderLodInfo,                  // @0x7C
                                   bool lbRenderCoronas,                                  // @0x84
                                   const void* lpVFXPropTable,                            // @0x8C
                                   bool lbUseZOnlyRendering,                              // @0x94
                                   u32 luCoronaSubmissionInterface );                     // @0x9C

    public:
        // ====================================================================
        // MEMBERS -- DWARF declaration order. Host layout is whatever the compiler
        // computes; the console offsets in the banner are PROVENANCE ONLY.
        // ====================================================================
        EPrepareStage mePrepareStage;                        // :190  console +0x228
        EReleaseStage meReleaseStage;                        // :191  console +0x22C

        PropZoneManager mZoneManager;                        // :193  console +0x280

        PropEntityDebugComponent mDebugComponent;            // :196  console +0xCD900

        // DWARF spells these mbUseOverides / mf<Field>ThresholdOverride -- see the NAMING
        // NOTE in the banner; the PC spelling is kept because the debug component's
        // committed TU already uses it.
        bool mbUseOverrides;                                 // :197  console +0xCD960
        f32  mfOverrideLeanThreshold;                        // :198  console +0xCD964
        f32  mfOverrideMoveThreshold;                        // :199  console +0xCD968
        f32  mfOverrideSmashThreshold;                       // :200  console +0xCD96C

        CgsModule::EventReceiverQueue<1024, 16> mReceiverQueue;  // :203 console +0xCD970

        CgsResource::ResourcePtr<PropPhysicsDataHeader>      mpPropPhysicsDataHeader; // :205
        CgsResource::ResourcePtr<BrnParticle::VFXPropCollection> mVFXPropCollection;  // :206

        Vector3 mLastPlayerResetPosition;                    // :208  console +0xCDDD0
        bool    mbPlayerWasJustReset;                        // :209  console +0xCDDE0

        RecentlyBrokenPropsArray   maRecentlyBrokenProps;    // :211  console +0xCDDE4
        RecentlyRecycledPropsArray maRecentlyRecycledProps;  // :212  console +0xCDE68
        RecentlyRecycledPartsArray maRecentlyRecycledParts;  // :213  console +0xCDEA8

        // :215. One resource pointer per prop zone; Construct binds all 500 to the null
        // handle, CachePropGraphicsLists walks them.
        CgsResource::ResourcePtr<PropGraphicsList> mapGraphicsLists[KU_MAX_ZONES]; // +0xCDF24

        // :216. DWARF type GuiOverheadSignInfoEvent::VisibleOverheadSignArray -- NO
        // COMMITTED HOME (the GUI overhead-sign event group has not landed). Modelled as
        // correctly-sized opaque storage so the member exists and is reachable by name;
        // Construct's reset of its live count is PARKED (see the .cpp). Console span
        // 0xD1DA4..0xD21BF == 1052 bytes.
        struct VisibleOverheadSignArrayStorage { u8 maBytes[1052]; };
        VisibleOverheadSignArrayStorage mVisibleOverheadSigns;   // :216 console +0xD1DA4

        s32                 miFramesUntilUpdateVisibleSigns;     // :217 console +0xD21C0
        PropGraphicsManager mPropGraphicsManager;                // :218 console +0xD21C4

        // X360-ONLY (no DWARF member): the prop-entity replay serialiser the module owns.
        // Construct @0x822FA188 calls BrnReplays::PropEntitySerialiser::Construct(this+0xD3180).
        BrnReplays::PropEntitySerialiser mPropEntitySerialiser;  // console +0xD3180

        EPropStreamingMode meStreamingMode;                  // :220  console +0xD3200

        // X360-ONLY (no DWARF member). A byte immediately after meStreamingMode, cleared by
        // Construct (`stbx r29, r31, 0xD3204`) and read as the replay-readiness gate in
        // PrepareForReplay @0x822A94B0 and RestoreFromReplay @0x822A95A8
        // (`if (!flag) return false;`, alongside `muNumberOfLoadedZones > 1`). The name is
        // DESCRIPTIVE OF THAT ROLE ONLY -- no symbol or assert string attests it.
        bool mbStreamingSettled;                             //       console +0xD3204

        u32  muMaxLoadedZones;                               // :222  console +0xD3208
        // DWARF `muZonesLoaded`; PC spelling kept (debug component consumer). The
        // module-stats overlay prints it as " Zones loaded: ".
        u32  muNumberOfLoadedZones;                          // :223  console +0xD320C
        u8   mu8PlayerIndex;                                 // :224  console +0xD3210

        Vector3 mPlayerPosition;                             // :226  console +0xD3220
        // :228. DWARF type Vector3Plus (a Vector3 whose w lane carries a scalar). No
        // committed home for Vector3Plus; Vector3 is the same 16-byte SIMD lane and every
        // access in this module is xyz, so the storage and the semantics are identical.
        Vector3 maRaceCarVelocity[8];                        //       console +0xD3230

        CgsContainers::BitArray<KU_MAX_ZONES> mabWaitingForGraphics;   // :230  console +0xD32B0
        CgsContainers::BitArray<KU_MAX_ZONES> mabWaitingForInstances;  // :231  console +0xD32F0

        // X360-ONLY. The replay entry/exit ladder PrepareForReplay/RestoreFromReplay walk
        // (observed values 0..5; they drive meStreamingMode = E_RESET_UNLOADING and gate on
        // mbStreamingSettled). Left as the raw state word rather than inventing enumerator
        // names for a DWARF-absent enum.
        s32  miReplayState;                                  //       console +0xD3330
        bool mbInReplay;                                     // :233  console +0xD3334
        // X360-ONLY. LeaveReplay @0x822C4810 removes scene entities
        // `(i << 10) | 0x22000000` for i < muReplayPropsInScene, then
        // `(i << 10) | 0x22000001` for i < muReplayPartsInScene, then zeroes both.
        u32  muReplayPropsInScene;                           //       console +0xD3338
        u32  muReplayPartsInScene;                           //       console +0xD333C

        bool mbCurrentlyOnline;                              // :235  console +0xD3340
        bool mbEasySmashProps;                               // :237  console +0xD3341
        bool mbAllowPropProgression;                         // :239  console +0xD3342
        bool mbPlayerCrashing;                               // :240  console +0xD3343
        bool mbPlayerWrecked;                                // :241  console +0xD3344
        bool mbResourceSystemStalled;                        // :243  console +0xD3345
        bool mbResetPropPosition;                            // :245  console +0xD3346
        bool mbOverrideLod;                                  // :247  console +0xD3347
        s32  miLodOverrideValue;                             // :248  console +0xD3348
        bool mbDrawBoundingSpheres;                          // :249  console +0xD334C
        bool mbOverrideLodDistances;                         // :250  console +0xD334D
        s32  mauOverrideLodDistances[KI_NUM_LODS];           // :251  console +0xD3350

        s32  miUpdatesSinceLastSimPause;                     // :254  console +0xD335C

        s32  miCollisionStreamingPM;                         // :259  console +0xD3360
        s32  miLoadingPM;                                    // :260  console +0xD3364
        s32  miUnloadingPM;                                  // :261  console +0xD3368
        s32  miProcessContactsPM;                            // :262  console +0xD336C
        s32  miUpdatePropsPM;                                // :263  console +0xD3370
        // X360-ONLY (assert text "miSerialisePM >= 0", BrnPropEntityModule.cpp:212).
        s32  miSerialisePM;                                  //       console +0xD3374

        f32  mrTimestep;                                     // :266  console +0xD3378

        PropZonesSet mLoadedZones;                           // :268  console +0xD337C
        CgsContainers::BitArray<KU_MAX_ZONES> mabLoadedWorldGraphics;  // :269  console +0xD33A0

        friend class PropEntityDebugComponent;
    };
}
