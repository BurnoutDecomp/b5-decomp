// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 07.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::UpdateMaxBoost   @ 0x822C1C80   (vtable slot 50 -- B3's own
//                                                   vtable extension, +0xC8)
//
// NOT here -- lives in the sibling partfile BoostBurnout3_wP_17.cpp:
//   BoostBurnout3::UpdateStuntBoost @ 0x822A6708   (base vtable slot 48, +0xC0)
//   It was parked while BrnGameState::GameStateModuleIO::CompletedStuntAction was
//   only FORWARD-DECLARED (BrnBoostStrategy.h:87); the record now has a real
//   definition in its home, GameSource/GameState/BrnGameActions.h:766, and the
//   body is landed. Do not re-add it here -- one definition only.
//
// Member layout / vtable slot order come from the wave-P keystone header
// BrnBoostStrategy.h; the BoostBurnout3-specific members (+0x130..+0x13C) are
// the DecFIGS DWARF order mfBoostChunkAmount / miBoostLevel / miOldBoostLevel /
// mfTimeBoosting, pinned on X360 by BoostBurnout3::Prepare @0x822C1680
// (stw 2 -> +0x134, stw 0 -> +0x138, stfs 0.0f -> +0x130 and +0x13C).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint, gxMessageFilterFlags

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// KF_BOOST_CHUNK_AMOUNT -- DecFIGS DWARF BrnBoostBurnout3.h:135, definition at
// BrnBoostBurnout3.cpp:41 (a private `static const float` of BoostBurnout3).
//
// Value from flt_82014A18, the rodata slot UpdateMaxBoost @0x822C1CBC loads.
// That slot has exactly ONE referencing function in the whole image
// (0x822C1C80, this file's body -- checked across all 30,086 exported X360
// functions), so it is NOT a merged shared literal and the DWARF name binding
// is safe here (contrast the 100.0f in partfile 03, which shares flt_82014808
// with BoostStrategy::Prepare and was therefore left a literal).
//
// Corroboration for the name: KI_BOOST_LEVELS == 3 (DWARF h:134, and the retail
// asm's `cmpwi r11, 3` boost-level ceiling in OnTakedown @0x822A6650 -- an
// earlier revision cited @0x822A6A4C here, which is OnDriveThru's copy of the
// same ceiling test, not OnTakedown's), and
// 3 * 26.666666f == 80.0f -- i.e. the constant is exactly one third of the full
// bar, "the amount of boost one chunk is worth". Note 26.666666f and 80.0f/3.0f
// are the SAME f32 (both round to 0x41D5_5555 == 26.66666603088378906), so the
// literal spelling is not recoverable; the value is.
//
// CONDUCTOR: if the TU's constants block (another partfile) also defines this,
// delete one of the two -- `cl /c` cannot see the duplicate, it becomes LNK2005.
// No sibling wave-P partfile defines it as of this writing.
// ---------------------------------------------------------------------------
const f32 BoostBurnout3::KF_BOOST_CHUNK_AMOUNT = 26.666666f;

