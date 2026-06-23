#ifndef PARTICLE_DESCRIPTION_RESOURCE_TYPE_H
#define PARTICLE_DESCRIPTION_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnParticle
{
// Resource-type handlers for particle descriptions, deriving from CgsResource::Type.
// GetTypeID/FixUp/GetImportPointer/DeSerialise are virtual overrides; Serialise is
// the type's own virtual. Base/signatures recovered from the DecFIGS DWARF
// (ParticleDescriptionResourceType.h).
class ParticleDescriptionCollectionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};

class ParticleDescriptionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    bool     DeSerialise(void* lpResource) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
