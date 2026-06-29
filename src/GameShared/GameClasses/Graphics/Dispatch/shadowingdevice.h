#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/VertexProgramState.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

namespace shadow
{
    class Device
    {
    public:
        struct DrawIndexedParameters
        {
            u32 mePrimitiveType;
            u32 muBaseVertexIndex;
            u32 muMinVertexIndex;
            u32 muNumVertices;
        };

        static bool Initialize();
        static renderengine::VertexProgramState* GetVertexProgramState(
            const renderengine::ProgramBufferData* lpVertexProgram,
            const renderengine::VertexDescriptorData* lpVertexDescriptor);
        static void SetVertexProgramInternal();
        static void DrawIndexedMultipleStreams_Custom(
            const DrawIndexedParameters& lrParameters);

        static void FlushDepthStencilState();
        static void FlushRasterizerState();

        // Set a low-level render state through the shadow cache (the immediate-mode SetState path,
        // X360 0x82276D08 calls this). lbWasUnset is true when no state had been set yet (the X360
        // passes (last == 0) so the device can take the full-set path). Returns the device/result
        // pointer (X360 r3 passthrough). Body is the X360 shadow device's low-level setter.
        static void* Xbox2SetStateLowLevelShadowed(void* lpState, bool lbWasUnset);

    private:
        // The X360 build keeps the live vertex-program binding as a small static
        // slot (5 dwords @ dword_83011118) whose first entry points at the real
        // state object (@ unk_83010920); InitializeNoBindX360 dereferences the
        // slot to reach it. Both are modelled here by name.
        static renderengine::VertexProgramState mVertexProgramState;
        static renderengine::VertexProgramState* mapVertexProgramStateSlot[5];
        static bool mbVertexProgramStateDirty;
    };
}
