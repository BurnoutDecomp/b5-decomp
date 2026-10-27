// Actual production reset-store/contact slices, queue writers, replay, and CPU tessellation.
// This fixture's explicit boundaries are listed in evidence/gameplay95/BOUNDARIES.md.
#undef private
#undef protected
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "pc/gcm/renderengine/VertexDescriptor.h"
#define private public
#define protected public
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#undef private
#undef protected
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "pc/math/VmxFloat.h"

static unsigned guChecks, guFailures, guAssertions, guUnexpected;
static unsigned guAcquires, guReleases, guHeld;
static void Check(bool pass, const char* id)
{
    ++guChecks;
    guFailures += !pass;
    std::printf("CHECK %s %s\n", pass ? "PASS" : "FAIL", id);
}
static void Unexpected(const char* label)
{
    ++guUnexpected;
    std::printf("UNEXPECTED %s\n", label);
}
static u32 Bits(f32 value) { u32 bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
static bool Zero(bool value) { return !value; }
static bool Zero(s32 value) { return value == 0; }
static bool Zero(f32 value) { return Bits(value) == 0; }

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int)
{
    ++guAssertions; std::printf("ASSERT %s\n", message); return 0;
}
void* EndAssert() { return nullptr; }
} namespace Log {
void WriteToLog(const char* message) { std::printf("ENGINE_LOG %s", message); }
} }
static char gaLogText[4096];
static CgsDev::StrStream gLogStream(gaLogText, sizeof(gaLogText));
namespace CgsDev { namespace Log { StrStreamBase* gpDebugPrint = &gLogStream; }
namespace Message { u64 gxMessageFilterFlags = 0; } }

// Boundary: submit complete CPU vertex batches to an observable bank endpoint.
// This is deliberately not the production ImRenderBuffer command/GPU implementation.
// DrawTriangle/DrawSolidSphere, tessellation, buffering, flushes and colour packing below
// are extracted production definitions. No fixture Draw* implementation emits vertices.
struct RecordedBatch
{
    s32 primitive;
    std::vector<CgsGraphics::BasicColouredTexturedVertex> vertices;
};
static std::vector<RecordedBatch> gBatches;
static std::vector<unsigned> gSubmissionOrder;
static std::vector<const renderengine::DepthStencilState*> gDepthBindings;
namespace CgsGraphics {
class Im3dRenderBuffer
{
public:
    bool inBlock = false;
    bool IsInARenderingBlock() { return inBlock; }
    void BeginRendering() { if (inBlock) Unexpected("nested bank Begin"); inBlock = true; gSubmissionOrder.push_back(0); }
    void EndRendering() { if (!inBlock) Unexpected("unopened bank End"); inBlock = false; gSubmissionOrder.push_back(1); }
    void SetState(const renderengine::BlendState*) { gSubmissionOrder.push_back(3); }
    void SetState(const renderengine::RasterizerState*) { gSubmissionOrder.push_back(6); }
    void SetState(const renderengine::DepthStencilState* state) { gDepthBindings.push_back(state); gSubmissionOrder.push_back(4); }
    void SetState(const renderengine::TextureState*) { gSubmissionOrder.push_back(9); }
    void SetTransform(Matrix44::InParam) { gSubmissionOrder.push_back(16); }
    void Render(renderengine::PrimitiveType primitive, const BasicColouredTexturedVertex* vertices, u32 count)
    {
        if (!inBlock) Unexpected("vertex submit outside bank Begin/End");
        gBatches.push_back({static_cast<s32>(primitive), std::vector<BasicColouredTexturedVertex>(vertices, vertices + count)});
        gSubmissionOrder.push_back(2);
    }
};
static renderengine::DepthStencilState gDepthOff{}, gDepthOn{};
static renderengine::TextureState gUntextured{};
const renderengine::DepthStencilState* GetIm3dDepthStencilZBufferOffPC() { return &gDepthOff; }
const renderengine::DepthStencilState* GetIm3dDepthStencilZBufferOnPC() { return &gDepthOn; }
const renderengine::TextureState* GetIm3dTextureStateUntexturedPC()
{
    // Resource identity only; the fixture neither dereferences nor certifies a texture.
    return &gUntextured;
}
}

