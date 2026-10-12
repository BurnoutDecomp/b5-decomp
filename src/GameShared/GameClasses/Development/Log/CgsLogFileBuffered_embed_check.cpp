// Embed check for CgsDev::Log::LogFileBuffered @ 0x826613A8 (default ctor).
// Constructs the stream. Streaming requires Open() and a nonzero scratch buffer.
#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"

namespace
{
    void ExerciseLogFileBuffered()
    {
        // Default ctor: closed, unbuffered stream.
        CgsDev::Log::LogFileBuffered lLog;

    }
}

extern void CgsLogFileBuffered_embed_check_anchor();
void CgsLogFileBuffered_embed_check_anchor() { ExerciseLogFileBuffered(); }
