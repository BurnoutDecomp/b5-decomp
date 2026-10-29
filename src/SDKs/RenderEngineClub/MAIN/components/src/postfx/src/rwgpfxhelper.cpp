#include "types.hpp"

#include <cmath>    // cosf / sinf / fabsf
#include <cstring>  // memset
#include "types.hpp"  // intptr_t

#include <new>      // placement new (the render-target debugger carve)

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator / rw::ResourceDescriptor / rw::Resource
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxrendertargetdebugger.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbufferhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/meshhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // RenderTarget (Blur9 / DownSample)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"                // shadow::Device::SetState / SetPixelProgram
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::PfxHelper::CreateProgram                 @ 0x823FE480
//   rw::graphics::postfx::PfxHelper::CreateStates                  @ 0x82402D88
//   rw::graphics::postfx::PfxHelper::InitWeights_Blur16WithBilinear@ 0x823F8E78
//   rw::graphics::postfx::PfxHelper::InitWeights_Blur16            @ 0x823F8F50
//   rw::graphics::postfx::PfxHelper::InitWeights_DirBlur9Quadratic @ 0x823F8C70
//
// PfxHelper is the post-fx singleton: it owns the full-screen-quad geometry, the opaque-blend /
// depth-stencil render states, and the blur/copy shader programs every effect compiles against.
// CreateProgram is the shader-program factory the post-fx effects call directly; CreateStates builds
// the shared opaque blend state. Both follow the standard render-engine idiom: build a parameters
// block -> GetResourceDescriptor sizes it -> the allocator's DoAllocate carves the storage ->
// Initialize constructs the runtime object in place.
//
// The InitWeights_* generators are pure float math that fill a caller-owned vec4 weight table; every
// literal below is the immediate the asm loads. The flt_82001C98 / flt_82001CC0 constants are
// 1.0 / 0.0 (the same pair the sibling DepthOfField TU documents).
//
// ADDED THIS WAVE: the PfxHelper constructor (0x82408348), Release (0x82402E58),
// Parameters::Parameters, the SafeRelease<> teardown template (0x823FF480 / 0x823FF4F8) and the
// gpPfxHelper singleton definition.

