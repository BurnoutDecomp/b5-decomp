#pragma once

#include "types.hpp"
#include <xtl.h>

namespace rw { struct Resource; }

namespace renderengine
{
    struct ProgramBufferData;
    struct VertexDescriptorData;

    // Opaque resource handle. ARTIST's NoBind path uses the caller's byte
    // storage; the C++ size of this interface is not a resource allocation size.
    class VertexProgramState
    {
    public:
        class Parameters
        {
        public:
            // DecFIGS stateparams.h:1435-1443; ARTIST 827E7998 writes these
            // two native pointers in program, descriptor order.
            void SetVertexProgram(const ProgramBufferData* value) { vertexProgram = value; }
            void SetVertexDescriptor(const VertexDescriptorData* value) { vertexDescriptor = value; }
            const ProgramBufferData* GetVertexProgram() const { return vertexProgram; }
            const VertexDescriptorData* GetVertexDescriptor() const { return vertexDescriptor; }

        private:
            friend class VertexProgramState;
            const ProgramBufferData* vertexProgram;
            const VertexDescriptorData* vertexDescriptor;
        };

        // Minimum capacity for the linked NoBind path, ending after its store
        // at +0x24. This does not assert the original full state size/alignment.
        enum { X360NoBindStorageSize = 0x28 };

        static VertexProgramState* InitializeNoBindX360(
            const rw::Resource& resource, const Parameters& parameters);
        ::D3DVertexShader* GetD3DVertexShader();

    protected:
        // DecFIGS states.h:511-513: resource construction owns the backing
        // storage; this interface is never instantiated as a C++ value.
        VertexProgramState();
        VertexProgramState(const VertexProgramState&);
        ~VertexProgramState();
    };

    CGS_STATIC_ASSERT((sizeof(VertexProgramState::Parameters) == 8), "ARTIST vertex state parameters");
}
