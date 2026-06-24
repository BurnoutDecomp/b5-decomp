#ifndef CGS_MATERIAL_ASSEMBLY_H
#define CGS_MATERIAL_ASSEMBLY_H

#include "types.hpp"

// CgsGraphics::MaterialAssembly @ CgsMaterialAssembly.h:54 (DecFIGS DWARF). A draw-time
// material: an array of MaterialTechnique* (one per technique / LOD slot), the texture
// samplers, and the per-stage shader-constant blocks. Layout + member order recovered
// verbatim from the DWARF; only the slice needed by the recovered GetMaterial accessor
// is bodied (CgsMaterialAssembly.cpp), the remaining methods are declared for parity.

namespace CgsGraphics
{
    // Forward declarations -- GetMaterial only returns a MaterialTechnique* read out of
    // the array, so the element types stay incomplete here.
    struct MaterialTechnique;
    struct Sampler;
    struct ShaderConstantsInternal;
    struct ShaderConstantsCPU;
    struct Texture;

    struct MaterialAssembly
    {
        MaterialTechnique** mappMaterials;          // +0x00  per-technique table
        u32                 muNameHash;             // +0x04
        u8                  mu8NumMaterials;        // +0x08  technique count (bounds limit)
        s8                  mi8NumSamplers;         // +0x09
        s8                  mi8NumInternalSamplers; // +0x0A
        s8                  mi8NumExternalSamplers; // +0x0B
        Sampler*            mpaSamplers;            // +0x0C

    private:
        ShaderConstantsInternal* mpVertexShaderConstants; // +0x10
        ShaderConstantsInternal* mpPixelShaderConstants;  // +0x14
        ShaderConstantsCPU*      mpCPUShaderConstants;     // +0x18

        void FixupAnimatedMaterial();

    public:
        // GetMaterial @ 0x827E6720: bounds-checked technique-table accessor.
        MaterialTechnique* GetMaterial(u32 luIndex) const;

        u32 GetNameHash() const { return muNameHash; }
        u8  GetLength() const   { return mu8NumMaterials; }
    };
}

#endif // CGS_MATERIAL_ASSEMBLY_H
