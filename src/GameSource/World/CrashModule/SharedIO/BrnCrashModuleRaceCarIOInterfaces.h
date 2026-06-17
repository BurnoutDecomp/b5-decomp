#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by RaceCarOutputInterface's own TU (DWARF home GameSource/World/CrashModule/SharedIO/
// BrnCrashModuleRaceCarIOInterfaces.h). Size 256 (NOMINAL).
//
// Per the DecFIGS DWARF, BrnWorld::CrashIO::RaceCarOutputInterface holds an
// EventQueue<RaceCarCrashCompleteEvent,10> (mRaceCarCrashCompleteEventQueue). The
// RaceCarEntityModuleIO InputBuffer_PostScene references it via its buffer-local typedef
// `CrashInterface` (BrnRaceCarEntityModuleIO.h:92/:200) and only holds/returns it by
// pointer, so a reserved-byte blob suffices here; the full member layout (and the sibling
// CrashIO Traffic/Network interfaces homed in this DWARF file) belongs to this type's own
// ledger TU. alignas(16) for the SIMD-aligned EventQueue payload.
#include "types.hpp"   // u8

namespace BrnWorld
{
namespace CrashIO
{
    struct alignas(16) RaceCarOutputInterface
    {
        unsigned char maReserved[256];
    };
}
}
