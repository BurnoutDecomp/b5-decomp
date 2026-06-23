#pragma once

#include "types.hpp"

// ===========================================================================
// CgsContainers::CgsHash -- the engine's reflected CRC-32 string/byte hash.
//   Home: GameShared/GameClasses/Containers/CgsHash.{h,cpp}
//
// Declarations for the hash entry points the X360 binary defines (bodies in
// CgsHash.cpp). GUI/text systems hash label strings through CalculateHash to key
// localised-text lookups; the path variant case- and slash-normalises first.
// ===========================================================================

namespace CgsContainers
{
namespace CgsHash
{
    // @ X360 0x828154A0 -- reflected CRC-32 over a1[0..a2) (no final inversion).
    unsigned int CalculateHash(char* a1, int a2);

    // @ X360 0x828154F0 -- case/slash-normalising reflected CRC-32 over a
    // NUL-terminated path string.
    unsigned int CalculatePathHash(unsigned char* a1);
}
}
