#pragma once

// BrnWorld::EBoostType -- the boost-bar type a race car is currently building/spending.
// Reconstructed verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h:34).
//
// NOTE: this is DISTINCT from BrnNetwork::EBoostType. BoostOutputInfo::meBoostType in
// BrnRaceCarEntityModuleOutputInterface.h:177 is BrnWorld::EBoostType per the authoritative
// DWARF, so this is the type that header embeds by value. Given an explicit underlying type
// (s32) so it is a complete type usable by value (E_BOOST_TYPE_NONE == -1 forces a signed base).
#include "types.hpp"

namespace BrnWorld
{
    enum EBoostType : s32
    {
        E_BOOST_TYPE_NONE       = -1,
        E_BOOST_TYPE_DANGER     = 0,
        E_BOOST_TYPE_AGGRESSION = 1,
        E_BOOST_TYPE_STUNT      = 2,
        E_BOOST_TYPE_COUNT      = 3
    };
}
