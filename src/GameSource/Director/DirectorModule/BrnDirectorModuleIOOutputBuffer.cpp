#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnDirector::DirectorIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Write-side sibling of the InputBuffer accessor TU
// (BrnDirectorModuleIOInputBuffer.cpp).
//
//   read-lock getters  @ 0x823B33B0 / 0x823B23F8 / 0x823B3458 / 0x823B2548 / 0x823B3500
//   write-lock getters @ 0x82207848 / 0x82206A58 / 0x822077A0 / 0x822078F0
//   camera setters     @ 0x8224EF70 (SetCameraOutput) / 0x82233BA0 (SetCgsCamera)
//
// Every accessor first checks the IOBuffer lock-state flag and asserts on violation, exactly as
// the X360 bodies do (the original streams the file/line via CgsDev::Assert; CGS_ASSERT carries the
// stringized condition). The bodies then return the address of the member the X360 addresses by raw
// offset, or forward operator= into the embedded camera. Note which lock bit each function tests --
// several Get* deliberately assert the WRITE lock; this mirrors the asm and is not "fixed".

namespace BrnDirector
{
namespace DirectorIO
{
    // ---- byte-offset pins (X360-recovered, x64-gate-safe) -----------------------------------
    // Pin ONLY offsets identical on console AND the x64 compile-gate host -- i.e. the members that
    // precede the widening BrnDirector::Camera::Camera (mCameraOutput). That camera holds three raw
    // pointer members (mpDebugInfoBehaviour / mpSourceShot / mpCrashAnalysis) that widen 4->8 on the
    // host (console sizeof 0x160 -> host 0x170), pushing every member at/after mCameraOutput 16 bytes
    // past its console offset. Camera.h documents this as "parity BY NAMED MEMBER, not by byte
    // offset". So mCameraOutput and everything after it are NOT byte-pinned here, and we do NOT
    // assert sizeof(BrnDirector::Camera::Camera). mCgsCamera (pointer-free, precedes the widening
    // member) and CgsGraphics::Camera's own pointer-free stride ARE identical on both and stay pinned.
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mCgsCamera)               == 0x0010, "mCgsCamera @0x0010");
        static_assert(sizeof(CgsGraphics::Camera)                      == 0x0170, "CgsGraphics::Camera stride 0x170");
    }

    // ---- read-lock-asserted getters ---------------------------------------------------------

    // X360 0x823B33B0: return &mResourceInterface (this+0x2E0), read-lock asserted.
    u8* OutputBuffer::Get()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mResourceInterface;
    }

    // X360 0x823B23F8: return &mDirectorInterface (this+0x720), read-lock asserted.
    u8* OutputBuffer::GetDir()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mDirectorInterface;
    }

    // X360 0x823B3458: return &mDirectorOutputInterface (this+0x4F0), read-lock asserted.
    u8* OutputBuffer::GetDirectorOu()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mDirectorOutputInterface;
    }

    // X360 0x823B2548: return &mReplayRequestInterface (this+0x724), read-lock asserted.
    u8* OutputBuffer::GetReplayRe()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mReplayRequestInterface;
    }

    // X360 0x823B3500: return &mTimerRequestInterface (this+0x500), read-lock asserted.
    u8* OutputBuffer::GetTimerRequestIn()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mTimerRequestInterface;
    }

    // ---- write-lock-asserted getters --------------------------------------------------------

    // X360 0x822077A0: return &mResourceInterface (this+0x2E0). Tests the WRITE lock (bit 3).
    u8* OutputBuffer::GetResour()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mResourceInterface;
    }

    // X360 0x82207848: return &mDirectorOutputInterface (this+0x4F0). Tests the WRITE lock (bit 3).
    u8* OutputBuffer::GetDirectorOutputIn()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mDirectorOutputInterface;
    }

    // X360 0x82206A58: return &mReplayRequestInterface (this+0x724). Tests the WRITE lock (bit 3).
    u8* OutputBuffer::GetReplayRequestI()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mReplayRequestInterface;
    }

    // X360 0x822078F0: return &mTimerRequestInterface (this+0x500). Tests the WRITE lock (bit 3).
    u8* OutputBuffer::GetTimerRequestInterfac()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mTimerRequestInterface;
    }

    // ---- +0x510 timer-request sub-interface accessors (read/write) --------------------------
    // The X360 returns this+0x510, which lands 16 bytes into the timer-request interface span
    // (@0x500); addressed via that member (&mTimerRequestInterface[0x10]) to preserve the exact
    // X360 offset without re-laying-out committed storage -> NO layout change. Exact Get-name is
    // IDA-truncated to the class token 'Output' -> honest placeholder (FLAG).

    // X360 0x823B24A0 (BrnDirectorModuleIO.h:469): read-lock; return this+0x510.
    u8* OutputBuffer::GetTimerRequestSubInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTimerRequestInterface[0x0510 - 0x0500];
    }

    // X360 0x822069B0 (BrnDirectorModuleIO.h:470): write-lock; return this+0x510. Non-const
    // (write-side) twin of the accessor above -- tests bit 3 (write-lock).
    u8* OutputBuffer::GetTimerRequestSubInterfaceW()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTimerRequestInterface[0x0510 - 0x0500];
    }

    // ---- write-lock-asserted camera setters -------------------------------------------------

    // X360 0x82233BA0: write-lock copy into the embedded CgsGraphics camera (this+16), forwarding
    // CgsGraphics::Camera::operator= and returning its result.
    CgsGraphics::Camera& OutputBuffer::SetCgsCamera(const CgsGraphics::Camera& lrCamera)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mCgsCamera = lrCamera;
    }

    // X360 0x8224EF70: write-lock copy into the embedded director camera (this+384), forwarding
    // BrnDirector::Camera::Camera::operator= and returning its result.
    BrnDirector::Camera::Camera& OutputBuffer::SetCameraOutput(const BrnDirector::Camera::Camera& lrCamera)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mCameraOutput = lrCamera;
    }

    // The read-lock twin of SetCameraOutput over the same member. The consumer side is the
    // renderer/world dispatch bridge (X360 BridgeRendererToWorld @0x823CDD20 reads the director
    // output's camera under LockForRead and hands it to the world dispatch input), which is the
    // path that turns a published director camera into the camera the world is drawn from.
    // Same read-lock assert as every other read-side getter in this TU.
    const BrnDirector::Camera::Camera* OutputBuffer::GetCameraOutput() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mCameraOutput;
    }
}
}
