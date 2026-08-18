#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"      // Vector3, Matrix44
#include "rw/rwcore_structs.h"   // rw::Resource, rw::IResourceAllocator
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // ProgramBufferData, ProgramVariableHandle
#include "pc/gcm/renderengine/VertexDescriptor.h"                            // VertexDescriptorData

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSunCorona::Construct                 @ 0x824009B0  (EXECUTED in the boot trace)
//   BrnSunCorona::Destruct                  @ 0x823F8298
//   BrnSunCorona::ComputeSunPositionOnScreen@ 0x823FD798
//   BrnSunCorona::GenerateOcclusionBuffer   @ 0x82400CF8
//   BrnSunCorona::RenderOccludedFlare       @ 0x824010A8
//
// The sun-corona graphics object: it owns the vertex descriptor and the four shader programs
// (occlusion vertex/pixel + flare vertex/pixel) that draw the screen-space sun flare, plus the
// corona's tunable parameters (sizes, brightness, screen position) and visibility flags. Owned by
// BrnRendererModule (mSunCorona); built/torn by BrnRendererModule::Construct @0x8240A778 /
// ::Destruct @0x82408A58, driven every frame from BrnRendererModule::ComputeSunCoronaVisibility
// @0x82405D80 and BrnRendererModule::Render @0x8240D688.
//
// HOW THE THREE DRAW-SIDE FUNCTIONS FIT TOGETHER (all three decoded from the X360 asm):
//   1. ComputeSunPositionOnScreen  places the sun 10,000 units along -sunDir from the eye,
//      projects it, and stores the result as SCREEN coordinates in [0,1] (mfXPos/mfYPos) plus
//      mbVisible = "the sun is in front of the camera".
//   2. GenerateOcclusionBuffer     draws a full-screen NDC quad into the 1x1 sun-corona render
//      target; its pixel program takes a 7x7 grid of depth taps around (mfXPos, mfYPos) and
//      writes the fraction of them that reads the far plane.
//   3. RenderOccludedFlare         draws the flare quad (mfSunSize, aspect-corrected) with the
//      1x1 buffer bound as a texture; the pixel program puts the occlusion fraction in ALPHA and
//      the ADDITIVE-RGB blend state multiplies the flare by it.
//
// LAYOUT is the DecFIGS DWARF struct (GameSource/Graphics/BrnSunCorona.h), verified store-for-store
// against the X360 Construct: +0x00 allocator, +0x04 the 5-word resource block, +0x18/+0x1C/+0x20
// the descriptor and the occlusion pair, +0x24 the FIRST handle, +0x28/+0x2C the flare pair,
// +0x30 the SECOND handle, +0x34..+0x4C the seven floats, +0x50/+0x51 the two bools.
// Members are reached BY NAME.
//
// ⚠ THE TWO HANDLES ARE VALUES, NOT POINTERS. The X360 passes `this + 0x24` and `this + 0x30`
// as GetVariableHandleByName's OUT parameter (`addi r5, r31, 0x24` @0x82400BCC / `addi r5, r31,
// 0x30` @0x82400CE4), i.e. the 4-byte ProgramVariableHandle lives IN the object -- the two
// adjacent members are 4 bytes apart on the console image. The previous reconstruction declared
// them as pointers to an invented `ProgramVariableHandleObject`, which is both the wrong storage
// and a phantom type. Same correction, same reason, as the six phantom externals coronas step 1
// removed from rwgcoronarenderer.cpp.

class CgsRenderTarget;

struct BrnSunCorona
{
    // 0x824009B0 -- adopt the four shader programs, resolve the two named pixel constants and
    // build the two-element vertex descriptor, then seed the corona's tunable parameters.
    // DWARF (BrnSunCorona.h:43) declares it `void`; the X360 simply leaves
    // GetVariableHandleByName's r3 in place and no caller reads it.
    void Construct(rw::IResourceAllocator* lpAllocator);

    // 0x823F8298 -- release the vertex descriptor and free its resource block through the
    // allocator's vtable Free slot (+0x14).
    void Destruct();

    // 0x823FD798 (DWARF BrnSunCorona.h:54 / .cpp:191) -- project the sun onto the screen.
    // Vector3 by value in v1/v2 on the console: vector arguments do NOT consume a GPR slot on
    // this ABI (proved by RenderOccludedFlare's argument allocation below).
    void ComputeSunPositionOnScreen(const Matrix44& lrViewProjectionMatrix,
                                    Vector3 lViewPosition,
                                    Vector3 lSunDir);

