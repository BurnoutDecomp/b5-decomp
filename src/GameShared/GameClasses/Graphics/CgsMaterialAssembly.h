#ifndef CGS_MATERIAL_ASSEMBLY_H
#define CGS_MATERIAL_ASSEMBLY_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"   // Ptr32<T> (the 32-bit slot)

#include <cstddef>

// renderengine::Texture -- the sampled texture type (forward-declared; pointer-only use).
namespace renderengine { class Texture; }

// The material resource-type handler drives the streamed-material fix-up and calls the private
// FixupAnimatedMaterial as part of PostFixUp (X360 0x828A83F0); grant it access.
namespace CgsResource { class MaterialResourceType; }

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

    // *** ON-DISC LAYOUT (world-pixels wave 2026-07-28) ***
    // MaterialAssembly is NOT a host object: it IS the streamed Material resource body
    // that CgsResource::MaterialResourceType relocates in place (its FixUp pokes exactly
    // the u32 slots at +0x00/+0x0C/+0x10/+0x14/+0x18 and walks 20-byte samplers), and the
    // world-data porter emits the console 32-bit form for Material. Declared with host-
    // width pointers the struct grew to 40 bytes and every field past mappMaterials moved:
    // GetLength() read byte +0x0C instead of the technique count at +0x08, so
    // DrawRenderable::Interpret's "GetNumVertexDescriptors() == GetLength()" tripwire
    // fired on the first streamed mesh. Same class of bug (and same fix) as
    // CgsGraphics::Model / Instance -- see CgsSerialisedPtr.h.
    struct MaterialAssembly
    {
        friend class CgsResource::MaterialResourceType;

        Ptr32<Ptr32<MaterialTechnique> > mappMaterials; // +0x00  per-technique table
        u32                 muNameHash;             // +0x04
        u8                  mu8NumMaterials;        // +0x08  technique count (bounds limit)
        s8                  mi8NumSamplers;         // +0x09
        s8                  mi8NumInternalSamplers; // +0x0A
        s8                  mi8NumExternalSamplers; // +0x0B
        Ptr32<Sampler>      mpaSamplers;            // +0x0C

    private:
        Ptr32<ShaderConstantsInternal> mpVertexShaderConstants; // +0x10
        Ptr32<ShaderConstantsInternal> mpPixelShaderConstants;  // +0x14
        Ptr32<ShaderConstantsCPU>      mpCPUShaderConstants;    // +0x18

        void FixupAnimatedMaterial();

    public:
        // GetMaterial @ 0x827E6720: bounds-checked technique-table accessor.
        MaterialTechnique* GetMaterial(u32 luIndex) const;

        u32 GetNameHash() const { return muNameHash; }
        u8  GetLength() const   { return mu8NumMaterials; }

        // UsesTexture: true if this assembly samples lpTexture in any technique stage.
        // Declaration recovered from the DecFIGS DWARF (CgsMaterialAssembly.h:96,
        // `bool UsesTexture(const Texture*) const`) and X360-attested (ledger:
        // CgsGraphics::MaterialAssembly::UsesTexture, reviewed). The pointer is the same
        // renderengine::Texture* the caller flows in from Renderable's texture set; bodied
        // in this class's own TU.
        bool UsesTexture(const renderengine::Texture* lpTexture) const;
    };

    // The console header size MaterialResourceType::FixUp relocates in place.
    static_assert(sizeof(MaterialAssembly) == 28,
                  "CgsGraphics::MaterialAssembly must be the console's 28-byte on-disc header");
    static_assert(offsetof(MaterialAssembly, mu8NumMaterials) == 0x08,
                  "MaterialAssembly::mu8NumMaterials must be at +0x08");
}

#endif // CGS_MATERIAL_ASSEMBLY_H
