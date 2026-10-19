// Link the complete production position-only renderer. The shadow/device calls
// are a deterministic boundary here; this fixture does not exercise D3D or shaders.
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsPositionOnlyVertex.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include <cstdio>
#include <cstdlib>

namespace CgsDev { namespace Assert
{
    int BeginAssert() { std::abort(); }
    int FireAssert(const char*, const char*, int) { std::abort(); }
    void* EndAssert() { std::abort(); }
} }
namespace
{
    const renderengine::ProgramBufferData* vertex = nullptr;
    const renderengine::ProgramBufferData* pixel = nullptr;
    const renderengine::VertexDescriptorData* descriptor = nullptr;
    int checks = 0;
    int failures = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value) ++failures;
        std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
    }
    struct Renderer : CgsGraphics::ImRenderer<CgsGraphics::PositionOnlyVertex>
    {
        alignas(16) unsigned char tokens[5][16]{};
        Renderer()
        {
            mapVertexProgramBuffer[0] = reinterpret_cast<renderengine::ProgramBuffer*>(tokens[0]);
            mapVertexProgramBuffer[1] = reinterpret_cast<renderengine::ProgramBuffer*>(tokens[1]);
            mapPixelProgramBuffer[0] = reinterpret_cast<renderengine::ProgramBuffer*>(tokens[2]);
            mapPixelProgramBuffer[1] = reinterpret_cast<renderengine::ProgramBuffer*>(tokens[3]);
            mpVertexDescriptor = reinterpret_cast<renderengine::VertexDescriptor*>(tokens[4]);
        }
        const renderengine::ProgramBufferData* Vertex(int slot) const
        { return reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[slot]); }
        const renderengine::ProgramBufferData* Pixel(int slot) const
        { return reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[slot]); }
        const renderengine::VertexDescriptorData* Descriptor() const
        { return reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor); }
        int Current() const { return mi8CurrentProgram; }
    };
}
// Model the existing shadow API's documented identity cache, using opaque handles.
// The renderer's state and control flow remain the whole production implementation.
namespace shadow
{
    void Device::ResetShadowing() { vertex = nullptr; pixel = nullptr; descriptor = nullptr; }
    bool Device::SetVertexProgram(const renderengine::ProgramBufferData* program)
    {
        const bool changed = vertex != program;
        vertex = program;
        return changed;
    }
    bool Device::SetPixelProgram(const renderengine::ProgramBufferData* program)
    {
        const bool changed = pixel != program;
        pixel = program;
        return changed;
    }
    bool Device::SetVertexDescriptor(const renderengine::VertexDescriptorData* value)
    {
        const bool changed = descriptor != value;
        descriptor = value;
        return changed;
    }
    void DeviceSetVertexProgramInternal(void* program)
    { Device::SetVertexProgram(static_cast<const renderengine::ProgramBufferData*>(program)); }
    void DeviceSetPixelProgram(void* program)
    { Device::SetPixelProgram(static_cast<const renderengine::ProgramBufferData*>(program)); }
}
CgsGraphics::ImRendererBase* CgsGraphics::ImRendererBase::mgpActiveRenderer = nullptr;

// Other explicitly emitted owner members are outside this fixture. Fail if any
// descriptor/program construction or shader-write boundary is unexpectedly used.
rw::BaseResourceDescriptor::BaseResourceDescriptor() { std::abort(); }
renderengine::VertexDescriptor::Parameters::Parameters() { std::abort(); }
void renderengine::VertexDescriptor::GetResourceDescriptor(void*, const Parameters*) { std::abort(); }
renderengine::VertexDescriptorData* renderengine::VertexDescriptor::Initialize(rw::Resource*, const Parameters*)
{ std::abort(); }
void CgsGraphics::ImRendererBase::ConstructOnceOnly(rw::IResourceAllocator*) { std::abort(); }
rw::BaseResourceDescriptors<5>* renderengine::ProgramBuffer::GetResourceDescriptor(
    rw::BaseResourceDescriptors<5>*, const renderengine::ProgramBufferParameters*) { std::abort(); }
renderengine::ProgramBufferData* renderengine::ProgramBuffer::Initialize(
    renderengine::ProgramResourceLayout*, const renderengine::ProgramBufferParameters*) { std::abort(); }
void* RenderEngineDeviceBeginShaderStates(void*, void**) { std::abort(); }

int main()
{
    Renderer renderer;
    renderer.BeginRendering();
    Check(CgsGraphics::ImRendererBase::mgpActiveRenderer == &renderer &&
          renderer.Current() == 0 && vertex == renderer.Vertex(0) &&
          pixel == renderer.Pixel(0) && descriptor == renderer.Descriptor(),
          "begin claims renderer and binds slot zero plus descriptor");
    Check(renderer.SetProgram(1) && renderer.Current() == 1 &&
          vertex == renderer.Vertex(1) && pixel == renderer.Pixel(1),
          "switch to slot one binds its pair");
    Check(!renderer.SetProgram(1), "same canonical program reports unchanged");
    renderer.EndRendering();
    renderer.BeginRendering();
    Check(renderer.SetProgram(1) && renderer.Current() == 1 &&
          vertex == renderer.Vertex(1) && pixel == renderer.Pixel(1),
          "slot switch after a new bracket observes the shared cache reset");
    shadow::Device::SetVertexProgram(renderer.Vertex(0));
    Check(renderer.SetProgram(1) && vertex == renderer.Vertex(1),
          "external canonical binding invalidates this renderer's prior selection");
    renderer.EndRendering();
    Check(CgsGraphics::ImRendererBase::mgpActiveRenderer == nullptr, "end releases renderer");
    std::printf("PositionOnlyProgramCache: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
