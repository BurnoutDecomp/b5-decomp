#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line body for the OutputBuffer header-handle accessor emitted separately
// by the X360 build (function group @0x8276DB18). Same idiom as the sibling
// BrnAIModuleIO_OutputBuffer.cpp accessors: raw u8* return to `this + <attested
// offset>`, guarded by whichever lock bit the asm names.

namespace BrnAI
{
namespace AIModuleIO
{
    // X360 0x8276DB18 (W, :430) -- write-lock (status bit 3, `lbz 0(this); extrwi 1,28`);
    // on failure streams "Not locked for writing". Returns this + 0x1014
    // (`addi r3,this,0x1014`). Callers AIModule::PausedUpdate, AIModule::Update.
    // Faithfully tests the WRITE bit (do not "fix" to read).
    u8* OutputBuffer::GetAIOutputBufferHeader()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_AI_OUTPUT_BUFFER_HEADER_OFFSET;
    }
}
}
