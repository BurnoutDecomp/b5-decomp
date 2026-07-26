#ifndef CGS_ATTRIB_SYS_VAULT_RESOURCE_TYPE_H
#define CGS_ATTRIB_SYS_VAULT_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised attribute-system vault. Derives from the real
// CgsResource::Type. FixUp/GetImportCount are sibling virtuals owned by other recon
// passes; GetSerialisedResourceDescriptor and GetTypeID are owned here. Base/signatures
// recovered from the DecFIGS DWARF (CgsAttribSysVaultResourceType.h:50).
class AttribSysVaultResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
};
}

#endif
