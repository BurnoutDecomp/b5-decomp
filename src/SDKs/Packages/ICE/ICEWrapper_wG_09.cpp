// ============================================================================
// The BrnDirector::ICEWrapper default constructor, split out of
// GameSource/Director/BrnDirectorICEWrapper.cpp: that TU's other bodies need
// ICEController::EditorOn / ::SetState, ICECameraMover::Construct and
// DebugInterface::Enable/DisableConsole, none of which has a body in the tree.
// DELETE-WHEN: BrnDirectorICEWrapper.cpp can mount -- then move this body back into it.
// This constructor runs before the debug log exists (MainDirector embeds the wrapper
// by value), so it must not log.
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"

#include "GameShared/GameClasses/Containers/CgsStack.h"   // CgsContainers::KI_STACK_UNCONSTRUCTED

namespace BrnDirector
{

ICEWrapper::ICEWrapper()
{
    // The action stack starts unconstructed: any use before Construct/Clear asserts.
    mActionQueue.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED;
}

} // namespace BrnDirector
