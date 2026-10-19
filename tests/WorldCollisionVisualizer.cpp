#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/rwcore_structs.h"
#include "SharedClasses/World/BrnCollisionTag.h"
#include "GameSource/World/DebugComponents/BrnCollisionDebugComponent.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "SDKs/XboxMath/XMVectorACos.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"
#undef CGS_ASSERT
#define CGS_ASSERT(c,m) do {if(!(c)){std::printf("ASSERT %s\n",m);std::abort();}} while(0)

// The actual merged component, callback-data, renderer, soup and math declarations
// are included above. Only UI registration, drawing, profiling, resource lookup,
// spatial rebuild and the three default-off query boundaries are doubled here.
namespace Fixture {
std::vector<std::string> maRegisteredNames;
std::map<const void*,std::pair<s32,s32>> maRanges;
const CgsDev::DebugUI::StringList* spOptions=nullptr;
const CgsGeometric::PolygonSoupList* spSoupList=nullptr;
s32 miBuildCalls=0,miQueryCalls=0;
[[noreturn]] void UnexpectedBoundary(const char* name) {
    std::printf("Unexpected drawing-fixture boundary: %s\n",name);
    std::abort();
}
struct RecordingRenderer:CgsDev::Debug3DImmediateRender {
    Matrix44 mMatrix{}; Vector3 mCamera{};
    int miSolidQuads=0,miWireQuads=0,miSolidTriangles=0,miWireTriangles=0,miBoxes=0,miArrows=0,miText=0,miLines=0;
    Vector3 maQuad[4]{};u32 muLastColour=0;std::string mLastText;
};
RecordingRenderer& Record(CgsDev::Debug3DImmediateRender* renderer) {
    return *static_cast<RecordingRenderer*>(renderer);
}
}
namespace CgsDev {
namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* expr,const char*,int){std::printf("ASSERT %s\n",expr);std::abort();}
void* EndAssert(){return nullptr;}
}
DebugComponent::DebugComponent():mbActive(false),mpDebugLinkedListNext(nullptr){}
void DebugComponent::Construct(){mbActive=false;mpDebugLinkedListNext=nullptr;}
void DebugComponent::Update(){}
void DebugComponent::RenderWorld(Debug3DImmediateRender*){}
void DebugComponent::RenderHUD(Debug2DImmediateRender*){}
const char* DebugComponent::GetName()const{return "";}
const char* DebugComponent::GetPath()const{return "";}
bool DebugComponent::IsSimple()const{return false;}
void DebugComponent::OnActivate(){}
void DebugComponent::OnRegister(){}
void DebugComponent::RegisterVariable(s32*,const char* name){Fixture::maRegisteredNames.push_back(name);}
void DebugComponent::RegisterVariable(bool*,const char* name){Fixture::maRegisteredNames.push_back(name);}
void DebugComponent::SetRange(s32* variable,s32 low,s32 high){Fixture::maRanges[variable]={low,high};}
void DebugComponent::SetOptions(s32*,const DebugUI::StringList* options){Fixture::spOptions=options;}
namespace PerfMonCpu {
s32 AddMonitor(const char*,PerfMonCpuPage,bool,f32,bool){return 0;}
}
Vector3 Debug3DImmediateRender::GetCameraPosition()const{return Fixture::Record(const_cast<Debug3DImmediateRender*>(this)).mCamera;}
const Matrix44 Debug3DImmediateRender::GetViewProjectionMatrix()const{return Fixture::Record(const_cast<Debug3DImmediateRender*>(this)).mMatrix;}
void Debug3DImmediateRender::DrawSolidQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA col){
    auto& record=Fixture::Record(this);++record.miSolidQuads;
    record.maQuad[0]=a;record.maQuad[1]=b;record.maQuad[2]=c;record.maQuad[3]=d;record.muLastColour=col.m_rgba;
}
void Debug3DImmediateRender::DrawQuad(Vector3,Vector3,Vector3,Vector3,rw::RGBA){++Fixture::Record(this).miWireQuads;}
void Debug3DImmediateRender::DrawSolidTriangle(Vector3,Vector3,Vector3,rw::RGBA col){auto& record=Fixture::Record(this);++record.miSolidTriangles;record.muLastColour=col.m_rgba;}
void Debug3DImmediateRender::DrawHollowTriangle(Vector3,Vector3,Vector3,rw::RGBA){++Fixture::Record(this).miWireTriangles;}
void Debug3DImmediateRender::DrawBox(Vector3,Vector3,rw::RGBA){++Fixture::Record(this).miBoxes;}
void Debug3DImmediateRender::DrawArrow(Vector3,Vector3,rw::RGBA){++Fixture::Record(this).miArrows;}
void Debug3DImmediateRender::DrawLine(Vector3,Vector3,rw::RGBA){++Fixture::Record(this).miLines;}
void Debug3DImmediateRender::DrawText(Vector3,const char* text,f32,rw::RGBA){auto& record=Fixture::Record(this);++record.miText;record.mLastText=text;}
}
namespace Attrib {
struct Collection {};
struct RefSpec {u64 mClassKey=0,mCollectionKey=0;void* mpCollection=nullptr;u64 GetCollectionKey()const{return mCollectionKey;}};
static_assert(sizeof(RefSpec)==0x18,"original surface reference default spans 24 bytes");
static u64 suLastClassKey=0,suLastCollectionKey=0;static int siDefaults=0;static RefSpec sDefault;
void* DefaultDataArea(std::size_t size){CGS_ASSERT(size==sizeof(RefSpec),"reference default size");++siDefaults;return &sDefault;}
Collection* FindCollectionWithDefault(u64 cls,u64 key){suLastClassKey=cls;suLastCollectionKey=key;return nullptr;}
namespace Gen {
struct surface {
    f32 maColour[4]={};const f32(&DebugRenderColor()const)[4]{return maColour;}
    void Change(Collection*){maColour[0]=.5f;maColour[1]=.25f;maColour[2]=1.f;}
};
struct surfacelist {RefSpec mRef{};bool mbMissing=false;mutable u32 muIndex=0;void* Surfaces(u32 i)const{muIndex=i;return mbMissing?nullptr:const_cast<RefSpec*>(&mRef);}};
}
}
namespace BrnWorld {
// Attribute/resource boundary: GetSurface itself is the complete production body.
class WorldEntityModule {
public:
    Attrib::Gen::surfacelist mSurfaceList;
    void GetSurface(u8,Attrib::Gen::surface*)const;
};
}
namespace CgsGeometric {
const PolygonSoupList* PolygonSoupListSpatialMap::GetPolySoupList(s32 index)const {
    CGS_ASSERT(index==0&&Fixture::spSoupList!=nullptr,"fixture soup resource lookup");
    return Fixture::spSoupList;
}
}
namespace CgsSceneManager {
const CgsGeometric::PolygonSoupListSpatialMap* TriangleCollisionManager::GetPolySoupListSpacialMap()const{return &mPolySoupListSpacialMap;}
void TriangleCollisionDebugComponent::BuildSpacialMap(){++Fixture::miBuildCalls;}
void TriangleCollisionDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*){Fixture::UnexpectedBoundary("triangle HUD");}
void TriangleCollisionDebugComponent::OnActivate(){Fixture::UnexpectedBoundary("triangle menu activation");}
void TriangleCollisionDebugComponent::TestPolySoupSphereCollision(CgsDev::Debug3DImmediateRender*){++Fixture::miQueryCalls;Fixture::UnexpectedBoundary("sphere query");}
void TriangleCollisionDebugComponent::TestPolySoupBoxCollision(CgsDev::Debug3DImmediateRender*){++Fixture::miQueryCalls;Fixture::UnexpectedBoundary("box query");}
void TriangleCollisionDebugComponent::TestPolySoupSweptSphereCollision(CgsDev::Debug3DImmediateRender*,CgsDev::Debug2DImmediateRender*){++Fixture::miQueryCalls;Fixture::UnexpectedBoundary("swept-sphere query");}
}
#include "world_collision_visualizer.inc"
int main(){
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* n){++checks;if(!ok){++failures;std::printf("FAIL %s\n",n);}};
    using World=BrnWorld::CollisionDebugComponent;using Scene=CgsSceneManager::TriangleCollisionDebugComponent;
    BrnWorld::WorldEntityModule owner;World world;world.Construct(&owner);world.OnActivate();
    check(Scene::siNumDrawPolyCallbacks==1,"world construction registers the real polygon callback");
    check(Fixture::maRegisteredNames==std::vector<std::string>{"Draw mode","Draw normals","Surface alpha","Draw tags"},"original world draw-mode controls");
    check(sizeof(world.maDrawModeList)/sizeof(world.maDrawModeList[0])==6&&world.maDrawModeList[5].miValue==5&&Fixture::spOptions==world.maDrawModeList&&Fixture::maRanges[&BrnWorld::siDrawMode]==std::make_pair(0,5),"six authored options and bounded native range");
    check(World::GetSurfaceRenderColour(1)==0x4bff3f7fu,"authored RGB truncation and alpha packing");
    owner.mSurfaceList.mRef.mCollectionKey=0x123456789abcdef0ull;
    Attrib::Gen::surface surface;owner.GetSurface(63,&surface);
    check(Attrib::suLastClassKey==0x68428A3C7836CF50ull&&Attrib::suLastCollectionKey==0x123456789abcdef0ull&&owner.mSurfaceList.muIndex==63,"surface lookup preserves both 64-bit keys and the surface index");
    owner.mSurfaceList.mbMissing=true;owner.GetSurface(2,&surface);
    check(Attrib::siDefaults==1&&Attrib::suLastCollectionKey==0,"missing surface reference uses the original default area");
    owner.mSurfaceList.mbMissing=false;
    Fixture::RecordingRenderer decodeRenderer;
    const Vector3 decodePoints[4]={{0,0,1,0},{1,0,1,0},{0,1,1,0},{0,0,0,0}};
    BrnWorld::sbDrawTags=true;
    World::DrawCollisionPolyCallback(&decodeRenderer,nullptr,decodePoints,3,Vector3{0,0,1,0},0x00100003,0);
    check(owner.mSurfaceList.muIndex==1&&decodeRenderer.muLastColour==0x4bff3f7fu&&decodeRenderer.mLastText=="( 3)","raw polygon tag has material in the high half and group in the low half");
    BrnWorld::sbDrawTags=false;
    BrnWorld::CollisionTag tag;tag.Construct(3,0x2000);
    check(World::GetFlagsRenderColour(tag)==0x80ffffffu,"driveable flag colour");
    tag.Construct(3,0x4000);check(World::GetFlagsRenderColour(tag)==0x800000ffu,"fatal flag colour");
    tag.Construct(3,0x5000);check(World::GetFlagsRenderColour(tag)==0xff00ffffu,"superfatal flag priority");
    BrnWorld::siDrawMode=World::E_DRAWMODE_ZONES;check(World::GetPolyColour(tag,1)==0x2000ffffu,"original zone palette");
    CgsGeometric::PolygonSoupPoly polys[2]={{0x00100003,{0,1,2,3},{}},{0x00100003,{0,1,2,255},{}}};
    u16 vertices[4][3]={{0,0,0},{4,0,0},{0,4,0},{4,4,0}};
    CgsGeometric::PolygonSoup soup{};soup.miPosX=-2;soup.miPosY=-2;soup.miPosZ=5;soup.mfScale=1;
    soup.mpPolygons=reinterpret_cast<u8*>(polys);soup.mpVertices=reinterpret_cast<u8*>(vertices);soup.mu8NumPolygons=2;soup.mu8NumQuads=1;soup.mu8NumVertices=4;
    auto v=soup.GetVertexPosition(3);check(v.x==2&&v.y==2&&v.z==5&&v.w==0,"unsigned packed vertex plus signed origin and scale");
    const CgsGeometric::PolygonSoup* soups[]={&soup};CgsGeometric::AxisAlignedBox4 boxes{};
    boxes.mafMinX[0]=boxes.mafMinY[0]=-2;boxes.mafMinZ[0]=5;boxes.mafMaxX[0]=boxes.mafMaxY[0]=2;boxes.mafMaxZ[0]=5.1f;
    CgsGeometric::PolygonSoupList list{};for(int i=0;i<3;++i){list.mOverallAabb[i]=-100;list.mOverallAabb[i+4]=100;}
    list.mpapPolySoups=reinterpret_cast<uintptr_t>(soups);list.mpaPolySoupBoxes=reinterpret_cast<uintptr_t>(&boxes);list.miNumPolySoups=1;
    CgsSceneManager::TriangleCollisionManager manager;manager.mPolySoupListSpacialMap.miNumSoupLists=1;Fixture::spSoupList=&list;
    Scene scene;scene.Construct(&manager);Fixture::RecordingRenderer renderer;renderer.mMatrix.SetIdentity();
    scene.RenderWorld(&renderer);check(renderer.miSolidTriangles==0&&renderer.miSolidQuads==0,"drawing is off by default");
    scene.mbDrawPolySoups=true;scene.mbDrawSoupAabbs=true;scene.RenderWorld(&renderer);
    check(renderer.miSolidQuads==1&&renderer.miSolidTriangles==1,"toggle traverses live soups and draws quads and triangles");
    check(renderer.miWireQuads==1&&renderer.miWireTriangles==1&&renderer.miBoxes==1,"wire outlines and optional soup bounds");
    check(renderer.maQuad[2].x==2&&renderer.maQuad[2].y==2&&renderer.maQuad[3].x==-2,"quad strip becomes perimeter order");
    check(std::fabs(renderer.maQuad[0].z-5.05f)<.00001f,"normal offsets collision geometry to avoid z fighting");
    check(renderer.muLastColour==0x208080ffu,"world callback supplies the authored zone colour");
    scene.mfSoupDrawDistance=.5f;scene.RenderWorld(&renderer);check(renderer.miSolidQuads==1,"distance control culls geometry");
    scene.mfSoupDrawDistance=27.5f;BrnWorld::siSurfaceAlpha=0;scene.RenderWorld(&renderer);
    check(renderer.miSolidQuads==1&&renderer.miWireQuads==2,"zero alpha keeps the wireframe");
    BrnWorld::sbDrawNormals=BrnWorld::sbDrawTags=true;scene.RenderWorld(&renderer);
    check(renderer.miArrows==2&&renderer.miText==2,"normal and nearby tag controls have consumers");
    renderer.mCamera={200,0,0,0};scene.RenderWorld(&renderer);check(renderer.miWireQuads==3,"lists outside the camera are skipped");
    const auto& lrNearPlane=scene.mViewFrustum.GetPlane(0);check(lrNearPlane.x==0&&lrNearPlane.y==0&&lrNearPlane.z==1,"D3D near plane uses the Z column");
    check(Fixture::miBuildCalls==6,"spatial update remains in each active render call");
    // One point lies behind repeated plane 0. Each original pass sees one outside
    // point; incorrectly accumulating it would reject on the third pass (1,2,3).
    Fixture::RecordingRenderer visibilityRenderer;
    const Vector3 mixedPoints[3]={{0,0,-1,0},{1,0,1,0},{0,1,1,0}};
    scene.mViewFrustum.SetPlane(0,rw::collision::Plane(Vector4{0,0,1,0}));
#if WORLD_COLLISION_VISIBILITY_COMPARISON
    CgsSceneManager::VisibilityComparison comparison;
    comparison.mViewFrustum=scene.mViewFrustum;
    check(comparison.IsPolyVisible(mixedPoints,3,&visibilityRenderer),
          "outside counter resets on each repeated plane pass");
#else
    check(scene.IsPolyVisible(mixedPoints,3,&visibilityRenderer),
          "outside counter resets on each repeated plane pass");
#endif
    check(Fixture::miQueryCalls==0,"drawing leaves all default-off query boundaries untouched");
    std::printf("WorldCollisionVisualizer: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
