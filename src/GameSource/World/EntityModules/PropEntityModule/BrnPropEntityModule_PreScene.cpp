// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule_PreScene.cpp
//
// BrnWorld::PropEntityModule::PreSceneUpdate @0x82309A40 (2289 insns) -- THE per-frame
// entry point of the whole prop subsystem. Landed 2026-08-12 (prop-spawn wave, agent B5).
//
// Nothing else calls the prop streaming machine: until this runs, UpdateInstanceStreaming
// is never entered, no "PRP_INST_<zone>" request is ever issued, no reply is ever drained,
// and LoadZone -- the actual spawner -- is never reached. Sibling TU of
// BrnPropEntityModule.cpp (lifecycle, B1), BrnPropEntityModule_Streaming.cpp (the state
// machine, B2) and BrnPropEntityModule_Render.cpp (dispatch, B3); all four define members
// of the ONE class declared in BrnPropEntityModule.h.
//
// ---- WHAT THE FRAME DOES, IN EXECUTION ORDER --------------------------------------
//   1  latch mbResourceSystemStalled from the update set; check the replay serialiser's
//      previous frame; take the buffer locks; run the replay hook.        [hook PARKED]
//   2  copy the player index + position out of the input buffer, and normalise the eight
//      race-car velocities into maRaceCarVelocity (xyz verbatim, w = speed in MPH).
//   3  fold the three tri-state game-action latches into the module's flags, then the
//      profile round-trip / reset legs of the streaming state machine.    [copy PARKED]
//   4  drain the "prop graphics UNLOADED" queue -> drop each zone's graphics list.
//   5  drain the "prop graphics LOADED" queue   -> request each zone's "PRP_GL__<n>".
//   6  scene/sim/contact-gen removal for the props+parts recycled last frame.  [PARKED]
//   7  clear the recycled lists; on a player reset, clear the props around the reset
//      point.                                                    [the clears land; the
//                                                                 ClearPropsNearPosition
//                                                                 call is PARKED]
//   8  UpdateInstanceStreaming -- decide which zone to ask for and ask for it.
//   9  PropCellManager::Update inside the miCollisionStreamingPM monitor.      [PARKED]
//  10  BreakPropIntoParts for everything broken last frame.                    [PARKED]
//  11  drain the GameData receiver queue: type 62 == a zone's prop instances arrived
//      -> **LoadZone**, the step that actually spawns props; type 63 == a zone's prop
//      graphics list arrived -> bind it into mapGraphicsLists.
//  12  publish the frame timestep, the overhead-sign scores, release the locks.
//      [the overhead-sign append and the corona-phase advance are PARKED]
//
// ---- LAYOUT DISCIPLINE --------------------------------------------------------------
// Not one console byte offset appears in the code below; every access is by named member
// on both sides (module and IO buffer). The offset -> member decode used to READ the asm
// is recorded per section. The recurring project bug -- transcribing a console offset or
// stride onto the x64 host -- cannot bite here because nothing is indexed by a constant.
//
// ---- THE OFFSET -> MEMBER DECODE (console; PROVENANCE ONLY) -------------------------
// Module (r16/a1 in the listing; every one of these was read off the ASSEMBLY):
//   +0xCD970 mReceiverQueue          +0xCDD88 mpPropPhysicsDataHeader
//   +0xCDDA8 mVFXPropCollection      +0xCDDD0 mLastPlayerResetPosition
//   +0xCDDE0 mbPlayerWasJustReset    +0xCDDE4 maRecentlyBrokenProps (count +0xCDE64)
//   +0xCDE68 maRecentlyRecycledProps (count +0xCDEA4)
//   +0xCDEA8 maRecentlyRecycledParts (count +0xCDF20)
//   +0xCDF24 mapGraphicsLists[500]   (stride 0x20: `slwi r11,zone,5` + 0xD0000-0x20DC)
//   +0xD1DA4 mVisibleOverheadSigns   +0xD3180 mPropEntitySerialiser
//   +0xD3200 meStreamingMode         +0xD320C muNumberOfLoadedZones
//   +0xD3210 mu8PlayerIndex          +0xD3220 mPlayerPosition
//   +0xD3230 maRaceCarVelocity[8]
//   +0xD32B0 mabWaitingForGraphics   (`8*(zone>>6 + 0x1A656)`  == 0xD32B0)
//   +0xD32F0 mabWaitingForInstances  (`8*(zone>>6 + 0x1A65E)`  == 0xD32F0)
//   +0xD33A0 mabLoadedWorldGraphics  (`8*(zone>>6 + 0x1A674)`  == 0xD33A0)
//   +0xD3340/41/42/43/44/45  mbCurrentlyOnline / mbEasySmashProps /
//                            mbAllowPropProgression / mbPlayerCrashing /
//                            mbPlayerWrecked / mbResourceSystemStalled
//   +0xD3360 miCollisionStreamingPM  +0xD3364 miLoadingPM     +0xD3378 mrTimestep
//   +0x280   mZoneManager   (its mauStartIndexOfZone[] at module+836930 + 2*zone)
// ⚠ The three 64-byte bit arrays sit 64 bytes apart and Hex-Rays prints all three bases
// as `8 * (idx + <constant>)`, so they are trivially confusable. Each one above was
// re-derived from the `addis/addi/slwi/ldx` triple in the ASSEMBLY, not the pseudocode:
// mabWaitingForGraphics is the one this function clears in step 4 and sets in step 5,
// mabLoadedWorldGraphics the one it asserts on and sets in step 5, and
// mabWaitingForInstances the one the type-62 reply clears in step 11.
//
// Input buffer (r29/a4): see BrnPropEntityModuleIO.h -- +0x6F0 mPlayerPos,
// +0x700..0x77F maRaceCarVelocity[8], +0x780 mpabHitPropBitArray, +0x784
// mfCurrentTimeStep, +0x788 miPlayerZoneNumber, +0x78C mu8PlayerCarIndex,
// +0x78D/E/F the three tri-state latches, +0x790/91/92/93/94 the five bools.
//
// ---- PARK LIST (no body here; the precise blocker for each) -------------------------
// Every park below is a MISSING DECLARATION in a header this lane must not edit
// (BrnPropZoneManager.h / BrnPropCellManager.h / BrnPropEntityModule.h are all on the
// do-not-touch list), not a missing decode. The decode for each is written out beside
// it so the follow-up is mechanical.
//
//  P1. step 1 -- `ReplayPreSceneUpdate( lpInput, lpOutput, lUpdateSet )` @0x822EF878.
//      ASM: `mr r6,r31(updateSet); mr r5,r30(lpOutput); mr r4,r29(lpInput); mr r3,r16;
//      bl ReplayPreSceneUpdate`. BLOCKER: the method is X360-only and is NOT declared in
//      the committed BrnPropEntityModule.h (which declares only PrepareForReplay /
//      RestoreFromReplay / LeaveReplay), and this lane may make only the two agreed
//      narrow edits to that header. BOOT-SAFE: 0x822EF878's first act is
//      `if ( (lUpdateSet>>8 & 1) != mbInReplay )`, and everything past it is gated on
//      `mPropEntitySerialiser.IsPlaying()`, so a non-replay boot frame does nothing.
//      MOUNT: add `void ReplayPreSceneUpdate( PropEntityIO::InputBuffer_PreScene*,
//      PropEntityIO::OutputBuffer_PreScene*, BrnUpdateSet );` and call it here.
//
//  P2. step 3 -- the profile round-trip's hit-props install. ASM:
//      `memcpy( module+0xC3200, *(lpInput+0x780), 37504 )` where module+0xC3200 is
//      mZoneManager.maPreviouslyHitProps and 37504 == sizeof(BitArray<300000>). The
//      original is PropZoneManager::SetHitPropBitArray (DWARF BrnPropZoneManager.h:171)
//      inlined. BLOCKER: `maPreviouslyHitProps` is PRIVATE in the committed
//      BrnPropZoneManager.h and there is no `SetHitPropBitArray` declaration and no
//      `friend class PropEntityModule`. MOUNT: add
//      `void SetHitPropBitArray( const HitPropsBitArray& );` to PropZoneManager and call
//      `mZoneManager.SetHitPropBitArray( lpInput->GetHitPropsBitArray() );` here.
//      The two asserts and the `meStreamingMode = E_STREAM` that bracket it DO land.
//
//  P3. step 6 -- the recycled-prop / recycled-part scene, sim and contact-generation
//      removal legs. The decode is complete: for each id in maRecentlyRecycledProps,
//      `lpProp = mZoneManager.GetProp( lPropEntityId )`,
//      `lpType = mpPropPhysicsDataHeader->GetType( lpProp->GetTypeId() )`, assert the
//      prop is not smashed, and -- unless its state is 4 -- call
//      RemovePropFromContactGeneration when `mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT` and
//      RemovePropFromScene when `mu8Flags & KU_ADDED_TO_SCENE_BIT`; then the same shape
//      for maRecentlyRecycledParts with RemovePropPartsFromSimIfPhysical +
//      RemovePropParts{FromContactGeneration,FromScene} and the mirrored asserts.
//      BLOCKER: `PropZoneManager::GetProp( PropEntityID )` @0x822CDA28 -- the
//      global-pool-index overload whose tripwires are BrnPropZoneManager.h:534..541 --
//      is NOT declared in the committed header (only the `GetProp(u16,u32)` zone+index
//      overload is), and PropCellManager is reached through the PRIVATE
//      PropZoneManager::mCellManager. MOUNT: add that GetProp overload plus
//      PropZoneManager forwarders for the four Remove* calls (the header already has the
//      sibling forwarder block at BrnPropZoneManager.h:232).
//      SCOPE NOTE: both lists are empty until a prop is smashed or a zone unloads, so
//      nothing on the spawn-and-render path runs through here. The two `Clear()`s that
//      terminate the loops DO land below, so the lists cannot grow unbounded.
//
//  P4. step 7 -- `mZoneManager.mCellManager.ClearPropsNearPosition( mLastPlayerResetPosition,
//      KVF_MIN_DIST_FROM_PLAYER, mpPropPhysicsDataHeader.GetMemoryResource(),
//      lpOutput->GetPropInputInterface(), lpOutput->GetSceneInputInterface(),
//      &mPropEntitySerialiser, mZoneManager.mauStartIndexOfZone )` (X360 @0x822E1600, and
//      the committed PropCellManager declaration matches that argument list exactly).
//      BLOCKER: `mCellManager` and `mauStartIndexOfZone` are both PRIVATE in
//      BrnPropZoneManager.h with no PropEntityModule friendship. MOUNT: a
//      `PropZoneManager::ClearPropsNearPosition(...)` forwarder. The `mbPlayerWasJustReset`
//      test and its clear DO land below (so the latch cannot stick).
//
//  P5. step 9 -- `PropCellManager::Update( mPlayerPosition, types, &maRecentlyBrokenProps,
//      lpOutput, mbInReplay, &mPropEntitySerialiser, mZoneManager.mauStartIndexOfZone )`
//      (X360 @0x822FC8F0, r3 == module+0x280 == the zone manager, whose mCellManager is
//      at offset 0), bracketed by PerfMonCpu Start/StopMonitor(miCollisionStreamingPM).
//      Same BLOCKER as P4 (private mCellManager + mauStartIndexOfZone); note also that
//      the committed PropCellManager::Update signature has no slot for the console's
//      `mbInReplay` r7 argument. MOUNT: a `PropZoneManager::Update(...)` forwarder.
//      SCOPE NOTE: cells only govern CONTACT GENERATION -- PropZoneManager::LoadZone ->
//      LoadProp already adds each prop to the SCENE as it spawns -- so props still spawn
//      and still render without this; they are simply not collidable. Breakable/physical
//      props are out of scope for this wave, which is why this is an acceptable park.
//
//  P6. step 10 -- the `BreakPropIntoParts` sweep over maRecentlyBrokenProps. Explicitly
//      out of scope (breakable props), and BreakPropIntoParts is itself PARKED by B2 in
//      BrnPropEntityModule_Streaming.cpp, so calling it would only bind a trap stub.
//
//  P7. step 11, type 63 -- the `luZone == Nicotine::DMixIO::GetDMixID( lGraphicsList )`
//      cross-check assert. BLOCKER: `Nicotine::DMixIO` has no committed home anywhere in
//      b5-decomp/src. It is an ASSERT ONLY; the binding it guards (step 11's
//      `mapGraphicsLists[luZone] = lGraphicsList`) lands in full.
//
//  P8. step 12 -- `Array<BrnGui::OverheadSignScore,32>::AppendArray<32>(
//      lpOutput->GetVisibleOverheadSignArray(), &mVisibleOverheadSigns )` (X360
//      @0x822E5348). BLOCKER: BOTH ends are opaque byte storage --
//      PropEntityModule::mVisibleOverheadSigns is a
//      `VisibleOverheadSignArrayStorage { u8 maBytes[1052]; }` in BrnPropEntityModule.h
//      and OutputBuffer_PreScene::mVisibleOverheadSignArray is a 1-byte placeholder --
//      so there is no typed array to append. The element type DOES exist
//      (BrnGui::OverheadSignScore, GameSource/Gui/BrnGuiEventTypeDefs.h, sizeof 0x20).
//      MOUNT: retype the module member to `CgsContainers::Array<BrnGui::OverheadSignScore,32>`
//      (that is an edit to BrnPropEntityModule.h beyond this lane's two agreed ones); the
//      output-buffer side is in this lane's file and can follow in the same change.
//
//  P9. step 12 -- the corona-phase advance that follows the timestep publish:
//      `if ( (lUpdateSet & 1) == 0 || mPropEntitySerialiser.IsPlaying() )` then, for each
//      of mVFXPropCollection->muCoronaDataTableSize entries (stride 0x20),
//      `phase = phase + mrTimestep; period = e[3] + e[2]; e[6] = phase - trunc(phase/period)*period`
//      -- a fmod wrap of each corona's animation phase (assert text
//      "luOffset < muCoronaDataTableSize", SharedClasses/Graphics/VFXPropsResourceType.h:636).
//      BLOCKER: `BrnParticle::VFXPropCollection` is only FORWARD-DECLARED in-tree; the
//      committed SharedClasses/Graphics/VFXPropsResourceType.h homes only the resource-type
//      HANDLER (VFXPropCollectionResourceType), not the resource payload with the corona
//      data table. Reproducing it would mean poking `*(f32*)(entry + 24)` into an unrecovered
//      blob -- exactly the offset hack the project forbids. MOUNT: reconstruct
//      BrnParticle::VFXPropCollection (muCoronaDataTableSize + the corona entry record).
//      It is a visual-only phase wrap; props spawn and render without it.
//
// ---- WHAT IS STILL MISSING FOR PROPS TO ACTUALLY APPEAR (report to the conductor) ----
// This function is now real, but its INPUT is not: both producers of
// PropEntityIO::InputBuffer_PreScene are still inert gates in WorldLinkStubs.cpp --
//   WorldModule::BridgeWorldModuleToPropModule_PreScene  @0x827AACF8  (fills the
//       instances-needed queue and the two graphics loaded/unloaded queues), and
//   WorldModule::BridgeRaceCarModuleToPropModule_PreScene @0x827A5510 (fills the player
//       position/zone/index and the eight race-car velocities)
// -- both called from BrnWorldModule.cpp:2016/2021. With them inert every queue stays at
// length 0 and miPlayerZoneNumber stays 0, so the streaming machine has nothing to ask
// for. Also note IOBufferStack::CreateIOBuffer<T> (CgsIOBufferStack.h:25) placement-news
// `T()` and never calls `T::Construct()` the way the X360 template does; the buffer is
// value-initialised so the queues read as empty (safe), but miPlayerZoneNumber will read
// 0 rather than the -1 "no zone" sentinel until Construct is wired in.
// ============================================================================

