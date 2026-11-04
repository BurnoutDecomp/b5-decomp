#ifndef CGS_ATTRIB_SYS_VAULT_RESOURCE_TYPE_H
#define CGS_ATTRIB_SYS_VAULT_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised attribute-system vault. Derives from the real
// CgsResource::Type. ARTIST vtable slot +0x10 shares the schema pointer FixUp.
// Base/signatures
// recovered from the DecFIGS DWARF (CgsAttribSysVaultResourceType.h:50).
class AttribSysVaultResourceType : public Type
{
public:
    uint32_t           GetTypeID() const ;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const ;
};
}

#endif
