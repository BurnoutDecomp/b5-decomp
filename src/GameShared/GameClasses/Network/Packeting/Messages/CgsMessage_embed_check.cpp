// Translation-unit embed check for the class:CgsNetwork::Message and
// class:CgsNetwork groups. Pulls in every reconstructed home so the gate
// compiles the headers + .cpp set together.
#include "CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsNetwork
{
    void Message_EmbedCheck()
    {
        Message lMsg;
        lMsg.mu8GameID    = KU8_INVALID_GAME_ID;
        lMsg.mx8Flags     = 0;
        lMsg.mi8Type      = 0;
        lMsg.mu16Frame    = 0;
        lMsg.mePackOrUnpack = Message::E_PACK_INTO_BITSTREAM;
        (void)lMsg;
    }
}
