#pragma once

#include "types.hpp"

// SharedClasses/BrnSharedConstants.h - cross-system shared constants + the BrnUpdateSet
// bitmask type. Recovered from the DecFIGS DWARF (SharedClasses/BrnSharedConstants.h);
// populated incrementally (the full constant set is added as the systems that use them are
// reconstructed).

// BrnUpdateSet - a u16 bitmask selecting which simulation sub-systems run this frame
// (built by BrnGameModule::ConstructUpdateSet / ConstructUpdateSetFromFsm).
typedef u16 BrnUpdateSet;