#include "BrnPropEntityModule.h"

#include "BrnPropEntityModuleIO.h"   // PropEntityIO::InputBuffer_PreScene / OutputBuffer_PreScene
#include "BrnPropZoneManager.h"      // PropZoneManager, KU_MAX_ZONES

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress / CgsIDUnCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // NULLResourceHandle
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"               // GetGameDataEvent
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"         // RequestInterface<1024>
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h" // PropEntitySerialiser
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"           // PropZoneData::GetZoneId
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"         // PropPhysicsDataHeader

#include <cstdlib>   // atoi  (the X360 calls the CRT's, @0x82C0B868)
#include <cmath>     // sqrtf (the vrsqrtefp + two Newton refinements, lowered)

namespace BrnWorld
{
    namespace
    {
        // ------------------------------------------------------------------
        // The pre-scene output buffer models its interface members as opaque sized spans
        // (BrnPropEntityModuleIO.h owns that decision). Re-type the handle exactly the way
        // BrnPropEntityModule.cpp / BrnPropEntityModule_Streaming.cpp already do -- same
        // idiom, same reason.
        // ------------------------------------------------------------------
        BrnResource::GameDataIO::RequestInterface<1024>*
        GetGameDataRequestInterface( PropEntityIO::OutputBuffer_PreScene* lpOutput )
        {
            return reinterpret_cast<BrnResource::GameDataIO::RequestInterface<1024>*>(
                lpOutput->GetResourceRequestInterface() );
        }

