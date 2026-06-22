// Embed check for CgsStrStream: exercises the ledger functions reconstructed in this pass
//   StrStream::Append          @ 0x82815E80
//   StrStream::Reset           @ 0x82815E68
//   SimpleStrStream::Reset     @ 0x82815EB8
//   StrStreamBase::AppendFormat @ 0x82817720
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace
{
    void ExerciseStrStream()
    {
        char lacBuffer[64];
        CgsDev::StrStream lStream(lacBuffer, sizeof(lacBuffer));

        // Named sink + operator<< forwarding.
        lStream.Append("hello ");
        lStream << "world" << 42 << CgsDev::E_PRINTMODE_HEX << 255u;

        // printf-style format path.
        lStream.AppendFormat(" n=%d", 7);

        // Reset clears the buffer and the print mode.
        lStream.Reset();

        // SimpleStrStream owns its inline buffer.
        CgsDev::SimpleStrStream lSimple;
        lSimple << "row";
        lSimple.AppendFormat(" %u", 3u);
        lSimple.Reset();
    }
}

extern void CgsStrStream_embed_check_anchor();
void CgsStrStream_embed_check_anchor() { ExerciseStrStream(); }
