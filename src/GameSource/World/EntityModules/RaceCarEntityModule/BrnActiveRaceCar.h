#pragma once

// ============================================================================
// BrnWorld::ActiveRaceCar -- the "active" (simulated, in-range) half of a race car.
//
// A RaceCar (BrnRaceCar.h) is the always-resident global slot; when it comes into
// range it is paired with an ActiveRaceCar that owns the physics/AI state. The two
// reference each other: RaceCar::mpActiveRaceCar <-> ActiveRaceCar::mpGlobalRaceCar.
//
// SCOPE OF THIS HEADER: this is the declaration surface BrnRaceCar.cpp needs -- the
// four accessors RaceCar's lifecycle/positioning code calls on an ActiveRaceCar* it
// holds by pointer (it never embeds one by value, so the full 0x1CD0/7376-byte
// layout is NOT laid out here). The bodies live in BrnActiveRaceCar.cpp (X360
// 0x822A1F10 IsAttached, etc.) and resolve at link; here we only declare them.
// Member offsets attested by the BrnRaceCar X360 asm are recorded in comments:
//   GetActiveRaceCarIndex() -> reads +0x748   (miActiveRaceCarIndex)
//   GetGlobalRaceCar()      -> reads +0x6F0   (mpGlobalRaceCar)
//   ToBePlacedOnTrack()     -> reads byte +0x7C4 (mbToBePlacedOnTrack)
// Method shapes are gated on the DecFIGS DWARF for BrnActiveRaceCar.h
// (GetActiveRaceCarIndex/GetGlobalRaceCar/IsAttached/ToBePlacedOnTrack const).
// ============================================================================

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex

namespace BrnWorld
{
class RaceCar;

class ActiveRaceCar
{
public:
    // X360: returns miActiveRaceCarIndex (this+0x748). DWARF BrnActiveRaceCar.h:736.
    EActiveRaceCarIndex GetActiveRaceCarIndex() const;

    // X360: returns mpGlobalRaceCar (this+0x6F0). DWARF BrnActiveRaceCar.h:742.
    RaceCar* GetGlobalRaceCar() const;

    // X360 0x822A1F10: returns mbIsAttached. DWARF BrnActiveRaceCar.h:763.
    bool IsAttached() const;

    // X360: returns mbToBePlacedOnTrack (byte this+0x7C4). DWARF BrnActiveRaceCar.h:853.
    bool ToBePlacedOnTrack() const;
};
}
