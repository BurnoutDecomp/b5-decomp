#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"  // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultReader.h"  // mInput

// CgsDev::DebugRenderStreamReader - the read-back side of the debug render-command stream. It owns a
// CgsMemory::DataStreamResultReader (mInput) over two buffers: a result buffer of fixed 256-byte
// command pages (15 debug-render commands per page) and a variable-size data buffer. Construct sizes
// and allocates both buffers through a supplied allocator, then hands them to mInput; Begin opens a
// read pass over mInput.
//
// X360 homes (DecFIGS DWARF / asm): Construct(IResourceAllocator*, liMaxCommands, liDataBufferSize)
// and Begin. End closes the read pass and replays every streamed command into the debug
// manager's buffered renderer. The remaining Construct overloads and Destruct are declared only.

namespace CgsMemory { class LinearMalloc; class HeapMalloc; }

namespace CgsDev
{
    namespace Internal { struct DebugStreamInput; }

    class DebugRenderStreamReader
    {
    public:
        // 0x82820CE8 - allocate the command + data buffers through lpAllocator and construct mInput.
        // liMaxCommands rounds up to whole 256-byte command pages (15 commands per page).
        void Construct(rw::IResourceAllocator* lpAllocator, s32 liMaxCommands, s32 liDataBufferSize);

        // 0x82817718 - open a read pass over the underlying result reader.
        void Begin();

        // Close the read pass, then (holding the debug manager) replay each streamed command into
        // the buffered renderer's 2D or world queue; an out-of-range event id is logged and dropped.
        void End();

        // Declared only.
        void Construct(Internal::DebugStreamInput* lpCommandBuffer, s32 liCommandBufferLength,
                       void* lpDataBuffer, s32 liDataBufferSize);
        void Construct(CgsMemory::LinearMalloc* lpAllocator, s32 liMaxCommands, s32 liDataBufferSize);
        void Construct(CgsMemory::HeapMalloc* lpAllocator, s32 liMaxCommands, s32 liDataBufferSize);
        void Destruct();

        // Commands per 256-byte page / page stride (X360: v5 = (liMaxCommands + 14) / 15, page = 256).
        static const s32 KI_COMMANDS_PER_PAGE = 15;
        static const s32 KI_PAGE_SIZE         = 256;

    private:
        CgsMemory::DataStreamResultReader mInput;   // DWARF CgsDebugRenderStreamReader.h:78
    };
}