// ---------------------------------------------------------------------------
// UpdateMaxBoost @ 0x822C1C80 -- vtable slot 50 (+0xC8), the FIRST slot of
// BoostBurnout3's own vtable extension. This is a NEW virtual that HIDES the
// non-virtual BoostStrategy::UpdateMaxBoost(bool) @0x822C0EB0; the base's
// version computes mfMaxBoost from miCombinedBoostLevel (+0xE8) * 10.0f, this
// one from BoostBurnout3's OWN miBoostLevel (+0x134). Both classes declare a
// member spelled `miBoostLevel` (base +0x104, B3 +0x134) and the asm reads
// 0x134, so the unqualified name below correctly resolves to B3's.
//
// Callers -- ELEVEN sites, every one of them a `lwz r11, 0xC8(vptr)` dispatch,
// never a direct bl. Each dispatch address below was re-derived by scanning this
// TU's exported listings for `0xC8(`, and each r4 value is the `li r4, N` that
// immediately precedes it:
//   r4 = 0 : Prepare                 @0x822C16C4  (li r4,0 @0x822C16BC)
//            OnTakedown              @0x822A666C  (li r4,0 @0x822A6664)
//            OnStuntCompletion       @0x822A688C  (li r4,0 @0x822A6884)
//            OnCrash                 @0x822A68F4  (li r4,0 @0x822A68EC)
//            OnTakenDownByAIOrPlayer @0x822A6954  (li r4,0 @0x822A6950)
//            OnEnterInfiniteBoost    @0x822A6A90  (li r4,0 @0x822A6A88)
//            OnStartCrashPlay        @0x822A6ACC  (li r4,0 @0x822A6AB8)
//            OnEndCrashPlay          @0x822A6B34  (li r4,0 @0x822A6B24)
//            RemoveAllBoostAndChunks @0x822A6B7C  (li r4,0 @0x822A6B74)
//            SetCarStatBoostLevel    @0x822C1BF4  (li r4,0 @0x822C1BE8)
//   r4 = 1 : OnDriveThru             @0x822A6A64  (li r4,1 @0x822A6A60) -- the
//            ONLY fill-to-max caller. It is the boost-level-UP path (increment
//            miBoostLevel to a ceiling of 3 @0x822A6A4C-58, then refill the bar),
//            which is what names the parameter: Feb-2007 spelled that site
//            `UpdateMaxBoost(); mfBoostAmount = mfMaxBoost;`
//            (BrnBoostBurnout3.cpp:122-123) and retail folded the second line
//            into the callee as this bool.
//   (OnWrecked @0x822C2438 is NOT among them -- it contains no 0xC8 dispatch and
//    no +0x134 access at all. An earlier revision of this comment grouped four of
//    the sites above under an "OnWrecked-family" label; that label was wrong and
//    is removed.)
//
//   0x822C1CA0  lwz    r11, 0x134(r31)          ; miBoostLevel  (B3's own)
//   0x822C1CA4  extsw  r11, r11                 ; signed
//   0x822C1CB4  fcfid  f0, f0                   ; -> f64
//   0x822C1CB8  frsp   f13, f0                  ; -> f32
//   0x822C1CBC  lfs    f0,  flt_82014A18        ; 26.666666f
//   0x822C1CC8  fmuls  f0,  f13, f0
//   0x822C1CCC  stfs   f0,  0xA4(r31)           ; mfMaxBoost =
//   0x822C1CD0  fcmpu  cr6, f0, f31             ; f31 = flt_82001CC0 = 0.0f
//   0x822C1CD4  bne    cr6, loc_822C1D0C        ; NaN also skips (EQ clear)
//   0x822C1CDC  ld     r11, CgsDev::Message::gxMessageFilterFlags
//   0x822C1CE0  clrldi r11, r11, 63             ; & 1
//   0x822C1CE8  beq    cr6, loc_822C1D00
//   0x822C1CFC  bl     CgsDev::StrStreamBase::operator<<(gpDebugPrint,"STOP\n")
// loc_822C1D00:                                 ; reached whether or not it printed
//   0x822C1D04  lfs    f0,  flt_82001C98        ; 1.0f
//   0x822C1D08  stfs   f0,  0xA4(r31)           ; mfMaxBoost = 1.0f
// loc_822C1D0C:
//   0x822C1D0C  lfs    f13, 0xA0(r31)           ; mfBoostAmount
//   0x822C1D14  fneg   f12, f13
//   0x822C1D18  lfs    f0,  0xA4(r31)           ; mfMaxBoost (post-fixup)
//   0x822C1D20  fsel   f13, f12, f31, f13       ; (-amount >= 0) ? 0.0f : amount
//   0x822C1D24  fsubs  f12, f0, f13
//   0x822C1D28  fsel   f13, f12, f13, f0        ; (max-amount >= 0) ? amount : max
//   0x822C1D2C  stfs   f13, 0xA0(r31)
//   0x822C1D30  beq    cr6, loc_822C1D38        ; cr6 still holds `arg == 0`
//   0x822C1D34  stfs   f0,  0xA0(r31)           ; mfBoostAmount = mfMaxBoost
//
// The two fsels are the compiler's branch-free rendering of the source-level
// clamp (de-optimised back to ifs per the project rule). They are NOT a hand-
// written NaN-polarity branch: fsel picks its FALSE operand on unordered, so a
// NaN mfBoostAmount would come out of the pair as mfMaxBoost, whereas the two
// ifs leave it NaN. mfBoostAmount is never NaN at this point in any reachable
// path (every writer -- Prepare, AddBoost, SetBoostAmount, OnEnterInfiniteBoost
// -- stores a clamped finite value), so the source form is the faithful one.
//
// DIVERGENCES FROM Feb-2007 (BrnBoostBurnout3.cpp:330):
//   * Feb-2007: `mfMaxBoost = (miBoostLevel + 1) * mfMaxMaxBoost / KI_BOOST_LEVELS;`
//     with KI_BOOST_LEVELS == 5. Retail: no `+ 1`, no division, no
//     mfMaxMaxBoost -- a straight multiply by the build constant, and
//     KI_BOOST_LEVELS is 3. miBoostLevel's retail range is 1..3, pinned by the
//     two GUARDS rather than by any single store:
//       floor   OnTakenDownByAIOrPlayer @0x822A6934 `cmpwi cr6, r11, 1` + `ble`
//               (decrement only while above 1)
//       ceiling OnTakedown  @0x822A6650 `cmpwi cr6, r11, 3` + `bge`
//               OnDriveThru @0x822A6A4C `cmpwi cr6, r11, 3` + `bge`
//               (increment only while below 3)
//     Unconditional stores agree: RemoveAllBoostAndChunks @0x822A6B84 stores 1,
//     Prepare @0x822C16C8 stores 2, OnStartCrashPlay @0x822A6AC4 stores 3.
//     (An earlier revision of this comment cited "OnWrecked stores 1" as the
//     witness. That was fabricated -- OnWrecked @0x822C2438 never touches +0x134
//     at all. The 1..3 conclusion stands; the witnesses above are the real ones.)
//   * Feb-2007 takes no parameter and has no zero-guard and no clamp; all three
//     are new in retail.
//   * The "STOP\n" debug print is new in retail; DWARF puts a
//     `using namespace CgsDev::Message;` inside this body at cpp:625, which is
//     exactly the gxMessageFilterFlags gate below.
// ---------------------------------------------------------------------------
void BoostBurnout3::UpdateMaxBoost(bool lbFillBoost)
{
    mfMaxBoost = static_cast<f32>(miBoostLevel) * KF_BOOST_CHUNK_AMOUNT;

    if( mfMaxBoost == 0.0f )
    {
        if( CgsDev::Message::gxMessageFilterFlags & 1 )
        {
            *CgsDev::Log::gpDebugPrint << "STOP\n";
        }

        mfMaxBoost = 1.0f;
    }

    if( mfBoostAmount < 0.0f )
    {
        mfBoostAmount = 0.0f;
    }

    if( mfBoostAmount > mfMaxBoost )
    {
        mfBoostAmount = mfMaxBoost;
    }

    if( lbFillBoost )
    {
        mfBoostAmount = mfMaxBoost;
    }
}

}
