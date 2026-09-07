// ChallengeListEntry.cpp
// BrnResource::ChallengeListEntryAction -- out-of-line trivial field accessors that
// the X360 build kept as standalone functions (the ones inlined everywhere live inline
// in ChallengeListEntry.h). Reconstructed store-for-store from the X360 ARTIST build.
// Member offsets are DWARF-attested (references/DecFIGS/.../ChallengeListEntry.h) and
// re-confirmed against every load displacement in the asm below.
//
//   GetConvoyTime   @ 0x8230EE20   lfs 0x44  == mfConvoyTime
//   GetTargetValue  @ 0x8230EE98   lwzx 0x34+4*i == maiTargetValue[i]
//   GetLocationType @ 0x8230EF10   lbz 0x05+i    == mauLocationType[i]  (IDA name GetLoc)
//   GetDistrict     @ 0x8230EF78   lwzx 0x10+8*i == maLocationData[i].meDistrict
//   GetRoadID       @ 0x8230F078   ldx  0x10+8*i == maLocationData[i].mRoadID
//   GetTimeLimit    @ 0x8231BD80   lfs 0x40  == mfTimeLimit
//   GetCgsIDTarget  (X360-inlined) lwz 0x34+4*i / lwz 0x34+4*(i+1), composed (hi<<32)+lo
//
// Each guard collapses de-inlined BeginAssert/FireAssert/EndAssert into one CGS_ASSERT
// (message string verbatim from rodata; file/line args dropped per project convention).
// All guards are non-fatal: the binary returns the raw field even on failure.

#include "SharedClasses/DataLists/ChallengeListEntry.h"

