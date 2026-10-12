// Expected values follow ARTIST instruction/branch counterexamples in the
// 319-site review. Injected bodies are production source, including old revisions.
// Renderer/owner/collision boundaries are recording spies, not native execution.
#include "BrnCommonTypes.h"
#include "rw/rwcore_structs.h"
#include "rw/math/vpu/vector3_operation.h"
#include "pc/math/VmxFloat.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <limits>

static unsigned checks=0, failures=0;
static void Check(bool ok,const char* label) { ++checks; if(!ok) { ++failures; std::printf("FAIL %s\n",label); } }
static u32 Bits(f32 x) { u32 b; std::memcpy(&b,&x,4); return b; }
static bool Near(f32 a,f32 b) { return std::fabs(a-b)<1e-6f; }
static void Assert(bool ok) { if(!ok) { std::puts("fixture owner assert"); ++failures; } }
#undef CGS_ASSERT
#define CGS_ASSERT(value, message) Assert(value)

struct Draw { Vector3 a,b,c,d; u32 colour; };
static std::vector<Draw> lines,quads,triangles;
static std::vector<f32> spheres;
static void Clear() { lines.clear();quads.clear();triangles.clear();spheres.clear(); }
namespace CgsDev {
using RGBA=u32;
struct Debug3DImmediateRender {
    enum { E_ZTEST_ON,E_ZTEST_OFF };
    void SetZTestEnable(int) {}
    void DrawLine(Vector3 a,Vector3 b,rw::RGBA c) { lines.push_back({a,b,{},{},c.m_rgba}); }
    void DrawQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA col) { quads.push_back({a,b,c,d,col.m_rgba}); }
    void DrawSolidQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA col) { DrawQuad(a,b,c,d,col); }
    void DrawHollowTriangle(Vector3 a,Vector3 b,Vector3 c,rw::RGBA col) { triangles.push_back({a,b,c,{},col.m_rgba}); }
    void DrawSolidTriangle(Vector3 a,Vector3 b,Vector3 c,rw::RGBA col) { triangles.push_back({a,b,c,{},col.m_rgba}); }
    void DrawSphere(Vector3,f32 radius,rw::RGBA) { spheres.push_back(radius); }
};
struct DebugRender {
    void DrawLine(Vector3 a,Vector3 b,RGBA c) { lines.push_back({a,b,{},{},c}); }
    void DrawCross(Vector3,f32,RGBA) {}
    void DrawText(Vector3,const char*,f32,RGBA) {}
};
static DebugRender renderer;
struct DebugInterface { DebugRender& GetRender() { return renderer; } };
struct PerfMonCpu { static void StartMonitor(s32) {} static void StopMonitor(s32) {} };
}
namespace CgsCore {
static void SPrintf(char* out,u32 n,const char* format,...) { va_list a; va_start(a,format); std::vsnprintf(out,n,format,a);va_end(a); }
}
namespace CgsSystem {
static u64 now=0, frequency=1, delta=1;
u64 GetSystemTimerBaseTime64() { u64 v=now;now+=delta;return v; }
u64 GetSystemTimerFrequency64() { return frequency; }
u32 GetSystemTimerBaseTime() { return static_cast<u32>(GetSystemTimerBaseTime64()); }
u32 GetSystemTimerFrequency() { return static_cast<u32>(frequency); }
}
namespace rw { namespace collision { struct Plane { f32 x,y,z,w; }; } }
namespace CgsGeometric {
struct SweptSphere { void Set(Vector3Plus,Vector3Plus) {} };
struct Triangle4 {
    using Mask4=Vector4;
    Vector4 mVertex0X,mVertex0Y,mVertex0Z,mVertex1X,mVertex1Y,mVertex1Z,mVertex2X,mVertex2Y,mVertex2Z;
    Vector4 mValidMasks,mSurfaceTags,mEdge0Cosigns,mEdge1Cosigns,mEdge2Cosigns;
};
template<class... T> Triangle4::Mask4 IntersectTriangle4SweptSphere(const SweptSphere&,const Triangle4&,T&... out) {
    ((out={}),...); return {};
}
static u32 guUnidentifiedFrustumDebugWord_830390E4=9;
static bool sbFrustumDebugClipCustomPlanesToHull=true;
struct Frustum {
    rw::collision::Plane planes[8]{};
    rw::collision::Plane GetPlaneByIndex(u32 n) const { return planes[n]; }
    void DebugRender() const {}
    void DebugRenderCustomPlanes() const;
};
// Valid intersections for the finite orthogonal-plane witness. This is a query
// producer seam; the production clipping loop and emitted lines are under test.
static bool IntersectionOf3Planes(const rw::collision::Plane& a,const rw::collision::Plane& b,const rw::collision::Plane& c,Vector3& out) {
    if(a.x==1 && Bits(b.x)==0 && b.y==1 && c.z==1) { out={1e38f,0,0,1};return true; } return false;
}
}
namespace CgsSceneManager {
struct SceneManagerDebugComponent {
    Vector3 mTriangleOffset{},mInitialCameraPosition{};
    f32 mfTriangleScale=1,mfPadding=.3f;
    s32 miTestsToExecute=1,miTotalCycles=0,miCyclesPerCall=0,miCyclesPerTriangle=0;
    bool mbDrawPadding=true;
    void DrawCachedTriangle(CgsDev::Debug3DImmediateRender*,Vector3,Vector3,Vector3,rw::RGBA);
    void DrawTriangle(CgsDev::Debug3DImmediateRender*,Vector3,Vector3,Vector3,Vector3,rw::math::vpu::MaskScalar,Vector3,Vector3);
    void DrawTriangleTest(CgsDev::Debug3DImmediateRender*);
};
}
enum EActiveRaceCarIndex { E_ACTIVE_RACE_CAR_INDEX_INVALID=-1,E_ACTIVE_RACE_CAR_INDEX_0=0,E_ACTIVE_RACE_CAR_INDEX_1=1,E_ACTIVE_RACE_CAR_INDEX_COUNT=8 };
static EActiveRaceCarIndex& operator++(EActiveRaceCarIndex& x,int) { x=static_cast<EActiveRaceCarIndex>(x+1);return x; }
namespace BrnPhysics { namespace Vehicle { struct RaceCarState { Vector3 mLinearVelocity{};f32 mfSpeedMPH=40; }; } }
namespace BrnWorld {
namespace vpu=rw::math::vpu;
struct ActiveRaceCar {
    bool active=false;
    Vector3 position{},velocity{},direction{0,0,1,0};
    Matrix44Affine transform{};
    BrnPhysics::Vehicle::RaceCarState state{};
    bool IsActive() const { return active; }
    bool IsAttached() const { return true; }
    Vector3 GetPosition() const { return position; }
    Vector3 GetVelocity() const { return velocity; }
    Vector3 GetDirection() const { return direction; }
    Matrix44Affine GetTransform() const { return transform; }
    const BrnPhysics::Vehicle::RaceCarState* GetPhysicsState() const { return &state; }
};
struct RaceCarEntityModule {
    ActiveRaceCar maActiveRaceCars[8];
    EActiveRaceCarIndex meIndexOfCarPlayerIsTailgating=E_ACTIVE_RACE_CAR_INDEX_INVALID;
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return E_ACTIVE_RACE_CAR_INDEX_0; }
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex n) { return &maActiveRaceCars[n]; }
    bool IsPlayerCarTailgatingOtherRaceCars(EActiveRaceCarIndex,const ActiveRaceCar*);
};
struct RaceCarEntityModuleDebugComponent {
    RaceCarEntityModule* mpRaceCarEntityModule=nullptr;
    s32 miAverageSpeedFrameCount=0;f32 mfAverageSpeed=0;
    void Update();
    void RenderTailgateCones(CgsDev::Debug3DImmediateRender*);
    void RenderSingleDebugCone(CgsDev::Debug3DImmediateRender*,Matrix44Affine,VecFloat,rw::RGBA,s32) const;
};
enum ShadowMapType { E_SHADOWMAP_TYPE_TSM=1,E_SHADOWMAP_TYPE_BOUNDINGBOX=2 };
struct ShadowMap { ShadowMapType maShadowMapTypes[3]{E_SHADOWMAP_TYPE_TSM};bool mbShowLightFrustumInDebugRender=false;void DebugRender(u32) const; };
}

