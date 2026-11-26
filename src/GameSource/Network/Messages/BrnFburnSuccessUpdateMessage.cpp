#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Messages/BrnFburnSuccessUpdateMessage.cpp"
#include "GameSource/Network/Messages/BrnFburnSuccessUpdateMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::FburnSuccessUpdateMessage::Construct             @ 0x82580720
//   BrnNetwork::FburnSuccessUpdateMessage::Destruct              @ 0x8257FAA0
//   BrnNetwork::FburnSuccessUpdateMessage::GetName              @ 0x8257C930
//   BrnNetwork::FburnSuccessUpdateMessage::GetPackedMessageSize  @ 0x8257FAB0
//   BrnNetwork::FburnSuccessUpdateMessage::PackOrUnpack          @ 0x8257C898
//   BrnNetwork::FburnSuccessUpdateMessage::PrepareForSend        @ 0x8257FAC8
//   BrnNetwork::FburnSuccessUpdateMessage::Retrieve              @ 0x8257FB50
//
// An UNRELIABLE per-frame message carrying a last-second challenge-success bit set and the
// frame/action it applies to. Construct zeroes the 8-byte bit set and sets both ints to -1;
// Destruct restores the two ints to -1; GetPackedMessageSize re-zeroes the bit set and the
// two ints before deferring to the base Message size.
//
// PackOrUnpack ORs the base Message::PackOrUnpack() status (a constant success: the base
// virtual serialises nothing) with: the 8-byte buffer field, the frames-since-start int
// ([0, 0x7FFFFFFF]) and the action index int ([0, 2]). Message type id 36 (0x24).

namespace BrnNetwork
{
    static const s32 KI_FBURN_SUCCESS_UPDATE_MESSAGE_TYPE = 36;

    void FburnSuccessUpdateMessage::Construct()
    {
        CgsNetwork::Message::Construct();
        mSuccessBitArray.UnSetAll();
        miFramesSinceStart        = -1;
        miActionIndex             = -1;
    }

    void FburnSuccessUpdateMessage::Destruct()
    {
        miFramesSinceStart = -1;
        miActionIndex      = -1;
    }

    const char* FburnSuccessUpdateMessage::GetName() const
    {
        return "Freeburn Challenge Success Update Message";
    }

    s32 FburnSuccessUpdateMessage::GetPackedMessageSize()
    {
        mSuccessBitArray.UnSetAll();
        miFramesSinceStart        = 0;
        miActionIndex             = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult FburnSuccessUpdateMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::Message::PackOrUnpack();
        lxResult |= CgsNetwork::Message::PackOrUnpackBuffer(reinterpret_cast<char*>(&mSuccessBitArray), 8);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&miFramesSinceStart, 0, 0x7FFFFFFF);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&miActionIndex, 0, 2);
        return lxResult;
    }

    void FburnSuccessUpdateMessage::PrepareForSend(u16 lu16FrameCount,
                                                   s32 liFramesSinceStart,
                                                   s32 liActionIndex,
                                                   const LastSecondChallengeSuccess* lpSuccessBitArray)
    {
        CGS_ASSERT_AT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
            "lu16FrameCount != KU16_INVALID_FRAME",
            CGS_ARTIST_FILE, 129 /* ARTIST 0x8257FB0C */);

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            // The bit set is read first (ARTIST 0x8257FB24 loads it ahead of the two int stores,
            // which could alias it).
            mSuccessBitArray   = *lpSuccessBitArray;   // 64-bit copy
            miFramesSinceStart = liFramesSinceStart;
            miActionIndex      = liActionIndex;
            CgsNetwork::Message::PrepareForSend(KI_FBURN_SUCCESS_UPDATE_MESSAGE_TYPE, lu16FrameCount);
        }
    }

    bool FburnSuccessUpdateMessage::Retrieve(s32* lpiFramesSinceStart,
                                             s32* lpiActionIndex,
                                             LastSecondChallengeSuccess* lpSuccessBitArray)
    {
        // ARTIST 0x8257FB60 branches to a trailing `return false` (0x8257FB90): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpSuccessBitArray   = mSuccessBitArray;   // 64-bit copy
            *lpiFramesSinceStart = miFramesSinceStart;
            *lpiActionIndex      = miActionIndex;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            return true;
        }
        return false;
    }
} // namespace BrnNetwork