namespace
{
    // ARTIST 0x82044240, SHA-256 deb110070ec6d4b2673fc070457e5a6bdfd6c41d544b47208de99b0badb05190.
    __declspec(align(4)) const u8 kQuadProgram[] = {
        0x10, 0x2A, 0x11, 0x01, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x53,
        0xFF, 0xFE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0x4C, 0x00, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x75, 0x76, 0x4F, 0x66, 0x66, 0x73, 0x65, 0x74,
        0x00, 0xAB, 0xAB, 0xAB, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x76, 0x73, 0x5F, 0x33, 0x5F, 0x30, 0x00, 0x32, 0x2E, 0x30, 0x2E, 0x36,
        0x35, 0x33, 0x34, 0x2E, 0x31, 0x00, 0xAB, 0xAB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
        0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x21,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x90,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x50, 0x05, 0x00, 0x00, 0x30, 0x50, 0x00, 0x00, 0x10, 0x06,
        0x10, 0x01, 0x10, 0x03, 0x00, 0x00, 0x12, 0x00, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04,
        0x00, 0x00, 0x12, 0x00, 0xC4, 0x00, 0x00, 0x00, 0x10, 0x09, 0x20, 0x05, 0x00, 0x00, 0x22, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x05, 0xF8, 0x10, 0x00, 0x00, 0x00, 0x0A, 0x88, 0x00, 0x00, 0x00, 0x00,
        0xC8, 0x0F, 0x80, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xE2, 0x01, 0x01, 0x00, 0x05, 0xF8, 0x00, 0x00,
        0x00, 0x00, 0x0F, 0xC8, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x03, 0x80, 0x00, 0x00, 0xB0, 0xB0, 0x00,
        0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    // ARTIST 0x820444F8, SHA-256 eedae0c33ba8deb32847b0c339f5395dcfc970c6735699867bdee883c5ffec69.
    __declspec(align(4)) const u8 kBlur9Program[] = {
        0x10, 0x2A, 0x11, 0x00, 0x00, 0x00, 0x01, 0x48, 0x00, 0x00, 0x01, 0x68, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x24, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0xEF,
        0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4,
        0x00, 0x02, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xC7, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD8,
        0x00, 0x00, 0x00, 0x00, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x4F, 0x66, 0x66, 0x73, 0x65, 0x74,
        0x73, 0x34, 0x5F, 0x31, 0x00, 0xAB, 0xAB, 0xAB, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x04,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x4F, 0x66,
        0x66, 0x73, 0x65, 0x74, 0x73, 0x34, 0x78, 0x34, 0x5F, 0x31, 0x00, 0xAB, 0x00, 0x02, 0x00, 0x03,
        0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x61, 0x6D, 0x70,
        0x6C, 0x65, 0x4F, 0x66, 0x66, 0x73, 0x65, 0x74, 0x73, 0x34, 0x78, 0x34, 0x5F, 0x32, 0x00, 0x73,
        0x63, 0x65, 0x6E, 0x65, 0x5F, 0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x00, 0xAB, 0xAB, 0xAB,
        0x00, 0x04, 0x00, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x73, 0x5F, 0x33, 0x5F, 0x30, 0x00, 0x32, 0x2E, 0x30, 0x2E, 0x36, 0x35, 0x33, 0x34, 0x2E,
        0x31, 0x00, 0xAB, 0xAB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x68, 0x10, 0x00, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x21, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x30, 0x50, 0x00, 0x00, 0x00, 0x00, 0x60, 0x03, 0xC4, 0x00,
        0x12, 0x00, 0x00, 0x00, 0x05, 0x50, 0x60, 0x09, 0x60, 0x0F, 0x12, 0x00, 0x12, 0x00, 0x09, 0x55,
        0x00, 0x00, 0x60, 0x15, 0x20, 0x1B, 0x12, 0x00, 0x22, 0x00, 0x00, 0x00, 0xC8, 0x0C, 0x00, 0x00,
        0x00, 0xAC, 0xAC, 0x00, 0xA0, 0x00, 0x08, 0x00, 0xC8, 0x03, 0x00, 0x08, 0x00, 0xB0, 0xB0, 0x00,
        0xA0, 0x00, 0x07, 0x00, 0xC8, 0x03, 0x00, 0x07, 0x00, 0xB0, 0xB0, 0x00, 0xA0, 0x00, 0x06, 0x00,
        0xC8, 0x03, 0x00, 0x06, 0x00, 0xB0, 0xB0, 0x00, 0xA0, 0x00, 0x05, 0x00, 0x14, 0x03, 0x00, 0x05,
        0x00, 0xB0, 0xB0, 0x6C, 0xA0, 0x00, 0x04, 0x00, 0x04, 0x13, 0x04, 0x03, 0x00, 0xB0, 0xB0, 0x6C,
        0x80, 0x00, 0x02, 0x03, 0x14, 0x03, 0x00, 0x02, 0x00, 0xB0, 0xB0, 0xB1, 0xA0, 0x00, 0x01, 0x00,
        0x04, 0x23, 0x04, 0x00, 0x00, 0xB0, 0xB0, 0xB1, 0x80, 0x00, 0x00, 0x03, 0x10, 0x08, 0x10, 0x01,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x20, 0x41, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x30, 0x61, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0x40, 0x81, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x50, 0xA1,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x60, 0xC1, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x70, 0xE1, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0x81, 0x01, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0xB8, 0x08, 0x00, 0x01,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0x00,
        0xA1, 0x00, 0x08, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xC0, 0xAB, 0x08, 0x07, 0x00,
        0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x07, 0x06, 0x00, 0xC8, 0x07, 0x00, 0x00,
        0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x06, 0x05, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4,
        0xAB, 0x05, 0x04, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x04, 0x03, 0x00,
        0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x03, 0x02, 0x00, 0xC8, 0x07, 0x00, 0x00,
        0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x02, 0x01, 0x00, 0xC8, 0x8F, 0xC0, 0x00, 0x00, 0xC0, 0xC6, 0xB4,
        0xAB, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    // ARTIST 0x820447A8, SHA-256 12749159acbe7fae46d1c437a6f7894a15ccadf3a0fa6580a2d735e38ca50baf.
    __declspec(align(4)) const u8 kBlur16Program[] = {
        0x10, 0x2A, 0x11, 0x00, 0x00, 0x00, 0x01, 0x50, 0x00, 0x00, 0x02, 0x64, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2C, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0xF7,
        0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x80, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x02, 0x00, 0x04,
        0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0,
        0x00, 0x02, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xC0, 0x00, 0x02, 0x00, 0x0C, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x4F, 0x66,
        0x66, 0x73, 0x65, 0x74, 0x73, 0x5F, 0x31, 0x00, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x04,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x4F, 0x66,
        0x66, 0x73, 0x65, 0x74, 0x73, 0x5F, 0x32, 0x00, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x4F, 0x66,
        0x66, 0x73, 0x65, 0x74, 0x73, 0x5F, 0x33, 0x00, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x4F, 0x66,
        0x66, 0x73, 0x65, 0x74, 0x73, 0x5F, 0x34, 0x00, 0x73, 0x63, 0x65, 0x6E, 0x65, 0x5F, 0x74, 0x65,
        0x78, 0x74, 0x75, 0x72, 0x65, 0x00, 0xAB, 0xAB, 0x00, 0x04, 0x00, 0x0C, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x73, 0x5F, 0x33, 0x5F, 0x30, 0x00, 0x32,
        0x2E, 0x30, 0x2E, 0x36, 0x35, 0x33, 0x34, 0x2E, 0x31, 0x00, 0xAB, 0xAB, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x64, 0x10, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x08, 0x21, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x30, 0x50,
        0x00, 0x00, 0x00, 0x00, 0x60, 0x05, 0xC4, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x0B,
        0x60, 0x11, 0x12, 0x00, 0x12, 0x00, 0x05, 0x54, 0x05, 0x55, 0x60, 0x17, 0x60, 0x1D, 0x12, 0x00,
        0x12, 0x00, 0x09, 0x55, 0x00, 0x00, 0x60, 0x23, 0x60, 0x29, 0x12, 0x00, 0x12, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x30, 0x2F, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x0C, 0x00, 0x00,
        0x00, 0xAC, 0xAC, 0x00, 0xA0, 0x00, 0x0F, 0x00, 0x14, 0x03, 0x00, 0x0F, 0x00, 0xB0, 0xB0, 0x6C,
        0xA0, 0x00, 0x0E, 0x00, 0x04, 0x13, 0x0E, 0x0D, 0x00, 0xB0, 0xB0, 0x6C, 0x80, 0x00, 0x0C, 0x0D,
        0x14, 0x03, 0x00, 0x0C, 0x00, 0xB0, 0xB0, 0xB1, 0xA0, 0x00, 0x0B, 0x00, 0x04, 0x23, 0x0E, 0x0B,
        0x00, 0xB0, 0xB0, 0xB1, 0x80, 0x00, 0x0A, 0x0D, 0x14, 0x03, 0x00, 0x0A, 0x00, 0xB0, 0xB0, 0x6C,
        0xA0, 0x00, 0x09, 0x00, 0x04, 0x13, 0x09, 0x08, 0x00, 0xB0, 0xB0, 0x6C, 0x80, 0x00, 0x07, 0x08,
        0x14, 0x03, 0x00, 0x07, 0x00, 0xB0, 0xB0, 0xB1, 0xA0, 0x00, 0x06, 0x00, 0x04, 0x23, 0x09, 0x06,
        0x00, 0xB0, 0xB0, 0xB1, 0x80, 0x00, 0x05, 0x08, 0x14, 0x03, 0x00, 0x05, 0x00, 0xB0, 0xB0, 0x6C,
        0xA0, 0x00, 0x04, 0x00, 0x04, 0x13, 0x04, 0x03, 0x00, 0xB0, 0xB0, 0x6C, 0x80, 0x00, 0x02, 0x03,
        0x14, 0x03, 0x00, 0x02, 0x00, 0xB0, 0xB0, 0xB1, 0xA0, 0x00, 0x01, 0x00, 0x04, 0x23, 0x04, 0x00,
        0x00, 0xB0, 0xB0, 0xB1, 0x80, 0x00, 0x00, 0x03, 0x10, 0x08, 0x10, 0x01, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x20, 0x41, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0x30, 0x61, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x40, 0x81,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x50, 0xA1, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x60, 0xC1, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0x70, 0xE1, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x81, 0x01,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x91, 0x21, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0xA1, 0x41, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0xB1, 0x61, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0xC1, 0x81,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0xD1, 0xA1, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0xE1, 0xC1, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0x10, 0x08, 0xF1, 0xE1, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0xB8, 0x08, 0x00, 0x01,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0x00,
        0xA1, 0x00, 0x0F, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x0F, 0x0E, 0x00,
        0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x0E, 0x0D, 0x00, 0xC8, 0x07, 0x00, 0x00,
        0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x0D, 0x0C, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4,
        0xAB, 0x0C, 0x0B, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x0B, 0x0A, 0x00,
        0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x0A, 0x09, 0x00, 0xC8, 0x07, 0x00, 0x00,
        0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x09, 0x08, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4,
        0xAB, 0x08, 0x07, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x07, 0x06, 0x00,
        0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x06, 0x05, 0x00, 0xC8, 0x07, 0x00, 0x00,
        0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x05, 0x04, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4,
        0xAB, 0x04, 0x03, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x03, 0x02, 0x00,
        0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x02, 0x01, 0x00, 0xC8, 0x8F, 0xC0, 0x00,
        0x00, 0xC0, 0xC6, 0xC0, 0xAB, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    // ARTIST 0x82044360, SHA-256 60fbdc1a2405cd75c48dfa4b15b9cee882fe22f43c3fad841283f16d1d1a2102.
    __declspec(align(4)) const u8 kBlur4Program[] = {
        0x10, 0x2A, 0x11, 0x00, 0x00, 0x00, 0x00, 0xE4, 0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x8B,
        0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x44, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x03, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x73, 0x61, 0x6D, 0x70,
        0x6C, 0x65, 0x4F, 0x66, 0x66, 0x73, 0x65, 0x74, 0x73, 0x00, 0xAB, 0xAB, 0x00, 0x02, 0x00, 0x03,
        0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x63, 0x65, 0x6E,
        0x65, 0x5F, 0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x00, 0xAB, 0xAB, 0x00, 0x04, 0x00, 0x0C,
        0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x73, 0x5F, 0x33,
        0x5F, 0x30, 0x00, 0x32, 0x2E, 0x30, 0x2E, 0x36, 0x35, 0x33, 0x34, 0x2E, 0x31, 0x00, 0xAB, 0xAB,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x21, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x30, 0x50, 0x00, 0x00, 0x00, 0x00, 0x60, 0x02, 0xC4, 0x00, 0x12, 0x00, 0x05, 0x00,
        0x00, 0x25, 0x60, 0x08, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x0C, 0x00, 0x00,
        0x00, 0xAC, 0xAC, 0x00, 0xA0, 0x00, 0x03, 0x00, 0xC8, 0x03, 0x00, 0x03, 0x00, 0xB0, 0xB0, 0x00,
        0xA0, 0x00, 0x02, 0x00, 0xC8, 0x03, 0x00, 0x02, 0x00, 0xB0, 0xB0, 0x00, 0xA0, 0x00, 0x01, 0x00,
        0xC8, 0x03, 0x00, 0x00, 0x00, 0xB0, 0xB0, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x10, 0x08, 0x10, 0x01,
        0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x20, 0x41, 0x1F, 0x1F, 0xFE, 0x88,
        0x00, 0x00, 0x40, 0x00, 0x10, 0x08, 0x30, 0x61, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00,
        0xB8, 0x08, 0x00, 0x01, 0x1F, 0x1F, 0xFE, 0x88, 0x00, 0x00, 0x40, 0x00, 0xC8, 0x07, 0x00, 0x00,
        0x00, 0xC0, 0xC6, 0x00, 0xA1, 0x00, 0x03, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xC0, 0xC6, 0xC0,
        0xAB, 0x03, 0x02, 0x00, 0xC8, 0x07, 0x00, 0x00, 0x00, 0xB4, 0xC6, 0xB4, 0xAB, 0x02, 0x01, 0x00,
        0xC8, 0x8F, 0xC0, 0x00, 0x00, 0xC0, 0xC6, 0xB4, 0xAB, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    // The renderengine allocator keeps its dispatch pointer as the first word; the helper stores that
    // word (m_allocator) and routes DoAllocate through it.
    rw::Resource AllocateResource(rw::IResourceAllocator* lpAllocator,
                                  const rw::BaseResourceDescriptors<5>& lrDescriptor)
    {
        return lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lrDescriptor), 0);
    }

    // ARTIST 0x82C4F910 initializes these positions; 0x82408494 uploads the first three.
    const rw::graphics::postfx::PfxHelper::Vertex kQuadVertices[3] =
    {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } }
    };

}

