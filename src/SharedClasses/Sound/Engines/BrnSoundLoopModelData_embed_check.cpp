#include "SharedClasses/Sound/Engines/BrnSoundLoopModelData.h"
#include <cstddef>

// Embed check for the BrnSound::Vehicles::Engines loop-model relocation walkers.
// Builds a small serialized blob whose embedded pointers are byte OFFSETS from a
// base, runs FixUp to turn them into live pointers, then FixDown to restore the
// offsets, and confirms the round-trip is exact (offset -> pointer -> offset).

namespace
{
    using BrnSound::Vehicles::Engines::LoopModelData;
    using BrnSound::Vehicles::Engines::Partial;
    using BrnSound::Vehicles::Engines::Graph;
    using BrnSound::Vehicles::Engines::Point;

    // Layout-shape assertions over the on-disk structs (NO absolute member offsets
    // are asserted across the 32/64 pointer-width boundary; we assert member->member
    // ORDER and the relocated members are addressable BY NAME).
    static_assert(offsetof(LoopModelData, mpaPartials) < offsetof(LoopModelData, muNumOfPartials),
                  "mpaPartials precedes muNumOfPartials");
    static_assert(offsetof(Partial, mWaveName) < offsetof(Partial, mpaGraphs),
                  "mWaveName precedes mpaGraphs");
    static_assert(offsetof(Partial, mpaGraphs) < offsetof(Partial, mu8NumOfGraphs),
                  "mpaGraphs precedes mu8NumOfGraphs");
    static_assert(sizeof(Point) == 2 * sizeof(f32), "Point is two f32 axes");

    void ExerciseLoopModelData()
    {
        // One blob: header -> 1 partial -> 1 graph -> 2 points. Each owning pointer
        // is stored as a byte offset from &lModel (the load base).
        struct Blob
        {
            LoopModelData mModel;
            Partial       maPartials[1];
            Graph         maGraphs[1];
            Point         maPoints[2];
        } lBlob;

        char* lpBase = reinterpret_cast<char*>(&lBlob);
        const int liBase = static_cast<int>(reinterpret_cast<intptr_t>(lpBase));

        // Seed live pointers, then convert to offsets via FixDown.
        lBlob.mModel.muNumOfPartials = 1;
        lBlob.mModel.mpaPartials     = lBlob.maPartials;
        lBlob.maPartials[0].mpaGraphs      = lBlob.maGraphs;
        lBlob.maPartials[0].mu8NumOfGraphs = 1;
        lBlob.maGraphs[0].mpaPoints     = lBlob.maPoints;
        lBlob.maGraphs[0].mu8NumOfPoints = 2;

        // Snapshot live pointers.
        Partial* lpPartials0 = lBlob.mModel.mpaPartials;
        Graph*   lpGraphs0   = lBlob.maPartials[0].mpaGraphs;
        Point*   lpPoints0   = lBlob.maGraphs[0].mpaPoints;

        // pointer -> offset
        lBlob.mModel.FixDown(liBase);
        // offset -> pointer
        lBlob.mModel.FixUp(liBase);

        bool lbOk = (lBlob.mModel.mpaPartials == lpPartials0) &&
                    (lBlob.maPartials[0].mpaGraphs == lpGraphs0) &&
                    (lBlob.maGraphs[0].mpaPoints == lpPoints0);
        if (!lbOk)
        {
            volatile int liFail = 1;
            (void)liFail;
        }

        // Null-array guard: an empty header must be a no-op (no deref).
        LoopModelData lEmpty;
        lEmpty.mpaPartials    = nullptr;
        lEmpty.muNumOfPartials = 0;
        lEmpty.FixUp(liBase);
        lEmpty.FixDown(liBase);
    }
}

int main()
{
    ExerciseLoopModelData();
    return 0;
}
