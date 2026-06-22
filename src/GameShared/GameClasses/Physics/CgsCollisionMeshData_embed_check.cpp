// Translation-unit embed check for CgsPhysics::CollisionMeshData.
// Forces the owning header to compile standalone and exercises the bodied
// FixUp/FixDown relocation helpers so their signatures stay wired to their home.
#include "GameShared/GameClasses/Physics/CgsCollisionMeshData.h"

namespace
{
void EmbedCheck()
{
    CgsPhysics::CollisionMeshData lMesh;
    void* lpDelta = reinterpret_cast<void*>(static_cast<size_t>(0x100));
    lMesh.FixUp(lpDelta);
    lMesh.FixDown(lpDelta);
}
}
