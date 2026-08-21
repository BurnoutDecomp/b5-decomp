#ifndef BRN_TRAFFIC_STATIC_PARAM_H
#define BRN_TRAFFIC_STATIC_PARAM_H

#include "types.hpp"

// ---------------------------------------------------------------------------
// BrnTraffic::StaticTrafficParam
//
// DWARF/asm home: GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h
// (the assert strings in the X360 retail XEX all cite this header path).
//
// A per-static-(parked)-vehicle parameter record, sibling to BrnTraffic::Param. Holds the
// hull it lives on, its index within that hull's static-vehicle list, a packed lifecycle
// flag byte, and the vehicle type. Construct resets it to "no vehicle"; Initialise stamps
// it live; the lifecycle setters (SetZombie / SetShouldBeRemoved / SetDivorced / ClearDying)
// flip flag bits with debug-assert preconditions; GetHull / GetIndexInHull read the slot
// identity (asserting the slot is alive first).
//
// Layout pinned by the X360 retail XEX (BURNOUT_X360_ARTIST.XEX) member STORE/LOAD offsets,
// corroborated by the Feb-2007 leak (SharedClasses/Traffic/BrnTrafficStaticParam.cpp ground
// truth for Construct/Initialise):
//   0x00 muHull (u16)                       -- GetHull: lhz r3, 0(this)
//   0x02 muStaticTrafficIndexOnHull (u8)    -- GetIndexInHull: lbz r3, 2(this)
//   0x03 mxFlags (u8)                       -- all lifecycle bits live here (lbz/stb 3(this))
//   0x04 muVehicleType (u8)
//
// Flag bit values match the sibling BrnTraffic::Param (X360 asm: ClearDying clears 0x02,
// SetZombie sets 0x20, SetShouldBeRemoved sets 0x10, SetDivorced sets 0x40; alive == 0x01).
// ---------------------------------------------------------------------------

namespace BrnTraffic
{
static const u16 KU_INVALID_HULL = 0xFFFF;

class StaticTrafficParam
{
public:
    // mxFlags bit values (X360 asm). IsAlive/IsDying/IsZombie/ShouldBeRemoved are the
    // predicates the lifecycle setters assert against (the assert strings name them).
    enum Flags
    {
        E_FLAG_ALIVE             = 0x01,
        E_FLAG_DYING             = 0x02,
        E_FLAG_SHOULD_BE_REMOVED = 0x10,
        E_FLAG_ZOMBIE            = 0x20,
        E_FLAG_DIVORCED          = 0x40,
    };

    u16 muHull;                    // 0x00
    u8  muStaticTrafficIndexOnHull; // 0x02
    u8  mxFlags;                   // 0x03
    u8  muVehicleType;             // 0x04

    // --- ground-truth lifecycle (Feb-2007 leak) ---
    void Construct();
    void Initialise(u8 luVehicleType, u16 luHull, u8 luIndexOnHull);

    // --- the 6 functions owned by this TU (X360 retail XEX) ---
    void ClearDying();          // @ 0x82706BA8
    u16  GetHull() const;       // @ 0x82713D78
    u8   GetIndexInHull() const; // @ 0x82713DD8
    void SetDivorced();         // @ 0x82706CE8
    void SetShouldBeRemoved();  // @ 0x82706D88
    void SetZombie();           // @ 0x82706C48

    static void _AssertLayout();   // never called; body in the .cpp

    // Predicates the asm inlines into the assert preconditions above.
    bool IsAlive() const { return (mxFlags & E_FLAG_ALIVE) != 0; }
    bool IsDying() const { return (mxFlags & E_FLAG_DYING) != 0; }
    bool IsZombie() const { return (mxFlags & E_FLAG_ZOMBIE) != 0; }
    bool ShouldBeRemoved() const { return (mxFlags & E_FLAG_SHOULD_BE_REMOVED) != 0; }
};
}

#endif
