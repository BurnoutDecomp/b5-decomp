#ifndef CGS_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_X360_H
#define CGS_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_X360_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace rw
{
    struct Resource;
}

namespace CgsResource
{
    // The X360 compiled-shader (vertex/pixel program buffer) resource-type handler (type id 0x12).
    // Home: GameShared/GameClasses/RenderWare/x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.cpp
    // (the assert rodata pins the exact source path + line 374). The runtime resource is a
    // renderengine::ProgramBuffer block (renderengine::ProgramBufferData header, see
    // SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h).
    //
    // Only the two virtuals attested by the X360 asm in this batch are declared here; the rest
    // (GetTypeID -> 0x12, DeSerialise, FixDown/FixUp, imports, ...) inherit CgsResource::Type and
    // are added when their X360 bodies are recovered. Reconstructed bodies:
    //   GetSerialisedResourceDescriptor  0x828A9910  (sizes microcode + descriptor table + interned names)
    //   ReBase                           0x828A8E90  (fixes name offsets by the move delta, re-registers the shader)
    //
    // NOTE: on X360 ReBase re-registers the microcode with the GPU via XGRegister{Vertex,Pixel}Shader;
    // those XDK entry points are declared in states/programbuffer.h and stubbed for the PC build.
    class RwShaderProgramBufferResourceType : public Type
    {
    public:
        // The registry id (0x12 == E_RESOURCETYPE_RW_SHADER_PROGRAM_BUFFER; see
        // the class note above). Trivial ICF-folded getter on the X360 -- the
        // VALUE is attested, the body is the one-line return. Needed for
        // registration (id-keyed lookup).
        virtual uint32_t           GetTypeID() const;
        virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
        virtual void               ReBase(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
                                          ResourceDescriptor& lrSize, s32 liMemType) const;
    };
}

#endif // CGS_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_X360_H