        // ------------------------------------------------------------------
        // RequestInterface<1024>::GetPropGraphicsList (DWARF BrnGameDataRequestQueue.h:231)
        // -- the X360 INLINED it into PreSceneUpdate rather than emitting it out of line,
        // so it is de-inlined here as a file-local helper instead of grown into another
        // lane's BrnGameDataRequestQueue.h. It is instruction-for-instruction the sibling
        // of the committed RequestInterface<1024>::GetPropInstances @0x82303E08, with the
        // format string and the asset set changed:
        //     SPrintf( name, 13, "PRP_GL__%d", zone )     0x8230A164 (r4 = 0xD, r5 = fmt)
        //     miEventId       = 0                         stw r30, var_D0
        //     mpReceiverQueue = &mReceiverQueue           stw r21, var_CC  (r21 == 0xCD970)
        //     miPoolId        = 3                         stw 3,   var_C8
        //     mId             = CgsIDCompress( name )     std r3,  var_C0
        //     meType          = 0 == E_ASSETSET_GRAPHICS  stw r30, var_B8
        //     mbFailFlag      = false                     stb r30, var_B4
        //     AddEvent<GetGameDataEvent>( &event, 49 )    li r5, 0x31
        // MOUNT (optional tidy): promote this to RequestInterface<N>::GetPropGraphicsList
        // in BrnGameDataRequestQueueImpl.h + the <1024> instance TU, and delete it here.
        // ------------------------------------------------------------------
        const s32 KI_PROP_GRAPHICS_LIST_EVENT_ID   = 0;
        const s32 KI_PROP_GRAPHICS_LIST_POOL_ID    = 3;
        const s32 KI_GET_GAME_DATA_EVENT_TYPE      = 49;

