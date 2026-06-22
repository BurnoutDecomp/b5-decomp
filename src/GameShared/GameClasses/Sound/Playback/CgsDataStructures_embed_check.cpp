// Tiny embed check: include the CgsDataStructures (IEntityFixer) home alongside
// the sibling homes it composes with (CgsCommon Name + CgsRegistry Entity/Registry)
// in one TU, to confirm they compose without ODR / redefinition clashes and that
// the static GetFixer entry point is reachable. Compile-only; not shipped.
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"

namespace
{
    void CgsDataStructuresEmbedCheck()
    {
        using CgsSound::Playback::IEntityFixer;
        using CgsSound::Playback::Name;

        // Look one up by an interned type name; result reachable by name.
        const Name lName("VoiceSpec");
        const IEntityFixer* lpFixer = IEntityFixer::GetFixer(lName);
        (void)lpFixer;
    }
}
