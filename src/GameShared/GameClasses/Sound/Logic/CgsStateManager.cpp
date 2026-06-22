#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"

// CgsSound::Logic::StateManager::IsStateAlias @ 0x82680D48 is homed inline in
// CgsStateManager.h (member access by name -- meMapState; no raw-offset cast). This
// TU exists so the header is exercised by a real compilation unit. The old opaque
// mPad[20] + miAliasState stub is superseded by the DWARF-named layout in the
// header.
