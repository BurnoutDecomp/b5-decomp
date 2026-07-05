#ifndef CGS_SHADER_TECHNIQUE_RESOURCE_TYPE_H
#define CGS_SHADER_TECHNIQUE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

// Forward declarations for the serialised sub-block types the technique blob embeds.
// Their concrete layouts are not needed in this header: every Get*SerialisedResourceDescriptorSize
// helper reads the serialised rw resource image by raw byte offset (the DOCUMENTED serialised-blob
// exception), exactly like FixUp. ShaderConstants{Internal,External} live at global scope
// (GameShared/GameClasses/Graphics/CgsShaderConstants.h).
namespace CgsGraphics
{
    class ShaderConstantHashTable;
    class ShaderTechnique;
}
struct ShaderConstantsInternal;
struct ShaderConstantsExternal;

namespace CgsResource
{
// Resource-type handler for a serialised GPU shader technique (vertex+pixel shader
// program pointers, the engine-internal/external shader-constant blocks, the constant
// hash table, and the per-technique sampler array). Derives from CgsResource::Type; the
// listed methods are virtual overrides. Base/signatures recovered from the DecFIGS DWARF
// (CgsShaderTechniqueResourceType.h) and matched to the CgsResource::Type virtual base
// (NOT to the Hex-Rays raw prototypes).
//
// GetTypeID returns the registry id 50. GetImportPointer exposes the two embedded shader
// imports (vertex shader @ slot 0 offset 0, pixel shader @ slot 1 offset 4). FixUp
// relocates the serialised constant blocks and the sampler table by the rw::Resource load
// base. PostFixUp is declared here but deferred (see the .cpp) - a declared-but-undefined
// virtual compiles cleanly under the per-TU cl /c gate, which does not link or instantiate.
class ShaderTechniqueResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override; // @0x827F7A68
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void     PostFixUp(void* lpResource, const rw::Resource& lrResource) const override; // DEFERRED - declared only

    // Non-virtual per-sub-block serialised-size helpers used by GetSerialisedResourceDescriptor.
    // The Internal/External sizers are not part of this batch (declared only; bodied in their own wave).
    uint32_t GetShaderConstantInternalSerialisedResourceDescriptorSize(ShaderConstantsInternal* lpBlock, uint32_t luBase) const;
    uint32_t GetShaderConstantExternalSerialisedResourceDescriptorSize(const ShaderConstantsExternal* lpBlock) const;
    uint32_t GetConstantHashTableSerialisedResourceDescriptorSize(const CgsGraphics::ShaderConstantHashTable* lpHashTable) const;   // @0x827E9D38
    uint32_t GetShaderSamplersSerialisedResourceDescriptorSize(CgsGraphics::ShaderTechnique* lpTechnique) const;                    // @0x827E9C30
};
}

#endif
