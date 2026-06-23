#include "types.hpp"

#include "GameSource/Network/Messages/BrnFburnSuccessUpdateMessage.h"
#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"   // base pack/unpack stub (COMDAT-folded)
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
// PackOrUnpack ORs the base pack/unpack status (the X360 build resolves the base-stub call
// to a COMDAT-folded copy reported as BrnWorld::PVSDebugComponent::IsSimple, which returns
// false == 0 == success) with: the 8-byte buffer field, the frames-since-start int
// ([0, 0x7FFFFFFF]) and the action index int ([0, 2]). Message type id 36 (0x24).

namespace BrnNetwork
{
    static const s32 KI_FBURN_SUCCESS_UPDATE_MESSAGE_TYPE = 36;

    void FburnSuccessUpdateMessage::Construct()
    {
        CgsNetwork::Message::Construct();
        mSuccessBitArray.mu64Bits = 0;
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
        mSuccessBitArray.mu64Bits = 0;
        miFramesSinceStart        = 0;
        miActionIndex             = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult FburnSuccessUpdateMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase =
            reinterpret_cast<BrnWorld::PVSDebugComponent*>(this)->IsSimple() ? 1 : 0;
        const CgsNetwork::PackOrUnpackResult lxBuffer =
            CgsNetwork::PackOrUnpackBuffer(this,
                                           reinterpret_cast<u8*>(&mSuccessBitArray),
                                           8) | lxBase;
        const CgsNetwork::PackOrUnpackResult lxFrames =
            CgsNetwork::PackOrUnpackInt(this, &miFramesSinceStart, 0, 0x7FFFFFFF);
        return CgsNetwork::PackOrUnpackInt(this, &miActionIndex, 0, 2) | (lxFrames | lxBuffer);
    }

    void FburnSuccessUpdateMessage::PrepareForSend(u16 lu16FrameCount,
                                                   s32 liFramesSinceStart,
                                                   s32 liActionIndex,
                                                   const FburnSuccessUpdatePayload* lpSuccessBitArray)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            miFramesSinceStart = liFramesSinceStart;
            miActionIndex      = liActionIndex;
            mSuccessBitArray   = *lpSuccessBitArray;   // 64-bit copy
            CgsNetwork::Message::PrepareForSend(KI_FBURN_SUCCESS_UPDATE_MESSAGE_TYPE, lu16FrameCount);
        }
    }

    bool FburnSuccessUpdateMessage::Retrieve(s32* lpiFramesSinceStart,
                                             s32* lpiActionIndex,
                                             FburnSuccessUpdatePayload* lpSuccessBitArray)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpSuccessBitArray   = mSuccessBitArray;   // 64-bit copy
        *lpiFramesSinceStart = miFramesSinceStart;
        *lpiActionIndex      = miActionIndex;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
} // namespace BrnNetwork
