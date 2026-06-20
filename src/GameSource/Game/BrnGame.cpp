#include "GameSource/Game/BrnGame.h"

// BrnGame::BrnG @ 0x827E3090 -- compiler-generated destructor thunk for a BaseCollisionGenerator
// embedded in an opaque BrnGame-namespace owner. See BrnGame.h for the full opaque-owner FLAG.
//
// X360 asm (verbatim shape):
//   r31 = a1 + 0x228; BaseCollisionGenerator::Destruct(r31); return r31;
// i.e. destruct the embedded collision generator and return a pointer to it.
namespace BrnGame
{
    CgsSceneManager::CgsCollision::BaseCollisionGenerator* BrnG(BrnGDtorThunkOwner* lpOwner)
    {
        // FLAG (semantic parity): the real thunk reaches the generator at owner+552; here it is a
        // named member, so the absolute offset differs but the operation is identical.
        lpOwner->mCollisionGenerator.Destruct();
        return &lpOwner->mCollisionGenerator;
    }
}
