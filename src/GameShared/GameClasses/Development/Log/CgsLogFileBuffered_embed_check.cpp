// Embed check for CgsDev::Log::LogFileBuffered @ 0x826613A8 (default ctor).
// Constructs the stream (exercising the bodied ctor) and streams text through it.
#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"

namespace
{
    void ExerciseLogFileBuffered()
    {
        // Default ctor: closed, unbuffered stream.
        CgsDev::Log::LogFileBuffered lLog;

        // The sink is a no-op while closed; just verify it links through operator<<.
        lLog << "stats: " << 42 << "\n";
    }
}

extern void CgsLogFileBuffered_embed_check_anchor();
void CgsLogFileBuffered_embed_check_anchor() { ExerciseLogFileBuffered(); }
