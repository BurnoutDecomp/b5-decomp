#pragma once

#include "types.hpp"

// CgsCoreConstants.h - the engine's shared unit and vector constants (DWARF namespace CgsCore,
// defined in CgsCoreConstants.cpp). Grown by attestation: only the constants an X360 body is
// shown to read from their own storage are declared here.
namespace CgsCore
{
    // CgsCoreConstants.h:58 (defined CgsCoreConstants.cpp:53). ARTIST storage 0x82F31928 ==
    // 0x3EE4E26D (0.44704f): one mile per hour in metres per second. Dynamic initialisers read
    // it from memory, e.g. BoostStrategy::KF_ONCOMING_MINSPEED's at 0x82C4C108.
    extern const f32 KF_MPH_TO_MPS;
}
