#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "SharedClasses/Graphics/BrnEffectsData.h"          // BrnEffects::BloomData / VignetteData (complete layout)

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

// ===========================================================================
// Templated keyframe field-value consumers  ConsumeFieldValue<T>
//
//   @ 0x82679160  ConsumeFieldValue<BrnEffects::BloomData>
//   @ 0x82679528  ConsumeFieldValue<BrnEffects::VignetteData>
//   @ 0x82679CB8  ConsumeFieldValue<EnvironmentSettings::LightingData>
//   @ 0x8267A080  ConsumeFieldValue<EnvironmentSettings::CloudsData>
//
// ONE shared template body, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Each instantiation walks a per-type static field
// descriptor table (record = { char name[256]; s32 type; s32 offset }, 264
// bytes) that lives in rodata; the record whose name matches lpFieldName selects
// a store path keyed by the type code, writing float(s) at
// (u8*)&lrData + offset. The four tables were recovered by a headless IDA dump
// of the rodata at 0x820A3AC8 / 0x820A3DE0 / 0x820A5D40 / 0x820A6688.
//
// Type-code store paths (from the disassembly):
//   0        one scalar float                        -> ConsumeFieldValue(f32&)
//   1        one <type-1> field  (UNUSED by these four tables -- sibling reader
//            @0x82675DC8; element type INFERRED, see FLAG on the declaration)
//   2 / 3    X / Y lane of a vec2 (read one float, overwrite that lane in place)
//   4 / 5    colour (multiple floats) -> sibling reader @0x82675E50; a type-5
//            field additionally matches "<name>Power" -> splat one scalar and
//            multiply the whole vec4 by it (vspltw + vmulfp128)
//   6 / 7 / 8  X / Y lane stores (UNUSED by these four tables)
//   9        colour/multiplier: read a colour, merge into the target preserving
//            one lane (vrlimi128 mask=1, see FLAG)
// ===========================================================================
namespace
{
struct FieldDescriptor
{
    char mName[256];
    s32  mType;
    s32  mOffset;
};
static_assert( sizeof( FieldDescriptor ) == 264,
               "FieldDescriptor record must be 264 bytes (name[256] + s32 type + s32 offset)" );

// --- @0x820A3AC8 : BrnEffects::BloomData (3 records) ---
const FieldDescriptor kaBloomFields[] =
{
    { "BloomLuminance",        0, 0x00 },
    { "BloomThreshold",        0, 0x04 },
    { "BloomColourMultiplier", 9, 0x10 },
};

// --- @0x820A3DE0 : BrnEffects::VignetteData (8 records) ---
const FieldDescriptor kaVignetteFields[] =
{
    { "VignetteAngle",       0, 0x00 },
    { "VignetteSharpness",   0, 0x04 },
    { "VignetteAmountX",     2, 0x10 },
    { "VignetteAmountY",     3, 0x10 },
    { "VignetteCentreX",     2, 0x20 },
    { "VignetteCentreY",     3, 0x20 },
    { "VignetteOuterColour", 9, 0x40 },
    { "VignetteInnerColour", 9, 0x30 },
};

// --- @0x820A5D40 : EnvironmentSettings::LightingData (9 records) ---
const FieldDescriptor kaLightingFields[] =
{
    { "KeyLightColour",         5, 0x00 },
    { "SpecularColour",         5, 0x10 },
    { "KeyFillColour",          5, 0x20 },
    { "ShadowFillColour",       5, 0x30 },
    { "RightFillColour",        5, 0x40 },
    { "LeftFillColour",         5, 0x50 },
    { "UpFillColour",           5, 0x60 },
    { "DownFillColour",         5, 0x70 },
    { "AmbientIrradianceScale", 0, 0x80 },
};

// --- @0x820A6688 : EnvironmentSettings::CloudsData (15 records) ---
const FieldDescriptor kaCloudsFields[] =
{
    { "CloudLayer0Density",    0, 0x40 },
    { "CloudLayer1Density",    0, 0x44 },
    { "CloudLayer0Feathering", 0, 0x48 },
    { "CloudLayer1Feathering", 0, 0x4C },
    { "CloudLayer0LiteColour", 4, 0x00 },
    { "CloudLayer1LiteColour", 4, 0x10 },
    { "CloudLayer0DarkColour", 4, 0x20 },
    { "CloudLayer1DarkColour", 4, 0x30 },
    { "CloudLayer0Opacity",    0, 0x50 },
    { "CloudLayer1Opacity",    0, 0x54 },
    { "CloudLayer0Speed",      0, 0x58 },
    { "CloudLayer1Speed",      0, 0x5C },
    { "CloudLayer0Scale",      0, 0x60 },
    { "CloudLayer1Scale",      0, 0x64 },
    { "CloudDirectionAngle",   0, 0x68 },
};

// Per-type table selector (tag-dispatched on a null T* so the shared body stays
// type-generic). Only the four recovered types have an overload; a fifth
// consumer -- ConsumeFieldValue<ScatteringData> -- is a sibling wave kept
// external via the extern-template declaration below.
const FieldDescriptor* GetEnvFieldTable( const BrnEffects::BloomData*,    s32& lrCount )
{ lrCount = 3;  return kaBloomFields; }
const FieldDescriptor* GetEnvFieldTable( const BrnEffects::VignetteData*, s32& lrCount )
{ lrCount = 8;  return kaVignetteFields; }
const FieldDescriptor* GetEnvFieldTable( const LightingData*,             s32& lrCount )
{ lrCount = 9;  return kaLightingFields; }
const FieldDescriptor* GetEnvFieldTable( const CloudsData*,               s32& lrCount )
{ lrCount = 15; return kaCloudsFields; }
} // namespace

