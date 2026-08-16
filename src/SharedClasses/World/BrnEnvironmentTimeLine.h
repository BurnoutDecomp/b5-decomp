#ifndef BRN_ENVIRONMENT_TIMELINE_H
#define BRN_ENVIRONMENT_TIMELINE_H

// ---------------------------------------------------------------------------
// SharedClasses/World/BrnEnvironmentTimeLine.h  (canonical home for
// BrnWorld::EnvironmentSettings::TimeLine)
//
// The season TIME LINE resource (registry type 0x10013 / 65555): per world
// LOCATION, an ascending array of keyframe TIMES (seconds of day) and a parallel
// array of pointers to the Keyframe resources that sit at those times. It is an
// IN-PLACE resource: the loaded blob IS this object, so this declaration is a
// WIRE CONTRACT with tools/assets/bundles/env_transcode.py (_relayout_timeline).
//
// Member NAMES/ORDER/TYPES are the DecFIGS DWARF verbatim
// (references/DecFIGS/dwarfdump/SharedClasses/World/BrnEnvironmentTimeLine.h:
//  TimeLine    { muVersion :66, LocationData :69, muLocationCnt :77, mpLocationDatii :78 }
//  LocationData{ muKeyframeCnt :71, mpfKeyframeTimes :72, mppKeyframes :73 }
//  Construct :61, BuildResourceName :83).
// The X360 relocation code (TimeLineResourceType::FixUp @0x8267E128 / FixDown
// @0x8267E0C8) attests the CONSOLE offsets: TimeLine +0/+4/+8 (stride 12 over
// LocationData) and LocationData +0/+4/+8.
//
// ⚠️ GUEST vs HOST OFFSET TABLE (the recurring X360-values-on-x64 bug class).
// The two pointer members widen 4 -> 8 bytes, so LocationData's stride grows and
// the whole nested arena moves:
//
//   TimeLine                guest (X360)      host (x64)
//     muVersion               +0x00             +0x00
//     muLocationCnt           +0x04             +0x04
//     mpLocationDatii         +0x08             +0x08
//     sizeof                   12                16
//
//   TimeLine::LocationData  guest (X360)      host (x64)
//     muKeyframeCnt           +0x00             +0x00
//     mpfKeyframeTimes        +0x04             +0x08
//     mppKeyframes            +0x08             +0x10
//     sizeof                   12                24
//
//   mppKeyframes element      4 bytes           8 bytes
//   mpfKeyframeTimes element  4 bytes           4 bytes (f32, unchanged)
//
// The _AssertLayout() block at the bottom pins the host column and the two
// pointer-INVARIANT facts (first member, and the f32 time element width).
//
// HOW mppKeyframes IS FILLED. FixUp NULLs every slot (the X360 zero-loop), then
// CgsResource::Pool::ResolveImportsForEntry writes each Keyframe resource's main
// memory pointer into it from the bundle's IMPORT table -- MEASURED in the retail
// bundle: ENV_TL_Paradise_ingame_junk carries 9 import entries at file offsets
// 0x20..0x40, one per city_HHMM keyframe, in ascending-time order. Nothing in
// EnvironmentManager::StreamIn @0x827D31E8 writes those slots; the import pass is
// the only writer. Because the slot is 8 bytes on x64, the converter has to move
// those import offsets too (see env_transcode.py).
// ---------------------------------------------------------------------------

