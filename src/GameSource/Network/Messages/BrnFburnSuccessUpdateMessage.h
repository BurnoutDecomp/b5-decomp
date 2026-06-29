#pragma once

// ===================================================================================
// BrnNetwork::FburnSuccessUpdateMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnFburnSuccessUpdateMessage.h
//
// SHAPE from DecFIGS DWARF (BrnFburnSuccessUpdateMessage.h:45) gated against the X360
// binary. An UNRELIABLE per-frame message (CgsNetwork::Message base, size 0x20) reporting
// a last-second challenge-success bit set plus the frame/action it applies to.
//
// LAYOUT (X360-AUTHORITATIVE offsets; Construct @ 0x82580720, PackOrUnpack @ 0x8257C898,
//          PrepareForSend @ 0x8257FAC8, Retrieve @ 0x8257FB50):
//   +0x00  (CgsNetwork::Message base, size 0x20)
//   +0x20  LastSecondChallengeSuccess mSuccessBitArray  (8 bytes; std 0 in ctor)
//   +0x28  s32 miFramesSinceStart                       (stw -1; PackOrUnpack int [0, 0x7FFFFFFF])
//   +0x2C  s32 miActionIndex                            (stw -1; PackOrUnpack int [0, 2])
//
// PackOrUnpack ORs the base virtual pack/unpack status (the X360 build resolves the
// base-stub call to a COMDAT-folded copy reported as BrnWorld::PVSDebugComponent::IsSimple,
// which returns false == 0 == success) with the 8-byte buffer field and the two ints.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace BrnGameState
{
    // Owning home for the last-second challenge-success bit set the update message carries.
    // DWARF spells it FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess; it is an
    // 8-byte bitset (the X360 ctor zero-fills it with a single 64-bit std at +0x20, and
    // PrepareForSend copies it as one 64-bit load/store).
    //
    // TYPE RECOVERED (BrnChallengeSuccessManager TU): the X360 frame-rate translators
    // (ChallengeSuccessManager::TranslateSuccessUpdate60HzTo50Hz @ 0x82555DD8 /
    //  ::TranslateSuccessUpdate50HzTo60Hz @ 0x825563F8) operate on this payload through the
    // inlined CgsContainers::FastBitArray<60> API (UnSetAll / IsBitSet / SetBit), with the
    // out-of-range guards asserting against CgsFastBitArray.h:396/431. So the payload IS a
    // FastBitArray<60> -- one 64-bit field (60 bits round up to 64), exactly the 8-byte size
    // the message ctor/copy assumes. The bit API is added here as inline forwards over the
    // single u64 field, keeping the mu64Bits member the message .cpp copies/zeroes by name (the
    // 8-byte size is unchanged). FLAG: if the real FburnChallengeSuccessUpdateAction class gains
    // members beyond this bit set, re-home it; the 8-byte payload size must not change.
    struct FburnChallengeSuccessUpdateAction
    {
        struct LastSecondChallengeSuccess
        {
            static const u32 KU_NUMBER_OF_BITS = 60;
            static const u32 KU_BITS_PER_FIELD = 64;

            u64 mu64Bits;   // 8 bytes (the single FastBitArray<60> field)

            void UnSetAll()              { mu64Bits = 0; }
            bool IsBitSet(s32 liIndex) const
            {
                return (mu64Bits & ((u64)1 << (liIndex & (KU_BITS_PER_FIELD - 1)))) != 0;
            }
            void SetBit(s32 liIndex)
            {
                mu64Bits |= (u64)1 << (liIndex & (KU_BITS_PER_FIELD - 1));
            }
        };
    };
} // namespace BrnGameState

namespace BrnNetwork
{
    typedef BrnGameState::FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess
        FburnSuccessUpdatePayload;

    struct FburnSuccessUpdateMessage : CgsNetwork::Message
    {
        FburnSuccessUpdatePayload mSuccessBitArray;     // +0x20 (8 bytes)
        s32                       miFramesSinceStart;   // +0x28
        s32                       miActionIndex;        // +0x2C

        void                          Construct();
        void                          Destruct();
        void                          PrepareForSend(u16 lu16FrameCount,
                                                     s32 liFramesSinceStart,
                                                     s32 liActionIndex,
                                                     const FburnSuccessUpdatePayload* lpSuccessBitArray);
        bool                          Retrieve(s32* lpiFramesSinceStart,
                                              s32* lpiActionIndex,
                                              FburnSuccessUpdatePayload* lpSuccessBitArray);
        s32                           GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                   GetName() const;
    };
} // namespace BrnNetwork
