#include "types.hpp"
#include <cstddef>
#define private public
#define protected public
#include "GameSource/Effects/EffectsModule.h"
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"
#include "vendor/renderware/collision/ClusteredMesh.hpp"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/Events/BrnGuiMugshotControlEvent.h"
#undef protected
#undef private
extern "C" const unsigned int R7TypedFacts[] = {
 sizeof(BrnEffects::EffectsModule),sizeof(BrnReplays::EffectsSerialiser),offsetof(BrnEffects::EffectsModule,mEffectsSerialiser),
 sizeof(CgsSceneManager::CacheSlot),__alignof(CgsSceneManager::CacheSlot),offsetof(CgsSceneManager::CacheSlot,mLastCachedSphere),offsetof(CgsSceneManager::CacheSlot,mInnerSpherePositionAndRadius),offsetof(CgsSceneManager::CacheSlot,miIndexIntoTriangleCache),offsetof(CgsSceneManager::CacheSlot,miNumCachedTriangleBatches),offsetof(CgsSceneManager::CacheSlot,miOverflow),
 sizeof(rw::collision::Aggregate),offsetof(rw::collision::ClusteredMesh,mpKDTree),offsetof(rw::collision::ClusteredMesh,mpCluster),
 sizeof(((BrnDirector::Camera::Camera*)0)->mState_uFlags),offsetof(BrnDirector::Camera::Camera,mState_uFlags),
 sizeof(BrnGui::GuiMugshotControlEvent),offsetof(BrnGui::GuiMugshotControlEvent,meResponse),offsetof(BrnGui::GuiMugshotControlEvent,meImageType),offsetof(BrnGui::GuiMugshotControlEvent,meOpponentARCI),offsetof(BrnGui::GuiMugshotControlEvent,mbLocalAggressor),sizeof(BrnGui::RaceMainHudState)
};
typedef void (BrnReplays::EffectsSerialiser::*EffectsCtor)();
EffectsCtor gR7EffectsCtor=&BrnReplays::EffectsSerialiser::Construct;