#include <cstddef>   // offsetof (the _AssertLayout pins)

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
    // Pointer-only use. The Keyframe record's canonical home in this tree is
    // GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h (the DWARF
    // filed it under SharedClasses/World/BrnEnvironmentKeyframe.h; the committed
    // tree wins on placement). A forward declaration keeps a SharedClasses header
    // from pulling a GameSource header -- the documented pointer-only exception.
    struct Keyframe;

    struct TimeLine
    {
        // DWARF BrnEnvironmentTimeLine.h:61. [FLAG NOT X360-ATTESTED] no standalone Construct
        // symbol exists in BURNOUT_X360_ARTIST for this struct and no call site was found; the
        // timeline is built by the offline data compiler and consumed in place at run time.
        // Declared because the DWARF declares it -- do NOT invent a body.
        void Construct();

        // DWARF BrnEnvironmentTimeLine.h:66
        u32 muVersion;

        // DWARF BrnEnvironmentTimeLine.h:69 -- one record per world location.
        struct LocationData
        {
            u32        muKeyframeCnt;      // :71
            f32*       mpfKeyframeTimes;   // :72  ascending seconds-of-day
            Keyframe** mppKeyframes;       // :73  parallel; filled by the import pass

            // Compile-time-only layout pin (see the block at the bottom).
            static void _AssertLayout();
        };

        u32           muLocationCnt;       // :77
        LocationData* mpLocationDatii;     // :78

        // DWARF BrnEnvironmentTimeLine.h:83 -- builds the per-location timeline resource name
        // into the caller's buffer.
        //
        // ⚠️ [FLAG NOT X360-ATTESTED] Unlike Dictionary::BuildResourceName (a real X360 function
        // at 0x827B03B8), there is NO TimeLine::BuildResourceName symbol in
        // BURNOUT_X360_ARTIST and no call site: EnvironmentManager::StreamIn @0x827D31E8 hashes
        // Dictionary::SeasonData::macResourceName DIRECTLY
        // (`v23 = (season << 8) + seasonBase; CgsResource::ID::HashString(v23)`), so on the
        // X360 spine this helper is either PS3-only or fully inlined away. Declared here to
        // match the DWARF shape -- envstream must NOT invent a body for it; use macResourceName
        // the way StreamIn does.
        //
        // ⭐ Static-vs-member is UNRESOLVED for the same reason (no asm to arbitrate, and
        // dwarfdump cannot tell). Left non-static to match the DWARF's `public:` placement;
        // if a body is ever needed, settle it from a call site first -- the sibling
        // Dictionary::BuildResourceName turned out to be STATIC.
        bool BuildResourceName( char* lpacName, const char* lpacLocation );

        // Compile-time-only layout pin (see the block at the bottom).
        static void _AssertLayout();
    };

    // The on-disk version word TimeLineResourceType::FixUp @0x8267E128 checks
    // (`cmplwi r11, 1`). MEASURED == 1 in the retail
    // ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE.x360 payload.
    static const u32 KU_ENVIRONMENT_TIMELINE_VERSION = 1;

    // ---- x64 layout pins (WIRE CONTRACT with env_transcode.py) ---------------
    inline void TimeLine::LocationData::_AssertLayout()
    {
        // Pointer-invariant on both targets: the count leads the record and the
        // time array is f32 either way.
        static_assert( offsetof( TimeLine::LocationData, muKeyframeCnt ) == 0,
                       "TimeLine::LocationData::muKeyframeCnt leads the record on both targets" );
        static_assert( sizeof( f32 ) == 4, "keyframe times stay 4-byte floats" );
        // Host column of the table in the banner.
        static_assert( offsetof( TimeLine::LocationData, mpfKeyframeTimes ) == 0x08,
                       "TimeLine::LocationData::mpfKeyframeTimes @0x08 on x64 (console +0x04)" );
        static_assert( offsetof( TimeLine::LocationData, mppKeyframes ) == 0x10,
                       "TimeLine::LocationData::mppKeyframes @0x10 on x64 (console +0x08)" );
        static_assert( sizeof( TimeLine::LocationData ) == 24,
                       "TimeLine::LocationData stride is 24 on x64 (console 12)" );
        static_assert( sizeof( Keyframe* ) == 8,
                       "mppKeyframes elements are 8-byte host pointers (console 4)" );
    }

    inline void TimeLine::_AssertLayout()
    {
        static_assert( offsetof( TimeLine, muVersion ) == 0,
                       "TimeLine::muVersion leads the record on both targets (FixUp reads *a2)" );
        static_assert( offsetof( TimeLine, muLocationCnt )  == 0x04, "TimeLine::muLocationCnt @0x04" );
        static_assert( offsetof( TimeLine, mpLocationDatii ) == 0x08, "TimeLine::mpLocationDatii @0x08" );
        static_assert( sizeof( TimeLine ) == 16, "TimeLine header is 16 bytes on x64 (console 12)" );
    }
}
}

#endif // BRN_ENVIRONMENT_TIMELINE_H