CGS_STATIC_ASSERT((sizeof(rw::graphics::postfx::PfxHelper::Vertex) == 20),
              "the post-fx quad vertex must be the console's 0x14 FLOAT3+FLOAT2 stride");

namespace rw
{
namespace graphics
{
namespace postfx
{
    // ARTIST 0x823FE480.
    renderengine::ProgramBufferData* PfxHelper::CreateProgram(s32 type, const void* microcode,
                                                               u32 size, rw::IResourceAllocator* allocator)
    {
        if (!allocator) allocator = gpPfxHelper->GetAllocator();
        renderengine::ProgramBufferParameters params = {};
        params.muFunction = reinterpret_cast<u32>(microcode);
        params.muShaderType = type;
        params.muReserved8 = size;
        rw::BaseResourceDescriptors<5> descriptor;
        renderengine::ProgramBuffer::GetResourceDescriptor(&descriptor, &params);
        rw::Resource resource = AllocateResource(allocator, descriptor);
        return renderengine::ProgramBuffer::Initialize(
            reinterpret_cast<renderengine::ProgramResourceLayout*>(&resource), &params);
    }

    // X360 0x82402D88. DWARF rwgpfxhelper.h:112 -- void; the console leaves the new state in r3 but
    // its only caller (the constructor) discards it, and the store into m_opaqueBlendState below is
    // what publishes it.
    void PfxHelper::CreateStates()
    {
        // Opaque-blend material parameters. The four leading words are the per-channel blend factors
        // (0x07060706 each, from the asm's lis 0x706 / ori 0x706 build), then the channel-write masks
        // (0xF x4), the blend op (7), the flag word (0x87 == 135), and a -1 word; the boolean tail and
        // the remaining state words are zero.
        renderengine::BlendStateParameters lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.maBlendFactor[0] = 0x07060706u;  // 117835526
        lParams.maBlendFactor[1] = 0x07060706u;
        lParams.maBlendFactor[2] = 0x07060706u;
        lParams.maBlendFactor[3] = 0x07060706u;
        // Set by which params WORD the asm writes (0x82402DC0..0x82402DE8), NOT by the
        // Hex-Rays v5[] index names: blendstate.h orders word4=muState15 .. word11=muState9.
        lParams.muState15        = 7;            // word4 = v5[4]
        lParams.muState4         = 15;           // word5 = v5[5]
        lParams.muState5         = 15;           // word6 = v5[6]
        lParams.muState6         = 15;           // word7 = v5[7]
        lParams.muState7         = 15;           // word8 = v5[8]
        lParams.muState8         = 135;          // word9 = v5[9] = 0x87
        lParams.muState17        = 0;            // word10 = v5[10]
        lParams.muState9         = static_cast<u32>(-1); // word11 = v5[11]

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::BlendState::GetResourceDescriptor(&lDescriptor);

        rw::Resource lResource = AllocateResource(m_allocator, lDescriptor);
        m_opaqueBlendState = static_cast<renderengine::BlendMaterialState*>(
            renderengine::BlendState::Initialize(
                reinterpret_cast<renderengine::BlendMaterialState**>(&lResource), &lParams));
    }

