#ifndef BRN_ENVIRONMENT_DICTIONARY_H
#define BRN_ENVIRONMENT_DICTIONARY_H

// ---------------------------------------------------------------------------
// SharedClasses/World/BrnEnvironmentDictionary.h  (canonical home for
// BrnWorld::EnvironmentSettings::Dictionary)
//
// The environment-settings DICTIONARY resource (registry type 0x10014 / 65556):
// the list of SEASONS (each naming its timeline resource, its keyframe bundle and
// its colour-cubes bundle) and the list of world LOCATIONS. It is an IN-PLACE
// resource -- the loaded blob IS this object -- so this declaration is a WIRE
// CONTRACT with tools/assets/bundles/env_transcode.py (_relayout_dictionary).
//
// Member NAMES/ORDER/TYPES are the DecFIGS DWARF verbatim
// (references/DecFIGS/dwarfdump/SharedClasses/World/BrnEnvironmentDictionary.h:
//  Dictionary  { muVersion :65, SeasonData :68, muSeasonCnt :76, mpSeasonDatii :77,
//                LocationData :80, muLocationCnt :86, mpLocationDatii :87 }
//  SeasonData  { macResourceName[128] :70, macBundle[64] :71,
//                macColourCubesBundle[64] :72 }
//  LocationData{ macName[64] :82 }
//  Construct :60, BuildResourceName :91).
// The X360 attests the console offsets two ways: DictionaryResourceType::FixUp
// @0x8267E278 rebases +8 and +0x10, and GetSerialisedResourceDescriptor
// @0x8267D310 sizes the payload as (muSeasonCnt<<8) + (muLocationCnt<<6) -- i.e.
// SeasonData stride 256 and LocationData stride 64.
//
// ⚠️ GUEST vs HOST OFFSET TABLE (the recurring X360-values-on-x64 bug class).
// Only the HEADER moves: both element records are pure char arrays and are
// stride-identical on the two targets.
//
//   Dictionary              guest (X360)      host (x64)
//     muVersion               +0x00             +0x00
//     muSeasonCnt             +0x04             +0x04
//     mpSeasonDatii           +0x08             +0x08
//     muLocationCnt           +0x0C             +0x10
//     mpLocationDatii         +0x10             +0x18
//     sizeof                   20                32
//
//   Dictionary::SeasonData   256               256   (128 + 64 + 64, no pointers)
//   Dictionary::LocationData  64                64   (char[64],      no pointers)
//
// PAYLOAD SIZE IS UNCHANGED, and so is GetSerialisedResourceDescriptor's formula:
//   size = ((((muSeasonCnt << 8) + 0x2F) & ~0xF) + (muLocationCnt << 6) + 0xF) & ~0xF
// The 0x2F is "the season array's 16-aligned start (0x20) + 0xF"; align16(20) == 32
// on the console and align16(32) == 32 on the host, so the season array still starts
// at +0x20 and the constant still holds. MEASURED against the retail
// ENVIRONMENTSETTINGS/DICTIONARY.BUNDLE.x360 payload: 1 season + 1 location ->
// 0x160 == 352 bytes, exactly the file size, seasons at +0x20 and locations at +0x120.
// ---------------------------------------------------------------------------

