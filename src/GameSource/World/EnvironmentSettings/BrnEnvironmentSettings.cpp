#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

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

// ---------------------------------------------------------------------------
// BuildTimeOfDay @ 0x826759C8
//
// Formats a time-of-day (in seconds) into a fixed 4-digit "HHMM" field with a
// trailing NUL at lpcOut[4]. The seconds value is scaled to whole minutes
// (x 1/60, truncated toward zero), split into hours (minutes / 60) and minutes
// (minutes % 60), each rendered with "%u" into a scratch buffer and then
// right-justified (zero-padded) into the output field: hours occupy [0..1],
// minutes occupy [2..3]. Reproduces the guest's de-inlined itoa + right-justify
// loops store-for-store (the two decimal scratch buffers, the '0' pre-fill, and
// the strlen-driven memcpy destinations).
void BuildTimeOfDay( char* lpcOut, float lfTimeSeconds )
{
    char lacHours[4];
    char lacMinutes[4];

    const unsigned int luMinutes =
        static_cast<unsigned int>( lfTimeSeconds * 0.016666668f );

    CgsCore::SPrintf( lacHours, 4, "%u", luMinutes / 0x3Cu );
    CgsCore::SPrintf( lacMinutes, 4, "%u", luMinutes % 0x3Cu );

    lpcOut[0] = '0';
    lpcOut[1] = '0';
    lpcOut[2] = '0';
    lpcOut[3] = '0';

    // Right-justify the hours string so it ends at lpcOut[2] (fills [0..1]).
    const char* lpcH = lacHours;
    while ( *lpcH++ )
        ;
    const char* lpcH2 = lacHours;
    while ( *lpcH2++ )
        ;
    memcpy( &lpcOut[3 - ( lpcH2 - lacHours )], lacHours,
            ( lpcH - lacHours ) - 1 );

    // Right-justify the minutes string so it ends at lpcOut[4] (fills [2..3]).
    const char* lpcM = lacMinutes;
    while ( *lpcM++ )
        ;
    const char* lpcM2 = lacMinutes;
    while ( *lpcM2++ )
        ;
    memcpy( &lpcOut[5 - ( lpcM2 - lacMinutes )], lacMinutes,
            ( lpcM - lacMinutes ) - 1 );

    lpcOut[4] = 0;
}

// ---------------------------------------------------------------------------
// ConsumeBlanks @ 0x82675BC8
//
// Skips a run of space (' ') characters. Returns false (0) if EOF is hit while
// consuming; otherwise pushes the first non-space character back and returns
// true. Reads are taken as signed char (matching the guest's extsb before both
// the space compare and the ungetc).
bool ConsumeBlanks( FILE* lpFile )
{
    if ( feof( lpFile ) )
        return false;

    char lcCh = static_cast<char>( fgetc( lpFile ) );
    if ( lcCh == ' ' )
    {
        while ( !feof( lpFile ) )
        {
            lcCh = static_cast<char>( fgetc( lpFile ) );
            if ( lcCh != ' ' )
                goto push_back;
        }
        return false;
    }

push_back:
    ungetc( lcCh, lpFile );
    return true;
}

