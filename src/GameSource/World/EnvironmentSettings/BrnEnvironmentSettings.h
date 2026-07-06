#ifndef BRN_ENVIRONMENT_SETTINGS_H
#define BRN_ENVIRONMENT_SETTINGS_H

#include "types.hpp"
#include <cstdio>

#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"

// BrnEffects::BloomData / VignetteData are defined in SharedClasses/Graphics/BrnEffectsData.h;
// only references are needed here for ParseEnvironmentFile's signature.
namespace BrnEffects
{
struct BloomData;
struct VignetteData;
}

// ============================================================================
// GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h
//
// Two BrnWorld::EnvironmentSettings namespace-scope helpers reconstructed from
// BURNOUT_X360_ARTIST.XEX. There is no Feb-2007 leak source / DecFIGS dwarfdump
// for these, so the shape is taken from the X360 pseudocode + disassembly.
//
//   FindKeyframeInds @ 0x827B0418 -- given an ascending array of keyframe time
//       stamps and a query time, find the bracketing keyframe pair and the
//       interpolation blend weights. Used by EnvironmentManager::
//       SetupTimeOfDayBlend.
//   HH_MM_SS         @ 0x827B0580 -- split a (possibly negative) seconds-of-day
//       value into hours / minutes / seconds, wrapping the day at 86400s. Used
//       by the EnvironmentSettings DebugComponent.
//
// These live in the BrnWorld::EnvironmentSettings namespace alongside the
// existing keyframe data structs (BrnEnvCloudsData / BrnEnvLightingData / ...).
// ============================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{
// @ 0x827B0418.
//
// Walks the ascending keyframe-time array lafTimes[0 .. luCount-1] and locates
// the index of the first time stamp >= the query time lfTime. From that it
// emits, for a keyframe pair, the bracketing indices and the two complementary
// blend weights that interpolate the query time between them:
//   *lruIndex1 -- the "later" keyframe index (the search index)
//   *lpfWeight0 -- blend weight applied to the "earlier" keyframe
//   *lruIndex0 -- the "earlier" keyframe index
//   *lpfWeight1 -- blend weight applied to the "later" keyframe (== 1 - weight0)
// Boundary cases (X360-faithful):
//   - query >= last time : clamp to (last, last), weight0 = 0, weight1 = 1
//   - query <  first time : clamp to (0, 0),       weight0 = 1, weight1 = 0
//   - in between          : linear blend between the bracketing pair
// Returns lruIndex1 (the X360 returns its result pointer).
//
// Stack/register parameter order matches the X360 ABI (r3..r8 + f1):
//   r3 lruIndex1, r4 lpfWeight0, r5 lruIndex0, r6 lpfWeight1, r7 lafTimes,
//   r8 luCount, f1 lfTime.
unsigned int* FindKeyframeInds( unsigned int* lruIndex1,
                                float*        lpfWeight0,
                                unsigned int* lruIndex0,
                                float*        lpfWeight1,
                                const float*  lafTimes,
                                unsigned int  luCount,
                                double        lfTime );

// @ 0x827B0580.
//
// Splits a seconds-of-day value into clock fields. A negative time is first
// wrapped forward by one day (+86400s). The integer second count is then peeled
// into seconds (% 60), minutes (% 60), and hours (% 24):
//   *lruHours   -- hours   (0..23)
//   *lruMinutes -- minutes (0..59)
//   *lpuSeconds -- seconds (0..59)
// Returns lruHours (the X360 returns its result pointer).
//
// Register parameter order matches the X360 ABI (r3..r5 + f1):
//   r3 lruHours, r4 lruMinutes, r5 lpuSeconds, f1 lfTimeSeconds.
unsigned int* HH_MM_SS( unsigned int* lruHours,
                        unsigned int* lruMinutes,
                        unsigned int* lpuSeconds,
                        double        lfTimeSeconds );

// ---------------------------------------------------------------------------
// Text-file environment-description parser helpers (BrnEnvironmentData.cpp in the
// original tree; folded into this TU's home here).
// ---------------------------------------------------------------------------

// @ 0x826759C8 -- format seconds-of-day into a 4-digit zero-padded "HHMM" field,
//   NUL at lpcOut[4].
void BuildTimeOfDay( char* lpcOut, float lfTimeSeconds );

// Sibling token consumers -- ConsumeBlanks / ConsumeEOL and the float-value
// ConsumeFieldValue are reconstructed in this batch; the char* string-token
// overload is a sibling wave (declared here so ParseEnvironmentFile compiles).
//   @ 0x82675BC8 ConsumeBlanks  -- skip runs of spaces; false at EOF.
//   @ 0x82675F58 ConsumeEOL     -- consume up to and including next newline.
//   @ 0x82675EE8 ConsumeFieldValue(char*, FILE*) -- read one string token (sibling).
bool ConsumeBlanks( FILE* lpFile );
bool ConsumeEOL( FILE* lpFile );
bool ConsumeFieldValue( char* lpBuffer, FILE* lpFile );

// @ 0x82675C60 -- read one float field value; false at EOF / immediate EOL.
bool ConsumeFieldValue( float& lrfValue, FILE* lpFile );

// Templated keyframe sub-block consumers (explicit specialisations exist for
// BloomData, VignetteData, ScatteringData, LightingData, CloudsData in sibling
// waves). NOT reconstructed in this batch.
template< typename T >
bool ConsumeFieldValue( T& lrData, const char* lpFieldName, FILE* lpFile );

// @ 0x82675B10 -- parse an "HHMM" integer time string into seconds-of-day.
void ParseTimeOfDay( float* lpfSeconds, const char* lpTimeStr );

// @ 0x8267CD70 -- parse a whole environment file (10-arg inlined form; the
//   colour-cube name array + weight array are separate register args in the X360,
//   hence the reference-to-array parameters).
bool ParseEnvironmentFile( float&                     lrfTimeOfDay,
                           char                       (&lacColourCubes)[4][256],
                           float                      (&lafColourCubeWeights)[4],
                           BrnEffects::BloomData&     lrBloom,
                           BrnEffects::VignetteData&  lrVignette,
                           char*                      lpName,
                           ScatteringData&            lrScattering,
                           LightingData&              lrLighting,
                           CloudsData&                lrClouds,
                           const char*                lpFilename );
}
}

#endif // BRN_ENVIRONMENT_SETTINGS_H
