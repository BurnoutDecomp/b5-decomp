#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h
//
// BrnWorldIO::DispatchInputBuffer -- the World module's per-frame input payload the graphics
// dispatch pass reads while building the CgsGraphics::DispatchFrame. Derives the shared
// CgsModule::IOBuffer (status-flag-guarded read/write locking; bit 4 = read lock, bit 3 = write
// lock at status byte @+0). Member ORDER + member TYPES are the DecFIGS DWARF for
// GameSource/World/BrnWorldModuleIO.h (struct BrnWorldIO::DispatchInputBuffer : public IOBuffer,
// DWARF :422..:438); the accessor byte offsets are X360-pinned.
//
// Reconstructed with NAMED members accessed BY NAME (the committed sibling pattern in
// RendererIO::OutputBuffer, GameSource/Graphics/BrnRendererModuleIO.h, and
// BrnEffects::EffectsIO::DispatchInputBuffer), NOT via a byte-image blob. Every member TYPE is a
// committed home reused BY NAME (see the includes); the render-payload pointers reuse the same
// committed types RendererIO::OutputBuffer does (BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*,
// BrnCoronaManager::BrnSubmissionInterface*, BrnEffectsFrame*, BrnShaderConstantsFrame*,
// CgsGraphics::DispatchFrame*), NOT invented parallel types.
//
// [x64 WIDENING RULE: mCameraInput embeds BrnDirector::Camera::Camera BY VALUE (X360 0x160 span);
// pointers/queues widen to 8 bytes on the host, so every member AFTER the camera lands at a
// different byte offset than the X360 32-bit layout below. Parity is BY NAMED MEMBER; no offsetof
// _AssertLayout() is possible past the widening camera. The X360 offsets are documented for
// reference only.]
//
// X360 (32-bit) byte offsets pinned by the accessor asm:
//   +0x000  IOBuffer status byte (base)
//   +0x004  mpDispatchFrame            CgsGraphics::DispatchFrame*                 DWARF :423
//   +0x010  mCameraInput               BrnDirector::Camera::Camera (0x160 span)   DWARF :424
//   +0x170  mpShaderConstantsFrame     BrnShaderConstantsFrame*                    DWARF :425
//   +0x174  mapEffectsFrames[4]        BrnEffectsFrame*[4]  (slot*4)               DWARF :429
//   +0x184  mpBlobbyShadowBuffer       BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*  DWARF :431
//   +0x188  mpCoronaSubmissionInterface BrnCoronaManager::BrnSubmissionInterface*  DWARF :432
//   +0x18C  mfGameTime                 f32                                         DWARF :434
//   +0x190  mfSimTime                  f32                                         DWARF :435 (X360 +0x190)
//   +0x194  mpDispatchThreadInputBuffer BrnGame::DispatchThreadInputBuffer*        DWARF :436
//   +0x198  mRenderSwitches            RendererIO::RenderSwitches (6 bools)        DWARF :438
// ============================================================================

#include "types.hpp"                                            // u8, f32
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"          // CgsModule::IOBuffer base
#include "GameSource/Director/Camera/Camera.h"                  // BrnDirector::Camera::Camera (embedded by value)
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"         // BrnBlobbyShadowManager::BrnBlobbyShadowBuffer (nested class)
#include "GameSource/Graphics/BrnCoronaManager.h"               // BrnCoronaManager::BrnSubmissionInterface (nested class)
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"       // BrnGame::DispatchThreadInputBuffer (pointer member)

// Forward decls -- these members are used by pointer only and are canonical committed types.
namespace CgsGraphics { class DispatchFrame; }
class BrnEffectsFrame;
class BrnShaderConstantsFrame;

// RendererIO::RenderSwitches -- the per-frame render-enable switches (6 bools) embedded by value at
// the tail of this buffer (DWARF :438). Its DWARF home is GameSource/Graphics/BrnRendererModuleIO.h
// (:68), but that header cannot be co-included here: it forward-declares BrnBlobbyShadowManager as a
// NAMESPACE and BrnCoronaManager with only a nested BrnSubmissionInterface, both of which ODR-clash
// with the REAL class homes this buffer must include (BrnBlobbyShadowManager.h / BrnCoronaManager.h,
// packet-authoritative). BrnRendererModule.h carries yet another forward-slice copy (+ its own
// conflicting BrnBlobbyShadowManager slice). Per the established project minimal-slice convention
// (see the many canonical-spelling payload slices in BrnWorldModuleIO.h), the 6-bool POD is
// reproduced here in its canonical RendererIO namespace. It is a trivially-copyable POD accessed
// only by value/copy-assign. FLAG (duplicate forward-slice to consolidate, not retype): the DWARF
// home already flags RenderSwitches for consolidation; when the renderer-IO forward-decl defect is
// fixed this slice should be dropped in favour of the real header.
namespace RendererIO
{
    struct RenderSwitches
    {
        bool mbRenderShadows;
        bool mbRenderEnvmap;
        bool mbRenderWorld;
        bool mbRenderProps;
        bool mbRenderRaceCars;
        bool mbRenderTraffic;

