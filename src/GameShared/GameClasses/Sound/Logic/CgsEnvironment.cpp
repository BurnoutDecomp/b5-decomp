// ============================================================================
// CgsEnvironment.cpp -- CgsSound::Logic::Environment runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   Environment::AddStateManager(StateManager*)  @ 0x82680D60
//
// Decoded from the asm:
//   if (lpStateManager == NULL) assert("lpStateManager");                 // :475
//   liType = lpStateManager->GetStateType();   (lwz r11, 0x14(r31))
//   if (liType >= 16) assert("...GetStateType() < KI_MAX_NUMBER_OF_STATES"); // :476
//   slot = &mapStateManagers[liType + 1];      ((liType+1)*4 + this)
//   if (*slot != NULL) assert("mapStateManagers[...] == NULL");           // :477
//   *slot = lpStateManager;
//   return 1;                                  (li r3, 1)
//
// Member access is BY NAME via the GetStateType() accessor and the named
// mapStateManagers array (see CgsEnvironment.h); no raw-offset cast.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Logic
{

bool Environment::AddStateManager(StateManager* apStateManager)
{
    CGS_ASSERT(apStateManager != nullptr, "lpStateManager");

    const s32 liStateType = apStateManager->GetStateType();
    CGS_ASSERT(liStateType < KI_MAX_NUMBER_OF_STATES,
               "lpStateManager->GetStateType() < KI_MAX_NUMBER_OF_STATES");

    CGS_ASSERT(mapStateManagers[liStateType + 1] == nullptr,
               "mapStateManagers[lpStateManager->GetStateType()] == NULL");

    mapStateManagers[liStateType + 1] = apStateManager;
    return true;
}

} // namespace Logic
} // namespace CgsSound
