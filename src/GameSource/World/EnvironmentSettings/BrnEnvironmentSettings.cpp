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

// ---------------------------------------------------------------------------
// The rest of the ConsumeFieldValue family (envfix round, 2026-08-16).
//
// All five share ONE guard, byte-identical to the float& form above and to
// ConsumeBlanks: `if (feof) return false; peek = (signed char)fgetc; ungetc(peek);
// if (peek == '\n') return false;` -- then one fscanf and `return true`. The
// per-arity difference is only the format string, which is why the guest emitted
// five near-clones (0x82675C60 / CD0 / D48 / DC8 / E50 / EE8).
//
// DWARF (SharedClasses/World/BrnEnvironmentData.cpp) names the whole family and
// its declaration shapes: :544 (float32_t&), :557 (float32_t&, float32_t&),
// :570 (three float32_t&), :583 (Vector2&), :597 (Vector3&), :611 (char*).
// ---------------------------------------------------------------------------

// @ 0x82675CD0 -- two semicolon-separated floats, "%f;%f" (DWARF :557).
bool ConsumeFieldValue( float& lrfValue0, float& lrfValue1, FILE* lpFile )
{
    if ( feof( lpFile ) )
        return false;

    const int liPeek = static_cast<signed char>( fgetc( lpFile ) );
    ungetc( liPeek, lpFile );
    if ( liPeek == '\n' )
        return false;

    fscanf( lpFile, "%f;%f", &lrfValue0, &lrfValue1 );
    return true;
}

// @ 0x82675D48 -- three semicolon-separated floats, "%f;%f;%f" (DWARF :570).
bool ConsumeFieldValue( float& lrfValue0, float& lrfValue1, float& lrfValue2, FILE* lpFile )
{
    if ( feof( lpFile ) )
        return false;

    const int liPeek = static_cast<signed char>( fgetc( lpFile ) );
    ungetc( liPeek, lpFile );
    if ( liPeek == '\n' )
        return false;

    fscanf( lpFile, "%f;%f;%f", &lrfValue0, &lrfValue1, &lrfValue2 );
    return true;
}

// @ 0x82675DC8 -- Vector2 (DWARF :583). Reads the pair through the two-float
// form, assembles it in a 16-byte stack slot whose upper half it first ZEROES
// (`li r10,0` / `std r10, 0(sp+0x68)` @0x82675E18-0x82675E2C), then stores the
// whole register over the target (lvx128 / stvx128 @0x82675E34-0x82675E38). So a
// successful read writes FOUR lanes: { x, y, 0, 0 }. On failure the target is
// untouched. Modelled with the tree's rw::math Vector2 (BrnCommonTypes.h), which
// is exactly that 16-byte four-lane register.
//
// ⚠ CORRECTED (envfix round): this overload was declared `ConsumeFieldValue(u32*,
// FILE*)` below, marked INFERRED. It is not a u32 reader -- the body reads two
// FLOATS. No field of type 1 exists in ANY of the five recovered descriptor
// tables, so the mis-typing was never reachable, but the fix removes the trap.
bool ConsumeFieldValue( Vector2& lrv2Value, FILE* lpFile )
{
    float lfX;
    float lfY;
    if ( !ConsumeFieldValue( lfX, lfY, lpFile ) )
        return false;

    lrv2Value.x = lfX;
    lrv2Value.y = lfY;
    lrv2Value.z = 0.0f;
    lrv2Value.w = 0.0f;
    return true;
}