// Boundary: manager ownership/critical-section service. The interface constructor,
// destructor and GetRender definitions come from production; the hosted owner supplies
// one real DebugRender and records balanced acquire/release without engine threads.
namespace CgsDev {
struct DebugManager
{
    DebugRender mBufferedRenderer;
    static DebugManager* ThreadSafeAquire();
    static void ThreadSafeRelease(DebugManager* manager);
};
struct DebugInterface
{
    DebugInterface();
    ~DebugInterface();
    DebugRender& GetRender();
    DebugManager* mpDebugManager;
    bool mbIsAutomaticClass;
};
}
static CgsDev::DebugManager gManager;
namespace CgsDev {
DebugManager* DebugManager::ThreadSafeAquire() { ++guAcquires; ++guHeld; return &gManager; }
void DebugManager::ThreadSafeRelease(DebugManager* manager)
{
    if (manager != &gManager || guHeld != 1) Unexpected("manager release ownership");
    ++guReleases; if (guHeld) --guHeld;
}
}

// Unselected immediate primitives are fail-observable link boundaries. If Dispatch3D
// takes any of them, replay.clear_clean fails. Selected Draw* definitions are production.
namespace CgsDev {
void Debug3DImmediateRender::DrawText(Vector3, const char*, f32, rw::RGBA) { Unexpected("DrawText"); }
void Debug3DImmediateRender::DrawLine(Vector3, Vector3, rw::RGBA) { Unexpected("DrawLine"); }
void Debug3DImmediateRender::DrawPoint(Vector3, rw::RGBA) { Unexpected("DrawPoint"); }
void Debug3DImmediateRender::DrawQuad(Vector3, Vector3, Vector3, Vector3, rw::RGBA) { Unexpected("DrawQuad"); }
void Debug3DImmediateRender::DrawAngleRad(Vector3, f32, rw::RGBA) { Unexpected("DrawAngleRad"); }
void Debug3DImmediateRender::DrawAxis(Matrix44Affine) { Unexpected("DrawAxis"); }
void Debug3DImmediateRender::DrawSphere(Vector3, f32, rw::RGBA) { Unexpected("DrawSphere"); }
void Debug3DImmediateRender::DrawHollowSphere(Vector3, f32, rw::RGBA) { Unexpected("DrawHollowSphere"); }
void Debug3DImmediateRender::DrawCircle(Vector3, Vector3, f32, rw::RGBA) { Unexpected("DrawCircle"); }
void Debug3DImmediateRender::DrawBox(Vector3, Vector3, Matrix44Affine, rw::RGBA) { Unexpected("DrawBox oriented"); }
void Debug3DImmediateRender::DrawBox(Vector3, Vector3, rw::RGBA) { Unexpected("DrawBox AA"); }
void Debug3DImmediateRender::DrawSolidBox(Vector3, Vector3, Matrix44Affine, rw::RGBA) { Unexpected("DrawSolidBox oriented"); }
void Debug3DImmediateRender::DrawSolidBox(Vector3, Vector3, rw::RGBA) { Unexpected("DrawSolidBox AA"); }
void Debug3DImmediateRender::DrawArrow(Vector3, Vector3, rw::RGBA) { Unexpected("DrawArrow"); }
void Debug3DImmediateRender::DrawSolidArrow(Vector3, Vector3, rw::RGBA) { Unexpected("DrawSolidArrow"); }
void Debug3DImmediateRender::DrawCylinder(Vector3, Vector3, f32, rw::RGBA) { Unexpected("DrawCylinder"); }
void Debug3DImmediateRender::DrawCapsule(Vector3, Vector3, f32, rw::RGBA) { Unexpected("DrawCapsule"); }
}

// Generated declarations contain the exact selected production field types, without
// in-class initializers. Generated Construct executes only actual unconditional stores
// present in that variant's production Construct. No expected-value fallback is emitted.
#include "gameplay95_fields.inc"
#include "gameplay95_production.inc"

static void ResetChecks()
{
    ResetOwner owner{};
    for (unsigned cycle = 0; cycle != 3; ++cycle)
    {
        // All fields are edited before EVERY Construct call, including the first.
#define FIELD(type, name) owner.name = static_cast<type>(cycle + 7);
#include "gameplay95_field_list.inc"
#undef FIELD
        owner.Construct();
#define FIELD(type, name) { char id[192]; std::snprintf(id, sizeof(id), "reset.%u.%s", cycle, #name); Check(Zero(owner.name), id); }
#include "gameplay95_field_list.inc"
#undef FIELD
    }
}

