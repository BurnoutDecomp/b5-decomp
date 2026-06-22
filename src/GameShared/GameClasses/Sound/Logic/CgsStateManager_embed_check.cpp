#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"

// Exercise CgsSound::Logic::StateManager::IsStateAlias @ 0x82680D48.
namespace
{
bool ExerciseIsStateAlias(const CgsSound::Logic::StateManager& lManager, s32 liState)
{
    return lManager.IsStateAlias(liState);
}
}

// StateManager is polymorphic (inherits MemBase, adds its own virtual). We do NOT
// static_assert absolute member offsets (host 64-bit vptr differs from X360 32-bit),
// only that the modelled type carries a vtable as expected of the keystone base.
static_assert(__is_polymorphic(CgsSound::Logic::StateManager),
              "StateManager must be polymorphic (MemBase vptr + IsStateAlias)");
