#pragma once
#include "pc/gcm/renderengine/reflections/SceneRender.h"
namespace CgsPC::Shadows
{
    bool HasDebris(const BrnParticle::ParticleModule::ParticleRenderData* lpData);
    void RenderDebris(u32 luCascade, const BrnParticle::ParticleModule::ParticleRenderData* lpData);
}
