#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"          // CgsModule::IOBuffer base + read/write lock-state queries
#include "GameShared/GameClasses/Graphics/CgsCamera.h"          // CgsGraphics::Camera (committed, sizeof 0x170, pointer-free)
#include "GameSource/Director/Camera/Camera.h"                  // BrnDirector::Camera::Camera (committed; pointer members widen on x64)

// BrnDirector::DirectorIO::OutputBuffer -- the Director module's per-frame OUTPUT payload buffer,
// the write-side sibling of BrnDirector::DirectorIO::InputBuffer (BrnDirectorModuleIO.h). Like
// every CgsModule IO buffer it derives the shared CgsModule::IOBuffer (status-flag-guarded
// read/write locking; bit 4 = read lock, bit 3 = write lock) at offset +0, then embeds the
// director's published camera state plus a set of consumer/producer interface aggregates that the
// world / game-state / gui / replay / timer bridges access each frame.
//
// LAYOUT PROVENANCE. The exact BYTE OFFSETS below are recovered directly from the X360 accessor
// bodies in BURNOUT_X360_ARTIST.XEX (the getters `return this + <off>`; the camera setters
// forward `operator=` on the sub-object at `this + <off>`):
//
//     mCgsCamera                @0x0010 (16)     SetCgsCamera:     CgsGraphics::Camera::operator= (this+16)
//     mCameraOutput             @0x0180 (384)    SetCameraOutput:  BrnDirector::Camera::Camera::operator= (this+384)
//     mResourceInterface        @0x02E0 (736)    Get (read) / GetResour (write)
//     mDirectorOutputInterface  @0x04F0 (1264)   GetDirectorOu (read) / GetDirectorOutputIn (write)
//     mTimerRequestInterface    @0x0500 (1280)   GetTimerRequestIn (read) / GetTimerRequestInterfac (write)
//                                                 + a sub-interface 16 bytes in @0x0510 (see below)
//     mDirectorInterface        @0x0720 (1824)   GetDir (read)
//     mReplayRequestInterface   @0x0724 (1828)   GetReplayRe (read) / GetReplayRequestI (write)
//
// x64-GATE RULE (why the trailing offsets are NOT byte-pinned). BrnDirector::Camera::Camera carries
// three raw pointer members (mpDebugInfoBehaviour / mpSourceShot / mpCrashAnalysis) that widen
// 4->8 bytes on the 64-bit compile-gate host. Its console sizeof is 0x160; the host sizeof rounds
// up (0x160 + 12, under alignas(16)) to 0x170. Camera.h itself DELIBERATELY does not assert its own
// sizeof and documents the rule: "parity here is BY NAMED MEMBER, not by byte offset (the x64-gate
// rule)". So this file MUST NOT static_assert sizeof(BrnDirector::Camera::Camera)==0x160, and MUST
// NOT offsetof-pin any member at/after mCameraOutput (mResourceInterface onward), because the
// by-value BrnDirector camera pushes those members 16 bytes past their console offsets on the host.
// Only offsets identical on BOTH targets are pinned: mCgsCamera @0x0010 (it precedes the widening
// member) and sizeof(CgsGraphics::Camera)==0x0170 (that type is pointer-free and self-asserts its
// own size). The trailing opaque interface spans are still sized RELATIONALLY from the console
// offsets (next-minus-this): correct CONSOLE layout, reproduced additively, simply no longer
// byte-pinnable once a widening member sits above them. Grow each into its real type when its home
// is reconstructed.
//
// HONEST PLACEHOLDERS. The interface aggregates are modelled as correctly-SIZED, byte-addressable
// opaque storage carrying their recovered names + offsets (a resource interface, the director-output
// interface, a timer-request interface, the small director interface word, and the replay-request
// interface). This preserves the exact console object layout the accessors index into while being
// honest that the interiors are not yet known.
//
// NOTE. The read-side accessors assert the READ lock (status bit 4); the write-side accessors
// (GetResour / GetDirectorOutputIn / GetTimerRequestInterfac / GetReplayRequestI, the +0x510
// GetTimerRequestSubInterfaceW, and both camera setters) assert the WRITE lock (status bit 3). Some
// Get* deliberately test the write bit -- this mirrors the X360 asm and must not be "fixed".

