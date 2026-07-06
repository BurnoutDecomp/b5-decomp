#ifndef CGS_MATERIAL_RESOURCE_TYPE_H
#define CGS_MATERIAL_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

// ShaderConstantsInternal / ShaderConstantsCPU are the on-disk shader-constant blocks
// embedded in a serialised material (relocated by FixUp). They live at global scope in
// GameShared/GameClasses/Graphics/CgsShaderConstants.h (the committed home), so they are
// forward-declared at global scope here.
struct ShaderConstantsInternal;
struct ShaderConstantsCPU;

namespace CgsResource
{
// Resource-type handler for a serialised material (a CgsGraphics::MaterialAssembly image).
// Derives from CgsResource::Type; FixUp/PostFixUp are vtable overrides. PostFixUpShaderConstants
// is a non-virtual helper. Only these three fix-up methods are bodied here; the class's other
// virtuals are homed by their own TUs and are not declared here (nothing in this TU needs them).
//
// The serialised blob operated on by these methods IS a CgsGraphics::MaterialAssembly:
//   +0x00  MaterialTechnique** mappMaterials      (technique table, rebased)
//   +0x08  u8                  mu8NumMaterials     (technique count)
//   +0x09  s8                  mi8NumSamplers
//   +0x0C  Sampler*            mpaSamplers         (20-byte samplers; name @+0, type @+12)
//   +0x10  ShaderConstantsInternal* mpVertexShaderConstants
//   +0x14  ShaderConstantsInternal* mpPixelShaderConstants
//   +0x18  ShaderConstantsCPU*      mpCPUShaderConstants
// Each MaterialTechnique holds a PROGRAM-block pointer @+0 (the program block embeds the
// ShaderConstantHashTable @+0x80) and per-stage constant-binding lists (+0x18 vertex /
// +0x1C pixel) with count bytes (+0x20 vertex / +0x21 pixel) and sampler-binding bookkeeping
// bytes (+0x22 total / +0x23 external / +0x24 list ptr).
class MaterialResourceType : public Type
{
public:
    void FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void PostFixUp(void* lpResource, const rw::Resource& lrResource) const override;

    // Non-virtual helper (X360 0x828A77E8). The ShaderConstantsInternal* argument is
    // DWARF-attested but unused by the body (the block is re-derived from the blob).
    void PostFixUpShaderConstants(void* lpResource, ShaderConstantsInternal* lpConstants, bool lbPixelStage) const;
};

// FixUp's sampler-type name table: the X360 walks a 7-entry / 28-byte-stride rodata table
// (unk_83011A78, limit 0xC4 = 7*28) to map a sampler's name string to a type index written
// at sampler+12. Only the leading char* of each 28-byte slot is dereferenced, so a flat
// char*[7] reproduces the same name sequence. The name strings are NOT present in the repo
// rodata exports, so the table CONTENTS are un-attested (placeholder empty strings -> every
// real sampler currently resolves to -1). The lookup loop itself is transcribed faithfully.
static const u32 KU_MATERIAL_SAMPLER_TYPE_COUNT = 7;
extern const char* const KapcMaterialSamplerTypeNames[KU_MATERIAL_SAMPLER_TYPE_COUNT];
}

#endif // CGS_MATERIAL_RESOURCE_TYPE_H
