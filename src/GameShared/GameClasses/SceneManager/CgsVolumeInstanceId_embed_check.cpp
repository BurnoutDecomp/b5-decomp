// Tiny embed/compile check for the CgsSceneManager::VolumeInstanceId home: includes the
// header and touches both reconstructed setters so the gate exercises the full TU.
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"

namespace
{
void CgsVolumeInstanceIdEmbedCheck()
{
    CgsSceneManager::VolumeInstanceId lId;
    lId.muId = 0;
    lId.SetEntityIDOwner(3);            // SetEntityIDOwner
    lId.SetEntityIDEntityIndex(0x123);  // SetEntityIDEntityIndex
}
} // namespace
