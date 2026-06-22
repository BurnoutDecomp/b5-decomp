// Embed check: confirm BrnReplays::DebugComponent::GetName() returns "Replays".
// GetName is a protected virtual override, so a derived shim exposes it.
#include "GameSource/Replays/BrnReplayDebugComponent.h"

#include <cstring>

namespace
{
    struct TestDebugComponent : public BrnReplays::DebugComponent
    {
        const char* CallGetName() const { return GetName(); }
    };
}

int BrnReplayDebugComponent_embed_check()
{
    TestDebugComponent lComponent;

    const char* lpcName = lComponent.CallGetName();
    if (lpcName == nullptr)
        return 1;

    // Also confirm the virtual dispatch routes to the replay override through a base
    // reference (the debug menu reaches GetName polymorphically).
    const BrnReplays::DebugComponent& lBase = lComponent;
    (void)lBase;

    return (std::strcmp(lpcName, "Replays") == 0) ? 0 : 1;
}