namespace BrnDirector
{
namespace DirectorIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // --- read-lock-asserted getters (return &member) ---
        u8*       Get();                        // -> mResourceInterface        @0x02E0
        u8*       GetDir();                     // -> mDirectorInterface        @0x0720
        u8*       GetDirectorOu();              // -> mDirectorOutputInterface  @0x04F0
        u8*       GetReplayRe();                // -> mReplayRequestInterface   @0x0724
        u8*       GetTimerRequestIn();          // -> mTimerRequestInterface    @0x0500
        // +0x510: 16 bytes into the timer-request interface (read-lock). Exact Get-name is
        // IDA-truncated to the class token 'Output' -> honest placeholder; offset+lock authoritative.
        u8*       GetTimerRequestSubInterface();     // -> mTimerRequestInterface+0x10 (this+0x510)

        // --- write-lock-asserted getters (return &member) ---
        u8*       GetResour();                  // -> mResourceInterface        @0x02E0
        u8*       GetDirectorOutputIn();        // -> mDirectorOutputInterface  @0x04F0
        u8*       GetReplayRequestI();          // -> mReplayRequestInterface   @0x0724
        u8*       GetTimerRequestInterfac();    // -> mTimerRequestInterface    @0x0500
        // +0x510 write-lock twin of GetTimerRequestSubInterface (distinct name: C++ cannot overload
        // two same-signature non-const u8* methods -- mirrors the committed 0x500 read/write split).
        u8*       GetTimerRequestSubInterfaceW();    // -> mTimerRequestInterface+0x10 (this+0x510)

        // --- write-lock-asserted camera setters (forward operator= on the sub-object) ---
        CgsGraphics::Camera&           SetCgsCamera(const CgsGraphics::Camera& lrCamera);
        BrnDirector::Camera::Camera&   SetCameraOutput(const BrnDirector::Camera::Camera& lrCamera);

        // --- read-lock-asserted camera getter (the consumer side of SetCameraOutput) ---
        // The published director camera. X360 consumers read it under LockForRead through the
        // renderer/world bridge (BridgeRendererToWorld @0x823CDD20); the setter beside it is
        // @0x8224EF70. Same member, same buffer, opposite lock.
        const BrnDirector::Camera::Camera* GetCameraOutput() const;

    private:
        // @0x0001 .. 0x0010: the IOBuffer base is 1 byte (FlagSet8); pad up to the 16-byte-aligned
        // first camera member.
        u8  maBasePad[0x0010 - 0x0001];

        // @0x0010 (16): the CgsGraphics camera (SetCgsCamera). Pointer-free, self-asserts 0x170; the
        // one member whose offset is identical on console and x64 (it precedes any widening member).
        CgsGraphics::Camera         mCgsCamera;                  // @0x0010 (console+host identical)

        // @0x0180 (console): the BrnDirector camera (SetCameraOutput). Committed type WITH pointer
        // members that widen on x64, so this member (and everything after it) sits 16 bytes lower on
        // the host than the console offset. Parity is BY NAMED MEMBER, not by byte offset.
        BrnDirector::Camera::Camera mCameraOutput;               // @0x0180 (console)

        // Trailing interface aggregates: HONEST opaque storage, sized RELATIONALLY from the console
        // offsets (next-minus-this). Correct console layout; not host-byte-pinnable behind the
        // widening camera above. Grow each into its real type additively when its home is
        // reconstructed.
        u8  mResourceInterface[0x04F0 - 0x02E0];                 // @0x02E0 (console)
        u8  mDirectorOutputInterface[0x0500 - 0x04F0];           // @0x04F0 (console)
        u8  mTimerRequestInterface[0x0720 - 0x0500];             // @0x0500 (console) (+0x10 sub-iface)
        u8  mDirectorInterface[0x0724 - 0x0720];                 // @0x0720 (console) 4-byte word
        u8  mReplayRequestInterface[4];                          // @0x0724 (console) 4-byte handle word

        // Pin ONLY the console==host-stable facts (before the widening camera).
        static void _AssertLayout();
    };
}
}
