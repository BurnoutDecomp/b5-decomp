#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer base (status-flag read/write lock)
#include "GameSource/Director/Camera/Camera.h"           // BrnDirector::Camera::Camera (embedded by value)

// RendererIO::InputBuffer / OutputBuffer -- the renderer module's per-frame IO payload buffers.
// Like every CgsModule IO buffer they derive CgsModule::IOBuffer (status-flag-guarded read/write
// locking; bit 4 = read lock, bit 3 = write lock) at offset +0. The InputBuffer carries the
// director's camera; the OutputBuffer carries the immediate-mode render buffers, effects frames,
// shader-constant frame and the per-frame render switches the renderer produces for the GPU push.
//
// LAYOUT PROVENANCE. Member ORDER + member TYPES are the DecFIGS DWARF for
// GameSource/Graphics/BrnRendererModuleIO.h (RenderSwitches:68, ExternallyVisiblePerformanceMonitors:108,
// InputBuffer:132, OutputBuffer:162). The InputBuffer camera offset (+0x10) and the OutputBuffer
// pointer/array members zeroed by Construct are recovered from the X360 ARTIST Construct bodies
// (InputBuffer::Construct @0x82400190, OutputBuffer::Construct @0x824001A8): InputBuffer sets the
// status byte then constructs the camera at this+0x10; OutputBuffer sets the status byte then nulls
// every owned pointer member + the two effects-frame pointer arrays.
//
// [x64: pointers are 8 bytes so byte offsets differ from the X360's 32-bit layout -- every member is
// accessed BY NAME (Construct nulls named members / runs the named-array loops), never by raw offset.
// The InputBuffer camera lands at this+0x10 on both targets because CgsModule::IOBuffer is a 1-byte
// base and Camera is alignas(16), forcing the camera to the first 16-byte boundary after the base.]
//
// NOTE: RendererIO::RenderSwitches is the DWARF home (line 68) for this struct; an identical
// forward-slice copy currently also lives in GameSource/Graphics/BrnRendererModule.h. The two are
// never co-included in one TU today; when BrnRendererModule.h is next touched it should include this
// header instead of carrying its own copy. (FLAG: duplicate forward-slice to consolidate, not retype.)

namespace CgsGraphics
{
    class DispatchFrame;
    // Im2dRenderBuffer cannot be forward-declared as a class: on the PC target it is a
    // TYPEDEF of Im2d (CgsImRenderBuffer.h:15 -- the buffer/renderer split folds onto the
    // one Im2d type), so the canonical header is included below instead. Surfaced when the
    // world-module mount first co-included this header after CgsImRenderBuffer.h in one TU.
    class Im3dRenderBuffer;
    // Im3dRenderBufferUntex is NOT forward-declared here, for exactly the reason given
    // above for Im2dRenderBuffer: on the PC target it is a TYPEDEF (CgsIm3d.h -- typedef
    // Im3dUntex Im3dRenderBufferUntex), so declaring it as a class is a redefinition with
    // a different basic type. The canonical header is included below instead. Surfaced
    // when ParticleModule.h began including the Lion blend renderer, which reaches
    // CgsIm3d.h through Im3dBlend's Im3dBase<V> base and put both in one TU.
}
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer (typedef of Im2d)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"  // CgsGraphics::Im3dRenderBufferUntex (typedef of Im3dUntex)
// ⭐ struct, not class, 2026-08-17 (found while landing BrnRendererModule::Update). Both are
// declared `struct` at their homes; forward-declaring them as `class` here made this header's
// setter signatures mangle with PEAV while any TU that included the real headers emitted PEAU,
// so the two never linked -- the accessors TU compiled fine and simply never got called. Same
// failure mode as the AllocatorList forward declaration in the director headers.
struct BrnEffectsFrame;
struct BrnShaderConstantsFrame;
// RECONCILED 2026-07-24 (ODR fix): BrnBlobbyShadowBuffer and BrnSubmissionInterface
// are NESTED classes of the real BrnBlobbyShadowManager / BrnCoronaManager CLASSES.
// A nested type cannot be forward-declared without (re)declaring its enclosing class,
// so the previous `namespace BrnBlobbyShadowManager {...}` + partial
// `class BrnCoronaManager { ... };` spellings collided with the real definitions
// (C2869 / C2011) in every TU that saw both -- which blocked
// WorldModule::GenerateFrustumQueries. Per AGENTS.md ("Reconstruct includes; don't
// fake them"), include the real homes instead of re-declaring the enclosing types.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"   // ::BrnBlobbyShadowBuffer
#include "GameSource/Graphics/BrnCoronaManager.h"         // ::BrnSubmissionInterface
namespace CgsMemory              { class LinearMalloc; }

