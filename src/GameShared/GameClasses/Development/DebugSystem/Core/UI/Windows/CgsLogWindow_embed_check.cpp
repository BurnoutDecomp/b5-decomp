// Embed check for CgsDev::DebugUI::LogWindow @ 0x827DFED0 (default ctor).
// Constructs the window (exercising the bodied ctor + the embedded LogWindowStrStream
// member ctor + the mLog.mpWindow back-wire). Also exercises the LogWindowStrStream sink
// directly via its public SetWindow wire-up + operator<<.
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"

namespace
{
    void ExerciseLogWindow()
    {
        // Default ctor: builds the CustomWindow base + embedded mLog and wires mLog.mpWindow = this.
        CgsDev::DebugUI::LogWindow lWindow;
        (void)lWindow;

        // The stream front-end: wire it to a window and push text through the sink.
        CgsDev::DebugUI::LogWindowStrStream lStream;
        lStream.SetWindow(&lWindow);
        lStream << "boot" << 7;
    }
}

extern void CgsLogWindow_embed_check_anchor();
void CgsLogWindow_embed_check_anchor() { ExerciseLogWindow(); }
