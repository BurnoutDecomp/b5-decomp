// ============================================================================
// SDKs/Packages/ICE/ICEDataICETake.cpp
//
// ICE::ICETake -- the camera-take EDITOR methods (the class:ICE::ICETake TU),
// separate from the runtime/eval ICETake methods that live in ICEData.cpp. This
// file holds the interval-bracket queries and the sub-take channel mark; the
// undo stack, element insert/delete/copy/paste, key harden/soften and resize
// operations are reconstructed in companion rounds.
// ============================================================================

#include "SDKs/Packages/ICE/ICEData.hpp"           // ICETake, ICEChannel, ICE_INVALID_INTERVAL
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace ICE
{
    // ------------------------------------------------------------------------
    // ICETake::GetIntervalKey -- map an interval index to its left-edge key
    // index: interval 0 starts at key 0; an interior interval n starts at the
    // stored key-index table entry [n-1]; the final interval starts at the
    // second-to-last key (numKeys - 2).
    // ------------------------------------------------------------------------
    u16 ICETake::GetIntervalKey(s32 liChannel, u16 lu16Interval) const
    {
        const ICEChannel& lrChannel = mChannels[liChannel];

        if (lu16Interval == 0)
            return 0;

        if ((u16)(lu16Interval + 1) < (u16)lrChannel.GetNumIntervals())
            return (u16)lrChannel.GetKeyIndex((u16)(lu16Interval - 1));

        return (u16)(lrChannel.GetNumKeys() - 2);
    }

    // ------------------------------------------------------------------------
    // ICETake::GetIntervalBracket -- return the parameter-space [start,end]
    // bracket of an interval. The invalid-interval sentinel brackets the whole
    // take (0..1); otherwise the start is the interval's parameter and the end is
    // the next interval's parameter.
    // ------------------------------------------------------------------------
    void ICETake::GetIntervalBracket(s32 liChannel, u16 lu16Interval,
                                     f32* lpfStart, f32* lpfEnd) const
    {
        const ICEChannel& lrChannel = mChannels[liChannel];

        if (lu16Interval == ICE_INVALID_INTERVAL)
        {
            *lpfStart = 0.0f;
            *lpfEnd   = 1.0f;
        }
        else
        {
            *lpfStart = lrChannel.GetIntervalParameter(lu16Interval);
            *lpfEnd   = lrChannel.GetIntervalParameter((u16)(lu16Interval + 1));
        }
    }

    // ------------------------------------------------------------------------
    // ICETake::MarkChannelFromSubTake -- flag a channel as sourced from the
    // sub-take rather than the primary take, by setting its bit in the sub-take
    // channel mask. SetDataPointers uses this in edit mode for channels with no
    // primary-take data.
    // ------------------------------------------------------------------------
    void ICETake::MarkChannelFromSubTake(s32 liChannel)
    {
        CGS_ASSERT(liChannel < 32, "liChannel < 32");
        mxSubTakeChannels |= (1 << liChannel);
    }
}