// ---------------------------------------------------------------------------
// ConsumeEOL @ 0x82675F58
//
// Consumes characters until a newline ('\n') is read; returns true when the
// newline is found, or false if EOF is reached first. The read value is
// compared to 0x0A as a full int (no sign extension), matching the guest.
bool ConsumeEOL( FILE* lpFile )
{
    while ( !feof( lpFile ) )
    {
        if ( fgetc( lpFile ) == '\n' )
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ConsumeFieldValue(float&, FILE*) @ 0x82675C60
//
// Reads one whitespace-delimited float field value from the environment file.
// End-of-file, or an immediate end-of-line (peeked, then pushed back), means
// there is no value to read: returns false without touching lrfValue. Otherwise
// scans a single "%f" into lrfValue and returns true. The peeked byte is
// sign-extended (extsb) before both the ungetc and the newline compare.
bool ConsumeFieldValue( float& lrfValue, FILE* lpFile )
{
    if ( feof( lpFile ) )
        return false;

    const int liPeek = static_cast<signed char>( fgetc( lpFile ) );
    ungetc( liPeek, lpFile );
    if ( liPeek == '\n' )
        return false;

    fscanf( lpFile, "%f", &lrfValue );
    return true;
}

// ---------------------------------------------------------------------------
// ParseTimeOfDay @ 0x82675B10
//
// Parses an "HHMM" clock integer (e.g. 1430 -> 14:30) from lpTimeStr via atoi.
// The hundreds are hours, the last two digits are minutes. If the value is a
// valid wall-clock time (hours < 24, minutes < 60) it is written to *lpfSeconds
// as seconds-of-day: (hours * 60 + minutes) * 60. Out-of-range input leaves
// *lpfSeconds untouched.
void ParseTimeOfDay( float* lpfSeconds, const char* lpTimeStr )
{
    const unsigned int luValue   = static_cast<unsigned int>( atoi( lpTimeStr ) );
    const unsigned int luHours   = luValue / 100u;
    const unsigned int luMinutes = luValue % 100u;

    if ( luHours < 24u && luMinutes < 60u )
    {
        *lpfSeconds = ( static_cast<float>( luHours ) * 60.0f
                        + static_cast<float>( luMinutes ) ) * 60.0f;
    }
}

// ---------------------------------------------------------------------------
// ParseEnvironmentFile @ 0x8267CD70
//
// Parses a text environment description file. Each line is "<FieldName> = <...>".
// The field name is scanned with "%s ="; the value(s) are dispatched to the
// matching consumer. Recognised fields fall into three groups:
//   * keyframe sub-blocks (Bloom / Vignette / Scattering / Lighting / Clouds),
//     handled by the templated ConsumeFieldValue<T> overloads;
//   * "TimeOfDay"  -> a time string parsed by ParseTimeOfDay into lrfTimeOfDay;
//   * the colour-cube name fields ("ColourCube" -> lpName buffer, "ColourCube0"
//     .. "ColourCube3" -> the four 256-byte lacColourCubes rows) and the four
//     "ColourCubeWeight0" .. "ColourCubeWeight3" floats -> lafColourCubeWeights.
// Returns true once the file is fully consumed; if the file cannot be opened the
// X360 returns the null FILE* (i.e. false).
bool ParseEnvironmentFile( float&                     lrfTimeOfDay,
                           char                       (&lacColourCubes)[4][256],
                           float                      (&lafColourCubeWeights)[4],
                           BrnEffects::BloomData&     lrBloom,
                           BrnEffects::VignetteData&  lrVignette,
                           char*                      lpName,
                           ScatteringData&            lrScattering,
                           LightingData&              lrLighting,
                           CloudsData&                lrClouds,
                           const char*                lpFilename )
{
    FILE* lpFile = fopen( lpFilename, "r" );
    if ( !lpFile )
        return false;

    *lpName = 0;

    for ( ;; )
    {
        unsigned char lacFieldName[256];

        for ( ;; )
        {
            bool lbHaveField;
            if ( feof( lpFile ) )
            {
                lbHaveField = false;
            }
            else
            {
                if ( fscanf( lpFile, "%s =", lacFieldName ) <= 0 )
                    lacFieldName[0] = 0;
                lbHaveField = ConsumeBlanks( lpFile );
            }

            if ( !lbHaveField )
            {
                fclose( lpFile );
                return true;
            }

            const char* lpcName = reinterpret_cast<const char*>( lacFieldName );

            if ( ConsumeFieldValue( lrBloom,      lpcName, lpFile )
              || ConsumeFieldValue( lrVignette,   lpcName, lpFile )
              || ConsumeFieldValue( lrScattering, lpcName, lpFile )
              || ConsumeFieldValue( lrLighting,   lpcName, lpFile )
              || ConsumeFieldValue( lrClouds,     lpcName, lpFile ) )
            {
                ConsumeEOL( lpFile );
                continue;
            }
            break;
        }

        const char* lpcName = reinterpret_cast<const char*>( lacFieldName );

        if ( strcmp( lpcName, "TimeOfDay" ) == 0 )
        {
            char lacTimeStr[192];
            ConsumeFieldValue( lacTimeStr, lpFile );
            ParseTimeOfDay( &lrfTimeOfDay, lacTimeStr );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCube" ) == 0 )
        {
            if ( !ConsumeFieldValue( lpName, lpFile ) )
                *lpName = 0;
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCube0" ) == 0 )
        {
            ConsumeFieldValue( lacColourCubes[0], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCube1" ) == 0 )
        {
            ConsumeFieldValue( lacColourCubes[1], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCube2" ) == 0 )
        {
            ConsumeFieldValue( lacColourCubes[2], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCube3" ) == 0 )
        {
            ConsumeFieldValue( lacColourCubes[3], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCubeWeight0" ) == 0 )
        {
            ConsumeFieldValue( lafColourCubeWeights[0], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCubeWeight1" ) == 0 )
        {
            ConsumeFieldValue( lafColourCubeWeights[1], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCubeWeight2" ) == 0 )
        {
            ConsumeFieldValue( lafColourCubeWeights[2], lpFile );
            ConsumeEOL( lpFile );
        }
        else if ( strcmp( lpcName, "ColourCubeWeight3" ) == 0 )
        {
            ConsumeFieldValue( lafColourCubeWeights[3], lpFile );
            ConsumeEOL( lpFile );
        }
        else
        {
            ConsumeEOL( lpFile );
        }
    }
}
}
}
