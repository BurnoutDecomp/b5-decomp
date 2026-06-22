#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnTrafficStaticParam.cpp - BrnTraffic::StaticTrafficParam methods.
//
// Construct / Initialise are corroborated by the Feb-2007 leak
// (SharedClasses/Traffic/BrnTrafficStaticParam.cpp, ground truth). The six lifecycle /
// accessor methods are reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly
// (authoritative): each reads/writes the packed flag byte mxFlags (offset 0x03) and asserts
// its lifecycle precondition in the same order the asm fires them. The layout lives in the
// owning header BrnTrafficStaticParam.h.

namespace BrnTraffic
{
    // @ Construct  (Feb-2007 leak) - reset to "no vehicle".
    void StaticTrafficParam::Construct()
    {
        muHull = KU_INVALID_HULL;
        muStaticTrafficIndexOnHull = static_cast<u8>(~0);
        mxFlags = 0;
        muVehicleType = static_cast<u8>(~0);
    }

    // @ Initialise  (Feb-2007 leak) - stamp live with a vehicle type, hull and per-hull index.
    void StaticTrafficParam::Initialise(u8 luVehicleType, u16 luHull, u8 luIndexOnHull)
    {
        mxFlags = E_FLAG_ALIVE;
        muVehicleType = luVehicleType;
        muHull = luHull;
        muStaticTrafficIndexOnHull = luIndexOnHull;
    }

    // @ 0x82706BA8 - clear the dying bit. Asserts IsDying() then !IsAlive() (asm order), then
    // mxFlags &= ~0x02 (andi. r11,r11,0xFD).
    void StaticTrafficParam::ClearDying()
    {
        CGS_ASSERT(IsDying(), "IsDying()");
        CGS_ASSERT(!IsAlive(), "!IsAlive()");
        mxFlags &= static_cast<u8>(~E_FLAG_DYING);
    }

    // @ 0x82713D78 - return the hull index (lhz, the u16 @ 0x00). Asserts IsAlive() first.
    u16 StaticTrafficParam::GetHull() const
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");
        return muHull;
    }

    // @ 0x82713DD8 - return the per-hull static index (lbz, the u8 @ 0x02). Asserts IsAlive().
    u8 StaticTrafficParam::GetIndexInHull() const
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");
        return muStaticTrafficIndexOnHull;
    }

    // @ 0x82706CE8 - set the divorced bit. Asserts IsAlive() then !ShouldBeRemoved() (asm
    // order), then mxFlags |= 0x40 (ori r11,r11,0x40).
    void StaticTrafficParam::SetDivorced()
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");
        CGS_ASSERT(!ShouldBeRemoved(), "!ShouldBeRemoved()");
        mxFlags |= E_FLAG_DIVORCED;
    }

    // @ 0x82706D88 - set the should-be-removed bit. Asserts IsAlive() then !IsZombie() (asm
    // order), then mxFlags |= 0x10 (ori r11,r11,0x10).
    void StaticTrafficParam::SetShouldBeRemoved()
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");
        CGS_ASSERT(!IsZombie(), "!IsZombie()");
        mxFlags |= E_FLAG_SHOULD_BE_REMOVED;
    }

    // @ 0x82706C48 - set the zombie bit. Asserts IsAlive() then !ShouldBeRemoved() (asm
    // order), then mxFlags |= 0x20 (ori r11,r11,0x20).
    void StaticTrafficParam::SetZombie()
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");
        CGS_ASSERT(!ShouldBeRemoved(), "!ShouldBeRemoved()");
        mxFlags |= E_FLAG_ZOMBIE;
    }
}
