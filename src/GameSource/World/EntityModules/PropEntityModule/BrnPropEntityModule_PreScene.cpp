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
//      previous frame; take the buffer locks; run the replay hook.
//   2  copy the player index + position out of the input buffer, and normalise the eight
//      race-car velocities into maRaceCarVelocity (xyz verbatim, w = speed in MPH).
//   3  fold the three tri-state game-action latches into the module's flags, then the
//      profile round-trip (install the persistent hit-props bit array) / reset legs of
//      the streaming state machine.
//   4  drain the "prop graphics UNLOADED" queue -> drop each zone's graphics list.
//   5  drain the "prop graphics LOADED" queue   -> request each zone's "PRP_GL__<n>".
//   6  scene/sim/contact-gen removal for the props+parts recycled last frame, each loop
//      terminated by its own list Clear().
//   7  on a player reset, clear the props around the reset point.
//   8  UpdateInstanceStreaming -- decide which zone to ask for and ask for it.
//   9  PropCellManager::Update inside the miCollisionStreamingPM monitor -- the cell
//      activation sweep that puts prop volumes into CONTACT GENERATION.
//  10  BreakPropIntoParts for everything broken last frame (skipped on network catch-up).
//  11  drain the GameData receiver queue: type 62 == a zone's prop instances arrived
//      -> **LoadZone**, the step that actually spawns props; type 63 == a zone's prop
//      graphics list arrived -> bind it into mapGraphicsLists.
//  12  publish the frame timestep, advance the corona phases [PARKED], append the
//      overhead-sign scores, release the locks.
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
// ---- PARK LIST -- REWRITTEN 2026-08-19 (wave Q5 lander) -----------------------------
// ⚠ READ THIS FIRST IF YOU ARE ABOUT TO TRUST A PARK LIST. The list that stood here was
// written on 2026-08-12 against headers that later waves grew, and by 2026-08-19 SEVEN of
// its nine entries were stale IN THE HELPFUL DIRECTION: every declaration each one named
// as "missing" had since been added by the wave-Q keystone, and the parks were the ONLY
// thing still keeping PropCellManager::Update out of the frame -- which is why no prop
// volume ever reached the broad phase even after the scene/volume middle went live.
// A park list is a claim about the tree TODAY; re-grep it before believing it.
//
// LANDED 2026-08-19 (wave Q5): P1, P2, P3, P4, P5, P6, P8 -- see the ⭐ block at each
// site for the per-call asm witness.
// LANDED 2026-08-19 (wave Q6 cluster C5): P7. STILL PARKED: P9 only, re-measured below.
//
//  P7. step 11, type 63 -- the `luZone == <callee at 0x8295F4A0>( lGraphicsList )`
//      cross-check assert (fired at 0x8230B25C, and again at 0x8230B368 to format the
//      message body). ⭐ LANDED -- see the ⭐ block at the site.
//      The Q5 park reason ("the callee is UNIDENTIFIED; no per-address export") was
//      correct at the time and is now CLOSED by a targeted headless `idat` export of
//      0x8295F4A0 on a private .i64 copy (scratchpad/waveQ6/ida_p7/, dump + out_p7.json).
//      The export settles it in two instructions -- `lwz r3,4(r3) ; blr` -- and its
//      xrefs_to name three unrelated callers (this function twice, NFSMixMap::SETSFXID,
//      NFSMixMap::GetObjectPtr), i.e. the classic ICF fold that produced IDA's bogus
//      "Nicotine::DMixIO::GetDMixID" label. r3 is the PropGraphicsList memory resource and
//      its +4 word is muZoneNumber, so the call is
//      `PropGraphicsList::GetZoneNumber() const` (DWARF BrnPropGraphicsList.h:157).
//      ⚠ RESIDUAL, for the owner of SharedClasses/Physics/Props/BrnPropGraphicsList.h:
//      that header does not declare GetZoneNumber() yet, so the site reads muZoneNumber
//      directly. One line -- `u32 GetZoneNumber() const { return muZoneNumber; }` -- turns
//      it back into the console's getter call.
//
//  P9. step 12 -- the corona-phase advance between the timestep publish and the
//      overhead-sign append. RE-MEASURED 2026-08-19 (wave Q6 C5). The DECODE IS NOW
//      COMPLETE AND NEEDS NO FURTHER MEASUREMENT -- it is blocked purely on a type having
//      no home in this tree, and that home is not this cluster's file.
//      Gate: `if ( (lUpdateSet & 1) == 0 || mPropEntitySerialiser.IsPlaying() )` (asm
//      0x8230BBF4 reloads the SAME `lUpdateSet & 1` stack slot step 10 wrote, then
//      0x8230BC08-0x8230BC28 tests the serialiser state against 4/5/6), then over
//      `mVFXPropCollection->muCoronaDataTableSize` (`lwz 0x2C`) entries of the table at
//      `lwz 0x28`, stride 0x20: `+0x18 = fmod( +0x18 + mrTimestep, e(+0xC) + e(+0x8) )`
//      -- computed as `x - trunc(x/period)*period` at 0x8230BD74..0x8230BDC8. Assert text
//      "luOffset < muCoronaDataTableSize", VFXPropsResourceType.h:636.
//      ⭐ EVERY ONE OF THOSE RAW OFFSETS IS NOW A NAME. The DecFIGS DWARF
//      (references/DecFIGS/dwarfdump/SharedClasses/Graphics/VFXPropsResourceType.h:282)
//      gives BrnParticle::VFXPropCollection's full member sequence -- six {pointer,count}
//      table pairs then muVersion -- which puts, on the 32-bit console,
//        +0x28 mpCoronaTypeDataTable (:660)   +0x2C muCoronaDataTableSize (:661)
//      and the same file (:97) gives VFXCoronaTypeData: mnID/mType/mrTimeOn/mrTimeOff/
//      mrSizeMin/mrSizeMax/mrMasterTime/mbSynchronised == console stride 0x20, with
//        +0x18 == mrMasterTime      +0x08 + +0x0C == mrTimeOn + mrTimeOff == GetTotalTime()
//      (VFXCoronaTypeData::GetTotalTime is a real DWARF method, :202). So the console
//      statement is, in names:
//        e->mrMasterTime = fmod( e->mrMasterTime + mrTimestep, e->GetTotalTime() );
//      over `GetCoronaTypeDataByOffset(i)` (:634, whose own bounds assert is the :636 text).
//      BLOCKER (unchanged, re-grepped): `BrnParticle::VFXPropCollection` is still only
//      FORWARD-DECLARED in-tree (BrnPropEntityModule.h:162); the committed
//      SharedClasses/Graphics/VFXPropsResourceType.h is 25 lines and homes only the
//      resource-type HANDLER (VFXPropCollectionResourceType), not the payload. `operator->`
//      on an incomplete type is a hard error, so this cannot be landed from here at all --
//      and that header is NOT this cluster's file.
//      MOUNT (one owner, one sitting): home VFXPropCollection + VFXCoronaTypeData in
//      SharedClasses/Graphics/VFXPropsResourceType.h from the DWARF rows above. ⚠ It is a
//      SERIALISED resource whose six embedded pointers WIDEN on the x64 host, so it needs
//      the same porter/static_assert contract BrnPropGraphicsList.h documents -- that, not
//      the arithmetic, is the work.
//      It is a visual-only phase wrap; props spawn, render and smash without it. The same
//      type blocks the corona tail of RenderPropAndCoronas (BrnPropEntityModule_Render.cpp:65).
//
// ---- STATE OF THE INPUT SIDE (this note also corrected 2026-08-19) ------------------
// The 2026-08-12 note here said both producers of PropEntityIO::InputBuffer_PreScene were
// "still inert gates in WorldLinkStubs.cpp". THAT IS NO LONGER TRUE -- both are real,
// bodied and mounted:
//   WorldModule::BridgeWorldModuleToPropModule_PreScene  @0x827AACF8
//       -> GameSource/World/Bridges/WorldBridgeWorldModuleToPropModule.cpp:112
//   WorldModule::BridgeRaceCarModuleToPropModule_PreScene @0x827A5510
//       -> GameSource/World/Bridges/WorldBridgeRaceCarToPropModule.cpp:124
// (both mounted at tools/build/build_game_exe.bat:1936/1937, both called from
// BrnWorldModule.cpp:2033/2038). CreateIOBuffer<T> was likewise made console-faithful by
// the 2026-08-15 perf wave, so T::Construct() now runs and miPlayerZoneNumber carries its
// real sentinel rather than 0.
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
#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"              // PropGraphicsList::muZoneNumber (step 11 / P7)
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

        // BrnUpdateSet bit 0 -- the NETWORK-CATCHUP bit. ASM 0x82309A74
        // `clrlwi r9, r31, 31` stores it in the prologue; step 10 (0x8230AE00) and step 12's
        // corona gate (0x8230BBEC) are its two consumers, and BOTH treat a set bit as "skip".
        // The name is not invented here: BrnPhysicsModuleUpdateFunctions.cpp:231 already reads
        // the same bit of the same word as `lbNetworkCatchup` (the console's catch-up fast
        // path, which locks the sim and skips contact generation). Fold it into
        // SharedClasses/BrnSharedConstants.h when that header's bit list lands.
        const BrnUpdateSet KU_UPDATESET_NETWORK_CATCHUP = 0x1;

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

        // ⭐ P1 LANDED 2026-08-19 (wave Q5). ASM 0x82309AA8-0x82309AB8:
        //   `mr r6,r31 (lUpdateSet) ; mr r5,r30 (lpOutput) ; mr r4,r29 (lpInput) ;
        //    mr r3,r16 (this) ; bl BrnWorld::PropEntityModule::ReplayPreSceneUpdate`
        // -- immediately after LockForRead and before anything reads the input buffer.
        // The declaration the old park cited as missing is now at BrnPropEntityModule.h:352
        // and the body at PropEntityModule_wQ_03.cpp:151.
        ReplayPreSceneUpdate( lpInput, lpOutput, lUpdateSet );

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
            // GetHitPropsBitArray(); it fires from there, in the console's own order,
            // because the install below is what calls the getter.

            // ⭐ P2 LANDED 2026-08-19 (wave Q5). ASM 0x82309CA0
            //   `memcpy( module + 0xC3200, *(lpInput + 0x780), 37504 )`
            // == PropZoneManager::SetHitPropBitArray inlined: module+0xC3200 is
            // mZoneManager(+0x280).maPreviouslyHitProps and 37504 == sizeof(BitArray<300000>).
            // Both ends are the SAME instantiation (CgsContainers::BitArray<300000u>) --
            // InputBuffer_PreScene::HitPropsBitArray (BrnPropEntityModuleIO.h:661) and
            // PropZoneManager::HitPropsBitArray (BrnPropZoneManager.h:125) -- so the whole-array
            // assignment inside the setter (BrnPropZoneManager.h:370) IS the memcpy. Read
            // through the READ-LOCKED handle, like every other input read in this function.
            mZoneManager.SetHitPropBitArray( lpInputRead->GetHitPropsBitArray() );

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
        // last frame.  ⭐ P3 LANDED 2026-08-19 (wave Q5) -- store for store off
        // 0x8230A5F0..0x8230AC18.
        //
        // TWO loops in the console's order, each terminated by its own Clear():
        //   props: 0x8230A6C0..0x8230A928, then `stwx 0 -> module+0xCDEA4` (the
        //          Array<PropEntityID,15> count word) at 0x8230A940;
        //   parts: 0x8230A988..0x8230AC00, then `stwx 0 -> module+0xCDF20` (the
        //          Array<PropEntityID,30> count word) at 0x8230AC18.
        // ⚠ The props Clear() sits BETWEEN the two loops in the shipped build, not after
        // both. The previous (loop-less) shape put the two clears adjacent; that was
        // indistinguishable only because neither loop existed. It is spelled the console's
        // way now so the ordering stays right if either loop ever grows a side effect.
        //
        // Every call below goes through the de-inlined PropZoneManager forwarders (the
        // console calls PropCellManager directly with `r3 = module + 0x280`, because
        // &mZoneManager == &mZoneManager.mCellManager -- the cell manager is embedded at
        // offset 0; and the forwarders own the private mauStartIndexOfZone read the
        // console inlines at 0x8230A8C0 / 0x8230AB98 as `2*(zone + 0x66161) + r25`).
        //
        // ⚠ The two resource-pointer tripwires differ and are NOT interchangeable:
        // this step bakes CgsResourcePtr.h line 544 == `operator->()` (0x8230A748 /
        // 0x8230AA34 both `li r5, 0x220`), while steps 7 and 9 bake line 581 ==
        // `GetMemoryResource()` (`li r5, 0x245`). Spelled accordingly.
        // ================================================================
        {
            const s32 liNumRecycledProps = static_cast<s32>( maRecentlyRecycledProps.GetLength() );

            for ( s32 liRecycled = 0; liRecycled < liNumRecycledProps; ++liRecycled )
            {
                const PropEntityID lPropEntityId = maRecentlyRecycledProps.GetItem(
                    static_cast<u32>( liRecycled ) );

                // 0x8230A6DC `sub_822CDA28` == PropZoneManager::GetProp(PropEntityID),
                // the GLOBAL-index overload (Hex-Rays drops its second argument).
                PropEntityInstance* lpProp = mZoneManager.GetProp( lPropEntityId );

                // 0x8230A6EC `lhz r28,0x44(prop)` == muTypeId, then the pair of type-id
                // tripwires GetType() carries (BrnPropPhysicsDataHeader.h:173/:174).
                const BrnPhysics::Props::PropTypeData* lpType =
                    mpPropPhysicsDataHeader->GetType( lpProp->muTypeId );

                // 0x8230A7DC `sldi r27,r27,32` -- the by-value X360 form of a
                // PropVolumeInstanceID carrying this prop's entity word and volume index 0.
                // SetPropEntityId is where the console's owner tripwire at 0x8230A7B4
                // ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278)
                // comes from.
                PropVolumeInstanceID lVolumeInstanceID;
                lVolumeInstanceID.SetPropEntityId( lPropEntityId );

                // 0x8230A7D8..0x8230A834. TWO `lbz 0x4D(prop)` loads, which are IsSmashed()'s
                // own two acts: the "mu8State < E_STATE_COUNT" tripwire
                // (BrnPropEntityInstance.h:653, fired at 0x8230A7E0) and then the
                // `state >= E_SMASHED` test, which the console spells branchlessly with the
                // subfc/subfe carry trick at 0x8230A808..0x8230A814.
                CGS_ASSERT( !lpProp->IsSmashed(), "!lpProp->IsSmashed()" );            // cpp:597

                // 0x8230A838..0x8230A864. A prop still in E_PHYSICAL is left alone entirely
                // (`cmplwi r11,4 ; beq` straight to the loop increment): the physics side
                // owns it until it leaves that state.
                if ( lpProp->GetState() != E_PHYSICAL )
                {
                    if ( ( lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT ) != 0 )
                    {
                        mZoneManager.RemovePropFromContactGeneration(
                            lpProp, lpType, lVolumeInstanceID,
                            lpOutput->GetSceneInputInterface() );
                    }

                    if ( ( lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT ) != 0 )
                    {
                        // The scene leg takes a FRESHLY fetched interface (0x8230A8AC is a
                        // second `sub_822B9738`, not a reuse of the first result) plus the
                        // replay serialiser in r8.
                        mZoneManager.RemovePropFromScene(
                            lpProp, lpType, lVolumeInstanceID,
                            lpOutput->GetSceneInputInterface(),
                            &mPropEntitySerialiser );
                    }
                }
            }
        }

        maRecentlyRecycledProps.Clear();

        {
            const s32 liNumRecycledParts = static_cast<s32>( maRecentlyRecycledParts.GetLength() );

            for ( s32 liRecycled = 0; liRecycled < liNumRecycledParts; ++liRecycled )
            {
                const PropEntityID lPartEntityId = maRecentlyRecycledParts.GetItem(
                    static_cast<u32>( liRecycled ) );

                // 0x8230A9BC `clrrwi r27,r31,10` -- clear the 10-bit part field, i.e. the
                // OWNING PROP's id. SetPartIndex(0) is exactly that mask (its X360 body
                // @0x822B7A70 is `clrrwi r11,r11,10` & index) and it carries the owner
                // tripwire the console fires first, at 0x8230A99C.
                PropEntityID lPropEntityId = lPartEntityId;
                lPropEntityId.SetPartIndex( 0 );

                PropEntityInstance* lpProp = mZoneManager.GetProp( lPropEntityId );

                const BrnPhysics::Props::PropTypeData* lpType =
                    mpPropPhysicsDataHeader->GetType( lpProp->muTypeId );

                PropVolumeInstanceID lVolumeInstanceID;
                lVolumeInstanceID.SetPropEntityId( lPropEntityId );

                // 0x8230AAEC..0x8230AB20. The MIRROR of the prop loop's tripwire: a prop
                // whose PARTS are being recycled must already be smashed.
                CGS_ASSERT( lpProp->IsSmashed(), "lpProp->IsSmashed()" );              // cpp:628

                // 0x8230AB3C -- UNCONDITIONAL, and it takes the OUTPUT BUFFER (r7 == a5),
                // not a scene interface; there is no E_PHYSICAL gate on this loop.
                mZoneManager.RemovePropPartsFromSimIfPhysical( lpProp, lpType,
                                                               lVolumeInstanceID, lpOutput );

                if ( ( lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT ) != 0 )
                {
                    mZoneManager.RemovePropPartsFromContactGeneration(
                        lpProp, lpType, lVolumeInstanceID,
                        lpOutput->GetSceneInputInterface() );
                }

                if ( ( lpProp->mu8Flags & KU_ADDED_TO_SCENE_BIT ) != 0 )
                {
                    mZoneManager.RemovePropPartsFromScene(
                        lpProp, lpType, lVolumeInstanceID,
                        lpOutput->GetSceneInputInterface(),
                        &mPropEntitySerialiser );
                }
            }
        }

        maRecentlyRecycledParts.Clear();

        // ================================================================
        // [7] the player was teleported/reset: sweep the props out from under them.
        // ⭐ P4 LANDED 2026-08-19 (wave Q5). ASM 0x8230AC20..0x8230AD0C:
        //   r3 = module + 0x280 (&mZoneManager)   r4 = *mpPropPhysicsDataHeader
        //   r5 = lpOutput->GetPropInputInterface()  (0x8230ACC4)
        //   r6 = lpOutput->GetSceneInputInterface() (0x8230ACB8 -- fetched FIRST)
        //   r7 = &mPropEntitySerialiser            r8 = &mauStartIndexOfZone[0]
        //   v1 = module + 0xCDDD0 (mLastPlayerResetPosition)
        //   v2 = unk_82FAD760 == the broadcast KF_MIN_DIST_FROM_PLAYER
        // then `stb 0 -> module+0xCDDE0` clears the latch.
        // The r8 argument is the zone manager's PRIVATE start-index table, which is why
        // this goes through the PropZoneManager forwarder (BrnPropZoneManager.h:401).
        // ================================================================
        if ( mbPlayerWasJustReset )
        {
            // The exclusion radius is a BROADCAST VecFloat on the console (the dynamic
            // initialiser at 0x82C4B9B8 splats flt_82004270 == 3.0f across all four lanes
            // into unk_82FAD760); built the same way here rather than as a scalar.
            const VecFloat lvClearRadius = { KF_MIN_DIST_FROM_PLAYER, KF_MIN_DIST_FROM_PLAYER,
                                             KF_MIN_DIST_FROM_PLAYER, KF_MIN_DIST_FROM_PLAYER };

            // GetMemoryResource() first (its 0x8230AC38 null test bakes CgsResourcePtr.h:581),
            // then the scene interface, then the prop-input interface -- the console's order.
            const PropPhysicsDataHeader* lpTypes = mpPropPhysicsDataHeader.GetMemoryResource();
            PropEntityIO::OutputBuffer_PreScene::SceneInputInterfaceStorage* lpScene =
                lpOutput->GetSceneInputInterface();
            PropCellManager::PropInputInterface* lpPropInput =
                reinterpret_cast<PropCellManager::PropInputInterface*>(
                    lpOutput->GetPropInputInterface() );

            mZoneManager.ClearPropsNearPosition( mLastPlayerResetPosition, lvClearRadius,
                                                 lpTypes, lpPropInput, lpScene,
                                                 &mPropEntitySerialiser );

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
        // [9] THE CELL SWEEP -- what actually turns loaded props into COLLIDABLE ones.
        // ⭐ P5 LANDED 2026-08-19 (wave Q5). ASM 0x8230AD30..0x8230ADFC:
        //   0x8230AD3C  StartMonitor( *(module + 0xD3360) )   == miCollisionStreamingPM
        //   0x8230ADF4  PropCellManager::Update with
        //       r3 = module + 0x280  (&mZoneManager; the cell manager is embedded at 0)
        //       r4 = *(module + 0xCDD88)  == mpPropPhysicsDataHeader.GetMemoryResource()
        //                                    (its null test bakes CgsResourcePtr.h:581)
        //       r5 = module + 0xCDDE4      == &maRecentlyBrokenProps
        //       r6 = lpOutput
        //       r7 = `lbzx r7, r30, 0xD3334` == mbInReplay   <-- the seventh slot the old
        //            park said the committed signature had no room for; it has one now
        //            (BrnPropCellManager.h:185 / BrnPropZoneManager.h:391)
        //       r8 = &mPropEntitySerialiser
        //       r9 = module + 0x280 + 0xCC342 == &mauStartIndexOfZone[0]  (PRIVATE -- which
        //            is why this goes through the PropZoneManager forwarder)
        //       v1 = the stack copy of mPlayerPosition
        //   0x8230ADFC  StopMonitor( the SAME reloaded *(module + 0xD3360) )
        // ================================================================
        CgsDev::PerfMonCpu::StartMonitor( miCollisionStreamingPM );

        mZoneManager.UpdateCollisionStreaming( mPlayerPosition,
                                               mpPropPhysicsDataHeader.GetMemoryResource(),
                                               &maRecentlyBrokenProps,
                                               lpOutput,
                                               mbInReplay,
                                               &mPropEntitySerialiser );

        CgsDev::PerfMonCpu::StopMonitor( miCollisionStreamingPM );

        // [DIAG] NOT IN THE X360 BINARY. Wave-Q5 probe: proves the cell-activation sweep
        // is now entered at all -- until this wave PropCellManager::Update was never
        // called, so AddPropToContactGeneration never ran and no prop volume ever reached
        // the broad phase. One-shot; the env latch is evaluated ONCE (a static bool).
        // ⚠ HONEST LIMIT: the requested "N active cells" cannot be printed. The count lives
        // in PropCellManager::miNumActiveCells (BrnPropCellManager.h:318), PropCellManager is
        // reached only through PropZoneManager::mCellManager which is PRIVATE, and neither
        // class exposes an accessor for it -- and both headers are outside this lane. What is
        // printed instead is reachable and true: that the step ran, and the module's own
        // loaded-zone count (no cell can be active before a zone is loaded). Boot-log grep:
        // "[Q5-props] first cell activation sweep".
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static bool sbLoggedFirstCellSweep = false;
            if ( sbPropDiag && !sbLoggedFirstCellSweep && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstCellSweep = true;
                *CgsDev::Log::gpDebugPrint
                    << "[Q5-props] first cell activation sweep ran; loadedZones "
                    << muNumberOfLoadedZones
                    << " (active-cell count not exposed: PropZoneManager::mCellManager is private)\n";
            }
        }

        // ================================================================
        // [10] break everything the cell sweep (and last frame's physics) reported broken.
        // ⭐ P6 LANDED 2026-08-19 (wave Q5). ASM 0x8230AE00..0x8230AEC8.
        //
        // GATE: `lbz var_300(r1)` is the `lUpdateSet & 1` computed in the prologue
        // (0x82309A74 `clrlwi r9,r31,31`, stored at 0x82309A8C); a NON-ZERO value skips the
        // whole sweep. Bit 0 is the network-catchup bit -- the same bit
        // BrnPhysicsModuleUpdateFunctions.cpp:231 already reads as `lbNetworkCatchup`.
        // ⚠ The `stw r11, var_238(r1)` at 0x8230AE0C is NOT a dead spill: step 12's corona
        // gate reloads that exact slot at 0x8230BBEC. Same latch, two consumers.
        //
        // The Set<PropEntityID,32> walk RE-READS its live count every iteration
        // (0x8230AE38 and 0x8230AE5C both `lwz 0x80(r31)`) and fires CgsSet.h:227 / :274 /
        // :275 per iteration -- GetLength() and operator[] carry those tripwires verbatim,
        // which is the idiom ProcessBrokenProps (PropEntityModule_wQ_07.cpp:447) already uses.
        // ⚠ MEASURED: PreSceneUpdate does NOT Clear() the set here -- no store to the count
        // word at module+0xCDE64 exists anywhere in this function. The owner of the clear is
        // ProcessBrokenProps @0x822EEFA0; UpdateCollisionStreaming above ERASES individual
        // entries as it deactivates cells (BrnPropCellManager.cpp:458).
        // ================================================================
        if ( ( lUpdateSet & KU_UPDATESET_NETWORK_CATCHUP ) == 0 )
        {
            for ( u32 luBroken = 0; luBroken < maRecentlyBrokenProps.GetLength(); ++luBroken )
            {
                BreakPropIntoParts( maRecentlyBrokenProps[luBroken], lpOutput );
            }
        }

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

                        // ⭐ P7 LANDED 2026-08-19 (wave Q6 cluster C5) -- the callee is
                        // IDENTIFIED. A targeted headless IDA export of 0x8295F4A0 on a private
                        // .i64 copy (scratchpad/waveQ6/ida_p7/) shows the whole function is TWO
                        // instructions:
                        //     0x8295F4A0  lwz  r3, 4(r3)
                        //     0x8295F4A4  blr
                        // i.e. "return the u32 at +4 of my argument". Its xrefs_to prove the
                        // ICF fold that made IDA mislabel it: PropEntityModule::PreSceneUpdate
                        // (here, twice) AND NFSMixMap::SETSFXID AND NFSMixMap::GetObjectPtr all
                        // branch to the same body, so "Nicotine::DMixIO::GetDMixID" is just
                        // whichever representative IDA kept -- not this call's signature.
                        // r3 here is the PropGraphicsList memory resource, whose +4 word is
                        // muZoneNumber (BrnPropGraphicsList.h), and the DWARF names the getter:
                        // `uint32_t PropGraphicsList::GetZoneNumber() const` (DWARF
                        // SharedClasses/Physics/Props/BrnPropGraphicsList.h:157).
                        //
                        // ASM 0x8230B1EC..0x8230B264:
                        //   lwz r11,var_1C0 ; cmplwi 0 ; bne -> skip    == ResourcePtr::operator->()'s
                        //       own `mpResource != NULL` tripwire, FireAssert li r5,0x220 ==
                        //       CgsResourcePtr.h:544 (the same :544 spelling steps 6/7/9 use)
                        //   lwz r3,var_1C0 ; bl 0x8295F4A0 ; cmplw r29,r3 ; beq -> past the assert
                        //   the miss arm composes the message through StrStream ("%u" / "0x%X"
                        //   for BOTH values) and fires it with li r5,0x2BE == cpp:702.
                        //   (0x8230B3E4/0x8230B3E8 `lwz r26,var_304` / `li r25,1` are the assert
                        //   block RESTORING the two constant registers it scratched -- r25==1,
                        //   r24==0 are live across the whole event loop. They are NOT a flag.)
                        //
                        // ⚠ TWO DEVIATIONS, both deliberate and both cheap to retire:
                        // 1. The member is read directly instead of through GetZoneNumber(),
                        //    because SharedClasses/Physics/Props/BrnPropGraphicsList.h does not
                        //    declare that getter yet and it is not this cluster's file. It is a
                        //    ONE-LINE header addition -- `u32 GetZoneNumber() const { return
                        //    muZoneNumber; }` (DWARF :157) -- after which this line should become
                        //    `lGraphicsList->GetZoneNumber()`. Same value either way: the console
                        //    getter IS `return muZoneNumber;`.
                        // 2. The console composes the assert TEXT at run time from two
                        //    StrStream AppendFormats; those literals were not recovered, so the
                        //    condition text stands in -- exactly the disposition the sibling
                        //    zone-data assert at cpp:727 (above) already took.
                        //
                        // The `!mbResourceSystemStalled` gate is the console's, not a guess:
                        // 0x8230B1DC-0x8230B1E8 `lwz r11,var_1D0 ; lbz r11,0(r11) ; cmplwi 0 ;
                        // bne -> 0x8230B3EC` -- the SAME byte, through the SAME cached address
                        // (seeded at 0x82309A80/0x82309A90 in the prologue), that the zone-data
                        // branch tests at 0x8230B808. Note where it branches TO: past the assert
                        // but NOT past the store, so a stalled resource system still binds the
                        // list -- it only stops the cross-check from firing on a torn load.
                        if ( !mbResourceSystemStalled )
                        {
                            CGS_ASSERT( luZone == lGraphicsList->muZoneNumber,
                                        "luZoneIndex == lpPropGraphicsList->GetZoneNumber()" ); // cpp:702
                        }

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

        // P9: the corona-phase advance -- STILL PARKED. Re-measured 2026-08-19 (wave Q6 C5):
        // the decode is COMPLETE and every raw offset now has a DWARF name
        //   e->mrMasterTime = fmod( e->mrMasterTime + mrTimestep, e->GetTotalTime() );
        // over mVFXPropCollection's muCoronaDataTableSize corona-type-data entries.
        // The ONLY thing missing is a home for BrnParticle::VFXPropCollection /
        // VFXCoronaTypeData (still forward-declaration-only in this tree), and that home --
        // SharedClasses/Graphics/VFXPropsResourceType.h -- is another owner's file.
        // Full recipe in the PARK LIST at the top of this file.

        // ⭐ P8 LANDED 2026-08-19 (wave Q5). ASM 0x8230BDD0..0x8230BDE8:
        //   `mr r3, lpOutput ; bl 0x822B9930` == GetVisibleOverheadSignArray() (the
        //   WRITE-lock overload; the read-lock twin is 0x827A1AC0), then
        //   `mr r4, module + 0xD1DB0` == &mVisibleOverheadSigns, then
        //   Array<BrnGui::OverheadSignScore,32>::AppendArray<32> @0x822E5348.
        // The park's blocker -- "BOTH ends are opaque byte storage" -- is gone: the module
        // member is ::Array<BrnGui::OverheadSignScore,32> (BrnPropEntityModule.h:726) and
        // OutputBuffer_PreScene::VisibleOverheadSignArrayStorage is the SAME instantiation
        // (BrnPropEntityModuleIO.h:226), so AppendArray's `const Array<T,N>&` binds directly.
        lpOutput->GetVisibleOverheadSignArray()->AppendArray( mVisibleOverheadSigns );

        lpInput->UnlockForRead();
        lpOutput->UnlockForWrite();
    }
}
