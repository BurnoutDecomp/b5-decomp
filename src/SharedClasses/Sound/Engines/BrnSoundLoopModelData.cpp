#include "SharedClasses/Sound/Engines/BrnSoundLoopModelData.h"

// =============================================================================
// BrnSound::Vehicles::Engines loop-model relocation walkers — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSoundLoopModelData.h for the
// serialized-offset / load-base relocation rationale.
//
// Each embedded pointer is stored on disk as a byte OFFSET from the load base.
// FixUp turns offsets into live pointers (offset + base); FixDown turns live
// pointers back into offsets (pointer - base). The add/subtract is expressed BY
// NAME against each owning member (mpaPartials / mpaGraphs / mpaPoints) — it is the
// documented relocation, not a cast to reach a member offset.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// Rebase helpers: add / subtract the load base to a serialized offset-pointer.
// The stored value is a byte offset (FixUp input) or a live address (FixDown
// input); both are reinterpreted through char* so the arithmetic is in bytes,
// matching the X360 integer add/subtract.
namespace
{
    template <typename T>
    inline T* RebaseUp(T* lpOffset, int liBase)
    {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(lpOffset) + liBase);
    }

    template <typename T>
    inline T* RebaseDown(T* lpPointer, int liBase)
    {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(lpPointer) - liBase);
    }
}

// ---------------------------------------------------------------------------
// Partial::FixUp  @ 0x8267F4B0
//
// If this Partial owns a graph array, rebase mpaGraphs, then rebase each owned
// Graph's (non-null) mpaPoints. X360: read mpaGraphs (+4), add base, store; loop
// mu8NumOfGraphs (+8) graphs (stride 8) adding base to each graph's mpaPoints (+0)
// when non-null.
// ---------------------------------------------------------------------------
int Partial::FixUp(int liBase)
{
    if (mpaGraphs)
    {
        mpaGraphs = RebaseUp(mpaGraphs, liBase);
        for (u32 i = 0; i < mu8NumOfGraphs; ++i)
        {
            if (mpaGraphs[i].mpaPoints)
            {
                mpaGraphs[i].mpaPoints = RebaseUp(mpaGraphs[i].mpaPoints, liBase);
            }
        }
    }
    return static_cast<int>(reinterpret_cast<intptr_t>(this)); // X360 returns `this` (r3); the resource-type wrapper discards it
}

// ---------------------------------------------------------------------------
// Partial::FixDown  @ 0x8267F510
//
// Mirror of FixUp in the opposite order: while mpaGraphs is still live, subtract
// the base from each owned Graph's (non-null) mpaPoints, then subtract the base
// from mpaGraphs itself.
// ---------------------------------------------------------------------------
int Partial::FixDown(int liBase)
{
    if (mpaGraphs)
    {
        for (u32 i = 0; i < mu8NumOfGraphs; ++i)
        {
            if (mpaGraphs[i].mpaPoints)
            {
                mpaGraphs[i].mpaPoints = RebaseDown(mpaGraphs[i].mpaPoints, liBase);
            }
        }
        mpaGraphs = RebaseDown(mpaGraphs, liBase);
    }
    return static_cast<int>(reinterpret_cast<intptr_t>(this)); // X360 returns `this` (r3); the resource-type wrapper discards it
}

// ---------------------------------------------------------------------------
// LoopModelData::FixUp  @ 0x826800E8
//
// If the header owns a Partial array, rebase mpaPartials, then FixUp every owned
// Partial. X360: read mpaPartials (+8); if non-null, add base, store, then loop
// muNumOfPartials (+12) partials (stride 12) calling Partial::FixUp.
// ---------------------------------------------------------------------------
int LoopModelData::FixUp(int liBase)
{
    if (mpaPartials)
    {
        mpaPartials = RebaseUp(mpaPartials, liBase);
        for (u32 i = 0; i < muNumOfPartials; ++i)
        {
            mpaPartials[i].FixUp(liBase);
        }
    }
    return static_cast<int>(reinterpret_cast<intptr_t>(this)); // X360 returns `this` (r3); the resource-type wrapper discards it
}

// ---------------------------------------------------------------------------
// LoopModelData::FixDown  @ 0x82680160
//
// Mirror of FixUp: while mpaPartials is still live, FixDown every owned Partial,
// then subtract the base from mpaPartials. X360 guards the partial loop on
// muNumOfPartials (+12) and only un-rebases mpaPartials (+8) when it is non-null.
// ---------------------------------------------------------------------------
int LoopModelData::FixDown(int liBase)
{
    if (mpaPartials)
    {
        for (u32 i = 0; i < muNumOfPartials; ++i)
        {
            mpaPartials[i].FixDown(liBase);
        }
        mpaPartials = RebaseDown(mpaPartials, liBase);
    }
    return static_cast<int>(reinterpret_cast<intptr_t>(this)); // X360 returns `this` (r3); the resource-type wrapper discards it
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
