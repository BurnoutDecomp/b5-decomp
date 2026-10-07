#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <type_traits>
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/device.h"

static unsigned checks, failures, assertions, begins, ends, flushes, fences, draws, stride, primitive, count;
static float shaderMatrix[16];
static std::vector<unsigned char> packed;
static void Check(bool good, const char* label) { ++checks; if (!good) { ++failures; std::printf("FAIL %s\n", label); } }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
namespace CgsGraphics {
ImRendererBase* ImRendererBase::mgpActiveRenderer = nullptr;
void* ImRendererBase::mgpDevice = reinterpret_cast<void*>(UINT64_C(0x1234567800000100));
}
namespace renderengine {
IDirect3DDevice9* gDevice = nullptr;
void Device::SetState(const RenderTargetState*) {}
}
namespace shadow {
void Device::ResetShadowing() { ++begins; }
void DeviceSetVertexProgramInternal(void*) {}
void DeviceSetPixelProgram(void*) {}
bool Device::SetVertexDescriptor(const renderengine::VertexDescriptorData*) { return true; }
void Device::FlushVertexProgramState() { ++flushes; }
void Device::SetState(const renderengine::BlendMaterialState*) {}
void Device::SetState(const renderengine::DepthStencilState*) {}
void Device::SetState(const renderengine::RasterizerState*) {}
void* Device::SetState(const renderengine::TextureState*, u32) { return nullptr; }
void* Device::SetState(void*, u32) { return nullptr; }
void* Device::SetResource(void*, u32) { return nullptr; }
}
extern "C" void D3DDevice_SetRenderState_StencilRef(IDirect3DDevice9*, u32) {}
extern "C" u32 D3DDevice_InsertFence(void*) { return ++fences; }
extern "C" void* D3DDevice_BeginVertices(void* device, u32 p, u32 n, u32 s) {
    Check(device == CgsGraphics::ImRendererBase::mgpDevice, "real immediate renderer retains native device ownership");
    primitive = p; count = n; stride = s; packed.assign(size_t(n) * s, 0xCD); ++draws; return packed.data();
}
extern "C" void D3DDevice_EndVertices(void*) { ++ends; }
void* RenderEngineDeviceBeginShaderStates(void*, void** out) { *out = shaderMatrix; return shaderMatrix; }
#include "untextured_methods.inc"

static Matrix44 Identity() { Matrix44 m = {}; m.xAxis.x = m.yAxis.y = m.zAxis.z = m.wAxis.w = 1; return m; }
int main() {
    using namespace CgsGraphics;
    static_assert(!std::is_same<Im3dRenderBufferUntex, Im3dUntex>::value, "buffer and renderer are distinct original owners");
    Im3dRenderBufferUntex buffer;
    unsigned char commands[2][1024] = {}, vertices[2][1024] = {};
    for (unsigned i = 0; i < 2; ++i) {
        buffer.maBuffers[i].mpu8CommandBuffer = commands[i]; buffer.maBuffers[i].mpu8VertexBuffer = vertices[i];
    }
    buffer.mpWriteBuffer = &buffer.maBuffers[0]; buffer.mpDispatchBuffer = &buffer.maBuffers[1];
    buffer.muCommandBufferSize = buffer.muVertexBufferSize = 1024;
    Im3dUntex renderer;
    renderer.mapVertexProgramBuffer[0] = reinterpret_cast<renderengine::ProgramBuffer*>(UINT64_C(0x1234567800000020));
    renderer.mapPixelProgramBuffer[0] = reinterpret_cast<renderengine::ProgramBuffer*>(UINT64_C(0x1234567800000030));
    renderer.mpVertexDescriptor = reinterpret_cast<renderengine::VertexDescriptor*>(UINT64_C(0x1234567800000040));
    BasicColouredVertex source[4] = {};
    for (unsigned i = 0; i < 4; ++i) {
        source[i].mv3Pos = Vector3{float(i + 1), float(i + 10), float(i + 20), -987.0f};
        source[i].mv4Colour = RGBA8{u8(i + 30), u8(i + 40), u8(i + 50), u8(i + 60)};
    }
    Matrix44 view = Identity(), model = Identity();
    model.wAxis = Vector4{2, 3, 4, 5}; view.xAxis = Vector4{7, 8, 9, 10};
    buffer.BeginRendering(); buffer.SetTransform(model, view);
    buffer.Render(static_cast<renderengine::PrimitiveType>(6), source, 4); buffer.EndRendering();
    Check(draws == 0 && begins == 0, "producer records untextured commands without immediate drawing");
    const auto* copied = reinterpret_cast<const BasicColouredVertex*>(buffer.mpWriteBuffer->mpu8VertexBuffer);
    Check(std::memcmp(copied, source, sizeof(source)) == 0, "command bank owns complete 32-byte CPU records");
    buffer.Swap(); buffer.Dispatch(&renderer);
    Check(draws == 1 && primitive == 6 && count == 4 && stride == 16, "original dispatch packs the untextured 16-byte GPU stream");
    Check(begins == 1 && ends == 1 && flushes == 1 && ImRendererBase::mgpActiveRenderer == nullptr, "real Begin/Render/End lifecycle is balanced");
    for (unsigned i = 0; i < 4; ++i) {
        const float* pos = reinterpret_cast<const float*>(packed.data() + 16 * i);
        Check(pos[0] == source[i].mv3Pos.x && pos[1] == source[i].mv3Pos.y && pos[2] == source[i].mv3Pos.z,
              "GPU position omits the CPU SIMD pad");
        Check(std::memcmp(packed.data() + 16 * i + 12, &source[i].mv4Colour, 4) == 0, "GPU colour is the original four-byte payload");
    }
    const float expectedMatrix[16] = {7, 8, 9, 10, 0, 1, 0, 0, 0, 0, 1, 0, 14, 19, 22, 25};
    for (unsigned lane = 0; lane < 16; ++lane)
        Check(shaderMatrix[lane] == expectedMatrix[lane], "paired transform retains every matrix lane, including non-affine W");
    const auto* frozen = buffer.GetFirstCommand(); buffer.Clear();
    Check(buffer.GetFirstCommand() == frozen, "rewinding the next writer cannot clear the dispatched bank");
    buffer.Dispatch(&renderer);
    Check(draws == 2 && packed.size() == 64, "a second native consumption replays the same immutable commands");
    std::vector<BasicColouredVertex> large(32769);
    renderer.BeginRendering(); renderer.Render(static_cast<renderengine::PrimitiveType>(4), large.data(), 32768);
    Check(fences == 0, "original fence threshold is strict greater-than 512 KiB");
    renderer.Render(static_cast<renderengine::PrimitiveType>(4), large.data(), 32769); renderer.EndRendering();
    Check(fences == 1 && stride == 16 && count == 32769, "one additional packed vertex crosses the original fence threshold");
    Check(assertions == 0, "all original block and renderer assertions are satisfied");
    std::printf("PCUntexturedCommandBuffer: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
