#ifndef BRN_SOUND_VEHICLES_ENGINES_LOOP_MODEL_DATA_H
#define BRN_SOUND_VEHICLES_ENGINES_LOOP_MODEL_DATA_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::Engines loop-model serialized data
//   SharedClasses/Sound/Engines/BrnSoundLoopModelData.h (DWARF home) +
//   SharedClasses/Sound/Engines/BrnSoundLoopModelData.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This is the on-disk (serialized)
// description of a vehicle engine loop model: a LoopModelData header points at an
// array of Partials, each Partial points at an array of Graphs, each Graph points
// at an array of Points. When the resource is loaded the embedded pointers are
// stored as BYTE OFFSETS relative to the resource's load base; FixUp adds the load
// base to turn them into real pointers and FixDown subtracts it to turn them back
// into offsets before the resource is written/relocated.
//
// This TU bodies the relocation walkers (the boot-trace functions):
//   LoopModelData::FixDown @ 0x82680160     LoopModelData::FixUp @ 0x826800E8
//   Partial::FixDown       @ 0x8267F510     Partial::FixUp       @ 0x8267F4B0
//
// The walkers operate on serialized OFFSET pointers, so the rebase is real pointer
// arithmetic against the load base. It is expressed BY NAME (mpaPartials /
// mpaGraphs / mpaPoints) — the base add/subtract is the documented relocation, not
// a cast used to hit a member offset.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): these structs are an ON-DISK layout
// whose embedded pointers are 4 bytes on X360. The member SEQUENCE mirrors the
// DWARF; absolute offsets are NOT asserted across the 32/64 pointer-width boundary
// (a 64-bit host widens the pointer members). Members are pinned BY NAME + ORDER.
//
// SIGNATURE NOTE (FixUp/FixDown argument): the committed resource-type wrapper
// (BrnSoundLoopModelResourceType.cpp) calls these workers with the load base as an
// `int` (CgsResource::GetLoadBase cast to int). The X360 FixUp pseudocode renders
// the argument as `_DWORD*` and reads `*a2`; that is Hex-Rays modelling the X360
// `MemoryResource` (a one-word wrapper around the base) passed by value — the value
// added in FixUp / subtracted in FixDown is the SAME load-base word the int carries.
// To match the committed wrapper's contract the workers take the base as `int`.
// FLAG: the DWARF declares the public overloads FixUp(const Resource&) /
// FixDown(const Resource&) / FixDown(MemoryResource); those public entry points are
// DEFERRED (resolved by the committed resource-type wrapper) — this home bodies the
// base-taking worker contract the wrapper already calls.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// BrnSoundLoopModelData.h:146 (DWARF). One graph control point (no embedded
// pointers -> no relocation of its own).
struct Point
{
    f32 mfXpos; // BrnSoundLoopModelData.h:176
    f32 mfYpos; // BrnSoundLoopModelData.h:177
};

// BrnSoundLoopModelData.h:190 (DWARF). A 2-axis graph: an array of Points plus the
// axis bookkeeping. Stride on X360 is 8 bytes (4-byte mpaPoints + 4 bytes of u8/i8).
struct Graph
{
    Point* mpaPoints;     // BrnSoundLoopModelData.h:221
    u8     mu8NumOfPoints; // BrnSoundLoopModelData.h:222
    s8     mi8XAxis;       // BrnSoundLoopModelData.h:223
    s8     mi8YAxis;       // BrnSoundLoopModelData.h:224
    s8     mPad;           // BrnSoundLoopModelData.h:225
};

// BrnSoundLoopModelData.h:240 (DWARF). One harmonic partial: an interned wave name,
// an array of Graphs, and the graph count. X360 stride is 12 bytes (4-byte Name +
// 4-byte mpaGraphs + 4 bytes for mu8NumOfGraphs and padding).
struct Partial
{
    static const u32 KU_MAX_WAVE_PATH = 128; // BrnSoundLoopModelData.h:243

    // CgsSoundUtils.h:39 (DWARF) Partial::Name. The interned wave name occupies one
    // 4-byte word on X360 (a name hash). Modelled here as the raw hash word so the
    // Partial stride and Graph-array offset match the serialized layout BY NAME
    // without pulling in the (8-byte-on-host) CgsSound::Playback::Name.
    // FLAG: shape-only 4-byte stand-in for the CgsSoundUtils Name; the relocation
    // walkers never touch it (they only rebase mpaGraphs / mpaPoints).
    struct Name
    {
        Name() : mHash(0) {}
        u32 mHash;
    };

    Name   mWaveName;       // BrnSoundLoopModelData.h:275
    Graph* mpaGraphs;       // BrnSoundLoopModelData.h:276
    u8     mu8NumOfGraphs;  // BrnSoundLoopModelData.h:277

    // @ 0x8267F4B0 — add the load base to mpaGraphs and
    // to every owned Graph's mpaPoints.
    int FixUp(int liBase);

    // @ 0x8267F510 — subtract the load base from every
    // owned Graph's mpaPoints and from mpaGraphs.
    int FixDown(int liBase);
};

// BrnSoundLoopModelData.h:75 (DWARF). The loop-model resource header: signature /
// version, the Partial array, its count and the total serialized size.
struct LoopModelData
{
    static const u32 KU32_VERSION   = 7;          // BrnSoundLoopModelData.h:79
    static const u32 KU32_SIGNATURE = 1497648705; // BrnSoundLoopModelData.h:81

    u32      muVersion;       // BrnSoundLoopModelData.h:125
    u32      muSignature;     // BrnSoundLoopModelData.h:126
    Partial* mpaPartials;     // BrnSoundLoopModelData.h:129
    u32      muNumOfPartials; // BrnSoundLoopModelData.h:130
    u32      muSizeInBytes;   // BrnSoundLoopModelData.h:131

    // @ 0x826800E8 — add the load base to mpaPartials,
    // then FixUp every owned Partial.
    int FixUp(int liBase);

    // @ 0x82680160 — FixDown every owned Partial, then
    // subtract the load base from mpaPartials.
    int FixDown(int liBase);
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_LOOP_MODEL_DATA_H
