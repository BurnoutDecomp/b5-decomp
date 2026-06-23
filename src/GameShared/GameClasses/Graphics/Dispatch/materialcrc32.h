#ifndef MATERIAL_CRC32_H
#define MATERIAL_CRC32_H

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5>

// MaterialCRC32 -- a RenderWare material identified by a CRC32: three serialised resource handles
// (a vertex-program buffer, a pixel-program buffer and a material state). It supplies its own
// serialised-resource descriptor (the sizing the resource pool needs to lay it out). The X360
// RwMaterialCRC32ResourceType handler forwards its GetSerialisedResourceDescriptor to
// MaterialCRC32::GetResourceDescriptor.
//
// Layout recovered from the DecFIGS DWARF (GameShared/GameClasses/Graphics/Dispatch/materialcrc32.h):
// three u32 program/state slots at +0/+4/+8. The descriptor type is the X360 five-entry form
// (rw::BaseResourceDescriptors<5>); the X360 spine is authoritative for descriptor width.
struct MaterialCRC32
{
public:
    // materialcrc32.h:47 -- the serialised-resource descriptor for this material (X360
    // 0x827F7878). The body computes the packed serialised size and seeds the descriptor; the
    // exact sizing is owned by the engine SDK home (materialcrc32.cpp) and is not reconstructed
    // here -- this declaration is the surface the resource-type handler forwards to.
    rw::BaseResourceDescriptors<5> GetResourceDescriptor();

    // materialcrc32.h:48 -- build/initialise the material from its loaded rw::Resource.
    MaterialCRC32* Initialize(const rw::Resource& lrResource);
    // materialcrc32.h:49 -- release the instanced GPU resources.
    void Release();
    // materialcrc32.h:50 -- the descriptor for the *instanced* (GPU-resident) form.
    rw::BaseResourceDescriptors<5> GetInstanceResourceDescriptor() const;

private:
    // materialcrc32.h:62..64 -- the three serialised resource-handle slots.
    u32 m_vertexProgramBuffer;   // +0x00
    u32 m_pixelProgramBuffer;    // +0x04
    u32 m_materialState;         // +0x08

    MaterialCRC32();                            // materialcrc32.h:68
    MaterialCRC32(const MaterialCRC32&);        // materialcrc32.h:69
    ~MaterialCRC32();                           // materialcrc32.h:70
};

#endif // MATERIAL_CRC32_H