// Sibling ConsumeFieldValue overloads whose bodies live in BrnEnvironmentData.cpp
// (a separate wave -- declared here, like the char* string overload, so this
// shared body links against them):
//   @ 0x82675E50 -- read a colour (vec4 of floats) into lpColour. Used by type
//                   codes 4 / 5 / 9.
//   @ 0x82675DC8 -- read a type-1 field. FLAG: no field of type 1 appears in any
//                   of the four recovered tables and this helper's asm was not in
//                   scope, so its element type (u32*) is INFERRED, not attested.
bool ConsumeFieldValue( f32* lpColour, FILE* lpFile );
bool ConsumeFieldValue( u32* lpField,  FILE* lpFile );

// ---------------------------------------------------------------------------
// ConsumeFieldValue<T> -- the one shared body (see the block comment above).
// Control flow mirrors the guest: a failed value read on a name-matched record
// resumes scanning (the guest's `goto LABEL_31`), reproduced here as `continue`.
// ---------------------------------------------------------------------------
template< typename T >
bool ConsumeFieldValue( T& lrData, const char* lpFieldName, FILE* lpFile )
{
    u8* const lpBase = reinterpret_cast<u8*>( &lrData );

    s32                    liCount = 0;
    const FieldDescriptor* const lpBegin =
        GetEnvFieldTable( static_cast<const T*>( 0 ), liCount );
    const FieldDescriptor* const lpEnd = lpBegin + liCount;

    for ( const FieldDescriptor* lpDesc = lpBegin; lpDesc < lpEnd; ++lpDesc )
    {
        const s32 liType = lpDesc->mType;

        if ( strcmp( lpDesc->mName, lpFieldName ) == 0 )
        {
            u8* const lpTarget = lpBase + lpDesc->mOffset;

            switch ( liType )
            {
                case 0:
                    return ConsumeFieldValue( *reinterpret_cast<f32*>( lpTarget ), lpFile );

                case 1:
                    return ConsumeFieldValue( reinterpret_cast<u32*>( lpTarget ), lpFile );

                case 2:
                {
                    f32 lfValue;
                    if ( !ConsumeFieldValue( lfValue, lpFile ) )
                        continue;
                    reinterpret_cast<f32*>( lpTarget )[0] = lfValue;
                    return true;
                }
                case 3:
                {
                    f32 lfValue;
                    if ( !ConsumeFieldValue( lfValue, lpFile ) )
                        continue;
                    reinterpret_cast<f32*>( lpTarget )[1] = lfValue;
                    return true;
                }
                case 4:
                case 5:
                    return ConsumeFieldValue( reinterpret_cast<f32*>( lpTarget ), lpFile );

                case 6:
                {
                    f32 lfValue;
                    if ( !ConsumeFieldValue( lfValue, lpFile ) )
                        continue;
                    reinterpret_cast<f32*>( lpTarget )[0] = lfValue;
                    return true;
                }
                case 7:
                {
                    f32 lfValue;
                    if ( !ConsumeFieldValue( lfValue, lpFile ) )
                        continue;
                    reinterpret_cast<f32*>( lpTarget )[1] = lfValue;
                    return true;
                }
                case 8:
                {
                    f32 lfValue;
                    if ( !ConsumeFieldValue( lfValue, lpFile ) )
                        continue;
                    reinterpret_cast<f32*>( lpTarget )[1] = lfValue;
                    return true;
                }
                case 9:
                {
                    f32 laColour[4];
                    if ( !ConsumeFieldValue( laColour, lpFile ) )
                        continue;
                    f32* const lpDst = reinterpret_cast<f32*>( lpTarget );
                    lpDst[0] = laColour[0];
                    lpDst[1] = laColour[1];
                    lpDst[2] = laColour[2];
                    // lpDst[3] preserved: the guest merges the read colour over the
                    // old target with vrlimi128 v0,v13,1,0. FLAG: mask=1 is read as
                    // "keep word 3 (.w)" per the VMX128 field-mask convention; the
                    // preserved lane was not byte-verified against a golden dump.
                    return true;
                }
                default:
                    continue;
            }
        }
        else if ( liType == 5 )
        {
            // Colour keyframes also accept "<ColourName>Power": read one scalar and
            // scale the whole vec4 by it (guest: vspltw + vmulfp128).
            const size_t luNameLen = strlen( lpDesc->mName );
            if ( strncmp( lpFieldName, lpDesc->mName, luNameLen ) == 0
              && strcmp( lpFieldName + luNameLen, "Power" ) == 0 )
            {
                f32 lfPower;
                if ( ConsumeFieldValue( lfPower, lpFile ) )
                {
                    f32* const lpColour =
                        reinterpret_cast<f32*>( lpBase + lpDesc->mOffset );
                    lpColour[0] *= lfPower;
                    lpColour[1] *= lfPower;
                    lpColour[2] *= lfPower;
                    lpColour[3] *= lfPower;
                    return true;
                }
            }
        }
    }

    return false;
}

// ConsumeFieldValue<ScatteringData> is a sibling wave (its descriptor table was
// not part of this recovery); keep it an external reference rather than
// implicitly instantiating a table-less body in this TU.
extern template bool ConsumeFieldValue< ScatteringData >(
    ScatteringData& lrData, const char* lpFieldName, FILE* lpFile );

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

// ---------------------------------------------------------------------------
// Explicit instantiations of the shared ConsumeFieldValue<T> body -- one per
// recovered field table.
//   @ 0x82679160 / 0x82679528 / 0x82679CB8 / 0x8267A080
// ---------------------------------------------------------------------------
template bool ConsumeFieldValue< BrnEffects::BloomData >(
    BrnEffects::BloomData& lrData, const char* lpFieldName, FILE* lpFile );
template bool ConsumeFieldValue< BrnEffects::VignetteData >(
    BrnEffects::VignetteData& lrData, const char* lpFieldName, FILE* lpFile );
template bool ConsumeFieldValue< LightingData >(
    LightingData& lrData, const char* lpFieldName, FILE* lpFile );
template bool ConsumeFieldValue< CloudsData >(
    CloudsData& lrData, const char* lpFieldName, FILE* lpFile );
}
}
