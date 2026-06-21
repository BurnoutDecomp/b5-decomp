#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

// Explicit instantiations of the ObjectPool<IceMovie,20,s32> methods (inline in CgsObjectPool.h).
// This playlist storage pool only ever allocates/frees movies (the order array drives lookup),
// so instantiate AllocateObject/FreeObject per-member -- NOT `template class` -- to avoid forcing
// FindObject (which would need IceMovie::operator==, a method this element type does not define).
template s32  CgsContainers::ObjectPool<BrnDirector::IceMovie, 20, s32>::AllocateObject();
template void CgsContainers::ObjectPool<BrnDirector::IceMovie, 20, s32>::FreeObject(s32);
