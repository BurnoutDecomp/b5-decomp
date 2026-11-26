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
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // GameStateModuleIO::LastSecondChallengeSuccess (FastBitArray<60>)

namespace BrnNetwork
{
    // DWARF h:73 types mSuccessBitArray as FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess,
    // the CgsContainers::FastBitArray<60> owned by BrnGameStateSharedIO.h:313 (the PS3 mangled
    // PrepareForSend/Retrieve take PKN13CgsContainers12FastBitArrayILi60EEE; PS3 Construct and
    // GetPackedMessageSize inline its UnSetAll, CgsFastBitArray.h:513).
    typedef BrnGameState::GameStateModuleIO::LastSecondChallengeSuccess LastSecondChallengeSuccess;

    struct FburnSuccessUpdateMessage : CgsNetwork::Message
    {
        LastSecondChallengeSuccess mSuccessBitArray;     // +0x20 (8 bytes)
        s32                        miFramesSinceStart;   // +0x28
        s32                        miActionIndex;        // +0x2C

        void                          Construct();
        void                          Destruct();
        void                          PrepareForSend(u16 lu16FrameCount,
                                                     s32 liFramesSinceStart,
                                                     s32 liActionIndex,
                                                     const LastSecondChallengeSuccess* lpSuccessBitArray);
        bool                          Retrieve(s32* lpiFramesSinceStart,
                                              s32* lpiActionIndex,
                                              LastSecondChallengeSuccess* lpSuccessBitArray);
        s32                           GetPackedMessageSize() ;
        CgsNetwork::PackOrUnpackResult PackOrUnpack() ;
        const char*                   GetName() const ;
    };

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    CGS_STATIC_ASSERT((sizeof(void*) != 4 || sizeof(FburnSuccessUpdateMessage) == 0x30), "sizeof(FburnSuccessUpdateMessage) == 0x30");
} // namespace BrnNetwork