namespace RendererIO
{
    // DWARF BrnRendererModuleIO.h:68 -- the per-frame render-enable switches.
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

    // DWARF BrnRendererModuleIO.h:108 -- CPU/GPU timing counters published for the externally
    // visible performance HUD.
    struct ExternallyVisiblePerformanceMonitors
    {
        s32 miCPU_DT_RenderShadowmapNear;
        s32 miCPU_DT_RenderShadowmapFar;
        s32 miCPU_DT_RenderEnvmap;
        s32 miCPU_DT_RenderPreZ;
        s32 miCPU_DT_RenderWorldOpaque;
        s32 miCPU_DT_RenderWorldTransparent;
        s32 miGPU_RenderShadowmaps;
        s32 miGPU_RenderEnvmap;
        s32 miGPU_RenderPreZ;
        s32 miGPU_RenderWorldOpaque;
        s32 miGPU_RenderWorldTransparent;
    };

    // DWARF BrnRendererModuleIO.h:132 -- the renderer's per-frame INPUT buffer: the director camera.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        void Construct();   // @ 0x82400190

        void SetBrnCamera(const BrnDirector::Camera::Camera& lrCamera);   // @ 0x823C8610 (write-lock)
        // ⭐ ADDED 2026-08-17 with BrnRendererModule::Update (boot audit F-P2-4). The console
        // has this getter -- Update @0x82405EA4/EC8/FA8 calls it three times -- and it was the
        // one piece of the input buffer's surface missing here; only the setter had a
        // declaration, so nothing could read the camera back out.
        const BrnDirector::Camera::Camera* GetBrnCamera() const;          // read-lock

