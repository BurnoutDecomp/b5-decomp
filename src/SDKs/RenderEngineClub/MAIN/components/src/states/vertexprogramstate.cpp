#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexprogramstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"

#include <cstring>

namespace renderengine
{
    CGS_STATIC_ASSERT((sizeof(rw::Resource) == 0x14), "ARTIST five-lane resource");
    CGS_STATIC_ASSERT((sizeof(ProgramBufferData) == 0x14), "ARTIST program payload prefix");

    // ARTIST 82B629C0. This API binds two pointers in opaque Resource payload
    // bytes, not in an invented C++ class prefix. Its only linked resource is
    // shadow::sVertexProgramStateResource. Bytes +0x00..+0x1F are untouched.
    VertexProgramState* VertexProgramState::InitializeNoBindX360(
        const rw::Resource& resource, const Parameters& parameters)
    {
        // The original lwz/stw API accesses word-aligned payload words. The
        // transport view preserves that granularity without declaring prefix
        // fields or imposing an alignment attribute on the backing storage.
        u32* payload = static_cast<u32*>(resource.m_baseResources[0]);
        std::memcpy(payload + 9, &parameters.vertexProgram, sizeof(parameters.vertexProgram));
        std::memcpy(payload + 8, &parameters.vertexDescriptor, sizeof(parameters.vertexDescriptor));
        return reinterpret_cast<VertexProgramState*>(payload);
    }

    // ARTIST 82B60B88: load the program pointer from the NoBind payload, then
    // return its native shader header. ProgramBuffer's variable header follows
    // the proven 0x14-byte ProgramBufferData prefix; it is not a stored pointer.
    ::D3DVertexShader* VertexProgramState::GetD3DVertexShader()
    {
        const ProgramBufferData* program;
        std::memcpy(&program, reinterpret_cast<const u8*>(this) + 0x24, sizeof(program));
        return reinterpret_cast<::D3DVertexShader*>(const_cast<ProgramBufferData*>(program) + 1);
    }
}