    // X360 0x823F8E78. 16-tap (4x4) box blur with bilinear sampling: for each of the 4x4 sample
    // positions emit (offsetX, offsetY, 0.25, 1.0). offsetX/offsetY step by 2 (bilinear pairs).
    void PfxHelper::InitWeights_Blur16WithBilinear(f32* lpBlurWeights, s32 liWidth, s32 liHeight)
    {
        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        f32* lpOut = lpBlurWeights;
        for (s32 liY = 0; liY < 4; liY += 2)
        {
            const f32 lfOffsetY = static_cast<f32>(liY - 1) * lfInvHeight;
            for (s32 liX = 0; liX < 4; liX += 2)
            {
                const f32 lfOffsetX = static_cast<f32>(liX - 1) * lfInvWidth;
                lpOut[0] = lfOffsetX;
                lpOut[1] = lfOffsetY;
                lpOut[2] = 0.25f;
                lpOut[3] = 1.0f;
                lpOut += 4;
            }
        }
    }

    // X360 0x823F8F50. 16-tap (4x4) box blur. Each of the four rows emits four samples at x-offsets
    // {-1.5, -0.5, 0.5, 1.5}/width and a constant y-offset of (row+0.5)/height, with a flat 0.0625
    // weight in lane 2. A second pass normalises those 16 weights so they sum to 1.
    void PfxHelper::InitWeights_Blur16(f32* lpBlurWeights, s32 liWidth, s32 liHeight)
    {
        static const f32 KAF_OFFSETS_X[4] = { -1.5f, -0.5f, 0.5f, 1.5f };

        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        f32 lfTotalWeight = 0.0f;
        f32* lpOut = lpBlurWeights;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            const f32 lfOffsetY = (static_cast<f32>(liRow) + 0.5f) * lfInvHeight;
            for (s32 liCol = 0; liCol < 4; ++liCol)
            {
                lpOut[0] = lfInvWidth * KAF_OFFSETS_X[liCol];
                lpOut[1] = lfOffsetY;
                lpOut[2] = 0.0625f;
                lfTotalWeight += 0.0625f;
                lpOut += 4;
            }
        }

        // Normalise the weight lane (lane 2 of each vec4) by the accumulated total.
        const f32 lfInvTotal = 1.0f / lfTotalWeight;
        for (s32 liSample = 0; liSample < 16; ++liSample)
        {
            lpBlurWeights[liSample * 4 + 2] *= lfInvTotal;
        }
    }