#include <cstddef>   // offsetof (the _AssertLayout pins)

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
    struct Dictionary
    {
        // DWARF BrnEnvironmentDictionary.h:60. [FLAG NOT X360-ATTESTED] no standalone
        // Construct symbol exists in BURNOUT_X360_ARTIST for this struct and no call site
        // was found; the dictionary is built by the offline data compiler and consumed in
        // place at run time. Declared because the DWARF declares it -- do NOT invent a body.
        void Construct();

        // DWARF BrnEnvironmentDictionary.h:65
        u32 muVersion;

        // DWARF BrnEnvironmentDictionary.h:68
        struct SeasonData
        {
            char macResourceName[128];       // :70  e.g. "Paradise_ingame_junk"
            char macBundle[64];              // :71  keyframe bundle leaf name
            char macColourCubesBundle[64];   // :72  colour-cubes bundle leaf name

            static void _AssertLayout();
        };

        u32         muSeasonCnt;             // :76
        SeasonData* mpSeasonDatii;           // :77

        // DWARF BrnEnvironmentDictionary.h:80
        struct LocationData
        {
            char macName[64];                // :82

            static void _AssertLayout();
        };

        u32           muLocationCnt;         // :86
        LocationData* mpLocationDatii;       // :87

        // DWARF BrnEnvironmentDictionary.h:91 -- writes the dictionary's own resource name
        // ("ENV_DICTIONARY") into the caller's buffer. Body: envstream. X360
        // BuildResourceName @0x827B03B8, called from EnvironmentManager::Prepare @0x827D4CE0.
        //
        // ⭐ STATIC, and the ASM is what says so -- dwarfdump cannot distinguish a static
        // member from a non-static one, and this one has NO implicit `this`:
        //     0x827B03C8  lis   r11, aEnvDictionary@ha
        //     0x827B03CC  mr    r31, r3            ; r3 (the FIRST arg) is the DESTINATION
        //     0x827B03D4  addi  r3, r1, var_20     ; Dst = a 15-byte stack copy of the literal
        //     0x827B03DC  bl    memcpy             ; Src = "ENV_DICTIONARY", Size = 0xF
        //     ...          byte-copy loop from the stack copy to r31, then `li r3, 1`
        // and the only call site sets r3 alone:
        //     0x827D4CDC  addi  r3, r1, 0x1F0+var_180
        //     0x827D4CE0  bl    ...Dictionary__BuildResourceName
        // Declaring it non-static would give it a phantom `this` and a wrong call shape.
        static bool BuildResourceName( char* lpacName );

        static void _AssertLayout();
    };

    // The on-disk version word DictionaryResourceType::FixUp @0x8267E278 checks
    // (`cmplwi r11, 2`). MEASURED == 2 in the retail
    // ENVIRONMENTSETTINGS/DICTIONARY.BUNDLE.x360 payload.
    static const u32 KU_ENVIRONMENT_DICTIONARY_VERSION = 2;

    // ---- layout pins (WIRE CONTRACT with env_transcode.py) -------------------
    inline void Dictionary::SeasonData::_AssertLayout()
    {
        // Pointer-INVARIANT: identical on console and host.
        static_assert( sizeof( Dictionary::SeasonData ) == 256,
                       "SeasonData stride is 256 on both targets (GetSerialisedResourceDescriptor's <<8)" );
        static_assert( offsetof( Dictionary::SeasonData, macResourceName )      == 0,   "macResourceName @0" );
        static_assert( offsetof( Dictionary::SeasonData, macBundle )            == 128, "macBundle @128" );
        static_assert( offsetof( Dictionary::SeasonData, macColourCubesBundle ) == 192, "macColourCubesBundle @192" );
    }

    inline void Dictionary::LocationData::_AssertLayout()
    {
        static_assert( sizeof( Dictionary::LocationData ) == 64,
                       "LocationData stride is 64 on both targets (GetSerialisedResourceDescriptor's <<6)" );
    }

    inline void Dictionary::_AssertLayout()
    {
        static_assert( offsetof( Dictionary, muVersion ) == 0,
                       "Dictionary::muVersion leads the record on both targets (FixUp reads *a2)" );
        static_assert( offsetof( Dictionary, muSeasonCnt )     == 0x04, "Dictionary::muSeasonCnt @0x04" );
        static_assert( offsetof( Dictionary, mpSeasonDatii )   == 0x08, "Dictionary::mpSeasonDatii @0x08" );
        static_assert( offsetof( Dictionary, muLocationCnt )   == 0x10, "Dictionary::muLocationCnt @0x10 on x64 (console +0x0C)" );
        static_assert( offsetof( Dictionary, mpLocationDatii ) == 0x18, "Dictionary::mpLocationDatii @0x18 on x64 (console +0x10)" );
        static_assert( sizeof( Dictionary ) == 32, "Dictionary header is 32 bytes on x64 (console 20)" );
    }
}
}

#endif // BRN_ENVIRONMENT_DICTIONARY_H