#include "debug_menu_draw_consumers.inc"

static void SceneWitnesses() {
    CgsSceneManager::SceneManagerDebugComponent s; CgsDev::Debug3DImmediateRender r;
    rw::math::vpu::MaskScalar mask{}; u32 all=0xffffffffu;
    std::memcpy(&mask.x,&all,4);std::memcpy(&mask.y,&all,4);std::memcpy(&mask.z,&all,4);std::memcpy(&mask.w,&all,4);
    Clear();s.DrawTriangle(&r,{},{},{},{},mask,{},{});
    Check(quads.size()==1 && quads[0].colour==0xffffffffu && lines.size()==6,"Scene hit keeps white quad and six contact ticks");
    Clear();s.DrawTriangle(&r,{},{},{},{},{},{},{});
    Check(quads.size()==1 && quads[0].colour==0xffff0000u && lines.empty(),"Scene no-hit is blue-only");
    f32 e=std::ldexp(1.0f,-23);Clear();
    s.DrawCachedTriangle(&r,{0,0,0,1},{0,-1,-(1+e),1},{0,-(1-e),-1,1},{});
    Check(triangles.size()==2 && Near(triangles[0].a.x,.04f),"fused cross retains finite nearly-collinear normal");
    Check(triangles.size()==2 && std::isfinite(triangles[0].b.x) && std::isfinite(triangles[0].c.x),"cached triangle emits finite offset vertices");
    Clear();CgsSystem::now=(1ULL<<33)+100;CgsSystem::delta=(1ULL<<32);CgsSystem::frequency=(1ULL<<32)+1;
    s.DrawTriangleTest(&r);
    Check(static_cast<u32>(s.miTotalCycles)==3199999999u,"triangle benchmark retains high timer and frequency bits until division");
    Check(spheres.size()==2 && Near(spheres[0],.5f) && Near(spheres[1],.8f),"padding sphere adds base radius");
}
static void ShadowWitnesses() {
    using namespace BrnWorld;ShadowMap s;auto& d=gaTsmDebugRenderInfo[0];d.mLightToWorld.SetIdentity();
    d.mSilhouetteAWorld.x=11;d.mSilhouetteADirWorld.x=12;d.mMaxPointWorld.x=21;d.mMaxPerpWorld.x=22;
    d.mSilhouetteBWorld.x=31;d.mSilhouetteBDirWorld.x=32;d.mMinPointWorld.x=41;d.mMinPerpWorld.x=42;
    Clear();s.DebugRender(0);Check(lines.size()==20,"TSM emits twenty direct lines");
    const f32 starts[4]={11,21,31,41};const f32 ends[4]={12,22,32,42};
    for(unsigned i=0;i<4;++i) Check(lines.size()>9+i && lines[9+i].a.x==starts[i] && lines[9+i].b.x==ends[i] && lines[9+i].colour==(i%2?0xff800080u:0xff808000u),"TSM interleaved endpoint/color contract");
    bool plane=true;for(const auto& l:lines) if(l.colour==0xff804080u) plane &= l.a.z==0 && l.b.z==0;
    Check(plane,"TSM guide plane remains at local Z zero");
    s.maShadowMapTypes[0]=E_SHADOWMAP_TYPE_BOUNDINGBOX;auto& b=gBoundingBoxDebugRenderInfo[0];b.mLightToWorld.SetIdentity();b.mZOffset.z=2;
    Clear();s.DebugRender(0);plane=true;for(const auto& l:lines)if(l.colour==0xff804080u)plane &= Near(l.a.z,2.01f)&&Near(l.b.z,2.01f);
    Check(plane,"bounding-box guide retains separate depth plus .01");
}
static void RaceWitnesses() {
    using namespace BrnWorld;RaceCarEntityModule m;RaceCarEntityModuleDebugComponent d;d.mpRaceCarEntityModule=&m;
    m.maActiveRaceCars[0].velocity={1e-20f,0,0,0};d.Update();
    Check(d.mfAverageSpeed==0 && d.miAverageSpeedFrameCount==1,"VMX tiny velocity yields zero first mean");
    d.miAverageSpeedFrameCount=0;d.mfAverageSpeed=0;m.maActiveRaceCars[0].velocity={1e20f,0,0,0};d.Update();
    Check(std::isnan(d.mfAverageSpeed),"overflow squared velocity keeps unconditional refinement NaN");
    d.miAverageSpeedFrameCount=3;d.mfAverageSpeed=1.00000011920928955078125f;m.maActiveRaceCars[0].velocity={.00000011920928955078125f,0,0,0};d.Update();
    Check(Bits(d.mfAverageSpeed)==Bits(std::fma(3.0f,1.00000011920928955078125f,.00000011920928955078125f)/4),"mean retains scalar fused multiply-add");
    CgsDev::Debug3DImmediateRender r;Matrix44Affine x{};x.SetIdentity();x.zAxis={0,0,1e-20f,0};Clear();
    d.RenderSingleDebugCone(&r,x,{.3f,.3f,.3f,.3f},{},4);
    bool collapsed=!quads.empty();for(auto& q:quads)collapsed &= q.c.x==0&&q.c.y==0&&q.d.x==0&&q.d.y==0;
    Check(collapsed,"tiny cone depth collapses radius after VMX zero selection");
    m.maActiveRaceCars[1].active=true;m.maActiveRaceCars[1].transform.SetIdentity();Clear();d.RenderTailgateCones(&r);
    Check(!quads.empty()&&quads[0].colour==0x5032b400u,"cone caller preserves packed RGBA word");
    m.maActiveRaceCars[0].position={0,0,2,1};m.maActiveRaceCars[1].position={0,0,0,1};
    Check(m.IsPlayerCarTailgatingOtherRaceCars(E_ACTIVE_RACE_CAR_INDEX_0,m.maActiveRaceCars),"coincident tailgate apex preserves ordered NaN acceptance");
    Check(m.meIndexOfCarPlayerIsTailgating==E_ACTIVE_RACE_CAR_INDEX_1,"coincident accepted candidate publishes rival index");
    auto c=Illumination({},1e-40f,0,0);Check(c.x==0,"editable subnormal self illumination flushes to zero");
    c=Illumination({},std::numeric_limits<f32>::quiet_NaN(),0,0);Check(std::isnan(c.x),"self illumination propagates unordered editable lane");
    const f32 band=LodBand(1,std::ldexp(1.0f,-23),.75f,1);
    Check(Bits(band)==0x3e800003u,"vehicle LOD interpolation retains fused final band bits");
}
static void FrustumWitness() {
    CgsGeometric::Frustum f;f.planes[0]={1,0,0,1e38f};f.planes[1]={0,1,0,0};f.planes[2]={0,0,1,0};f.planes[3]={0,0,1,0};
    for(unsigned i=4;i<7;++i)f.planes[i]={0,0,0,-1};f.planes[7]={1e-40f,1,0,.005f};
    Clear();f.DebugRenderCustomPlanes();Check(lines.empty(),"custom-plane clip flushes subnormal coefficient before signed-distance test");
    CgsGeometric::sbFrustumDebugClipCustomPlanesToHull=false;Clear();f.DebugRenderCustomPlanes();Check(!lines.empty(),"custom-plane bypass still publishes retained pair");
}
int main() {
    SceneWitnesses();ShadowWitnesses();RaceWitnesses();FrustumWitness();
    std::printf("DebugMenuDrawConsumers: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
