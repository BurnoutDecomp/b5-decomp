#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (range/index invariants)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"     // CgsNumeric::Random (the LCG draw source)

// CgsAlgorithms::Shuffle - the in-place Fisher-Yates shuffle the project runs over its
// fixed-capacity containers (DWARF/asm home GameShared/GameClasses/Algorithms/CgsShuffle.h).
//
// SHAPE (recovered from the X360 asm of the three instantiations this TU homes --
// Shuffle<u8,Stack<u8,199>> @ 0x8271B298, Shuffle<u16,Stack<u16,1>> @ 0x8271B420,
// Shuffle<u16,Stack<u16,400>> @ 0x8271B110, all driven by BrnTraffic::TrafficEntityModule::
// Reset). The template carries two explicit parameters -- the element `Type` and the
// container `Container` -- matching the mangled instances
// (??$Shuffle@EV?$Stack@E$0MH@@CgsContainers@@@ == Shuffle<u8, Stack<u8,199>>, etc.).
//
// ALGORITHM (asm, all three instances byte-identical apart from the element width):
//   * assert liStart <= liEndPlusOne                                     (CgsShuffle.h:109)
//   * walk li from liEndPlusOne-1 down to 1 (loop guard `li > 0`); for each li draw a
//     swap partner in the inclusive range [liStart, li] via the container's Random. The
//     asm inlines CgsNumeric::Random's LCG here (reads/writes muSeed @ Random+0x20, the
//     modulo `randomHigh % (li-liStart+1) + liStart`, with the RandomInt-internal asserts
//     liMax>=liMin @ CgsRandom.h:320 and luMod>0 @ CgsRandom.h:323); de-inlined back to a
//     RandomInt(liStart, li) call here.
//   * assert liSwapWith < liEndPlusOne                                   (CgsShuffle.h:118)
//   * when liSwapWith != li, swap Container::operator[](liSwapWith) with operator[](li).
//
// The X360 threads a CgsNumeric::Random* as the 4th argument (a4; the seed lives at a4+0x20);
// de-inlined here to a CgsNumeric::Random& so the LCG stays owned by CgsRandom.h rather than
// re-homed. Returns the container reference (the asm returns the container pointer in r3).

namespace CgsAlgorithms
{
    // X360 CgsAlgorithms::Shuffle<Type,Container>. In-place Fisher-Yates over the half-open
    // index window [liStart, liEndPlusOne): for each position from the top down, swap it with
    // a uniformly random earlier-or-equal position drawn from lrRandom. `Type` is the element
    // scratch type used for the swap temporary (u8 / u16 for this TU's instances).
    template <class Type, class Container>
    Container& Shuffle(Container& lrContainer, s32 liStart, s32 liEndPlusOne, CgsNumeric::Random& lrRandom)
    {
        CGS_ASSERT(liStart <= liEndPlusOne, "liStart <= liEndPlusOne");

        for (s32 li = liEndPlusOne - 1; li > 0; --li)
        {
            const s32 liSwapWith = lrRandom.RandomInt(liStart, li);
            CGS_ASSERT(liSwapWith < liEndPlusOne, "liSwapWith < liEndPlusOne");

            if (liSwapWith != li)
            {
                Type lTemp = lrContainer[liSwapWith];
                lrContainer[liSwapWith] = lrContainer[li];
                lrContainer[li] = lTemp;
            }
        }

        return lrContainer;
    }
}