        void Construct();
    };
}

namespace BrnWorldIO
{
    // DWARF BrnWorldModuleIO.h:422 -- struct DispatchInputBuffer : public IOBuffer.
    struct DispatchInputBuffer : public CgsModule::IOBuffer
    {
        // --- accessors bodied in BrnWorldModuleIO_DispatchInputBuffer.cpp ---------------
        // Getters read-lock (status bit 4, "Not locked for reading\n"); setters write-lock
        // (status bit 3, "Not locked for writing\n") -- reproduced exactly as the X360 asm tests
        // them. (The X360 rodata carries the trailing \n on both lock strings.)

        // ---- dispatch frame ----
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;                                   // 0x827A4EB0 (:383 R)
        void                        SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame); // 0x823B6B30 (W)

        // ---- director camera (embedded by value) ----
        const BrnDirector::Camera::Camera* GetCameraInput() const;                              // 0x827A4248 (:385 R)
        void                               SetCameraInput(const BrnDirector::Camera::Camera* lpCamera); // 0x823C8F28 (W)

        // ---- shader-constants frame ----
        BrnShaderConstantsFrame* GetShaderConstantsFrame() const;                               // 0x827BBEF0 (R)
        void                     SetShaderConstantsFrame(BrnShaderConstantsFrame* lpFrame);      // 0x823B50B0 (W)

        // ---- per-Fx-layer effects frames (slot<4) ----
        BrnEffectsFrame* GetEffectsFrame(u8 luSlot) const;                                      // 0x827BCCD8 (:393 R)
        void             SetEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame);                  // 0x823B6BD8 (W)

        // ---- blobby-shadow submission buffer ----
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* GetBlobbyShadowBuffer() const;           // 0x827A42F0 (:398 R)
        void SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBuffer);    // 0x823B5158 (W)

        // ---- corona submission interface ----
        BrnCoronaManager::BrnSubmissionInterface* GetCoronaSubmissionInterface() const;         // 0x827A4398 (:401 R)
        void SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface* lpInterface); // 0x823B5200 (W)

        // ---- game/sim time scalars ----
        f32  GetGameTime() const;                                                               // 0x827BBF98 (R)
        void SetGameTime(f32 lfGameTime);                                                       // 0x823B52A8 (W)
        f32  GetSimTime() const;                                                                 // 0x827BC040 (R)
        void SetSimTime(f32 lfSimTime);                                                          // 0x823B5358 (W)

        // ---- dispatch-thread input buffer handle ----
        BrnGame::DispatchThreadInputBuffer* GetDispatchThreadInputBuffer() const;                // 0x827BC0E8 (:411 R)
        void SetDispatchThreadInputBuffer(BrnGame::DispatchThreadInputBuffer* lpBuffer);         // 0x823B5408 (W)

        // ---- per-frame render switches (embedded by value) ----
        void SetRenderSwitches(const RendererIO::RenderSwitches& lrSwitches);                    // 0x823B6CB8 (:438 W)

        // NOTE: no offsetof _AssertLayout() -- mCameraInput embeds a Camera by value (0x160 span on
        // X360) so every member after it widens on the x64 gate; the X360 byte offsets cannot be
        // static_assert-pinned. Parity is BY NAMED MEMBER per the x64-gate rule.

    private:
        // --- Layout (DWARF member order; offsets X360-pinned in the banner) ------------
        CgsGraphics::DispatchFrame*                    mpDispatchFrame;             // :423  (X360 +0x04)
        BrnDirector::Camera::Camera                    mCameraInput;                // :424  (X360 +0x10, 0x160 span -- WIDENS)
        BrnShaderConstantsFrame*                       mpShaderConstantsFrame;      // :425  (X360 +0x170)
        BrnEffectsFrame*                               mapEffectsFrames[4];         // :429  (X360 +0x174)
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* mpBlobbyShadowBuffer;        // :431  (X360 +0x184)
        BrnCoronaManager::BrnSubmissionInterface*      mpCoronaSubmissionInterface; // :432  (X360 +0x188)
        f32                                            mfGameTime;                  // :434  (X360 +0x18C)
        f32                                            mfSimTime;                   // :435  (X360 +0x190)
        BrnGame::DispatchThreadInputBuffer*            mpDispatchThreadInputBuffer; // :436  (X360 +0x194)
        RendererIO::RenderSwitches                     mRenderSwitches;             // :438  (X360 +0x198, 6 bools)
    };
}
