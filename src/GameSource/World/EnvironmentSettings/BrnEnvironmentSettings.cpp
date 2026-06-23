#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"

// ============================================================================
// BrnWorld::EnvironmentSettings::FindKeyframeInds @ 0x827B0418
// BrnWorld::EnvironmentSettings::HH_MM_SS         @ 0x827B0580
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The two rodata
// single-precision constants the X360 stores are flt_82001C98 == 1.0f and
// flt_82001CC0 == 0.0f (verified in the disassembly); both literals below come
// from those slots.
// ============================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{
// ---------------------------------------------------------------------------
// FindKeyframeInds @ 0x827B0418
//
//   *lruIndex0 = 0;
//   for ( ; index < count && time >= times[index]; ++index ) ;   // advance
//   if ( index == count ) {            // clamped past the last keyframe
//       *lruIndex0  = count - 1;
//       *lpfWeight1 = 1.0f;
//       *lruIndex1  = *lruIndex0;
//       *lpfWeight0 = 0.0f;
//   } else if ( index ) {              // bracketing pair found
//       *lruIndex1  = index - 1;
//       w = (times[index] - time) / (times[index] - times[index - 1]);
//       *lpfWeight0 = w;
//       *lpfWeight1 = 1.0f - w;
//   } else {                           // clamped before the first keyframe
//       *lruIndex1  = 0;
//       *lpfWeight0 = 1.0f;
//       *lruIndex0  = 0;
//       *lpfWeight1 = 0.0f;
//   }
//
// Note the X360 keeps the running search index in *lruIndex0 (the r5 out slot)
// while walking, then re-reads it for the branch decisions; this reproduces
// that exact aliasing rather than introducing a separate local.
// ---------------------------------------------------------------------------
unsigned int* FindKeyframeInds( unsigned int* lruIndex1,
                                float*        lpfWeight0,
                                unsigned int* lruIndex0,
                                float*        lpfWeight1,
                                const float*  lafTimes,
                                unsigned int  luCount,
                                double        lfTime )
{
    *lruIndex0 = 0;

    if ( luCount )
    {
        unsigned int luIndex;
        do
        {
            if ( lfTime < lafTimes[*lruIndex0] )
                break;
            luIndex = *lruIndex0 + 1;
            *lruIndex0 = luIndex;
        }
        while ( luIndex < luCount );
    }

    const unsigned int luSearch = *lruIndex0;

    if ( luSearch == luCount )
    {
        *lruIndex0  = luCount - 1;
        *lpfWeight1 = 1.0f;
        *lruIndex1  = *lruIndex0;
        *lpfWeight0 = 0.0f;
    }
    else if ( luSearch )
    {
        *lruIndex1 = luSearch - 1;

        const double lfWeight =
            ( lafTimes[*lruIndex0] - lfTime ) /
            ( lafTimes[*lruIndex0] - lafTimes[*lruIndex1] );

        *lpfWeight0 = static_cast<float>( lfWeight );
        *lpfWeight1 = static_cast<float>( 1.0 - lfWeight );
    }
    else
    {
        *lruIndex1  = 0;
        *lpfWeight0 = 1.0f;
        *lruIndex0  = *lruIndex1;
        *lpfWeight1 = 0.0f;
    }

    return lruIndex1;
}

// ---------------------------------------------------------------------------
// HH_MM_SS @ 0x827B0580
//
//   if ( time < 0.0 ) time += 86400.0;
//   total = (unsigned int)time;        // truncate to whole seconds
//   *lpuSeconds = total;
//   *lruMinutes = total / 60;
//   *lpuSeconds %= 60;                  // seconds  := total % 60
//   *lruHours   = *lruMinutes / 60;
//   *lruMinutes %= 60;                  // minutes  := (total / 60) % 60
//   *lruHours   %= 24;                  // hours    := (total / 3600) % 24
//
// The X360 reuses the seconds out slot (r5) as the running total before peeling
// it down; reproduced exactly.
// ---------------------------------------------------------------------------
unsigned int* HH_MM_SS( unsigned int* lruHours,
                        unsigned int* lruMinutes,
                        unsigned int* lpuSeconds,
                        double        lfTimeSeconds )
{
    if ( lfTimeSeconds < 0.0 )
        lfTimeSeconds = lfTimeSeconds + 86400.0;

    const unsigned int luTotal = static_cast<unsigned int>( lfTimeSeconds );

    *lpuSeconds = luTotal;
    *lruMinutes = luTotal / 0x3C;
    *lpuSeconds %= 0x3Cu;
    *lruHours   = *lruMinutes / 0x3C;
    *lruMinutes %= 0x3Cu;
    *lruHours   %= 0x18u;

    return lruHours;
}
}
}
