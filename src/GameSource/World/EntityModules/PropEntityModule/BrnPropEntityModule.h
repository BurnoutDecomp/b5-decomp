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
// ⭐ WAVE Q KEYSTONE (2026-08-18): UpdatePropEvent, the element of the post-physics
// update queue UpdateProps drains (its parameter used to be a `const void*` placeholder).
// No cycle: BrnPropEvents.h pulls only BrnCommonTypes.h, BrnPropEntityID.h and
// BrnPropEntityInstance.h, none of which reaches back here.
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"     // BrnPhysics::Props::UpdatePropEvent
// ⭐ WAVE Q KEYSTONE: BrnGui::OverheadSignScore -- the element type of mVisibleOverheadSigns,
// which used to be modelled as opaque byte storage (see that member's note).
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::OverheadSignScore
// ⭐ WAVE Q KEYSTONE: BrnCoronaManager::BrnSubmissionInterface -- the retyped corona
// submission handle RenderPropAndCoronas / RenderReplayProp carry.
#include "GameSource/Graphics/BrnCoronaManager.h"                     // BrnCoronaManager::BrnSubmissionInterface

namespace BrnPhysics { namespace Props { class PropPhysicsDataHeader; class PropGraphicsList; } }
// ⭐ WAVE Q KEYSTONE: pointer-only parameter uses in the break pipeline (GetDesiredState takes a
// const PropTypeData*, the three contact handlers take a PropInputInterface*). Both have real
// homes (SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h and
// GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h) which the BODIES include;
// forward-declared here to keep this header's dependency tail where it already is.
namespace BrnPhysics { namespace Props { class PropTypeData; struct PropInputInterface; } }
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

        // ⭐ WAVE Q KEYSTONE (2026-08-18) -- the rest of the X360-only replay set. All five
        // are real ledger functions with per-address exports; none is in the DecFIGS DWARF
        // (the PS3 build predates the prop replay path -- the same merge-window delta that
        // gives the class miReplayState / muReplayPropsInScene / muReplayPartsInScene /
        // mbStreamingSettled / miSerialisePM / mPropEntitySerialiser). Signatures below come
        // from the ARTIST PROLOGUES, not from Hex-Rays (which renders all three of the
        // 2-parameter ones as 14-`int` blobs).
        //
        // @0x822EF878 (park P1). Prologue: r3 this, r4 lpInput, r5 lpOutput, r6 lUpdateSet
        // (`extrwi r11,r6,8,16 ; clrlwi r30,r11,31` == (lUpdateSet >> 8) & 1). ⚠️ MEASURED:
        // the body NEVER READS r4 -- lpInput is moved nowhere and no instruction touches it.
        // It is declared because the CALL SITE (PreSceneUpdate @0x82309AB8) passes it.
        void ReplayPreSceneUpdate( PropEntityIO::InputBuffer_PreScene* lpInput,
                                   PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                   BrnUpdateSet lUpdateSet );
        // @0x822DB370 / @0x822DB900. Prologue: r3 this, r4 lpOutput -- two parameters, no more
        // (0x822DB390/94 and 0x822DB920/24 move exactly r3 and r4). Both reconcile the scene's
        // replay prop/part entity population against the playback frame's counts
        // (GetStaticLayout()[0x5010] vs muReplayPropsInScene, [0x6A10] vs muReplayPartsInScene)
        // via InSceneUpdateInterface Add/RemoveEntity + SetEntityPosition/SetEntityRadius.
        void ReplayUpdatePropsInScene( PropEntityIO::OutputBuffer_PreScene* lpOutput );
        void ReplayUpdatePartsInScene( PropEntityIO::OutputBuffer_PreScene* lpOutput );
        // @0x822EF968. The playback-time draw of one recorded prop: resolve its recorded type
        // and transform out of the replay frame, look the graphics record up in
        // mPropGraphicsManager, and hand the whole render tail to RenderPropAndCoronas.
        // PARAMETER MAP recovered by the same save-area rule that pins RenderPropAndCoronas
        // (8-byte slots from incoming SP+0x14; a __vector4 consumes NO GPR; a float DOES
        // consume its GPR slot). Every entry is proven by the forwarding store at 0x822EFA2C..
        // 0x822EFA84, which copies each incoming slot to RenderPropAndCoronas' matching one:
        //     r4   -> GetEntityIndex()                r5   -> outgoing r7  (dispatch frame)
        //     r6   -> outgoing r8  (view-projection)  v1   -> v1           (camera position)
        //     f1   -> f1           (lod zoom)         r8   -> outgoing r10 (model-only list)
        //     r9   -> outgoing @0x64                  r10  -> outgoing @0x6C
        //     @0x64-> outgoing @0x74                  @0x6C-> outgoing @0x7C
        //     @0x74-> outgoing @0x84                  @0x7C-> outgoing @0x8C
        //     @0x84-> outgoing @0x94                  @0x8C-> outgoing @0x9C
        // Return type is void: the epilogue sets no result and the `beq loc_822EFA88` early-out
        // leaves r3 holding a leftover.
        void RenderReplayProp( PropEntityID lPropEntityId,                       // r4
                               DispatchFrame* lpDispatchFrame,                   // r5
                               Matrix44::InParam lCameraViewProjection,          // r6
                               Vector3::InParam lCameraPosition,                 // v1
                               f32 lfLodDistanceZoomScale,                       // f1 (skips r7)
                               s32 liModelOnlyDisplayList,                       // r8
                               s32 liOpaqueList, s32 liTransparentList,          // r9, r10
                               bool lbRenderingEnvironmentMap,                   // @0x64
                               const ShaderLodInfo* lpShaderLodInfo,             // @0x6C
                               bool lbRenderCoronas,                             // @0x74
                               const void* lpVFXPropTable,                       // @0x7C
                               bool lbUseZOnlyRendering,                         // @0x84
                               BrnCoronaManager::BrnSubmissionInterface*
                                   lpCoronaSubmissionInterface );                // @0x8C

        // @0x827E0488. The class's own constructor -- a real ledger function and, in the
        // shipped image, EXACTLY the compiler's implicit one: the ModuleSingleBuffered base
        // (two EA::Thread::RWMutex ctors at this+16 / this+280 and the two vtable stores),
        // then the members that have non-trivial default ctors -- the two ResourcePtr<>s and
        // the 500-entry mapGraphicsLists (the `v3 += 8` loop over 500 console-32-byte
        // ResourcePtrs, self-linking each intrusive node), and the `-1` KI_UNCONSTRUCTED
        // sentinel into the count word of each embedded ::Array / ::Set
        // (module+841872 == mZoneManager.mauTrafficLightsToRestore, +843428 ==
        // maRecentlyRecycledProps, +843552 == maRecentlyRecycledParts, +860592 ==
        // mVisibleOverheadSigns -- see that member's note). Declaring it user-provided-but-
        // empty is the faithful port: an empty body default-initialises exactly the same set,
        // and the host compiler emits the same chain. It changes nothing about the class --
        // the base already made it non-trivial.
        PropEntityModule();

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
        // ⭐ PARAMETER RETYPED 2026-08-18 (wave Q keystone). The second parameter was a
        // `const void*` PLACEHOLDER. The DWARF spells it
        // `const PropOutputInterface::UpdatePropEventQueue *`, and that typedef and
        // PropEntityIO::InputBuffer_PostPhysics::UpdatePropEventQueue are the SAME
        // instantiation -- CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 200>
        // (BrnPropOutputInterface.h:53 and BrnPropEntityModuleIO.h's InputBuffer_PostPhysics).
        // The instantiation is spelled directly here for the same reason
        // PropInstancesNeededForZoneQueue is (see the RESOLVED note above the class): the
        // owning buffers are only forward-declared in this header, so their nested typedefs
        // cannot be named. The X360 body confirms the element type end to end -- it reads
        // `lwz r11, 8(queue)` (BaseEventQueue::miLength), calls GetEvent(i), then reads the
        // event's +0x60 packed PropEntityID, +0x68 mbFrozen and the two __vector4s at +0x40 /
        // +0x50, which is UpdatePropEvent's exact layout.
        typedef CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 200> UpdatePropEventQueue;
        void UpdateProps( PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                          const UpdatePropEventQueue* lpUpdatePropEventQueue );
        // BrnPropEntityModule.h:386 -- the debug overlay's "Reset props" action target.
        void ResetProps();

        // ====================================================================
        // ⭐ WAVE Q KEYSTONE (2026-08-18) -- THE BREAKABLE-PROP PIPELINE.
        // contact -> state change -> break -> parts physical -> broken-prop event.
        // Every declaration below is a DecFIGS DWARF declaration of this class (source line
        // quoted) AND a real X360 ledger function with a per-address export; nothing here is
        // shaped from Hex-Rays, which renders most of them as `int` blobs.
        // ====================================================================

        // @0x822A93A8. DWARF BrnPropEntityModule.cpp:1648
        //   `BrnWorld::EPropState GetDesiredState(const PropTypeData *, float32_t);`
        // Classify what state a car arriving at `lfIncomingCarSpeed` should put a prop of this
        // type into. The float rides f1 and SKIPS its GPR slot -- the body uses r3 and r4 only,
        // never r5.
        // ⭐ PARAMETERS RENAMED 2026-08-18 (round 3) to the DWARF's own names
        // (_compile/BrnEntityModuleUnity.cpp:1299
        // `GetDesiredState(const PropTypeData* lpPropType, float32_t lfIncomingCarSpeed)`),
        // matching the definition already committed at PropEntityModule_wQ_02.cpp:127. The old
        // second name `lfImpulseMagnitude` was a misnomer: the sole caller
        // (ProcessPotentialContactWithProp, bl @0x822DB11C) passes GetRaceCarSpeed(), the .w
        // lane of the cached race-car velocity, not an impulse.
        EPropState GetDesiredState( const BrnPhysics::Props::PropTypeData* lpPropType,
                                    f32 lfIncomingCarSpeed );

        // @0x822EF550. DWARF BrnPropEntityModule.cpp:1538
        //   `void ChangePropState(PropEntityInstance *, PropEntityID, EPropState, EPropState,
        //                         OutputBuffer_PrePhysics *);`
        void ChangePropState( PropEntityInstance* lpProp, PropEntityID lPropEntityId,
                              EPropState leOldState, EPropState leNewState,
                              PropEntityIO::OutputBuffer_PrePhysics* lpOutput );

        // BrnPropEntityModule.h:394 (DWARF) -- the state-transition legality predicate.
        //
        // ⭐⭐ CORRECTED 2026-08-18 (wave Q round 2). This inline used to read
        //     return ( leOldState < leNewState ) || ( leOldState == E_MOVED );
        // and was justified as "de-inlined from ChangePropState's assert message VERBATIM".
        // That message really is `"leOldState < leNewState || (leOldState == E_MOVED)"` (I
        // dumped the full rodata string at 0x8201DFAC out of the IDB -- IDA truncates it in
        // the listing) and ChangePropState's fold at 0x822EF5AC really is just
        // `cmpw r29,r25 ; blt ; cmpwi r29,5 ; beq` -- but that ASSERT IS NOT THIS PREDICATE.
        // It is a laxer guard that happens to sit next to it. CanChangeState itself is folded
        // at the two places the DWARF records it as an INLINED SUBROUTINE
        // (_compile/BrnEntityModuleUnity.cpp:32042 inside ProcessPotentialContactWithProp,
        // :32159 inside ProcessPotentialContacts), and both folds -- 0x822FA760..0x822FA7BC
        // and 0x822DB2E0..0x822DB33C, instruction-for-instruction identical -- carry a
        // conjunct the assert does not:
        //     r11 = (new > old)          -> r9                       lbHigherState
        //     if (old == 5) r11 = (new == 4) else r11 = 0            lbMovedToPhysical
        //     take the branch when (r9 | r11)
        // The old two-term form was therefore WEAKER than the shipped predicate: it also
        // admitted E_MOVED -> E_NON_PHYSICAL / E_STATIC / E_LEANING /
        // E_PHYSICAL_WITH_EXTRA_COM_OFFSET, which the console rejects.
        //
        // The three-term spelling below is the DWARF's own: its body block declares exactly
        // three locals, each with its own header source line --
        //   BrnPropEntityModule.h:397 `bool lbHigherState`
        //   BrnPropEntityModule.h:400 `bool lbMovedToPhysical`
        //   BrnPropEntityModule.h:403 `bool lbMovedToSmashed`
        // (references/DecFIGS/dwarfdump/_compile/BrnEntityModuleUnity.cpp:31983-31996).
        // lbMovedToSmashed is INVISIBLE in both ARTIST folds because it is arithmetically
        // subsumed: E_MOVED == 5 and E_SMASHED == 6, so (old == E_MOVED && new == E_SMASHED)
        // implies (old < new) and the compiler dropped it. Written out anyway so the next
        // sweep does not "discover" a missing conjunct and re-derive all of this.
        //
        // Also from the DWARF (references/DecFIGS/dwarfdump/.../BrnPropEntityModule.h:336,
        // `bool CanChangeState(BrnWorld::EPropState, BrnWorld::EPropState);`): the method is
        // NOT const and the second parameter is leNextState, not leNewState. Both corrected.
        //
        // ⚠️ CALLERS: this predicate is NOT the ChangePropState assert. PropEntityModule_wQ_04
        // .cpp:186 currently spells that assert as `CGS_ASSERT(CanChangeState(leOldState,
        // leNewState), "leOldState < leNewState || (leOldState == E_MOVED)")`, which was
        // correct against the old weak inline and is now STRICTER than the shipped guard --
        // it would fire on an E_MOVED -> lower transition the console permits. That call site
        // must be respelled as the assert's own two-term expression. Reported to the round-2
        // fixer who owns the wQ partfiles (see scratchpad/waveQ2/worldio.owner.md).
        bool CanChangeState( EPropState leOldState, EPropState leNextState )
        {
            const bool lbHigherState     = ( leOldState < leNextState );
            const bool lbMovedToPhysical = ( leOldState == E_MOVED ) && ( leNextState == E_PHYSICAL );
            const bool lbMovedToSmashed  = ( leOldState == E_MOVED ) && ( leNextState == E_SMASHED );

            return lbHigherState || lbMovedToPhysical || lbMovedToSmashed;
        }

        // @0x822FA538. DWARF BrnPropEntityModule.cpp:1171. Drain the scene's per-frame
        // potential-contact queue and route each entry to the prop or the part handler.
        void ProcessPotentialContacts( const PropEntityIO::InputBuffer_PrePhysics* lpInput,
                                       PropEntityIO::OutputBuffer_PrePhysics* lpOutput );
        // @0x822DB038. DWARF BrnPropEntityModule.cpp:1265. NOTE: this one is ledger-attributed
        // to the BrnPropZoneManager.h catch-all TU, not to BrnPropEntityModule.cpp -- an
        // inlining artefact; it is a member of THIS class and belongs with its two siblings.
        // ⚠️ The DWARF spells the second parameter bare `EntityId`; it is
        // CgsSceneManager::EntityId, not the raw BrnCommonTypes word. MEASURED at 0x822DB0C4 /
        // 0x822DB0EC / 0x822DB224, where the body decomposes it with `srwi 24`,
        // `extrwi 14,8` and `clrlwi 22` -- literally CgsSceneManager::EntityId::GetOwner /
        // GetEntityIndex / GetPartIndex (CgsEntityId.h's KU_OWNER_BASE 24 / 14-bit entity /
        // 10-bit part) -- and then at 0x822DB230 feeds the whole word to
        // PropEntityID::PropEntityID(u32) through that class's `operator u32()`.
        void ProcessPotentialContactWithProp( PropEntityID lPropEntityId,
                                              CgsSceneManager::EntityId lContactEntityId,
                                              Vector3 lContactImpulse,
                                              BrnPhysics::Props::PropInputInterface* lpPropInput );
        // @0x822EEDA8. DWARF BrnPropEntityModule.cpp:1351.
        void ProcessPotentialContactWithPart( PropEntityID lPropEntityId,
                                              BrnPhysics::Props::PropInputInterface* lpPropInput );

        // @0x822EEFA0. DWARF BrnPropEntityModule.cpp:1430. Retire the props/parts the physics
        // side reported broken last frame: free their physical slots and re-issue the parts.
        void ProcessBrokenProps( const PropEntityIO::InputBuffer_PrePhysics* lpInput,
                                 PropEntityIO::OutputBuffer_PrePhysics* lpOutput );

        // @0x822FA890 (476 insns -- the biggest of the set). DWARF BrnPropEntityModule.cpp:1743.
        // THE SMASH RECORDER: drain the contact-spy prop-contact queue, mark each hit prop in
        // the progression bit array (PropZoneManager::RecordHitProp), and emit the outbound
        // PropEntityIO::BrokenPropEvent + PropVFXLocatorEvent that GameState's StuntManager
        // latches into a smash/billboard score.
        void ProcessContacts( const PropEntityIO::InputBuffer_PostPhysics* lpInput,
                              PropEntityIO::OutputBuffer_PostPhysics* lpOutput );

        // BrnPropEntityModule.h:412 / :420 / :427 (DWARF). The per-race-car velocity cache
        // PreSceneUpdate fills and the contact classifier reads. All three are header inlines
        // the X360 folds at every use site; the committed PreSceneUpdate body already
        // open-codes the setter's two halves (xyz verbatim, w == speed in MPH), which is what
        // attests the split between GetRaceCarVelocity (the vector) and GetRaceCarSpeed (the
        // scalar carried in the w lane -- the DWARF's `Vector3Plus`).
        // ⚠️ INFERENCE, marked: the BODIES below are the member reads/writes the declaration
        // shapes force; no out-of-line X360 symbol exists to compare against.
        void    SetRaceCarVelocity( s32 liCarIndex, Vector3 lVelocity ) { maRaceCarVelocity[liCarIndex] = lVelocity; }
        Vector3 GetRaceCarVelocity( s32 liCarIndex ) const              { return maRaceCarVelocity[liCarIndex]; }
        // ⭐ NEW 2026-08-18 (wave Q round 2). The scalar half the prose above promised and the
        // declaration list did not deliver -- DWARF BrnPropEntityModule.h:345
        //     `VecFloat GetRaceCarSpeed(int32_t) const;`
        // The fold that grounds it is MEASURED in ProcessPotentialContactWithProp
        // @0x822DB0EC..0x822DB118:
        //     extrwi r11, r25, 14,8      ; EntityId::GetEntityIndex()
        //     ... slwi 4 ; add ; lvx128 v0     ; &maRaceCarVelocity[liRaceCarIndex] (16B stride)
        //     vspltw v0, v0, 3                 ; SPLAT LANE 3 == the .w lane (big-endian VMX)
        //     stvx128 ; lfs f1                 ; -> the float argument of GetDesiredState
        // and the DWARF for that same function records the pair `GetRaceCarSpeed(...)` then
        // `rw::math::vpu::VecFloat::operator float(...)` with a local `int32_t liRaceCarIndex`
        // (_compile/BrnEntityModuleUnity.cpp:32060-32066).
        // ⚠️ TYPE DEVIATION, deliberate and marked (same shape as BrnPropConstants.h's
        // KVF_PROP_FLOOR): the DWARF return type is VecFloat, and `vspltw ...,3` really does
        // broadcast the lane to all four -- but the committed rw POD Vector4 has no
        // `operator float`, and every consumer immediately wants the scalar. Returning f32
        // here is that splat's single distinct value; nothing else about the read changes.
        // Consumers should prefer this over spelling `GetRaceCarVelocity( i ).w`: the SDK
        // reconstruction at SDKs/EATech/include/rw/math/vpu/vector3.h documents Vector3's w as
        // unused, so the raw `.w` read is structurally fragile as well as unfaithful.
        f32     GetRaceCarSpeed( s32 liCarIndex ) const                 { return maRaceCarVelocity[liCarIndex].w; }

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
                                   // ⭐ RETYPED 2026-08-18 (wave Q keystone): was `u32
                                   // luCoronaSubmissionInterface`, a console-width word standing
                                   // in for a pointer. See the note on
                                   // PropEntityIO::InputBuffer_Dispatch::mpCoronaSubmissionInterface.
                                   BrnCoronaManager::BrnSubmissionInterface*
                                       lpCoronaSubmissionInterface );                     // @0x9C

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

        // :216. ⭐ RETYPED 2026-08-18 (wave Q keystone; park P8 in
        // BrnPropEntityModule_PreScene.cpp). This was `struct { u8 maBytes[1052]; }` opaque
        // storage, on the belief that the DWARF type GuiOverheadSignInfoEvent::
        // VisibleOverheadSignArray had no committed home. IT DOES, in both halves:
        //   * DWARF BrnGuiEventTypeDefs.h:1001 --
        //         typedef Array<BrnGui::OverheadSignScore,32u> VisibleOverheadSignArray;
        //   * the element BrnGui::OverheadSignScore is committed at
        //     GameSource/Gui/BrnGuiEventTypeDefs.h:155 with its 0x20 stride static_assert'd.
        // ⚠️ THE OLD BANNER'S CONSOLE ARITHMETIC WAS WRONG and is corrected here, because it
        // is what made the type look unmodellable. The array does NOT start at 0xD1DA4 (the
        // byte after mapGraphicsLists) and is NOT 1052 bytes:
        //   OverheadSignScore leads with an alignas(16) Vector3, so ::Array<it,32> is
        //   16-ALIGNED. mapGraphicsLists ends at 0xD1DA4, which is 4 mod 16, so the compiler
        //   pads 12 bytes and the array really begins at 0xD1DB0.
        //   32 elements * 0x20 = 0x400 -> maElements spans 0xD1DB0..0xD21AF, and the count
        //   word lands at 0xD21B0 -- EXACTLY where the constructor @0x827E0488 writes the
        //   ::Array KI_UNCONSTRUCTED sentinel (`a1[215148] = -1`, 215148*4 == 0xD21B0).
        //   sizeof rounds 1028 up to the 16-alignment -> 1040, ending at 0xD21C0, which is
        //   exactly where miFramesUntilUpdateVisibleSigns sits. Every byte is accounted for;
        //   the old "1052" was the span from the pre-padding address and had 12 bytes spare.
        // Console offsets stay PROVENANCE ONLY -- the host lays this out itself.
        ::Array<BrnGui::OverheadSignScore, 32> mVisibleOverheadSigns;  // :216 console +0xD1DB0

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
        //
        // ⭐ RETYPED u32 -> s32 on 2026-08-18 (wave Q round 3). Every compare LeaveReplay makes
        // against these two counters is SIGNED, which an unsigned counter cannot produce:
        //     0x822C4834  lwz  r11, 0(r25)        ; muReplayPropsInScene
        //     0x822C4838  cmpwi cr6, r11, 0       ; SIGNED entry guard (unsigned -> cmplwi)
        //     0x822C4894  cmpw  cr6, r31, r11     ; SIGNED back-edge  (unsigned -> cmplw)
        //     0x822C48AC  cmpwi cr6, r11, 0       ; the same pair for muReplayPartsInScene
        //     0x822C4900  cmpw  cr6, r31, r11
        // No `cmplwi`/`cmplw` against either counter appears anywhere in the function. This is
        // what lets PropEntityModule_wQ_01.cpp drop the `static_cast<s32>` it carries today --
        // ⚠️ do NOT let a simplification sweep strip those casts before that file is rewritten;
        // they are correct either way.
        // ⚠️ The `mu` prefix is now a misnomer under CXX_NAMING_CONVENTIONS. Renaming to
        // mi* touches PropEntityModule_wQ_01.cpp and PropEntityModule_wQ2_01.cpp, which are
        // other lanes' files, so the rename is deliberately left as a coordinated follow-up.
        s32  muReplayPropsInScene;                           //       console +0xD3338
        s32  muReplayPartsInScene;                           //       console +0xD333C

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
