#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // the header-baked lock asserts

// Out-of-line body for the OutputBuffer accessor the X360 build emitted in its own
// function group (@0x8276DB18). Same idiom as the sibling
// BrnAIModuleIO_OutputBuffer.cpp accessors: return the address of the member the X360
// offset names, guarded by whichever lock bit the asm names.
//
// DecFIGS declares the mutable and const GetRouteMapResponseQueue overloads.
// ARTIST 0x8279CB50 returns the same member under a read lock.

namespace BrnAI
{
namespace AIModuleIO
{
    // X360 0x8276DB18 (W, :430) -- write-lock (status bit 3, `lbz 0(this); extrwi 1,28`);
    // on failure streams "Not locked for writing". Returns the route response queue
    // (`addi r3,this,0x1014` on the console spine).
    // Callers AIModule::PausedUpdate, AIModule::Update.
    // Faithfully tests the WRITE bit (do not "fix" to read).
    OutputBuffer::RouteResponseQueue* OutputBuffer::GetRouteMapResponseQueue()
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\ai\\BrnAIModuleIO.h", 430);
        return &mRouteResponseQueue;
    }
}
}