namespace BrnResource
{

// GetConvoyTime @ 0x8230EE20  (DWARF: f32 GetConvoyTime() const, ChallengeListEntry.h:175)
f32 ChallengeListEntryAction::GetConvoyTime() const
{
    CGS_ASSERT( mfConvoyTime > 0.0f, "HasConvoyTime()" );

    return mfConvoyTime;
}

// GetDistrict @ 0x8230EF78  (DWARF: BrnWorld::EDistrict GetDistrict(u8) const,
// ChallengeListEntry.h:210; modeled with int32_t return to match the committed header,
// which stores meDistrict as int32_t.) lwzx == 32-bit read at 0x10+8*index.
int32_t ChallengeListEntryAction::GetDistrict( uint8_t lu8Index ) const
{
    CGS_ASSERT( lu8Index < KU_MAX_LOCATIONS_PER_ACTION,
                "luLocationIndex < KU_MAX_LOCATIONS_PER_ACTION" );
    CGS_ASSERT( (ELocationType)mauLocationType[ lu8Index ] == E_LOCATION_TYPE_DISTRICT,
                "(ELocationType) mauLocationType[luLocationIndex] == E_LOCATION_TYPE_DISTRICT" );

    return maLocationData[ lu8Index ].meDistrict;
}

// GetLoc @ 0x8230EF10  (IDA GetLoc == DWARF GetLocationType, ChallengeListEntry.h:206)
// Returns the byte mauLocationType[index] (lbz 0x05+index) cast to ELocationType. Only
// the index bound is guarded (single assert), no type check.
ChallengeListEntryAction::ELocationType
ChallengeListEntryAction::GetLocationType( uint8_t lu8Index ) const
{
    CGS_ASSERT( lu8Index < KU_MAX_LOCATIONS_PER_ACTION,
                "luLocationIndex < KU_MAX_LOCATIONS_PER_ACTION" );

    return static_cast<ELocationType>( mauLocationType[ lu8Index ] );
}

// GetRoadID @ 0x8230F078  (DWARF: CgsID GetRoadID(u8) const, ChallengeListEntry.h:218)
// ldx == 64-bit read of mRoadID at 0x10+8*index.
CgsID ChallengeListEntryAction::GetRoadID( uint8_t lu8Index ) const
{
    CGS_ASSERT( lu8Index < KU_MAX_LOCATIONS_PER_ACTION,
                "luLocationIndex < KU_MAX_LOCATIONS_PER_ACTION" );
    CGS_ASSERT( (ELocationType)mauLocationType[ lu8Index ] == E_LOCATION_TYPE_ROAD,
                "(ELocationType) mauLocationType[luLocationIndex] == E_LOCATION_TYPE_ROAD" );

    return maLocationData[ lu8Index ].mRoadID;
}

// GetTargetValue @ 0x8230EE98  (DWARF: int32_t GetTargetValue(int32_t) const,
// ChallengeListEntry.h:179)  lwzx == 32-bit read at 0x34+4*index.
int32_t ChallengeListEntryAction::GetTargetValue( int32_t liTargetIndex ) const
{
    CGS_ASSERT( liTargetIndex >= 0, "liTargetIndex >= 0" );
    CGS_ASSERT( liTargetIndex < KI_MAX_TARGETS_PER_CHALLENGE_ACTION,
                "liTargetIndex < KI_MAX_TARGETS_PER_CHALLENGE_ACTION" );

    return maiTargetValue[ liTargetIndex ];
}

// GetTimeLimit @ 0x8231BD80  (DWARF: f32 GetTimeLimit() const, ChallengeListEntry.h:169)
// lfs 0x40 == mfTimeLimit.
f32 ChallengeListEntryAction::GetTimeLimit() const
{
    CGS_ASSERT( mfTimeLimit > 0.0f, "HasTimeLimit()" );

    return mfTimeLimit;
}

// GetCgsIDTarget  (DWARF: CgsID GetCgsIDTarget(int32_t) const, ChallengeListEntry.h:187)
// [challenge-manager mount 2026-09-07] X360-INLINED -- no standalone address of its own. The
// whole body is attested verbatim inside ChallengeManager::HandleRoadRuleScore @0x82334D48,
// which is the image's only caller:
//     0x82334E38  bl   BeginAssert
//     0x82334E44  r3 = "liTargetIndex + 1 < KI_MAX_TARGETS_PER_CHALLENGE_ACTION"
//     0x82334E40  li   r5, 0x2CE            ; ChallengeListEntry.h line 718
//     0x82334E50  bl   EndAssert
//     0x82334E54  lwz  r11, 0x38(action)    ; maiTargetValue[liTargetIndex]      (0x34 + 4*1)
//     0x82334E58  lwz  r10, 0x3C(action)    ; maiTargetValue[liTargetIndex + 1]  (0x34 + 4*2)
//     0x82334E5C  extldi r11, r11, 64,32    ; hi << 32
//     0x82334E60  add  r11, r11, r10        ; + lo
//     0x82334E64  cmpld cr6, r11, r27       ; == the scored road id
// i.e. a CgsID that occupies TWO consecutive 32-bit target slots, composed high-word-first.
// The compose is `(hi << 32) + lo` (an ADD, not an OR) exactly as written below -- reproduced
// as arithmetic over the named members rather than a 64-bit reinterpret, so it is
// endianness-independent on the host, where the console's big-endian `ld`-equivalent pair
// would not be.
//
// ⚠️ THE ONLY GUARD IS THE CONSOLE'S, AND ON THE CONSOLE IT ALWAYS FIRES. The single call site
// passes liTargetIndex == 1, and KI_MAX_TARGETS_PER_CHALLENGE_ACTION is 2, so
// `1 + 1 < 2` is false at compile time -- which is why the X360 compiler folded the test away
// and left the Begin/Fire/End sequence UNCONDITIONAL ahead of the two loads (there is no
// compare at 0x82334E30..0x82334E38 other than the action-type / score-type tests). The
// companion `liTargetIndex >= 0` guard that GetTargetValue carries cannot be observed here for
// the same reason -- it folds away true -- so it is NOT invented back in. CGS_ASSERT is
// log-and-continue, so the console reads and returns the pair regardless, and so do we.
//
// ⚠️ THE SECOND WORD IS PAST maiTargetValue -- the console's own behaviour, not a
// reconstruction artefact. With liTargetIndex == 1 the low half lands on record bytes
// 0x3C..0x3F, i.e. mau8TargetDataType[0..1] plus the two align bytes. That is how the
// road-rule challenge records are AUTHORED: SetCgsIDTarget(1, roadID) writes the same two
// slots, so the data and the reader agree. This record is external serialised resource data
// whose layout is fixed by the file, not by the C++ class, so the overlap is a property of
// the format. Index-based access over the named member is kept (rather than a raw offset
// cast) and this body is deliberately OUT-OF-LINE so liTargetIndex stays a runtime value.
CgsID ChallengeListEntryAction::GetCgsIDTarget( int32_t liTargetIndex ) const
{
    CGS_ASSERT( liTargetIndex + 1 < KI_MAX_TARGETS_PER_CHALLENGE_ACTION,
                "liTargetIndex + 1 < KI_MAX_TARGETS_PER_CHALLENGE_ACTION" );

    const CgsID lHighWord =
        static_cast<CgsID>( static_cast<uint32_t>( maiTargetValue[ liTargetIndex ] ) );
    const CgsID lLowWord =
        static_cast<CgsID>( static_cast<uint32_t>( maiTargetValue[ liTargetIndex + 1 ] ) );

    return ( lHighWord << 32 ) + lLowWord;
}

} // namespace BrnResource