        bool RequestPropGraphicsList( BrnResource::GameDataIO::RequestInterface<1024>* lpRequestInterface,
                                      CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                                      u32 luZoneId )
        {
            char lacName[KI_CGSID_STRING_LEN];
            CgsCore::SPrintf( lacName, KI_CGSID_STRING_LEN, "PRP_GL__%d", luZoneId );

            BrnResource::GameDataIO::GetGameDataEvent lEvent;
            lEvent.miEventId       = KI_PROP_GRAPHICS_LIST_EVENT_ID;
            lEvent.mpReceiverQueue = lpReceiverQueue;
            lEvent.miPoolId        = KI_PROP_GRAPHICS_LIST_POOL_ID;
            lEvent.mId             = CgsIDCompress( lacName );
            lEvent.meType          = BrnResource::E_ASSETSET_GRAPHICS;
            lEvent.mbFailFlag      = false;

            return lpRequestInterface->mRequestQueue.AddEvent( &lEvent, KI_GET_GAME_DATA_EVENT_TYPE );
        }

        // ---- receiver-queue reply types (the two `cmpwi r3, 0x3E / 0x3F` arms) --------
        const s32 KI_EVENT_PROP_INSTANCES_LOADED    = 62;   // "PRP_INST_<zone>" arrived
        const s32 KI_EVENT_PROP_GRAPHICS_LIST_LOADED = 63;  // "PRP_GL__<zone>"  arrived
        const s32 KI_EVENT_QUEUE_EMPTY               = -1;

        // The two id prefixes the replies carry, and therefore where atoi starts reading
        // the zone number back out of the uncompressed CgsID:
        //   type 62 -> "PRP_INST_"  (9 chars)   asm `addi r3, buf, 9`
        //   type 63 -> "PRP_GL__"   (8 chars)   asm `addi r3, buf, 8`
        const s32 KI_PRP_INST_PREFIX_LEN = 9;
        const s32 KI_PRP_GL_PREFIX_LEN   = 8;

        // BrnUpdateSet bit 10. ASM 0x82309A5C `extrwi r10,r31,6,16 ; clrlwi r10,r10,31`
        // extracts exactly bit 0x400 of the update set into mbResourceSystemStalled. The
        // shared BrnUpdateSet header (SharedClasses/BrnSharedConstants.h) types the mask
        // but does not yet name its bits, so the one bit this TU reads is named here;
        // fold it into that header when its bit list lands.
        const BrnUpdateSet KU_UPDATESET_RESOURCE_SYSTEM_STALLED = 0x400;

        // The eight per-car velocity slots the X360 loop walks (`li r9, 8`, stride 16).
        const s32 KI_NUM_RACE_CAR_VELOCITIES = 8;

