#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line body for the OutputBuffer accessor the X360 build emitted in its own
// function group (@0x8276DB18). Same idiom as the sibling
// BrnAIModuleIO_OutputBuffer.cpp accessors: return the address of the member the X360
// offset names, guarded by whichever lock bit the asm names.
//
// ⭐ RENAMED 2026-08-25 (aimodule wave): this used to be `GetAIOutputBufferHeader`, an
// invented placeholder name for "the untyped thing at this+0x1014". The ARTIST body at
// 0x8279CB50 returns the SAME this+0x1014 under a READ lock at BrnAIModuleIO.h:437 --
// i.e. it is the read twin of this write-side :430 -- and the tree's declared-only
// `GetRouteResponseQueue()` is exactly that read twin. So +0x1014 is the ROUTE RESPONSE
// QUEUE and this is its write-side handle. The old name had no caller outside this
// group (verified by grep before the rename).

namespace BrnAI
{
namespace AIModuleIO
{
    // X360 0x8276DB18 (W, :430) -- write-lock (status bit 3, `lbz 0(this); extrwi 1,28`);
    // on failure streams "Not locked for writing". Returns the route response queue
    // (`addi r3,this,0x1014` on the console spine).
    // Callers AIModule::PausedUpdate, AIModule::Update.
    // Faithfully tests the WRITE bit (do not "fix" to read).
    u8* OutputBuffer::GetRouteResponseQueueForWrite()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(&mRouteResponseQueue);
    }
}
}
