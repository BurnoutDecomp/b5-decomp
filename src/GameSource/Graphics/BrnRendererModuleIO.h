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
    class Im2dRenderBuffer;
    class Im3dRenderBuffer;
    class Im3dRenderBufferUntex;
}
class BrnEffectsFrame;
class BrnShaderConstantsFrame;
namespace BrnBlobbyShadowManager { class BrnBlobbyShadowBuffer; }
namespace BrnCoronaManager       { class BrnSubmissionInterface; }
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

    private:
        BrnDirector::Camera::Camera mBrnCamera;   // DWARF :150 (camera lands at this+0x10)
    };

    // DWARF BrnRendererModuleIO.h:162 -- the renderer's per-frame OUTPUT buffer. Construct nulls every
    // owned pointer + both effects-frame arrays; the camera/switches/perfmon aggregates are left at
    // their default-constructed state (the X360 Construct does not touch them).
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        void Construct();   // @ 0x824001A8

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
