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
// --include=*.cpp b5-decomp/src` finds no definition, and neither WorldLinkStubs.cpp nor
// BrnPhysicsConductorGates.cpp carries an inert gate for them. Mounting THIS partfile in
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
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h"

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
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
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
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
