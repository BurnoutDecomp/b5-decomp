#ifndef CGS_SHADER_TECHNIQUE_RESOURCE_TYPE_H
#define CGS_SHADER_TECHNIQUE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

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
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void     PostFixUp(void* lpResource, const rw::Resource& lrResource) const override; // DEFERRED - declared only
};
}

#endif
