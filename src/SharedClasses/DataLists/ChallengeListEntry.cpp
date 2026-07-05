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

} // namespace BrnResource