    // X360 0x823F8C70. 9-tap directional blur with a quadratic falloff. The 9 taps step along
    // (cos angle, sin angle); each tap is centred about index 4 and quantised in 2/9 (0.22222222)
    // steps. The weight is (1 - d^2)/radius + 0.05 (d == normalised step). The weight lane is then
    // normalised by the accumulated total and scaled by weightFactor.
    void PfxHelper::InitWeights_DirBlur9Quadratic(f32* lpBlurWeights, s32 liWidth, s32 liHeight,
                                                  f32 lfAngle, f32 lfRadius, f32 lfWeightFactor)
    {
        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        // XMVectorCos / XMVectorSin splat the scalar cos/sin of the angle.
        f32 lfCos = cosf(lfAngle);
        f32 lfSin = sinf(lfAngle);
        // Snap near-axis directions to the axis (asm: |cos| < 0.001 -> 0, |sin| > 0.999 -> 1).
        if (fabsf(lfCos) < 0.001f)
            lfCos = 0.0f;
        if (fabsf(lfSin) > 0.99900001f)
            lfSin = 1.0f;

        const f32 lfInvRadius = 1.0f / lfRadius;

        f32 lfTotalWeight = 0.0f;
        f32* lpOut = lpBlurWeights;
        for (s32 liSample = 0; liSample < 9; ++liSample)
        {
            const f32 lfStep = (static_cast<f32>(liSample) - 4.0f) * lfRadius * 0.22222222f;
            const f32 lfNorm = lfStep * lfInvRadius;
            const f32 lfWeight = -((lfNorm * lfNorm) - 1.0f);

            // asm 0x823F8D90-DA8: lane0 = step*cos*(1/width), lane1 = step*sin*(1/height).
            lpOut[0] = (lfStep * lfCos) * lfInvWidth;
            lpOut[1] = (lfStep * lfSin) * lfInvHeight;
            lpOut[2] = lfWeight * lfInvRadius + 0.050000001f;
            lfTotalWeight += lpOut[2];
            lpOut += 4;
        }

        // Normalise the weight lane and fold in weightFactor.
        const f32 lfInvTotal = 1.0f / lfTotalWeight;
        for (s32 liSample = 0; liSample < 9; ++liSample)
        {
            lpBlurWeights[liSample * 4 + 2] = lpBlurWeights[liSample * 4 + 2] * lfInvTotal * lfWeightFactor;
        }
    }
}
}
}

// ================================================================================================
// rw::graphics::postfx::PfxHelper -- constructor @0x82408348, Release @0x82402E58,
// Parameters::Parameters (inlined), the gpPfxHelper singleton (X360 off_82FAEE80) and the
// SafeRelease<> teardown template (X360 0x823FF480 / 0x823FF4F8).
//
// Goes into b5-decomp/src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.cpp.
// ================================================================================================

// ---- THE SINGLETON ------------------------------------------------------------------------------
// X360 off_82FAEE80. Written by the constructor (`stw r3, off_82FAEE80@l(r11)` @0x8240835C-ish, the
// second store the ctor makes) and cleared by Release (`stw r31, off_82FAEE80@l(r11)` @0x824030A4).
// The DWARF spells it `static PfxHelper* PfxHelper::m_instance` (rwgpfxhelper.h:150); this tree
// already committed the file-scope declaration in rwgpfxhelper.h and BrnPostFx::Destruct reads it by
// that name, so this TU is its DEFINITION and there is exactly one home.
//
// ⚠ SPLIT BRAIN REMOVED. The committed rwgpfxhelper.cpp carried a SECOND, TU-local copy of the same
// pointer (`namespace { PfxHelper* gpPfxHelperSingleton = nullptr; }`) that CreateProgram read while
// nothing ever wrote it -- so on this build CreateProgram's no-override path dereferenced a null
// pointer while the real singleton sat in the header's extern. One pointer now.
namespace rw
{
namespace graphics
{
namespace postfx
{
    PfxHelper* gpPfxHelper = 0;
}
}
}

namespace
{
    // X360 dword_82FAEE84 -- the post-fx render-target debugger singleton the constructor builds
    // once. TU-local: the only two instructions in the whole export set that touch 82FAEE84 are the
    // load and the store inside PfxHelper::PfxHelper (grep pasted in the report), so no other TU can
    // name it and it stays file scope.
    rw::graphics::postfx::RenderTargetDebugger* gpRenderTargetDebugger = 0;

    // ---- the per-T half of SafeRelease -----------------------------------------------------------
    // On the console SafeRelease<T> calls `T::Release(ptr)` before handing the block back. This tree
    // splits the renderengine vocabulary differently from the vendor original -- the OBJECT is
    // `VertexDescriptorData` / `VertexBufferHeader` / `ProgramBufferData` / `MeshHelper::MeshData`
    // while the static `Release` lives on the API class (`VertexDescriptor`, `VertexBuffer`,
    // `ProgramBuffer`) -- so the per-T step is spelled as this overload set instead of `T::Release`.
    // The two out-of-line X360 instantiations prove the two EMPTY ones:
    //   SafeRelease<BlendState>        @0x823FF480 -- DoFree + null only, no Release call
    //   SafeRelease<DepthStencilState> @0x823FF4F8 -- DoFree + null only, no Release call
    void ReleaseObject(renderengine::VertexDescriptorData* lpObject)
    {
        renderengine::VertexDescriptor::Release(lpObject);
    }
    void ReleaseObject(renderengine::VertexBuffer* lpObject)
    {
        lpObject->Release();
    }
    void ReleaseObject(renderengine::ProgramBufferData* lpObject)
    {
        renderengine::ProgramBuffer::Release(lpObject);
    }
    void ReleaseObject(renderengine::MeshHelper* object)
    {
        object->Release();
    }
    void ReleaseObject(renderengine::DepthStencilState*)
    {
        // Empty on the console: SafeRelease<DepthStencilState> @0x823FF4F8 emits no Release call.
    }
    void ReleaseObject(renderengine::BlendMaterialState*)
    {
        // Empty on the console: SafeRelease<BlendState> @0x823FF480 emits no Release call.
    }