struct EventImage
{
    s32 type;
    s32 size;
    std::vector<u8> payload;
};
static std::vector<EventImage> ReadEvents(const CgsDev::DebugRender& render)
{
    std::vector<EventImage> images;
    const CgsModule::Event* event = nullptr;
    s32 size = 0;
    s32 type = render.m3DQueue.GetFirstEvent(&event, &size);
    while (type >= 0)
    {
        if (size < 0 || size > 16384 || images.size() >= 100) { Unexpected("malformed event iteration"); break; }
        const auto* bytes = reinterpret_cast<const u8*>(event);
        images.push_back({type, size, std::vector<u8>(bytes, bytes + size)});
        type = render.m3DQueue.GetNextEvent(event, &event, &size);
    }
    return images;
}
static f32 FloatAt(const EventImage& event, size_t offset)
{
    f32 value = std::numeric_limits<f32>::quiet_NaN();
    if (offset + sizeof(value) <= event.payload.size()) std::memcpy(&value, event.payload.data() + offset, sizeof(value));
    return value;
}
static u32 WordAt(const EventImage& event, size_t offset)
{
    u32 value = 0;
    if (offset + sizeof(value) <= event.payload.size()) std::memcpy(&value, event.payload.data() + offset, sizeof(value));
    return value;
}
static bool Position(const Vector3& p, f32 x, f32 y, u32 zBits)
{
    return p.x == x && p.y == y && Bits(p.z) == zBits;
}
static std::vector<CgsGraphics::BasicColouredTexturedVertex> AllVertices()
{
    std::vector<CgsGraphics::BasicColouredTexturedVertex> out;
    for (const auto& batch : gBatches) out.insert(out.end(), batch.vertices.begin(), batch.vertices.end());
    return out;
}
static Matrix44 Identity()
{
    Matrix44 matrix{}; matrix.SetIdentity(); return matrix;
}
static bool SphereGeometry(const std::vector<CgsGraphics::BasicColouredTexturedVertex>& vertices)
{
    if (vertices.size() != 1299) return false;
    const f32 cx[] = {1, 0, -1}, cy[] = {0, 1, 0};
    for (size_t sphere = 0; sphere != 3; ++sphere)
    {
        f32 minY = 100, maxY = -100;
        for (size_t i = 0; i != 432; ++i)
        {
            const auto& p = vertices[3 + sphere * 432 + i].mv3Pos;
            const f32 dx = p.x - cx[sphere], dy = p.y - cy[sphere], dz = p.z;
            if (!std::isfinite(dx + dy + dz) || std::fabs(dx*dx + dy*dy + dz*dz - 0.09f) > 0.000001f) return false;
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        if (std::fabs(minY - (cy[sphere] - 0.3f)) > 0.000001f ||
            std::fabs(maxY - (cy[sphere] + 0.3f)) > 0.000001f) return false;
    }
    return true;
}

static void ContactChecks(const char* classification, unsigned mask)
{
    const bool wall = std::strcmp(classification, "wall") == 0;
    const bool curb = std::strcmp(classification, "curb") == 0;
    const bool enabled = (wall && (mask & 1)) || (curb && (mask & 2));
    const u32 colour = wall ? 0xFF0000FFu : 0xFF00FF00u;
    char id[192];
    const auto check = [&](bool pass, const char* label) {
        std::snprintf(id, sizeof(id), "contact.%s.%u.%s", classification, mask, label); Check(pass, id);
    };
    gManager.mBufferedRenderer.Clear(); gBatches.clear(); gSubmissionOrder.clear(); gDepthBindings.clear();
    guAcquires = guReleases = guHeld = guAssertions = guUnexpected = 0;
    ContactProducer producer{};
    producer.mDebugComponent.mbRenderWallContacts = (mask & 1) != 0;
    producer.mDebugComponent.mbRenderGroundContacts = (mask & 2) != 0;
    CgsGeometric::Triangle4::AOSTriangle triangle{};
    triangle.mVertex0 = {1, 0, 0, 0}; triangle.mVertex1 = {0, 1, 0, 0};
    triangle.mVertex2 = {-1, 0, 0, 0}; triangle.mNormal = {0, 0, 1, 0};
    // Classification and valid cached triangle are explicit collision-system inputs.
    producer.Produce(triangle, wall, curb);
    const auto events = ReadEvents(gManager.mBufferedRenderer);
    check(events.size() == (enabled ? 4u : 0u), "queue.count");
    bool types = events.empty();
    if (enabled && events.size() == 4)
        types = events[0].type == 22 && events[0].size == 40 &&
                events[1].type == 13 && events[1].size == 20 &&
                events[2].type == 13 && events[2].size == 20 &&
                events[3].type == 13 && events[3].size == 20;
    if (enabled && events.size() != 4) types = false;
    if (!enabled && !events.empty()) types = false;
    check(types, "queue.types_sizes");
    bool tri = !enabled && events.empty(), spheres = tri;
    if (enabled && events.size() == 4)
    {
        tri = FloatAt(events[0], 0) == 1 && FloatAt(events[0], 4) == 0 && Bits(FloatAt(events[0], 8)) == 0x3d4ccccdu &&
              FloatAt(events[0], 12) == 0 && FloatAt(events[0], 16) == 1 && Bits(FloatAt(events[0], 20)) == 0x3d4ccccdu &&
              FloatAt(events[0], 24) == -1 && FloatAt(events[0], 28) == 0 && Bits(FloatAt(events[0], 32)) == 0x3d4ccccdu &&
              WordAt(events[0], 36) == colour;
        const f32 cx[] = {1, 0, -1}, cy[] = {0, 1, 0}; spheres = true;
        for (size_t sphere = 0; sphere != 3; ++sphere)
        {
            const auto& event = events[sphere + 1];
            spheres &= FloatAt(event, 0) == cx[sphere] && FloatAt(event, 4) == cy[sphere] &&
                       Bits(FloatAt(event, 8)) == 0 && Bits(FloatAt(event, 12)) == 0x3e99999au && WordAt(event, 16) == colour;
        }
    }
    check(tri, "queue.triangle"); check(spheres, "queue.spheres");
    check(guAcquires == (enabled ? 1u : 0u) && guReleases == guAcquires && guHeld == 0, "scope.acquire_release");
    check(gBatches.empty() && gSubmissionOrder.empty(), "producer.deferred");
    CgsGraphics::Im3dRenderBuffer bank;
    CgsDev::Debug3DImmediateRender renderer{};
    renderer.SetRenderBuffer(&bank);
    renderer.Begin(Identity(), {0, 0, 0, 0});
    gManager.mBufferedRenderer.Dispatch3D(&renderer, true);
    renderer.End();
    const auto vertices = AllVertices();
    bool geometry = !enabled && vertices.empty();
    if (enabled && vertices.size() == 1299)
        geometry = Position(vertices[0].mv3Pos, 1, 0, 0x3d4ccccdu) &&
                   Position(vertices[1].mv3Pos, 0, 1, 0x3d4ccccdu) &&
                   Position(vertices[2].mv3Pos, -1, 0, 0x3d4ccccdu) && SphereGeometry(vertices);
    check(geometry, "replay.geometry");
    bool packed = enabled ? vertices.size() == 1299 : vertices.empty();
    for (const auto& vertex : vertices)
    {
        const auto& c = vertex.mv4Colour;
        const u32 word = u32(c.r) | (u32(c.g) << 8) | (u32(c.b) << 16) | (u32(c.a) << 24);
        packed &= word == colour && vertex.mv2Tex0UV.x == 0.5f && vertex.mv2Tex0UV.y == 0.5f;
    }
    check(packed, "replay.colour_uv");
    bool batches = gBatches.empty();
    if (enabled && gBatches.size() == 3)
        batches = gBatches[0].primitive == 4 && gBatches[0].vertices.size() == 3 &&
                  gBatches[1].primitive == 4 && gBatches[1].vertices.size() == 999 &&
                  gBatches[2].primitive == 4 && gBatches[2].vertices.size() == 297;
    if (enabled && gBatches.size() != 3) batches = false;
    if (!enabled && !gBatches.empty()) batches = false;
    const std::vector<unsigned> expectedOrder = enabled ? std::vector<unsigned>{0,3,6,4,16,9,4,4,2,9,4,4,2,9,4,4,2,1}
                                                       : std::vector<unsigned>{0,3,6,1};
    const std::vector<const renderengine::DepthStencilState*> expectedDepths = enabled
        ? std::vector<const renderengine::DepthStencilState*>{&sgDebug3DRenderStates.mDepth3D,
            &CgsGraphics::gDepthOff, &CgsGraphics::gDepthOn, &CgsGraphics::gDepthOff,
            &CgsGraphics::gDepthOn, &CgsGraphics::gDepthOff, &CgsGraphics::gDepthOn}
        : std::vector<const renderengine::DepthStencilState*>{};
    check(batches && gSubmissionOrder == expectedOrder && gDepthBindings == expectedDepths, "replay.batches");
    check(gManager.mBufferedRenderer.m3DQueue.GetLength() == 0 &&
          gManager.mBufferedRenderer.m2DQueue.GetLength() == 0 &&
          !bank.inBlock && guAssertions == 0 && guUnexpected == 0 && guHeld == 0,
          "replay.clear_clean");
}

int main()
{
    ResetChecks();
    gManager.mBufferedRenderer.Construct();
    for (const char* classification : {"none", "wall", "curb"})
        for (unsigned mask = 0; mask != 4; ++mask) ContactChecks(classification, mask);
    gManager.mBufferedRenderer.Destruct();
    std::printf("Gameplay95: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
