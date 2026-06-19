#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGame::DispatchThreadInputBuffer::GetParticleData() const @ 0x8227F4F0 (DWARF h:97).
//
// The X360 body tests IOBuffer status bit 4 ((*a1 >> 4) & 1 -> the read-lock bit) and, on failure,
// inlines a StrStream to build the message "Not locked for reading\n" before firing the assert at
// BrnDispatchThreadInputBuffer.h:97, then returns a1 + 16 (= &mParticleData).
//
// Mirrors b5-decomp/src/GameShared/GameClasses/Memory/CgsMemoryModuleIO.cpp EXACTLY: the simple
// CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n") (IsBufferLockedForReading() is
// the inherited CgsModule::IOBuffer read-lock query) then return &mParticleData.
//
// FLAG (benign parity gap): the X360 inlined a StrStream (BeginAssert/BasePriorityQueue::Clear/the
// stream operator/FireAssert/EndAssert) to compose the assert message; we deliberately use the
// committed CGS_ASSERT macro instead and do NOT reproduce that StrStream machinery. Parity YELLOW
// for the missing assert-stream calls is the expected/committed convention.
namespace BrnGame
{
    const BrnParticle::ParticleModule::DispatchThreadUpdateData* DispatchThreadInputBuffer::GetParticleData() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mParticleData;
    }
}
