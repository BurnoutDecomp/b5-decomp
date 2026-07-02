#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"

// CgsSceneManager::EntityManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameShared/GameClasses/SceneManager/CgsEntityManager.cpp):
//   EntityManager::Prepare @0x828C5FC8
//
// Asm walk: the two ObjectPool Clear instantiations are real X360 symbols
// (SceneManagerEntity<10000> at this+0, VolumeInstance<5048> at this+0x27600);
// the two IndexedHashTable Clears are inlined bucket loops (541 bins @+0xE7840
// over the shared element array @+0xCA380, then 509 bins @+0x123B20 over
// +0xE91A0), each ending in the one-byte constructed flag; return true.

namespace CgsSceneManager
{

// @ 0x828C5FC8
bool EntityManager::Prepare()
{
    mEntityPool.Clear();
    mVolumeInstancePool.Clear();

    mEntityIdToIndex.Clear(maEntityIdHashElements);
    mVolumeInstanceIdToIndex.Clear(maVolumeInstanceIdHashElements);

    return true;
}

}