// @ 0x82675E50 -- Vector3 (DWARF :597), the COLOUR reader that field types 4 / 5
// and 9 use. Same shape as the Vector2 form one arity up: three floats through
// the "%f;%f;%f" reader, the fourth lane zeroed (`li r10,0` /
// `stw r10, 0(sp+0x6C)` @0x82675EA4-0x82675EC0) and all four stored with
// stvx128 @0x82675ECC. So a successful read writes { x, y, z, 0 } -- the w lane
// IS cleared, which is why every colour that comes out of an environment text
// file has w == 0, exactly like the CRT-initialised defaults.
//
// Modelled as f32* rather than Vector3& because the tree's ScatteringData /
// LightingData / CloudsData spell their colour members as float[4] arrays (see
// those headers); the callers below pass &member, so the four writes land on the
// same four lanes the guest's stvx128 covers.
bool ConsumeFieldValue( f32* lpv3Colour, FILE* lpFile )
{
    float lfX;
    float lfY;
    float lfZ;
    if ( !ConsumeFieldValue( lfX, lfY, lfZ, lpFile ) )
        return false;

    lpv3Colour[0] = lfX;
    lpv3Colour[1] = lfY;
    lpv3Colour[2] = lfZ;
    lpv3Colour[3] = 0.0f;
    return true;
}

// @ 0x82675EE8 -- one whitespace-delimited string token, "%s" (DWARF :611).
// Declared in BrnEnvironmentSettings.h; ParseEnvironmentFile calls it for the
// time-of-day field, the keyframe name and the four colour-cube URIs.
bool ConsumeFieldValue( char* lpBuffer, FILE* lpFile )
{
    if ( feof( lpFile ) )
        return false;

    const int liPeek = static_cast<signed char>( fgetc( lpFile ) );
    ungetc( liPeek, lpFile );
    if ( liPeek == '\n' )
        return false;

    fscanf( lpFile, "%s", lpBuffer );
    return true;
}

// ===========================================================================
// Templated keyframe field-value consumers  ConsumeFieldValue<T>
//
//   @ 0x82679160  ConsumeFieldValue<BrnEffects::BloomData>
//   @ 0x82679528  ConsumeFieldValue<BrnEffects::VignetteData>
//   @ 0x826798F0  ConsumeFieldValue<EnvironmentSettings::ScatteringData>
//   @ 0x82679CB8  ConsumeFieldValue<EnvironmentSettings::LightingData>
//   @ 0x8267A080  ConsumeFieldValue<EnvironmentSettings::CloudsData>
//
// The ScatteringData instantiation was previously believed absent: it has no JSON
// of its own in .ida-exports (IDA folded 0x826798F0 into the 0x82679528 record),
// but the symbol is real -- it is named in the xrefs_to of 0x82675C60 / 0x82675DC8
// / 0x82675E50 and in the xrefs_from of ParseEnvironmentFile @0x8267CD70.
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