    // 0x82400CF8 (DWARF BrnSunCorona.h:60 / .cpp:248) -- render the 1x1 occlusion measurement.
    // lpOcclusionRt is the SUN-CORONA pool target (slot 10); lpSourceDepthRt is the target whose
    // depth texture the taps read (the DOWN-SAMPLE buffer, pool slot 4).
    void GenerateOcclusionBuffer(CgsRenderTarget* lpOcclusionRt, CgsRenderTarget* lpSourceDepthRt);

    // 0x824010A8 (DWARF BrnSunCorona.h:69 / .cpp:367) -- draw the flare, modulated by the
    // occlusion buffer. The parameter ORDER is the DWARF's and the X360's: r4 = the render
    // target, v1 = the colour, f1/f2 = the two floats (each consuming a GPR slot, which is why
    // the bool lands in r7 and not r5), r7 = the debug-override flag.
    void RenderOccludedFlare(CgsRenderTarget* lpOcclusionRt,
                             Vector3 lSunColour,
                             f32 lfWhiteLevel,
                             f32 lfDebugOverrideBrightness,
                             bool lbDebugOverrideBrightness);

    // [FLAG PC bring-up] NOT an X360 function. The console builds this object inside
    // BrnRendererModule::Construct, which is not reconstructed, so the PC side comes up lazily on
    // the first frame that has a device (BrnRendererModule.cpp's EnsureSunCoronaBringUp) and the
    // gate has to be able to ask "did that succeed". Same shape and same DELETE-WHEN as
    // BrnCoronaManager::IsConstructed(): delete when BrnRendererModule::Construct is
    // reconstructed and can call Construct at the console's own point.
    bool IsConstructed() const
    {
        return mpVertexDescriptor != 0
            && mpOcclusionVertexProgram != 0 && mpOcclusionPixelProgram != 0
            && mpFlareVertexProgram != 0 && mpFlarePixelProgram != 0;
    }

    // [FLAG PC bring-up] Seed the console's OWN switch (mbRenderSunCorona, +0x51) from the
    // config.ini knob. NOT a second piece of state (AGENTS.md rule 3): the byte already exists and
    // Construct already sets it true; on the console its only other writer is
    // BrnGraphics::DebugComponent, which registers the debug-menu toggle and is an empty
    // placeholder in this tree. DELETE-WHEN DebugComponent is reconstructed.
    void PCBringUpSetRenderSunCorona(bool lbRender) { mbRenderSunCorona = lbRender; }

    // ---- layout (DWARF GameSource/Graphics/BrnSunCorona.h, verified vs the X360 store offsets) --
    rw::IResourceAllocator*             mpAllocator;                                 // +0x00
    rw::Resource                        mVertexDescriptorResource;                   // +0x04 (5 words)
    renderengine::VertexDescriptorData* mpVertexDescriptor;                          // +0x18
    renderengine::ProgramBufferData*    mpOcclusionVertexProgram;                    // +0x1C
    renderengine::ProgramBufferData*    mpOcclusionPixelProgram;                     // +0x20
    renderengine::ProgramVariableHandle mOcclusionPixelVariableHandleUvStartOffset;  // +0x24
    renderengine::ProgramBufferData*    mpFlareVertexProgram;                        // +0x28
    renderengine::ProgramBufferData*    mpFlarePixelProgram;                         // +0x2C
    renderengine::ProgramVariableHandle mFlarePixelVariableHandleColourAndPower;     // +0x30
    f32                                 mfSunVectorYMultiplier;                      // +0x34 (0.5)
    f32                                 mfOcclusionSize;                             // +0x38 (2.0)
    f32                                 mfSunFlarePow;                               // +0x3C (2.0)
    f32                                 mfSunBrightness;                             // +0x40 (0.30000001)
    f32                                 mfSunSize;                                   // +0x44 (0.30000001)
    f32                                 mfXPos;                                      // +0x48 (0.0)
    f32                                 mfYPos;                                      // +0x4C (0.0)
    bool                                mbVisible;                                   // +0x50 (false)
    bool                                mbRenderSunCorona;                           // +0x51 (true)
};
