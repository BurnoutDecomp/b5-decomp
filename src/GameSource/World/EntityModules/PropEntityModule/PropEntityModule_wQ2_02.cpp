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
//  * INERT BOOT GATE -- DO NOT LAND THIS FILE WITHOUT RETIRING IT.
//    b5-decomp/src/GameSource/World/WorldLinkStubs.cpp defines
//    `BrnWorld::PropEntityModule::PostPhysicsUpdate(...)` with the identical 5-parameter
//    signature. EXACT RANGE, RE-MEASURED 2026-08-18 (an earlier banner said 3164-3179, which
//    is off by two at BOTH ends and would have mangled the neighbouring retired-gate note and
//    left an orphan `}` -- a compile error in the very file being edited to fix the link):
//        lines 3166-3170  the `// BOOT GATE (world-drive wave 2026-07-27):` comment block
//        line  3171       the definition
//        lines 3172-3180  the one-shot-log body, 3180 being the closing brace
//    Line 3164 belongs to the RETIRED PreSceneUpdate note above it (:3158-3164); 3165 and 3181
//    are blank. Delete 3166-3180 and nothing else.
//    WorldLinkStubs.cpp IS mounted in tools/build/build_game_exe.bat, so mounting this file
//    without deleting that block is LNK2005. Per AGENTS.md gotcha 7 the gate is NOT retired
//    here -- the conductor mounts + retires together, then re-LINKS (the retired PreSceneUpdate
//    gate at :3158 is the precedent and the marker form to copy).
//    RecordPropPositions has NO gate anywhere (grepped WorldLinkStubs.cpp and
//    BrnPhysicsConductorGates.cpp).
//  * NOT MOUNTED: this file, and every PropEntityModule_wQ_0*.cpp / _wQ2_0*.cpp
//    (`grep -c PropEntityModule_wQ tools/build/build_game_exe.bat` == 0). The MOUNT SET this
//    file needs is: itself + wQ2_03 (ProcessContacts) + wQ_06 (UpdateProps) + wQ_01
//    (PrepareForReplay / RestoreFromReplay).
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

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"

#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"

#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