    void BindProgramVariable(const renderengine::ProgramBufferData* program, const char* name,
                             renderengine::ProgramVariableHandle& handle)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(program, reinterpret_cast<const u8*>(name), &handle);
    }

}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 rwgpfxhelper.h:176, instantiations @0x823FF480 / @0x823FF4F8 and seven inlined copies
    // inside Release. The rw::Resource handed to DoFree is REBUILT FROM THE OBJECT POINTER: the
    // console zeroes a five-word block and stores the pointer in word 0
    // (`stw r31, 0..0x10(r11)` then `stw r10, 0(r11)`), which is the same allocator convention
    // rwcore_structs.h pins (DoFree is the vtbl+0x14 slot). The host retains the
    // same five-lane Paradise Resource shape.
    template< class T >
    void PfxHelper::SafeRelease(T*& lprObject)
    {
        if (lprObject != 0)
        {
            ReleaseObject(lprObject);

            rw::Resource lResource;
            for (u32 luSlot = 0u; luSlot < rw::KU_RESOURCE_LANE_COUNT; ++luSlot)
            {
                lResource.m_baseResources[luSlot] = 0;
            }
            lResource.m_baseResources[0] = lprObject;
            m_allocator->DoFree(lResource);

            lprObject = 0;
        }
    }

    // X360 rwgpfxhelper.h:43. INLINED at BrnPostFx::Construct -- the caller writes the allocator
    // word itself (`stw r28, 0x300+var_278(r1)` @0x82409FC4) and the block has no other member, so
    // the constructor emits nothing. Reproduced as the empty body it is; `allocator` is left
    // uninitialised exactly as the console leaves it, and every call site assigns it immediately.
    PfxHelper::Parameters::Parameters()
    {
    }

    // ============================================================================================
    // PfxHelper::PfxHelper @0x82408348
    //
    // The post-fx singleton's one-time build. In console order:
    //   1. m_allocator = parameters.allocator          (`*&this->char0 = *parameters`)
    //   2. gpPfxHelper = this                          (`off_82FAEE80 = this`)   <- publishes itself
    //   3. the full-screen-quad vertex descriptor + vertex buffer + its four vertices + the mesh
    //   4. the quad depth/stencil state
    //   5. four shader programs + their named-constant handles
    //   6. CreateStates()                              (the shared opaque blend state, +0x4C)
    //   7. the render-target debugger singleton, built once
    //
    // ============================================================================================
    PfxHelper::PfxHelper(const Parameters& lrParameters)
    {
        // asm: `*&this->char0 = *parameters` then `off_82FAEE80 = this`. The publish happens BEFORE
        // anything is built, which is why CreateProgram (step 5) can reach the singleton for its
        // default allocator while the object is still half-constructed. Faithful, order included.
        m_allocator  = lrParameters.allocator;
        gpPfxHelper  = this;

        {
            renderengine::VertexDescriptor::Parameters params;
            params.maElements[0].mu16Stream = 0;
            params.maElements[0].miOffset = 0x002A23B9;
            params.maElements[0].mu8UsageIndex = 1;
            params.maElements[1].mu16Stream = 0;
            params.maElements[1].miOffset = 0x002C23A5;
            params.maElements[1].mu8UsageIndex = 6;
            rw::BaseResourceDescriptors<5> descriptor;
            renderengine::VertexDescriptor::GetResourceDescriptor(&descriptor, &params);
            rw::Resource resource = AllocateResource(m_allocator, descriptor);
            m_quadVertexDescriptor = renderengine::VertexDescriptor::Initialize(&resource, &params);
        }
        {
            renderengine::VertexBufferHelper::Parameters params;
            params.type = 0;
            params.bufferSize = 0;
            params.numVertices = 3;
            for (u32 i = 0; i < 16; ++i) params.elementFormats[i] = renderengine::VERTEXFORMAT_NA;
            params.elementFormats[0] = renderengine::VERTEXFORMAT_FLOAT3;
            params.elementFormats[1] = renderengine::VERTEXFORMAT_FLOAT2;
            params.numElements = 0;
            params.stride = 0;
            rw::ResourceDescriptor descriptor = renderengine::VertexBufferHelper::GetResourceDescriptor(params);
            rw::Resource resource = m_allocator->DoAllocate(descriptor, 0);
            renderengine::VertexBufferHelper* buffer = renderengine::VertexBufferHelper::Initialize(resource, params);
            m_quadVertexBuffer = buffer;
            renderengine::VertexBuffer::BatchIterator iterator;
            if (buffer->Lock(iterator, params, 2))
            {
                Vertex* dst = static_cast<Vertex*>(iterator.GetLockedBuffer());
                for (u32 i = 0; i < 3; ++i) dst[i] = kQuadVertices[i];
                buffer->Unlock(iterator, params);
            }
            renderengine::MeshHelper::Parameters meshParams;
            meshParams.SetVertexBuffer(buffer);
            rw::ResourceDescriptor meshDescriptor = renderengine::MeshHelper::GetResourceDescriptor(meshParams);
            rw::Resource meshResource = m_allocator->DoAllocate(meshDescriptor, 0);
            m_quadMeshState = renderengine::MeshHelper::Initialize(meshResource, meshParams);
        }

        // ---- 4. the quad depth/stencil state (asm 0x8240857C-0x82408600) --------------------------
        // Every word is an asm immediate: function 3, both stencil functions 7 (ALWAYS), all four
        // stencil masks -1, everything else 0, and all six trailing flag bytes 0 -- i.e. depth test
        // OFF, depth write OFF, stencil OFF, which is what a full-screen post-fx pass wants.
        {
            renderengine::DepthStencilState::Parameters lParameters = {};
            lParameters.muFunction        = 3u;              // v40[0]
            lParameters.maState1[0]       = 0u;              // v40[1..3]
            lParameters.maState1[1]       = 0u;
            lParameters.maState1[2]       = 0u;
            lParameters.muState4          = 7u;              // v40[4]  == E_FUNCTION_ALWAYS
            lParameters.maState5[0]       = 0u;              // v40[5..7]  (the memset)
            lParameters.maState5[1]       = 0u;
            lParameters.maState5[2]       = 0u;
            lParameters.muState8          = 7u;              // v40[8]  == E_FUNCTION_ALWAYS
            lParameters.muState9          = 0u;              // v40[9]
            lParameters.muState10         = 0u;              // v40[10]
            lParameters.muStencilReadMask  = static_cast<u32>(-1);  // v40[11]
            lParameters.muStencilWriteMask = static_cast<u32>(-1);  // v40[12]
            lParameters.muState13         = 0u;              // v40[13]
            lParameters.muState14         = static_cast<u32>(-1);   // v40[14]
            lParameters.muState15         = static_cast<u32>(-1);   // v40[15]
            lParameters.muState16         = 0u;              // v40[16]
            lParameters.mbDepthTestEnable  = 0u;             // v41
            lParameters.mbDepthWriteEnable = 0u;             // v42
            lParameters.mu8Flag2 = 0u;                       // v43
            lParameters.mu8Flag3 = 0u;                       // v44
            lParameters.mu8Flag4 = 0u;                       // v45
            lParameters.mu8Flag5 = 0u;                       // v46

            renderengine::ResourceDescriptor5 lDescriptor;
            renderengine::DepthStencilState::GetResourceDescriptor(&lDescriptor, &lParameters);

            rw::Resource lResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), 0);
            m_quadDepthStencilState = renderengine::DepthStencilState::Initialize(
                reinterpret_cast<renderengine::DepthStencilState**>(&lResource), &lParameters);
        }

        m_quadVertexProgram = CreateProgram(0, kQuadProgram, sizeof(kQuadProgram), m_allocator);
        BindProgramVariable(m_quadVertexProgram, "uvOffset", m_uvOffsetHandle);

        m_9tapPixelProgram = CreateProgram(1, kBlur9Program, sizeof(kBlur9Program), m_allocator);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4x4_1", m_blur9SamplesHandle[0]);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4x4_2", m_blur9SamplesHandle[1]);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4_1",   m_blur9SamplesHandle[2]);

        m_16tapPixelProgram = CreateProgram(1, kBlur16Program, sizeof(kBlur16Program), m_allocator);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_1", m_blur16SamplesHandle[0]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_2", m_blur16SamplesHandle[1]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_3", m_blur16SamplesHandle[2]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_4", m_blur16SamplesHandle[3]);

        m_4tapPixelProgram = CreateProgram(1, kBlur4Program, sizeof(kBlur4Program), m_allocator);
        BindProgramVariable(m_4tapPixelProgram, "sampleOffsets", m_blur4SamplesHandle);

        // ---- 6. the shared opaque blend state (asm 0x824086F4) -------------------------------------
        CreateStates();

        // ---- 7. the render-target debugger singleton (asm 0x824086F8-0x824087B0) -------------------
        // Built ONCE, guarded by its own file-scope pointer. The carve is the console's:
        // entry 0 = {0xC, 0x10} (`v37[0] = 0xC00000010LL` -- size 12, alignment 16), the remaining
        // entries the {0, 1} identity. 12 is the GUEST size of the three-pointer debugger, so the
        // host sizeof is used and the console immediate documented -- the same rule (and the same
        // failure it prevents) as BrnPostFx::Construct's five carves.
        if (gpRenderTargetDebugger == 0)
        {
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size =
                static_cast<u32>(sizeof(rw::graphics::postfx::RenderTargetDebugger));   // X360: 0xC
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;
            for (u32 luEntry = 1u; luEntry < rw::KU_RESOURCE_LANE_COUNT; ++luEntry)
            {
                lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
                lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
            }

            const rw::Resource lResource = m_allocator->DoAllocate(lDescriptor, 0);
            gpRenderTargetDebugger = (lResource.m_baseResources[0] != 0)
                ? new (lResource.m_baseResources[0]) rw::graphics::postfx::RenderTargetDebugger()
                : 0;
        }
    }

    // ============================================================================================
    // PfxHelper::Release @0x82402E58
    //
    // Nine SafeRelease<> calls in the console's own order -- which is NOT declaration order, and is
    // reproduced as found (blend state, vertex descriptor, vertex buffer, mesh, depth/stencil, then
    // the four programs quad/4tap/16tap/9tap) -- followed by clearing the allocator and the
    // singleton. m_quadDrawParams (+0x10) is deliberately NOT released: the console never touches it.
    // ============================================================================================
    void PfxHelper::Release()
    {
        SafeRelease(m_opaqueBlendState);       // 0x82402E70  +0x4C  (out-of-line SafeRelease<BlendState>)
        SafeRelease(m_quadVertexDescriptor);   // 0x82402E78  +0x08
        SafeRelease(m_quadVertexBuffer);       // 0x82402EC8  +0x0C
        SafeRelease(m_quadMeshState);          // 0x82402F14  +0x04
        SafeRelease(m_quadDepthStencilState);  // 0x82402F60  +0x14  (out-of-line SafeRelease<DepthStencilState>)
        SafeRelease(m_quadVertexProgram);      // 0x82402F6C  +0x18
        SafeRelease(m_4tapPixelProgram);       // 0x82402FB8  +0x44
        SafeRelease(m_16tapPixelProgram);      // 0x82403004  +0x30
        SafeRelease(m_9tapPixelProgram);       // 0x82403050  +0x20

        // asm 0x82403098-0x824030A4: `stw r31, 0(r30)` then `stw r31, off_82FAEE80@l(r11)`.
        m_allocator = 0;
        gpPfxHelper = 0;
    }

    // ARTIST Blur9 0x824030C0 and DownSample 0x824032C8 use these shared tables.
    namespace
    {
        // The console's file-scope weight tables (float4 rows): unk_82FAED00 (9), unk_82FAEC00
        // (16), unk_82FAED90 (4). Sized as the generators fill them.
        f32 safBlur9Weights [ 9 * 4];
        f32 safBlur16Weights[16 * 4];
        f32 safBlur4Weights [ 4 * 4];

        // One BeginShaderStates run for lrHandle, then luRows float4 rows out of lpafTable in
        // order -- the console's `do { *cursor++ = *table++ x4 } while (--n)` loop.
        void PushWeightRows(renderengine::ProgramVariableHandle& lrHandle,
                            const f32* lpafTable, u32 luRows)
        {
            renderengine::FloatShaderStateIterator iterator;
            renderengine::Device::BeginShaderStates(lrHandle, iterator);
            f32* lpfRow = reinterpret_cast<f32*>(iterator.m_dest);
            for (u32 luRow = 0; luRow < luRows; ++luRow)
            {
                lpfRow[0] = lpafTable[0];
                lpfRow[1] = lpafTable[1];
                lpfRow[2] = lpafTable[2];
                lpfRow[3] = lpafTable[3];
                lpfRow    += 4;
                lpafTable += 4;
            }
        }

        const u32 KU_HELPER_SAMPLER_SOURCE = 0u;   // `li r4, 0` at every sub_8227D158 call
    }

    // X360 0x824030C0.
    void PfxHelper::Blur9(RenderTarget* lpDestRenderTarget, RenderTarget* lpSourceRenderTarget,
                          f32 lfAngle, f32 lfRadius, f32 lfWeightFactor)
    {
        lpDestRenderTarget->Begin(0u);                                                // bl @0x824030F8 (li r4,0)

        // The 9-tap directional table from the SOURCE's extent (`lwz r4, 4(r5)` / `lwz r5, 8(r5)`).
        InitWeights_DirBlur9Quadratic(safBlur9Weights,
                                      static_cast<s32>(lpSourceRenderTarget->muWidth),
                                      static_cast<s32>(lpSourceRenderTarget->muHeight),
                                      lfAngle, lfRadius, lfWeightFactor);            // bl @0x8240311C

        // Three runs through the 9-tap program's three handles: 4 + 4 + 1 rows (`addi r3, r30, 0x24 /
        PushWeightRows(m_blur9SamplesHandle[0], &safBlur9Weights[0 * 4], 4u);
        PushWeightRows(m_blur9SamplesHandle[1], &safBlur9Weights[4 * 4], 4u);
        PushWeightRows(m_blur9SamplesHandle[2], &safBlur9Weights[8 * 4], 1u);

        shadow::Device::SetState(lpSourceRenderTarget->maColourTargets[0].mpTextureState,
                                 KU_HELPER_SAMPLER_SOURCE);                            // lwz r3,0x2C(r28) / bl @0x824031F8
        shadow::Device::SetPixelProgram(m_9tapPixelProgram);                           // bl @0x82403200

        // The DESTINATION's half texel (`lwz 4(r4)`, `lwz 8(r4)` -> 1/w, 1/h -> * 0.5 splat), the
        // uvOffset the quad's vertex program applies (0x82403208-0x82403298).
        __declspec(align(16)) f32 lafUvOffset[4];
        lafUvOffset[0] = 0.5f * (1.0f / static_cast<f32>(lpDestRenderTarget->muWidth));
        lafUvOffset[1] = 0.5f * (1.0f / static_cast<f32>(lpDestRenderTarget->muHeight));
        lafUvOffset[2] = 0.0f;
        lafUvOffset[3] = 0.0f;
        RenderQuad(lafUvOffset);                                                       // bl @0x8240329C

        lpDestRenderTarget->Resolve(true, true);                                       // bl @0x824032AC
    }

    // X360 0x824032C8.
    void PfxHelper::DownSample(RenderTarget* lpDestRenderTarget, RenderTarget* lpSourceRenderTarget,
                               Method leMethod)
    {
        lpDestRenderTarget->Begin(0u);                                                // bl @0x824032EC (li r4,0)

        if (leMethod == METHOD_16TAP)                                                  // cmpwi cr6, r31, 0 @0x824032F0
        {
            InitWeights_Blur16(safBlur16Weights,
                               static_cast<s32>(lpSourceRenderTarget->muWidth),
                               static_cast<s32>(lpSourceRenderTarget->muHeight));     // bl @0x8240330C
            // ARTIST writes all sixteen rows through one iterator from handle +0x34.
            PushWeightRows(m_blur16SamplesHandle[0], safBlur16Weights, 16u);
            shadow::Device::SetState(lpSourceRenderTarget->maColourTargets[0].mpTextureState,
                                     KU_HELPER_SAMPLER_SOURCE);                        // lwz r3,0x2C(r30) / bl @0x82403364
            shadow::Device::SetPixelProgram(m_16tapPixelProgram);                      // +0x30, bl @0x8240336C
            RenderQuad(0);                                                       // li r4, 0
        }
        else if (leMethod == METHOD_16TAP_WITH_BILINEAR)                               // cmpwi cr6, r31, 1 @0x82403374
        {
            InitWeights_Blur16WithBilinear(safBlur4Weights,
                                           static_cast<s32>(lpSourceRenderTarget->muWidth),
                                           static_cast<s32>(lpSourceRenderTarget->muHeight));
            // The console binds the program BEFORE the constant run on this arm (SetPixelProgram
            // @0x82403398, then BeginShaderStates @0x824033A4) -- order preserved.
            shadow::Device::SetPixelProgram(m_4tapPixelProgram);                       // +0x44
            PushWeightRows(m_blur4SamplesHandle, safBlur4Weights, 4u);                 // +0x48
            shadow::Device::SetState(lpSourceRenderTarget->maColourTargets[0].mpTextureState,
                                     KU_HELPER_SAMPLER_SOURCE);
            RenderQuad(0);
        }
        // Any other method: no table, no draw -- the console falls straight to the Resolve.

        lpDestRenderTarget->Resolve(true, true);                                       // bl @0x82403410
    }

    // ARTIST 0x823FE530.
    void PfxHelper::RenderQuad(const f32* uvOffset)
    {
        shadow::Device::SetState(m_quadDepthStencilState);
        shadow::Device::SetState(m_quadMeshState);
        shadow::Device::SetVertexProgram(m_quadVertexProgram);
        shadow::Device::SetVertexDescriptor(m_quadVertexDescriptor);
        renderengine::FloatShaderStateIterator iterator;
        renderengine::Device::BeginShaderStates(m_uvOffsetHandle, iterator);
        f32* row = reinterpret_cast<f32*>(iterator.m_dest);
        for (u32 i = 0; i < 4; ++i) row[i] = uvOffset ? uvOffset[i] : 0.0f;
        shadow::Device::FlushVertexProgramState();
        D3DDevice_DrawVertices(gpD3DDevice, D3DPT_RECTLIST, 0, 3);
    }
}
}
}
