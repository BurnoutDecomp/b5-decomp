#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

// Explicit instantiations of the ObjectPool<ICEMoviePlaylist::DebugMenuRemoveData,20,s32> methods
// (inline in CgsObjectPool.h). The dev-menu remove-command pool additionally uses FindObject to
// locate an existing remove command by value, so DebugMenuRemoveData defines operator== and this
// instantiation includes FindObject alongside AllocateObject/FreeObject.
template s32  CgsContainers::ObjectPool<BrnDirector::ICEMoviePlaylist::DebugMenuRemoveData, 20, s32>::AllocateObject();
template s32  CgsContainers::ObjectPool<BrnDirector::ICEMoviePlaylist::DebugMenuRemoveData, 20, s32>::FindObject(const BrnDirector::ICEMoviePlaylist::DebugMenuRemoveData&) const;
template void CgsContainers::ObjectPool<BrnDirector::ICEMoviePlaylist::DebugMenuRemoveData, 20, s32>::FreeObject(s32);
