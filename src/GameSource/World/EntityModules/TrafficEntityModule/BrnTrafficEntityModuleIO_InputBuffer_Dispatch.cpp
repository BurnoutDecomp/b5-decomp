#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch members, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Eight accessors over four contiguous pointer slots at the buffer tail
// (console words +0x8014..+0x8020; host pointers, see the header's POINTER WIDTH note).
// Mirrors the sibling BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists.
//
// Getters test the read-lock bit (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 ==
// IsBufferLockedForReading()) and fire "Not locked for reading\n"; setters test the write-lock
// bit (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting()) and fire
// "Not locked for writing\n". The streamed-message asserts map to the house CGS_ASSERT with the
// trailing "\n" dropped from the stringized condition. Each field is a 32-bit word reached by
// `ori r11,0,<off>; lwzx/stwx r,this,r11` (base is u8*, so displacement == true byte offset).
//
// Two Hex-Rays names were recovered truncated: "G" == GetBlobbyShadowBuffer (paired with
// SetBlobbyShadowBuffer @0x827A0F70, +0x8018) and "GetCor" == GetCoronaSubmissionInterface
// (paired with SetCoronaSubmissionInterface @0x827A1020, +0x801C).

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    void InputBuffer_Dispatch::_AssertLayout()
    {
        // HOST layout, not the console's: all four tail slots are real 8-byte host pointers now
        // (see the header's POINTER WIDTH note), so the block starts at the
        // next 8-byte boundary after the 0x8014-byte opaque payload and steps by sizeof(void*).
        // Transcribing the console's 0x8014/0x8018/0x801C/0x8020 here would re-assert exactly the
        // 32-bit-word shape the widening removes.
        static_assert(offsetof(InputBuffer_Dispatch, mpDispatchFrame) == 0x8018,
                      "mpDispatchFrame @0x8018 (host: 0x8014 payload rounded up to pointer alignment)");
        static_assert(offsetof(InputBuffer_Dispatch, mpBlobbyShadowBuffer)
                          == offsetof(InputBuffer_Dispatch, mpDispatchFrame) + sizeof(void*),
                      "mpBlobbyShadowBuffer immediately follows mpDispatchFrame");
        static_assert(offsetof(InputBuffer_Dispatch, mpCoronaSubmissionInterface)
                          == offsetof(InputBuffer_Dispatch, mpBlobbyShadowBuffer) + sizeof(void*),
                      "mpCoronaSubmissionInterface immediately follows mpBlobbyShadowBuffer");
        static_assert(offsetof(InputBuffer_Dispatch, mpShadowMap)
                          == offsetof(InputBuffer_Dispatch, mpCoronaSubmissionInterface) + sizeof(void*),
                      "mpShadowMap immediately follows mpCoronaSubmissionInterface");
    }

    // ------------------------------------------------------------------------
    // InputBuffer_Dispatch::Construct   @ 0x8275CF40
    // ------------------------------------------------------------------------
    // The console body is five
    // statements:
    //     stb 1, 0(this)                                   -> IOBuffer::Construct()
    //     bl  VariableEventQueue<32768,16>::Construct(+4)   -> the scene-result queue
    //     stw 0, 0x8014(this)                              -> muDispatchFrame = 0
    //     stw 0, 0x8018(this)                              -> muBlobbyShadowBuffer = 0
    //     stw 0, 0x801C(this)                              -> muCoronaSubmissionInterface = 0
    //     stw 0, 0x8020(this)                              -> muShadowMap = 0
    // The four handle words are the ones TrafficEntityModule::GenerateDispatchLists reads
    // every frame; without this they hold the previous IO-stack tenant's bytes now that
    // CreateIOBuffer<T> default-initialises instead of value-initialising.
    // [FLAG] the `VariableEventQueue<32768,16>::Construct(this+4)` leg is NOT emitted: that
    // queue lives inside the documented opaque maPayloadAndPad span (see the header FLAG) and
    // has no named member to reach. Nothing in this tree reads it -- the only accessor,
    // GetSceneResultQueue, is still the inert WorldLinkStubs gate that returns NULL. Restore
    // this call together with the member when that slice lands.
    void InputBuffer_Dispatch::Construct()
    {
        CgsModule::IOBuffer::Construct();      // stb 1, 0(this) @0x8275CF40

        // (the four console words at +0x8014..+0x8020 are the host pointers -- see the header's
        //  POINTER WIDTH note -- so they are nulled by name, not by offset)
        mpDispatchFrame             = 0;       // *(this+0x8014) = 0 @0x8275CF40
        mpBlobbyShadowBuffer        = 0;       // *(this+0x8018) = 0 @0x8275CF40
        mpCoronaSubmissionInterface = 0;       // *(this+0x801C) = 0 @0x8275CF40
        mpShadowMap                 = 0;       // *(this+0x8020) = 0 @0x8275CF40
    }

    // X360 0x827120D8 (asm-line :482) -- read-lock; return the dispatch frame (console word @this+0x8014).
    CgsGraphics::DispatchFrame* InputBuffer_Dispatch::GetDispatchFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mpDispatchFrame;
    }

    // X360 0x827A0EC0 (asm-line :483) -- write-lock; set the dispatch frame (console word @this+0x8014).
    void InputBuffer_Dispatch::SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mpDispatchFrame = lpDispatchFrame;
    }

    // X360 0x82712188 (Hex-Rays "G", asm-line :488) -- read-lock; return the blobby-shadow
    // buffer (console word @this+0x8018).
    BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* InputBuffer_Dispatch::GetBlobbyShadowBuffer() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mpBlobbyShadowBuffer;
    }

    // X360 0x827A0F70 (asm-line :489) -- write-lock; set the blobby-shadow buffer (console word @this+0x8018).
    void InputBuffer_Dispatch::SetBlobbyShadowBuffer(
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobbyShadowBuffer)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mpBlobbyShadowBuffer = lpBlobbyShadowBuffer;
    }

    // X360 0x82712238 (Hex-Rays "GetCor", asm-line :491) -- read-lock; return the corona-
    // submission interface (console word @this+0x801C).
    BrnCoronaManager::BrnSubmissionInterface* InputBuffer_Dispatch::GetCoronaSubmissionInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mpCoronaSubmissionInterface;
    }

    // X360 0x827A1020 (asm-line :492) -- write-lock; set the corona-submission interface (console word @this+0x801C).
    void InputBuffer_Dispatch::SetCoronaSubmissionInterface(
        BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mpCoronaSubmissionInterface = lpCoronaSubmissionInterface;
    }

    // X360 0x827122E8 (asm-line :494) -- read-lock; return the shadow map (console word @this+0x8020).
    BrnWorld::ShadowMap* InputBuffer_Dispatch::GetShadowMap() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mpShadowMap;
    }

    // X360 0x827A10D0 (asm-line :495) -- write-lock; set the shadow map (console word @this+0x8020).
    void InputBuffer_Dispatch::SetShadowMap(BrnWorld::ShadowMap* lpShadowMap)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mpShadowMap = lpShadowMap;
    }
}
}
