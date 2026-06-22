// Compile-only embedder: exercises PropUpdateNotification::GetEntityId.
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

namespace
{
    BrnWorld::PropEntityID _exercise(const BrnPhysics::Props::PropUpdateNotification& lNote)
    {
        return lNote.GetEntityId();
    }
}
