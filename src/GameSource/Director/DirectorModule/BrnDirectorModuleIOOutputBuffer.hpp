#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"          // CgsModule::IOBuffer base + read/write lock-state queries
#include "GameShared/GameClasses/Graphics/CgsCamera.h"          // CgsGraphics::Camera (committed, sizeof 0x170, pointer-free)
#include "GameSource/Director/Camera/Camera.h"                  // BrnDirector::Camera::Camera (committed; pointer members widen on x64)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"  // GameDataIO::RequestInterface<512> (mResourceInterface)
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"      // CgsSystem::TimerRequestInterface (mTimerRequestInterface)
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"      // AttribSysRequestInterface<512> (mVaultRequestInterface)

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
//                                                 == GameDataIO::RequestInterface<512> (GROWN)
//     mDirectorOutputInterface  @0x04F0 (1264)   GetDirectorOu (read) / GetDirectorOutputIn (write)
//     mTimerRequestInterface    @0x0500 (1280)   GetTimerRequestIn (read) / GetTimerRequestInterfac (write)
//                                                 + a sub-interface 16 bytes in @0x0510 (see below)
//
// ⭐ 2026-07-29 FINDING about the +0x510 sub-interface -- ACTED ON 2026-08-01 (Prepare wave).
// LoadDirectorModule @0x823E74C0 feeds the @0x510 read-getter (0x823B24A0) straight into
// `VariableEventQueue<32768,16>::Append<512,16>` on the GameData input's queue at +0x8014 --
// which is the GameData input's ATTRIBSYS request queue (the same destination
// LoadWorldModule's AttribSys append uses). So @0x510 is a VariableEventQueue<512,16> holding
// ATTRIBSYS VAULT requests, not a timer sub-record; "GetTimerRequestSubInterface" was a
// mis-naming inherited from the truncated IDA symbol.
// The span is now SPLIT into its two real members and the accessors carry the DWARF name
// `GetVaultRequestInterface` (BrnDirectorResourceManager.h's own note demanded exactly this):
//     mTimerRequestInterface  @0x0500  CgsSystem::TimerRequestInterface          (16 bytes:
//                                       two 8-byte TimerRequests -- game + sim)
//     mVaultRequestInterface  @0x0510  AttribSysRequestInterface<512>            (528 bytes:
//                                       a VariableEventQueue<512,16>; 0x720-0x510 == 0x210)
// The two console strides add up EXACTLY to the old opaque 0x220 span, which is the
// independent confirmation that the split is where the console puts it. It is
// DirectorResourceManager::Prepare @0x8225CA08 that posts on this queue (RegisterVault).
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
        // The X360 CreateIOBuffer<OutputBuffer> instantiation runs Construct after the stack
        // alloc: the IOBuffer base status plus the embedded request queue's own Construct
        // (a VariableEventQueue asserts "Not Constructed" on its first AddEvent otherwise).
        // The generic PC CreateIOBuffer<T> template only placement-news, so every creation
        // site calls this explicitly -- same restoration as the GUI input buffer's Construct.
        void Construct();

        // --- read-lock-asserted getters (return &member) ---
        // X360 0x823B33B0 -> mResourceInterface @0x02E0. The consumer is
        // LoadDirectorModule's InputBuffer::AppendRequestInterface<512>, whose parameter is
        // `const RequestInterface<512>&` -- hence the const return.
        const BrnResource::GameDataIO::RequestInterface<512>* Get();
        u8*       GetDir();                     // -> mDirectorInterface        @0x0720
        u8*       GetDirectorOu();              // -> mDirectorOutputInterface  @0x04F0
        u8*       GetReplayRe();                // -> mReplayRequestInterface   @0x0724
        CgsSystem::TimerRequestInterface* GetTimerRequestIn();   // -> mTimerRequestInterface @0x0500
        // X360 0x823B24A0 -> &mVaultRequestInterface (this+0x510), READ-lock. DWARF name
        // (BrnDirectorResourceManager.h:319 recorded the correction); the const overload is
        // the read-side twin so both can carry the one DWARF spelling.
        const CgsAttribSys::AttribSysIO::AttribSysRequestInterface<512>*
                  GetVaultRequestInterface() const;

        // --- write-lock-asserted getters (return &member) ---
        // X360 0x822077A0 -> mResourceInterface @0x02E0. This is the interface
        // DirectorModule::Prepare hands to WorldMap::LoadData, which stages the trigger /
        // traffic-lane / AI-lane requests onto it.
        BrnResource::GameDataIO::RequestInterface<512>* GetResour();
        u8*       GetDirectorOutputIn();        // -> mDirectorOutputInterface  @0x04F0
        u8*       GetReplayRequestI();          // -> mReplayRequestInterface   @0x0724
        CgsSystem::TimerRequestInterface* GetTimerRequestInterfac();  // -> mTimerRequestInterface @0x0500
        // X360 0x822069B0 -> &mVaultRequestInterface (this+0x510), WRITE-lock. Same DWARF name
        // as the const twin above; the constness is what separates the two lock bits here.
        CgsAttribSys::AttribSysIO::AttribSysRequestInterface<512>* GetVaultRequestInterface();

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
        // @0x02E0 (console): GROWN to its REAL type (2026-07-29, DJ fly-by campaign).
        // Attested by LoadingScriptedState::LoadDirectorModule @0x823E74C0, which feeds this
        // exact member to BrnResource::GameDataIO::InputBuffer::AppendRequestInterface<512>
        // (mangled ??$AppendRequestInterface@$0CAA@...), and by WorldMap::LoadData
        // @0x8225F5A0, which calls VariableEventQueue<512,16>::AddEvent and
        // RequestInterface<512>::LoadTrafficLanes/GetAILanes on it. Its console stride is
        // 0x4F0-0x2E0 == 528 == 512 + the 16-byte VariableEventQueue header, and the host
        // stride is the same (the queue is byte storage plus three s32 -- no pointer member),
        // so growing it does NOT move mDirectorOutputInterface or anything after it.
        BrnResource::GameDataIO::RequestInterface<512> mResourceInterface;   // @0x02E0 (console)
        u8  mDirectorOutputInterface[0x0500 - 0x04F0];           // @0x04F0 (console)
        // @0x0500 (console): GROWN to its REAL type (2026-08-01, Prepare wave). The console
        // stride to the next member is 0x510-0x500 == 16 == sizeof(CgsSystem::TimerRequests)*2,
        // which is exactly what CgsTimerRequestInterface.h declares (mGameTimer + mSimTimer,
        // both {u32 muFlags; f32 mfMultiplier}). Pointer-free, so console == host stride.
        CgsSystem::TimerRequestInterface mTimerRequestInterface;  // @0x0500 (console)
        // @0x0510 (console): GROWN to its REAL type. Attested by LoadDirectorModule
        // @0x823E74C0 (feeds it to VariableEventQueue<32768,16>::Append<512,16> on the GameData
        // input's ATTRIBSYS queue) and by DirectorResourceManager::Prepare @0x8225CA08 (posts
        // AttribSysRequestInterface<512>::RegisterVault @0x82256428 on it). Console stride
        // 0x720-0x510 == 528 == 512 + the 16-byte VariableEventQueue header.
        CgsAttribSys::AttribSysIO::AttribSysRequestInterface<512> mVaultRequestInterface;  // @0x0510 (console)
        u8  mDirectorInterface[0x0724 - 0x0720];                 // @0x0720 (console) 4-byte word
        u8  mReplayRequestInterface[4];                          // @0x0724 (console) 4-byte handle word

        // Pin ONLY the console==host-stable facts (before the widening camera).
        static void _AssertLayout();
    };
}
}