    private:
        BrnDirector::Camera::Camera mBrnCamera;   // DWARF :150 (camera lands at this+0x10)
    };

    // DWARF BrnRendererModuleIO.h:162 -- the renderer's per-frame OUTPUT buffer. Construct nulls every
    // owned pointer + both effects-frame arrays; the camera/switches/perfmon aggregates are left at
    // their default-constructed state (the X360 Construct does not touch them).
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        void Construct();   // @ 0x824001A8

        // Per-frame data-interface accessors, reconstructed from BURNOUT_X360_ARTIST.XEX.
        // Bodies in BrnRendererModuleIO_OutputBuffer_Accessors.cpp. Getters read-lock (status bit 4),
        // setters write-lock (status bit 3); the embedded-aggregate handle getters return a mutable
        // address (non-const); GetExternallyVisiblePerformanceMonitors is a bare thunk (no lock guard).
        CgsGraphics::DispatchFrame*                     GetDispatchFrame() const;                  // @ 0x823B35A8
        CgsGraphics::Im2dRenderBuffer*                  GetIm2dRenderBuffer() const;               // @ 0x823B36F8
        CgsGraphics::Im3dRenderBuffer*                  GetIm3dRenderBuffer() const;               // @ 0x823B37A0
        CgsGraphics::Im3dRenderBufferUntex*             GetIm3dRenderBufferUntex() const;          // @ 0x823B3848
        CgsGraphics::Im3dRenderBuffer*                  GetIm3dDebugRenderBuffer() const;          // @ 0x823B3A40
        CgsGraphics::Im2dRenderBuffer*                  GetIm2dDebugRenderBuffer() const;          // @ 0x823B3AE8
        CgsGraphics::Im3dRenderBuffer*                  GetIm3dRenderBufferRacePosition() const;   // @ 0x823B38F0
        CgsGraphics::Im3dRenderBuffer*                  GetIm3dRenderBufferMenusAndHud() const;    // @ 0x823B3998
        BrnEffectsFrame*                                GetBaseEffectsFrame() const;               // @ 0x823B3B90
        BrnEffectsFrame*                                GetWorldEffectsFrame(u8 luSlot) const;    // @ 0x823B3C38
        BrnEffectsFrame*                                GetFXEventsEffectsFrame(u8 luSlot) const; // @ 0x823B3D10
        BrnShaderConstantsFrame*                        GetShaderConstantsFrame() const;           // @ 0x823B3DE8
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*  GetBlobbyShadowBuffer() const;             // @ 0x823B3E90
        BrnCoronaManager::BrnSubmissionInterface*       GetCoronaSubmissionInterface() const;      // @ 0x823B3F38
        CgsMemory::LinearMalloc*                        GetReusableLoadingScreenAllocator() const; // @ 0x823B4088
        BrnDirector::Camera::Camera*                    GetBrnCamera();                            // @ 0x823B3650 (read-lock)
        RenderSwitches*                                 GetRenderSwitches();                       // @ 0x823B3FE0 (read-lock)
        ExternallyVisiblePerformanceMonitors*           GetExternallyVisiblePerformanceMonitors(); // @ 0x823F6B38 (no lock guard)

        void SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame);                        // @ 0x823FAE60
        void SetIm2dRenderBuffer(CgsGraphics::Im2dRenderBuffer* lpBuffer);                         // @ 0x823FAF08
        void SetIm3dRenderBuffer(CgsGraphics::Im3dRenderBuffer* lpBuffer);                         // @ 0x823FAFB0
        void SetIm3dRenderBufferUntex(CgsGraphics::Im3dRenderBufferUntex* lpBuffer);               // @ 0x823FB058
        void SetIm3dDebugRenderBuffer(CgsGraphics::Im3dRenderBuffer* lpBuffer);                    // @ 0x823FB250
        void SetIm2dDebugRenderBuffer(CgsGraphics::Im2dRenderBuffer* lpBuffer);                    // @ 0x823FB2F8
        void SetIm3dRenderBufferRacePosition(CgsGraphics::Im3dRenderBuffer* lpBuffer);             // @ 0x823FB100
        void SetIm3dRenderBufferMenusAndHud(CgsGraphics::Im3dRenderBuffer* lpBuffer);              // @ 0x823FB1A8
        void SetBaseEffectsFrame(BrnEffectsFrame* lpFrame);                                        // @ 0x823FB3A0
        void SetWorldEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame);                           // @ 0x823FB448
        void SetFXEventsEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame);                        // @ 0x823FB528
        void SetShaderConstantsFrame(BrnShaderConstantsFrame* lpFrame);                            // @ 0x823FB608
        void SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBuffer);       // @ 0x823FB6B0
        void SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface* lpInterface);  // @ 0x823FB758
        void SetBrnCamera(const BrnDirector::Camera::Camera& lrCamera);                            // @ 0x82405980
        void SetRenderSwitches(const RenderSwitches& lrSwitches);                                  // @ 0x823FB800
        void SetReusableLoadingScreenAllocator(CgsMemory::LinearMalloc* lpAllocator);              // @ 0x823FB8C8

    private:
        CgsGraphics::DispatchFrame*           mpDispatchFrame;                 // DWARF :330
        CgsGraphics::Im2dRenderBuffer*        mpIm2dRenderBuffer;              // DWARF :332
        CgsGraphics::Im3dRenderBuffer*        mpIm3dRenderBuffer;              // DWARF :336
        CgsGraphics::Im3dRenderBufferUntex*   mpIm3dRenderBufferUntex;         // DWARF :337
        CgsGraphics::Im3dRenderBuffer*        mpIm3dDebugRenderBuffer;         // DWARF :339
        CgsGraphics::Im2dRenderBuffer*        mpIm2dDebugRenderBuffer;         // DWARF :340
        CgsGraphics::Im3dRenderBuffer*        mpIm3dRenderBufferRacePosition;  // DWARF :342
        CgsGraphics::Im3dRenderBuffer*        mpIm3dRenderBufferMenusAndHud;   // DWARF :343
        BrnEffectsFrame*                      mpBaseEffectsFrame;              // DWARF :345
        BrnEffectsFrame*                      mapWorldEffectsFrames[4];        // DWARF :346
        BrnEffectsFrame*                      mapFXEventsEffectsFrames[2];     // DWARF :347
        BrnShaderConstantsFrame*              mpShaderConstantsFrame;          // DWARF :349
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* mpBlobbyShadowBuffer;   // DWARF :351
        BrnCoronaManager::BrnSubmissionInterface*      mpCoronaSubmissionInteface; // DWARF :352
        BrnDirector::Camera::Camera           mBrnCamera;                      // DWARF :354
        RenderSwitches                        mRenderSwitches;                 // DWARF :355
        ExternallyVisiblePerformanceMonitors  mExternallyVisiblePerfmons;      // DWARF :356
        CgsMemory::LinearMalloc*              mpReusableLoadingScreenAllocator;// DWARF :358
        void*                                 mpSnapshotBuffer;                // DWARF :362
    };
}
