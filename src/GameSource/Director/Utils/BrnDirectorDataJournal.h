#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_DATA_JOURNAL_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_DATA_JOURNAL_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the index bounds asserts)

// ============================================================================
// GameSource/Director/Utils/BrnDirectorDataJournal.h
//
// DataJournal<T, N> -- the director's fixed-capacity history ring the vehicle tracker keeps
// for the last N frames of a quantity (position, linear velocity, MPH, timestep). It is a
// circular buffer of N entries with a "current" write cursor: SetCurrent pushes a new most-
// recent sample (advancing the cursor backward one slot so [0] is always the newest), SetAll
// seeds every slot to one value, and operator[](i) reads i frames back ( [0] = newest,
// [1] = previous, ... ). GetCurrent/GetPrevious are the [0]/[1] shorthands.
//
// ----------------------------------------------------------------------------
// LAYOUT (pinned from BrnDirector::VehicleTracker::Update @0x8223B1A8 and
//   ::GetImplicitVelocity @0x82205F70, both of which index these journals directly):
//   the entry array sits first, then the write cursor, then the live count.
//
//   +0                 T   maEntries[N]
//   +N*sizeof(T)       s32 miWriteIndex   (the slot holding [0]; advances backward on push)
//   +N*sizeof(T)+4     s32 miSize         (live entry count, saturating at N)
//
//   For T = rw::math::vpu::Vector3 (16-byte) and N = 8 the entry array is 0x80 bytes, the
//   cursor lands at +0x80 and the count at +0x84 (a2[32]/a2[33] in GetImplicitVelocity); the
//   journal is padded to a 16-byte multiple (0x90) where embedded back-to-back. For
//   T = f32 and N = 8 the entry array is 0x20, the cursor at +0x20 and the count at +0x24.
// ----------------------------------------------------------------------------

template <class T, int N>
class DataJournal
{
public:

    enum { KI_SIZE = N };

    // Reset to empty (cursor at 0, no live entries). The tracker calls this via the
    // CgsSystem::Time / ClearData seeding path in Construct; the entries are left for the
    // first SetAll/SetCurrent to populate.
    void Construct()
    {
        miWriteIndex = 0;
        miSize = 0;
    }

    // Seed every slot to one value and mark the journal full (size == N, cursor at 0). The
    // tracker uses this on the first frame so the history reads as if the car had been at
    // rest there for the whole window.
    void SetAll(const T& lValue)
    {
        for (int liLoop = 0; liLoop < N; ++liLoop)
        {
            maEntries[liLoop] = lValue;
        }
        miWriteIndex = 0;
        miSize = N;
    }

    // Push a new newest sample. The cursor steps back one slot (modulo N) so the new sample
    // becomes [0]; the live count saturates at N.
    void SetCurrent(const T& lValue)
    {
        miWriteIndex = (miWriteIndex + N - 1) % N;
        maEntries[miWriteIndex] = lValue;
        if (miSize < N)
        {
            ++miSize;
        }
    }

    // The sample i frames back ( [0] newest .. [N-1] oldest ). Asserts the index is live.
    const T& operator[](int liIndex) const
    {
        CGS_ASSERT(liIndex < miSize, "liIndex < miSize");
        return maEntries[(miWriteIndex + liIndex) % N];
    }

    const T& GetCurrent() const  { return (*this)[0]; }
    const T& GetPrevious() const { return (*this)[1]; }

    int GetSize() const { return miSize; }

private:

    T   maEntries[N];     // +0
    s32 miWriteIndex;     // +N*sizeof(T)
    s32 miSize;           // +N*sizeof(T)+4
};

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_DATA_JOURNAL_H
