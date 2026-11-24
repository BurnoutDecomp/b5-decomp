#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"     // CgsNumeric::Random (the LCG draw source)

// CgsAlgorithms::Shuffle - the in-place Fisher-Yates shuffle the project runs over its
// fixed-capacity containers (DWARF/asm home GameShared/GameClasses/Algorithms/CgsShuffle.h).
//
// ARTIST's range instantiations at 0x8271B110, 0x8271B298 and 0x8271B420
// return void and call the owning container's indexed accessor. The range
// checks use CgsShuffle.h lines 109 and 118. RandomInt owns the draw and its
// bounds checks, as in the expansion at 0x8271B18C.

namespace CgsAlgorithms
{
    static const char KAC_SHUFFLE_ASSERT_FILE[] = "..\\..\\..\\GameShared\\GameClasses\\Algorithms\\CgsShuffle.h"; // ARTIST 0x82025340

    // X360 CgsAlgorithms::Shuffle<Type,Container>. In-place Fisher-Yates over the half-open
    // index window [liStart, liEndPlusOne): for each position from the top down, swap it with
    // a uniformly random earlier-or-equal position drawn from lrRandom. `Type` is the element
    // scratch type used for the swap temporary (u8 / u16 for this TU's instances).
    template <class Type, class Container>
    void Shuffle(Container& lrContainer, s32 liStart, s32 liEndPlusOne, CgsNumeric::Random& lrRandom)
    {
        CGS_ASSERT(liStart <= liEndPlusOne,
            CGS_ASSERT_MESSAGE_AT("liStart <= liEndPlusOne", KAC_SHUFFLE_ASSERT_FILE, 109));

        for (s32 li = liEndPlusOne - 1; li > 0; --li)
        {
            const s32 liSwapWith = lrRandom.RandomInt(liStart, li);
            CGS_ASSERT(liSwapWith < liEndPlusOne,
                CGS_ASSERT_MESSAGE_AT("liSwapWith < liEndPlusOne", KAC_SHUFFLE_ASSERT_FILE, 118));

            if (liSwapWith != li)
            {
                Type lTemp = lrContainer[liSwapWith];
                lrContainer[liSwapWith] = lrContainer[li];
                lrContainer[li] = lTemp;
            }
        }
    }

    // Whole-container entry (the twin of CgsBubbleSort.h's 1-arg BubbleSort). Read off the
    // instantiation the console inlines into ModeManager::SetupOnlineStartingGrid: read the live
    // length (GetLength() carries the "Array used before Construct/Clear was called" guard), assert
    // it is not negative with CgsShuffle.h's OWN message, then shuffle the whole live window
    // [0, GetLength()). The length guard is a signed test on the console, hence the cast.
    template <class Type, class Container>
    Container& Shuffle(Container& lrContainer, CgsNumeric::Random& lrRandom)
    {
        const s32 liEndPlusOne = static_cast<s32>(lrContainer.GetLength());
        CGS_ASSERT(liEndPlusOne >= 0,
            CGS_ASSERT_MESSAGE_AT("Array is too large to shuffle!", KAC_SHUFFLE_ASSERT_FILE, 156));

        Shuffle<Type, Container>(lrContainer, 0, liEndPlusOne, lrRandom);
        return lrContainer;
    }
}
