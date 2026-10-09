// Observe packets emitted by the production world reflection dispatch body.
#include <cstdio>
#include "types.hpp"
#include "pc/gcm/renderengine/ReflectionDistancePCLeaf.h"

static u32 suChecks = 0, suFailures = 0;
static void Check(bool lbResult, const char* lpcName)
{
    ++suChecks;
    if (!lbResult) { ++suFailures; std::printf("FAIL %s\n", lpcName); }
}
#define CGS_ASSERT(value, message) Check(!!(value), message)
namespace rw { namespace math { namespace vpu {
struct Vector3 { using InParam = const Vector3&; f32 x = 0, y = 0, z = 0; };
static Vector3 operator-(Vector3 lA, Vector3 lB) { return {lA.x-lB.x,lA.y-lB.y,lA.z-lB.z}; }
static f32 MagnitudeSquared(Vector3 lV) { return lV.x*lV.x+lV.y*lV.y+lV.z*lV.z; }
struct Matrix44 { using InParam = const Matrix44&; };
struct Matrix44Affine { Vector3 mPosition; const Vector3& Pos() const { return mPosition; } };
} } }
using namespace rw::math::vpu;
namespace CgsSceneManager {
struct EntityId { u32 GetOwner() const { return 1; } u32 GetEntityIndex() const { return 0; }
                 u32 GetPartIndex() const { return 0; } };
}
template<class Type, u32 Capacity> struct Array
{
    Type mItem;
    u32 GetLength() const { return 1; }
    const Type& GetItem(u32) const { return mItem; }
};
namespace CgsGraphics {
struct Renderable { s32 miState; };
struct Model
{
    enum State { E_STATE_LOD_0, E_STATE_LOD_1, E_STATE_LOD_2 };
    bool mabExists[3] = {true,true,true};
    Renderable maRenderables[3] = {{0},{1},{2}};
    bool DoesStateExist(State leState) const { return mabExists[leState]; }
    f32 GetLodDistance(u32 luState) const { static const f32 KAF_DISTANCES[] = {10,20,100}; return KAF_DISTANCES[luState]; }
    const Renderable* GetRenderable(State leState) const { return &maRenderables[leState]; }
};
struct Instance { Model* mpModel; Matrix44Affine mTransform; };
struct InstanceList { Instance* mpInstance; Instance* GetInstance(u32) const { return mpInstance; } };
struct DispatchBin { void BeginPacket() {} const void* EndPacket() { return this; } };
struct DispatchList { u32 muCount = 0; u32 GetCount() const { return muCount; } void Submit(u32,const void*) { ++muCount; } };
struct DispatchFrame { DispatchBin mBin; DispatchList maLists[16];
    DispatchList* GetList(u32 luIndex) { return &maLists[luIndex]; } DispatchBin& GetBin() { return mBin; } };
static s32 siDrawState = -1, siOpaque = -1, siTransparent = -1;
static u8 su8Technique = 0;
struct DrawRenderable
{
    static void AddToBin(const Renderable* lpRenderable, DispatchFrame*, bool, s8 liOpaque,
        s8 liTransparent, u32, u8 luTechnique, bool, u8, u8, int, u32)
    {
        siDrawState = lpRenderable->miState; siOpaque = liOpaque;
        siTransparent = liTransparent; su8Technique = luTechnique;
    }
};
struct Constants { void SetShaderConstantData(int, const Matrix44Affine&) {} };
static Constants mShaderConstantTable;
}
namespace renderengine {
enum GraphicsModelCategoryPC { E_GRAPHICS_ENVMAP };
struct Diagnostics { u32 muEnvLod=2,muEnvConsidered=0,muEnvMissing=0,muEnvCulled=0,muEnvExtended=0; };
static bool GraphicsDiagnosticsEnabledPC() { return true; }
static Diagnostics& GetGraphicsDiagnosticsPC() { static Diagnostics sDiag; return sDiag; }
static void RecordGraphicsModelPC(GraphicsModelCategoryPC, const CgsGraphics::Model*, u32) {}
}
namespace BrnWorld {
using namespace CgsGraphics;
const u32 KU_ENTITY_OWNER_WORLD = 1;
struct ShaderLodInfo { u8 GetEnvMapTechnique() const { return 7; } };
struct WorldEntityIO
{
    struct InputBuffer_GenerateDispatchLists
    {
        DispatchFrame* mpFrame;
        mutable u32 muReadLocks = 0;
        void LockForRead() const { ++muReadLocks; }
        void UnlockForRead() const { --muReadLocks; }
        DispatchFrame* GetDispatchFrame() const { return mpFrame; }
    };
};
struct WorldEntityModule
{
    s32 miEnvironmentMapLOD = 0;
    struct Streamer { InstanceList* mpList; InstanceList* GetInstanceList(u32) { return mpList; } } mWorldGraphicsStreamer;
    void GenerateDispatchListsForEnvironmentMap(const WorldEntityIO::InputBuffer_GenerateDispatchLists*,
        const Array<CgsSceneManager::EntityId,4500u>&, Matrix44::InParam, Vector3::InParam,
        const ShaderLodInfo*, s32, s32, s32);
};
}
#include "pc_world_reflection_distance.inc"

int main()
{
    using namespace CgsGraphics;
    using namespace BrnWorld;
    Model lModel;
    Instance lInstance = {&lModel,{}};
    InstanceList lList = {&lInstance};
    WorldEntityModule lModule;
    lModule.mWorldGraphicsStreamer.mpList = &lList;
    DispatchFrame lFrame;
    WorldEntityIO::InputBuffer_GenerateDispatchLists lInput = {&lFrame};
    Array<CgsSceneManager::EntityId,4500u> lVisible;
    Matrix44 lView;
    ShaderLodInfo lShader;
    auto Draw = [&](f32 lfDistance)
    {
        siDrawState = -1;
        lInstance.mTransform.mPosition = {lfDistance,0,0};
        const u32 luBefore = lFrame.maLists[5].muCount;
        lModule.GenerateDispatchListsForEnvironmentMap(&lInput,lVisible,lView,{},&lShader,5,6,7);
        Check(lInput.muReadLocks == 0,"reflection feed always releases the input read lock");
        return lFrame.maLists[5].muCount != luBefore;
    };
    Check(!Draw(15),"original LOD0 distance culls geometry before reflection submission");
    renderengine::EnvironmentMapDrawDistancePC() = 300;
    Check(Draw(200) && siDrawState == 0 && su8Technique == 7 && siOpaque == 6 && siTransparent == 7,
        "extended distance emits the selected high-detail world packet with original reflection routing");
    Check(renderengine::GetGraphicsDiagnosticsPC().muEnvExtended == 1,"live witness counts an actual draw beyond the authored cutoff");
    Check(!Draw(300),"reflection distance boundary remains exclusive");
    lModel.mabExists[0] = false;
    Check(!Draw(15),"distance extension cannot submit a missing reflection LOD");
    lModel.mabExists[0] = true;
    renderengine::EnvironmentMapDrawDistancePC() = 0;
    Check(!Draw(15) && Draw(5),"live reset restores the authored LOD cutoff");
    renderengine::EnvironmentMapDrawDistancePC() = 5;
    Check(Draw(7),"positive extension never shortens a longer authored cutoff");
    renderengine::EnvironmentMapDrawDistancePC() = 0;
    std::printf("PCWorldReflectionDistance: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
