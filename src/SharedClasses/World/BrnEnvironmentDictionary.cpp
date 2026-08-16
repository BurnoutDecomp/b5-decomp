#include "SharedClasses/World/BrnEnvironmentDictionary.h"
#include "SharedClasses/World/BrnEnvironmentTimeLine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy

// =============================================================================================
// SharedClasses/World/BrnEnvironmentDictionary.cpp
//
// The two BuildResourceName helpers of the environment-settings in-place resources.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (envstream, wave 2026-08-16).
//
//   BrnWorld::EnvironmentSettings::Dictionary::BuildResourceName @0x827B03B8
//   BrnWorld::EnvironmentSettings::TimeLine::BuildResourceName   -- [FLAG BLOCKED], see below
//
// The two structs themselves are homed in the headers above (owned by this wave's `envdata`
// group); this TU carries only the bodies, so it has no layout opinion of its own.
// =============================================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{

// ---------------------------------------------------------------------------------------------
// @0x827B03B8 (HOLES_DUMP.md -- no JSON; disassembled by the conductor).
//
// Write the dictionary's resource name into the caller's buffer and report success. The X360
// body is a two-step the compiler produced from one `strcpy`-shaped source line:
//     0x827B03C8  lis  r11, aEnvDictionary
//     0x827B03D4  addi r3, r1, var_20          ; a 16-byte STACK copy
//     0x827B03D8  li   r5, 0xF                 ; 15 == strlen("ENV_DICTIONARY") + NUL
//     0x827B03DC  bl   memcpy
//     0x827B03EC  ...  the byte-at-a-time copy of that stack buffer into r31 (the out param),
//                      stopping after the NUL
//     0x827B0400  li   r3, 1                   ; return true
// i.e. the literal is materialised on the stack (the compiler's inlined 15-byte constructor for
// a local `char[16]`) and then copied out with a NUL-terminated loop. Semantic parity is the one
// bounded copy below; the intermediate stack buffer has no observable effect.
//
// STATIC: the call site (Prepare @0x827D4CDC) passes ONE register -- r3 == the destination
// buffer -- so there is no `this`. dwarfdump cannot distinguish static from non-static, so the
// asm is the authority here (AGENTS.md).
//
// The name is the id Prepare hashes to acquire the dictionary resource from pool 16.
// ---------------------------------------------------------------------------------------------
bool Dictionary::BuildResourceName( char* lpacName )
{
    // aEnvDictionary @ the X360 rodata reached by `lis/addi r11, aEnvDictionary` -- 14 chars + NUL.
    static const char KAC_DICTIONARY_RESOURCE_NAME[] = "ENV_DICTIONARY";

    CGS_ASSERT( lpacName != 0, "lpacName" );
    if ( lpacName == 0 )
    {
        return false;
    }

    memcpy( lpacName, KAC_DICTIONARY_RESOURCE_NAME, sizeof( KAC_DICTIONARY_RESOURCE_NAME ) );
    return true;
}

// ---------------------------------------------------------------------------------------------
// [FLAG BLOCKED: no attested body -- TimeLine::BuildResourceName is not reachable in the ARTIST
// image and has no export.]
//
// DWARF declares `bool TimeLine::BuildResourceName(char*, const char*)`
// (SharedClasses/World/BrnEnvironmentTimeLine.h:83) and the second argument is, by the wave
// note, the "HH_MM" time-of-day stamp of a keyframe. But:
//   * there is no .ida-exports JSON for it (the exporter emits every NAMED function), and
//   * NOTHING in the whole export references it or any "ENV_..." keyframe-name literal -- see
//     the grep + output pasted in the report's BLOCKED section. StreamIn @0x827D31E8, which the
//     wave note suspected of inlining it, does not: it builds no keyframe names at all, it only
//     acquires the season TimeLine resource by the dictionary's own macResourceName string.
// So the format string that would turn (timeline, "HH_MM") into a keyframe resource id is NOT
// recoverable from this image, and inventing one (e.g. "ENV_KF_%s_%s" from the shipped keyframe
// name ENV_KF_Paradise_ingame_junk_city_1200) would be fabrication.
//
// This body therefore does the safe thing -- it produces an EMPTY name and reports failure, so a
// future caller cannot silently acquire a wrong resource -- and the missing item is listed under
// BLOCKED with a CONDUCTOR DUMP REQUEST for the function's address/body.
// ---------------------------------------------------------------------------------------------
bool TimeLine::BuildResourceName( char* lpacName, const char* lpcTime )
{
    (void)lpcTime;

    if ( lpacName != 0 )
    {
        lpacName[ 0 ] = '\0';
    }
    return false;
}

}
}