        // metres/second -> miles/hour. The X360 bakes it as flt_82014648 == 2.2369363f and
        // splats it across the register before the multiply.
        const f32 KF_METRES_PER_SECOND_TO_MPH = 2.2369363f;

        // BrnPhysics::Props::KU_MAX_LOADED_ZONES -- the prop-pool slot count. Pinned by
        // the type-62 arm's `cmpwi muNumberOfLoadedZones, 9` gate (and by LoadZone's own
        // "muZonesLoaded <= BrnPhysics::Props::KU_MAX_LOADED_ZONES" tripwire).
        const u32 KU_MAX_LOADED_ZONES = 9;

        // ---- value tripwires for the facts this TU relies on -------------------------
        static_assert( KU_MAX_ZONES == 500,
                       "the 0x1F4 bound every zone-index tripwire in this TU quotes" );
        static_assert( KU_NUM_ZONE_SLOTS == KU_MAX_LOADED_ZONES,
                       "one prop-pool slot per simultaneously-loaded zone" );
        static_assert( KI_CGSID_STRING_LEN == 13,
                       "the literal 13 SPrintf's buffer size argument carries (li r4, 0xD)" );
    }

    // ========================================================================
    // PropEntityModule::PreSceneUpdate   @ 0x82309A40   (2289 insns)
    // ========================================================================
    void PropEntityModule::PreSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                           CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                           PropEntityIO::InputBuffer_PreScene* lpInput,
                                           PropEntityIO::OutputBuffer_PreScene* lpOutput,
                                           BrnUpdateSet lUpdateSet )
    {
        // ================================================================
        // [1] frame set-up.
        // ASM 0x82309A5C: `extrwi r10,r31,6,16 ; clrlwi r10,r10,31` == bit 10 of the
        // update set (0x400); stored to module+0xD3345 BEFORE the serialiser call.
        // ================================================================
        mbResourceSystemStalled = ( lUpdateSet & KU_UPDATESET_RESOURCE_SYSTEM_STALLED ) != 0;

        mPropEntitySerialiser.CheckPreviousFrameCleared();   // r3 = module+0xD3180

        lpOutput->LockForWrite();
        lpInput->LockForRead();

        // ⭐ 2026-08-12 (prop-BOOT wave, agent B8) -- READ HANDLE FOR THE INPUT BUFFER.
        // The input buffer is READ-locked on the line above, so every one of its three queue
        // reads below must go through the CONST getter (which tests the read-lock bit, X360
        // 0x822B90A8 / 0x822B9150 / 0x822B91F8 -- the three the console's PreSceneUpdate
        // actually calls). Spelling them on the non-const `lpInput` picked the WRITE-lock
        // overloads (0x827A1380 / 0x827A1428 / 0x827A14D0, which belong to the world->prop
        // BRIDGE, not to this consumer) and fired a "Not locked for writing" tripwire per
        // queue per frame -- 33 of the boot's 35 asserts. Same idiom as
        // BrnWorldModule.cpp's `lpWorldEntityRead` in the prepare bridge.
        const PropEntityIO::InputBuffer_PreScene* lpInputRead = lpInput;

        // P1: ReplayPreSceneUpdate( lpInput, lpOutput, lUpdateSet ) -- see the PARK LIST.

        // DIAGNOSTIC (prop-spawn wave 2026-08-12) -- NOT in the X360 binary. Deliberately
        // NOT gated on gxMessageFilterFlags (bit 0 is off in a default PC boot), so a boot
        // log always proves whether the prop subsystem is ticking at all.
        // Boot-log grep: "PROPS-BOOT PreSceneUpdate".
        {
            static bool sbLoggedFirstTick = false;
            if ( !sbLoggedFirstTick && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstTick = true;
                *CgsDev::Log::gpDebugPrint
                    << "PROPS-BOOT PreSceneUpdate first tick: streamingMode "
                    << static_cast<s32>( meStreamingMode )
                    << " loadedZones " << muNumberOfLoadedZones
                    << " playerZone " << lpInput->GetPlayerZoneNumber() << "\n";
            }
        }

        // ================================================================
        // [2] player index / position, and the eight race-car velocities.
        //
        // The velocity loop is the one piece of real SIMD in the function. Per car:
        //   v0  = lpInput->maRaceCarVelocity[i]              lvx128 v0, r0, r8
        //   v0' = vmsum3fp128(v0,v0)                          == dot3(v,v), splatted
        //   v13 = vrsqrtefp(v0') + TWO Newton refinements     (the vnmsubfp/vmaddfp pairs;
        //         note IDA prints vmaddfp operands in raw encoding order vD,vA,vB,vC while
        //         the semantics are vD = vA*vC + vB -- read that way both pairs are the
        //         textbook e' = e + 0.5*e*(1 - dot*e*e))
        //   len = v0' * v13                                   == sqrt(dot)
        //   len = vsel(len, 0, vcmpeqfp(0, v0'))              == 0 when the input is zero
        //   w   = len * 2.2369363                             m/s -> mph
        //   vrlimi128 v7, <w>, 1, 0                           insert into the W lane only
        // The xyz lanes are the input's, verbatim. (The compiler also emits an earlier,
        // immediately-overwritten `stvx128 v7` that keeps the OLD w lane -- a dead
        // intermediate store, not reproduced.)
        // ================================================================
        mu8PlayerIndex  = lpInput->GetPlayerCarIndex();
        mPlayerPosition = lpInput->GetPlayerPosition();

        for ( s32 liCar = 0; liCar < KI_NUM_RACE_CAR_VELOCITIES; ++liCar )
        {
            const Vector3 lVelocity = lpInput->GetRaceCarVelocity( liCar );

            const f32 lfSpeedSquared = lVelocity.x * lVelocity.x
                                     + lVelocity.y * lVelocity.y
                                     + lVelocity.z * lVelocity.z;

            // vsel on vcmpeqfp: a zero-length vector yields a zero speed rather than the
            // 0 * inf the reciprocal estimate would otherwise produce.
            const f32 lfSpeed = ( lfSpeedSquared == 0.0f )
                                ? 0.0f
                                : lfSpeedSquared * ( 1.0f / sqrtf( lfSpeedSquared ) );

            Vector3& lrStored = maRaceCarVelocity[liCar];
            lrStored.x = lVelocity.x;
            lrStored.y = lVelocity.y;
            lrStored.z = lVelocity.z;
            // The W ("plus") lane of the DWARF's Vector3Plus: the car's speed in MPH.
            lrStored.w = lfSpeed * KF_METRES_PER_SECOND_TO_MPH;
        }

        // ================================================================
        // [3] the game-action latches.
        //
        // Each of the three is a PAIR of tests, not a copy: the input's status byte is a
        // tri-state seeded to E_CHANGESTATUS_NO_CHANGE by InputBuffer_PreScene::Construct,
        // and only 0/1 mean "the game just changed this". State 2 leaves the module's flag
        // exactly as it was. ASM 0x82309F00-ish: `lbz r11,0x78D(r29) ; cmplwi 1 ; bne ...
        // stb 1,0xD3340(r16)` immediately followed by `cmplwi 0 ; bne ... stb 0,...`.
        // ================================================================
        if ( lpInput->HasJustChangedToOnline() )        { mbCurrentlyOnline      = true;  }
        if ( lpInput->HasJustChangedToOffline() )       { mbCurrentlyOnline      = false; }
        if ( lpInput->HasJustChangedToEasySmashOn() )   { mbEasySmashProps       = true;  }
        if ( lpInput->HasJustChangedToEasySmashOff() )  { mbEasySmashProps       = false; }
        if ( lpInput->HasPropProgressionBeenEnabled() ) { mbAllowPropProgression = true;  }
        if ( lpInput->HasPropProgressionBeenDisabled() ){ mbAllowPropProgression = false; }

        // The profile came back: install its hit-props and resume streaming.
        if ( lpInput->IsReloadingProfile() )
        {
            CGS_ASSERT( meStreamingMode == E_WAITING_FOR_PROFILE_DATA,
                        "meStreamingMode == E_WAITING_FOR_PROFILE_DATA" );        // cpp:512
            // The X360's second tripwire here, "mpabHitPropBitArray != NULL"
            // (BrnPropEntityModuleIO.h:848), is the input buffer's OWN assert inside
            // GetHitPropsBitArray(); it fires from there once P2 calls the getter.

            // P2: mZoneManager.SetHitPropBitArray( lpInput->GetHitPropsBitArray() );
            //     -- see the PARK LIST. The state transition below still lands, so the
            //     machine does not stall in E_WAITING_FOR_PROFILE_DATA.

            meStreamingMode = E_STREAM;
        }

        mbPlayerCrashing = lpInput->IsPlayerCrashing();
        mbPlayerWrecked  = lpInput->IsPlayerWrecked();

        // The debug menu / a mode change asked for a full reset. This is
        // PropEntityModule::ResetProps() inlined (it stores E_RESET_UNLOADING and nothing
        // else -- see its banner in BrnPropEntityModule_Streaming.cpp).
        if ( lpInput->ShouldResetProps() )
        {
            ResetProps();
        }

        // We are about to hand the profile our prop progression: drop everything first.
        if ( lpInput->IsSendingPropProgression() )
        {
            CGS_ASSERT( meStreamingMode != E_RESET_UNLOADING_FOR_PROFILE,
                        "meStreamingMode != E_RESET_UNLOADING_FOR_PROFILE" );      // cpp:528
            CGS_ASSERT( meStreamingMode != E_WAITING_FOR_PROFILE_DATA,
                        "meStreamingMode != E_WAITING_FOR_PROFILE_DATA" );         // cpp:529
            meStreamingMode = E_RESET_UNLOADING_FOR_PROFILE;
        }

        // ================================================================
        // [4] the world module says a zone's prop graphics went away.
        // Queue accessor: X360 0x822B9150 == GetPropGraphicsUnloadedQueue() const.
        // Per zone: stop waiting for it, drop the graphics-list resource pointer, then
        // (with a tripwire) mark the world graphics no longer resident.
        // ================================================================
        {
            const PropEntityIO::InputBuffer_PreScene::PropGraphicsUnloadedQueue* lpUnloaded =
                lpInputRead->GetPropGraphicsUnloadedQueue();
            const s32 liNumUnloaded = lpUnloaded->GetLength();

            for ( s32 liEvent = 0; liEvent < liNumUnloaded; ++liEvent )
            {
                const u32 luZone = lpUnloaded->GetEvent( liEvent ).muPropGraphicsId;
                CGS_ASSERT( luZone < KU_MAX_ZONES,
                            "luZone < BrnPhysics::Props::KU_MAX_ZONES" );          // cpp:549

                mabWaitingForGraphics.UnSetBit( luZone );
                mapGraphicsLists[luZone] = CgsResource::NULLResourceHandle;

                CGS_ASSERT( mabLoadedWorldGraphics.IsBitSet( luZone ),
                            "Graphics not loaded\n" );                             // cpp:559
                mabLoadedWorldGraphics.UnSetBit( luZone );
            }
        }

        // ================================================================
        // [5] the world module says a zone's prop graphics are resident -- go and fetch
        // that zone's prop graphics LIST ("PRP_GL__<n>") from the GameData module.
        // Queue accessor: X360 0x822B90A8 == GetPropGraphicsLoadedQueue() const.
        // ================================================================
        {
            const PropEntityIO::InputBuffer_PreScene::PropGraphicsLoadedQueue* lpLoaded =
                lpInputRead->GetPropGraphicsLoadedQueue();
            const s32 liNumLoaded = lpLoaded->GetLength();

            for ( s32 liEvent = 0; liEvent < liNumLoaded; ++liEvent )
            {
                const u32 luZone = lpLoaded->GetEvent( liEvent ).muPropGraphicsId;

                RequestPropGraphicsList( GetGameDataRequestInterface( lpOutput ),
                                         &mReceiverQueue,
                                         luZone );

                CGS_ASSERT( luZone < KU_MAX_ZONES, "luIndex < NUMBITS" );  // CgsBitArray.h:222
                mabWaitingForGraphics.SetBit( luZone );

                CGS_ASSERT( !mabLoadedWorldGraphics.IsBitSet( luZone ),
                            "!mabLoadedWorldGraphics.IsBitSet( luZone )" );        // cpp:580
                mabLoadedWorldGraphics.SetBit( luZone );

                // DIAGNOSTIC -- NOT in the X360 binary. Ungated one-shot.
                // Boot-log grep: "PROPS-BOOT first graphics-list request".
                {
                    static bool sbLoggedFirstGraphicsRequest = false;
                    if ( !sbLoggedFirstGraphicsRequest && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        sbLoggedFirstGraphicsRequest = true;
                        *CgsDev::Log::gpDebugPrint
                            << "PROPS-BOOT first graphics-list request: zone " << luZone << "\n";
                    }
                }
            }
        }

        // ================================================================
        // [6] scene / sim / contact-generation removal for the props and parts recycled
        // last frame -- PARKED (P3). Only the two list clears that TERMINATE those loops
        // land, so the fixed-capacity lists cannot overflow while P3 is outstanding.
        // ASM: `stw r30, 0xCDEA4(module)` and `stw r30, 0xCDF20(module)` -- the two
        // Array<PropEntityID,N> count words.
        // ================================================================
        maRecentlyRecycledProps.Clear();
        maRecentlyRecycledParts.Clear();

        // ================================================================
        // [7] the player was teleported/reset: sweep the props out from under them.
        // The ClearPropsNearPosition call is PARKED (P4); the latch handling is faithful,
        // so the flag cannot stick set.
        // ================================================================
        if ( mbPlayerWasJustReset )
        {
            // P4: mZoneManager.ClearPropsNearPosition( mLastPlayerResetPosition, ... );
            mbPlayerWasJustReset = false;
        }

        // ================================================================
        // [8] THE streaming step -- decide which zone to ask for, and ask for it.
        // ASM 0x8230BCxx: `lwz r30, 0x788(a4)` (the player's zone, -1 when invalid),
        // `bl sub_822B91F8` (the instances-needed queue), then
        // `UpdateInstanceStreaming( this, queue, zone, lpOutput )`.
        // ================================================================
        UpdateInstanceStreaming( lpInputRead->GetPropInstancesNeededForZoneQueue(),
                                 lpInput->GetPlayerZoneNumber(),
                                 lpOutput );

        // ================================================================
        // [9] PropCellManager::Update inside miCollisionStreamingPM -- PARKED (P5).
        // [10] the BreakPropIntoParts sweep                          -- PARKED (P6).
        // ================================================================

        // ================================================================
        // [11] drain the GameData receiver queue.
        //
        // Record shape (BaseEventReceiverQueue): [s32 type][s32 size][payload]. The X360
        // inlines GetFirstEvent as `lwz miStartOffset ; lwz mpBuffer ; add ; addi r16,8`
        // and then loops on GetNextEvent @0x82204690 until the type is -1.
        //
        // Both arms recover the zone number the same way: the reply's CgsID is expanded
        // back to text and the numeric suffix is atoi'd -- `CgsIDUnCompress( event->mId,
        // buf ) ; atoi( buf + <prefix> )`. The reply's resource handle is the event's
        // GameDataAssetEvent::mHandle.
        // ================================================================
        {
            const CgsModule::Event* lpEventData = 0;
            s32 liEventSize = 0;
            s32 liEventType = mReceiverQueue.GetFirstEvent( &lpEventData, &liEventSize );

            while ( liEventType != KI_EVENT_QUEUE_EMPTY )
            {
                const BrnResource::GameDataIO::GameDataAssetEvent* lpAssetEvent =
                    static_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>( lpEventData );

                if ( liEventType == KI_EVENT_PROP_INSTANCES_LOADED )
                {
                    // ---- "PRP_INST_<zone>" came back: SPAWN THE ZONE ----------------
                    char lacName[KI_CGSID_STRING_LEN];
                    CgsIDUnCompress( lpAssetEvent->mId, lacName );
                    const u32 luZone =
                        static_cast<u32>( atoi( lacName + KI_PRP_INST_PREFIX_LEN ) );

                    CGS_ASSERT( luZone < KU_MAX_ZONES, "luIndex < NUMBITS" );  // CgsBitArray.h:203
                    CGS_ASSERT( mabWaitingForInstances.IsBitSet( luZone ),
                                "mabWaitingForInstances.IsBitSet( luZoneIndex )" );  // cpp:720
                    // The zone must NOT already be resident. The X360 spells this as
                    // IsZoneLoaded @0x822A4390 inlined -- it reads the zone manager's
                    // start-index sentinel directly (`lhz 2*(zone + 0x662A1)(module) ;
                    // cmplwi 0xFFFF`) behind that function's own
                    // "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES" bounds tripwire
                    // (BrnPropZoneManager.h:645), then fires a formatted "<zone>\n" at
                    // cpp:721. Spelled by name here because mauStartIndexOfZone is private
                    // to the zone manager.
                    CGS_ASSERT( !mZoneManager.IsZoneLoaded( static_cast<u16>( luZone ) ),
                                "!mZoneManager.IsZoneLoaded( luZoneIndex )" );       // cpp:721

                    if ( !mbResourceSystemStalled && muNumberOfLoadedZones != KU_MAX_LOADED_ZONES )
                    {
                        CgsResource::ResourcePtr<PropZoneData> lZoneData( lpAssetEvent->mHandle );

                        if ( !mbResourceSystemStalled )
                        {
                            CGS_ASSERT( luZone == lZoneData->GetZoneId(),
                                        "luZoneIndex == lpZoneData->GetZoneId()" );  // cpp:727
                        }

                        CgsDev::PerfMonCpu::StartMonitor( miLoadingPM );
                        LoadZone( lZoneData.GetMemoryResource(), lpOutput );
                        CgsDev::PerfMonCpu::StopMonitor( miLoadingPM );

                        // DIAGNOSTIC -- NOT in the X360 binary. Ungated one-shot. THIS is
                        // the line that proves a zone's props actually loaded: if it never
                        // appears, nothing downstream can possibly render a prop.
                        // Boot-log grep: "PROPS-BOOT prop instances arrived".
                        {
                            static bool sbLoggedFirstInstances = false;
                            if ( !sbLoggedFirstInstances && CgsDev::Log::gpDebugPrint != 0 )
                            {
                                sbLoggedFirstInstances = true;
                                *CgsDev::Log::gpDebugPrint
                                    << "PROPS-BOOT prop instances arrived: zone " << luZone
                                    << " -> LoadZone done, loadedZones now "
                                    << muNumberOfLoadedZones << "\n";
                            }
                        }
                    }

                    // Cleared UNCONDITIONALLY -- outside the stalled / pool-full gate, so
                    // a dropped reply does not wedge the zone as "requested for ever".
                    mabWaitingForInstances.UnSetBit( luZone );
                }
                else if ( liEventType == KI_EVENT_PROP_GRAPHICS_LIST_LOADED )
                {
                    // ---- "PRP_GL__<zone>" came back: bind the zone's graphics list ----
                    char lacName[KI_CGSID_STRING_LEN];
                    CgsIDUnCompress( lpAssetEvent->mId, lacName );
                    const u32 luZone =
                        static_cast<u32>( atoi( lacName + KI_PRP_GL_PREFIX_LEN ) );

                    CGS_ASSERT( luZone < KU_MAX_ZONES,
                                "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES" );   // cpp:697

                    // The reply is only interesting if we are still waiting for it (the
                    // zone can have been unloaded again while the fetch was in flight).
                    if ( mabWaitingForGraphics.IsBitSet( luZone ) )
                    {
                        CgsResource::ResourcePtr<PropGraphicsList> lGraphicsList( lpAssetEvent->mHandle );

                        // P7: the `luZone == Nicotine::DMixIO::GetDMixID( lGraphicsList )`
                        //     cross-check assert (cpp:702) -- see the PARK LIST.

                        mapGraphicsLists[luZone] = lGraphicsList;
                        mabWaitingForGraphics.UnSetBit( luZone );

                        // DIAGNOSTIC -- NOT in the X360 binary. Ungated one-shot.
                        // Boot-log grep: "PROPS-BOOT graphics list bound".
                        {
                            static bool sbLoggedFirstGraphicsList = false;
                            if ( !sbLoggedFirstGraphicsList && CgsDev::Log::gpDebugPrint != 0 )
                            {
                                sbLoggedFirstGraphicsList = true;
                                *CgsDev::Log::gpDebugPrint
                                    << "PROPS-BOOT graphics list bound: zone " << luZone << "\n";
                            }
                        }
                    }
                }

                liEventType = mReceiverQueue.GetNextEvent( lpEventData, &lpEventData, &liEventSize );
            }
        }

        // The X360 open-codes BaseEventReceiverQueue::Clear here (miCount = 0, then
        // miStartOffset = alignment - (mpBuffer % alignment) - 8 normalised positive, and
        // miWriteOffset = miStartOffset) -- 0x821F1D50 inlined.
        mReceiverQueue.Clear();

        // ================================================================
        // [12] publish the frame.
        // ================================================================
        mrTimestep = lpInput->GetCurrentTimestep();

        // P9: the corona-phase advance -- see the PARK LIST.
        // P8: Array<BrnGui::OverheadSignScore,32>::AppendArray<32>(
        //         lpOutput->GetVisibleOverheadSignArray(), &mVisibleOverheadSigns )
        //     -- see the PARK LIST.

        lpInput->UnlockForRead();
        lpOutput->UnlockForWrite();
    }
}