// --- @0x820A4690 : EnvironmentSettings::ScatteringData (22 records) ---
// Recovered in the envfix round from the shipped image. The table ADDRESS comes
// from the instantiation's own prologue (`addis r11,r0,0x820A` @0x826798FC /
// `addi r25,r11,0x4690` @0x82679904 -- and that is the ONLY reference to
// 0x820A4690 in the whole 0x82000000..0x82D00000 text range). The RECORD COUNT
// comes from the same body's end-of-table pointer, `addi r11,r25,0x17B4`
// @0x82679C18: 0x17B4 == 22*264 + 260, exactly the form the LightingData twin
// uses (`addi r11,r25,0x0A4C` @0x82679FE0 == 9*264 + 260). It is corroborated
// geometrically: 22 records starting at 0x820A4690 end at 0x820A5D40, which is
// where the LightingData table below begins.
const FieldDescriptor kaScatteringFields[] =
{
    { "SkyTopColour",           5, 0x00 },
    { "SkyHorColour",           5, 0x10 },
    { "SkySunColour",           5, 0x20 },
    { "SkyHorShape",            0, 0x30 },
    { "SkySunShape",            0, 0x34 },
    { "SkyDarkness",            0, 0x38 },
    { "SkyHorBleedHeight",      0, 0x3C },
    { "SkyHorBleedWidth",       0, 0x40 },
    { "SkySunHorBleedBal",      0, 0x44 },
    { "ScattTopColour",         5, 0x50 },
    { "ScattHorColour",         5, 0x60 },
    { "ScattSunColour",         5, 0x70 },
    { "ScattHorShape",          0, 0x80 },
    { "ScattSunShape",          0, 0x84 },
    { "ScattDarkness",          0, 0x88 },
    { "ScattHorBleedHeight",    0, 0x8C },
    { "ScattHorBleedWidth",     0, 0x90 },
    { "ScattSunHorBleedBal",    0, 0x94 },
    { "ScattDist0",             0, 0x98 },
    { "ScattDist1",             0, 0x9C },
    { "ScattPower",             0, 0xA0 },
    { "ScattCap",               0, 0xA4 },
};
// Independent confirmation of the ScatteringData layout: these 22 offsets are
// exactly the 22 named fields BrnEnvScatteringData.h declares, and NOTHING sits
// at +0x48 -- the same hole ScatteringData::SetToBlend @0x827AF468 skips.

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
// type-generic). All FIVE instantiations the X360 emitted now have an overload.
const FieldDescriptor* GetEnvFieldTable( const BrnEffects::BloomData*,    s32& lrCount )
{ lrCount = 3;  return kaBloomFields; }
const FieldDescriptor* GetEnvFieldTable( const BrnEffects::VignetteData*, s32& lrCount )
{ lrCount = 8;  return kaVignetteFields; }
const FieldDescriptor* GetEnvFieldTable( const ScatteringData*,           s32& lrCount )
{ lrCount = 22; return kaScatteringFields; }
const FieldDescriptor* GetEnvFieldTable( const LightingData*,             s32& lrCount )
{ lrCount = 9;  return kaLightingFields; }
const FieldDescriptor* GetEnvFieldTable( const CloudsData*,               s32& lrCount )
{ lrCount = 15; return kaCloudsFields; }
} // namespace

// The two sibling readers the store paths below use -- @0x82675E50 (Vector3, the
// colour reader for type codes 4 / 5 / 9) and @0x82675DC8 (Vector2, type code 1)
// -- are now DEFINED above in this TU, so the forward declarations that used to
// stand here are gone with the "sibling wave" note that came with them.
// The type-1 reader's element type is no longer INFERRED: its body reads two
// floats, so it is Vector2 (DWARF BrnEnvironmentData.cpp:583), not u32.

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
                    // Vector2 (@0x82675DC8): two floats, upper two lanes zeroed.
                    // No record of type 1 exists in any of the five tables, so
                    // this arm is unreachable in the shipped data -- it is kept
                    // because the guest's switch has it.
                    return ConsumeFieldValue( *reinterpret_cast<Vector2*>( lpTarget ), lpFile );

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
                    // lpDst[3] preserved. FLAG RETIRED (envfix round, 2026-08-16).
                    // The type-9 arm reads @0x8267938C-0x826793C0:
                    //     bl sub_82675E50            ; Vector3 reader -> stack {x,y,z,0}
                    //     lvx128 v13, r0, r31        ; v13 = the OLD TARGET
                    //     lvx128 v0,  r0, r11        ; v0  = the freshly READ colour
                    //     vrlimi128 v0, v13, 1, 0
                    //     stvx128 v0, r0, r31
                    // vD is the READ colour and vB is the OLD TARGET, so mask=1 ==
                    // "insert word 3 of vB" gives { colour.xyz, target.w } -- exactly
                    // the three-lane copy below. Two independent arguments agree:
                    // (a) the VMX128 field mask numbers words 0..3 from bit 3 down, so
                    //     1 selects word 3; (b) the Vector3 reader has ALREADY zeroed
                    //     the stack slot's w lane, so if the vrlimi did not restore the
                    //     target's w the instruction would be pure cost and a plain
                    //     stvx128 of the read vector would do -- and the opposite
                    //     reading (preserve word 0) would preserve RED, which is
                    //     meaningless for a colour field.
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

// ConsumeFieldValue<ScatteringData> @0x826798F0 is now instantiated HERE, from the
// descriptor table above. The `extern template` declaration that used to stand at
// this spot made the symbol an unresolved external -- which is exactly what it was
// at link time, because no other TU in the tree ever defined it.

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
