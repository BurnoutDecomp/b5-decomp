#include "SharedClasses/Graphics/ParticleDescriptionResourceType.h"
#include "rw/rwcore_structs.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"

// ARTIST 8267C288: sizing only; Init does not allocate.
CgsResource::ResourceDescriptor BrnParticle::ParticleDescriptionResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    cLionSerialiser lSerialiser;
    lSerialiser.Init();
    const ParticleDescription* lpDescription = static_cast<const ParticleDescription*>(lpResource);
    cLionEffectDefinition* lpDefinition = lpDescription->GetDefinition();
    if (lpDefinition)
    {
        lSerialiser.DataSizeUpdate(84);
        lpDefinition->GetParticles()->GetSerialiseSize(lSerialiser);
    }
    CgsResource::ResourceDescriptor lResult;
    lResult.m_baseResourceDescriptors[0].m_size = ((lSerialiser.mStringSize + lSerialiser.mDataSize + 15u) & ~15u) + 16u;
    lResult.m_baseResourceDescriptors[0].m_alignment = 16;
    return lResult;
}
