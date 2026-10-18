#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/rwcore_structs.h"
#include "SharedClasses/World/BrnCollisionTag.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "vendor/renderware/collision/Frustum.hpp"
#undef CGS_ASSERT
#define CGS_ASSERT(c,m) do {if(!(c)){std::printf("ASSERT %s\n",m);std::abort();}} while(0)

// Fixtures replace only renderer/registration/attribute/resource boundaries.
// Polygon records, math types, RGBA packing and all exercised bodies are production.
namespace CgsDev {
namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* expr,const char*,int){std::printf("ASSERT %s\n",expr);std::abort();}
void* EndAssert(){return nullptr;}
}
namespace DebugUI { struct StringList {s32 miValue;const char* mpcName;}; }
struct DebugComponent {
    std::vector<std::string> maNames;
    void Construct(){}
    template<class T>void RegisterVariable(T*,const char* n){maNames.push_back(n);}
    template<class T>void SetRange(T*,s32,s32){}
    void SetOptions(s32*,const DebugUI::StringList*){}
};
struct Debug3DImmediateRender {
    Matrix44 mMatrix{}; Vector3 mCamera{};
    int miSolidQuads=0,miWireQuads=0,miSolidTriangles=0,miWireTriangles=0,miBoxes=0,miArrows=0,miText=0;
    Vector3 maQuad[4]{};u32 muLastColour=0;std::string mLastText;
    Matrix44 GetViewProjectionMatrix()const{return mMatrix;}
    Vector3 GetCameraPosition()const{return mCamera;}
    void DrawSolidQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA col){++miSolidQuads;maQuad[0]=a;maQuad[1]=b;maQuad[2]=c;maQuad[3]=d;muLastColour=col.m_rgba;}
    void DrawQuad(Vector3,Vector3,Vector3,Vector3,rw::RGBA){++miWireQuads;}
    void DrawSolidTriangle(Vector3,Vector3,Vector3,rw::RGBA col){++miSolidTriangles;muLastColour=col.m_rgba;}
    void DrawHollowTriangle(Vector3,Vector3,Vector3,rw::RGBA){++miWireTriangles;}
    void DrawBox(Vector3,Vector3,rw::RGBA){++miBoxes;}
    void DrawArrow(Vector3,Vector3,rw::RGBA){++miArrows;}
    void DrawText(Vector3,const char* text,f32,rw::RGBA=rw::RGBA(255,255,255,255)){++miText;mLastText=text;}
};
}
namespace Attrib {
struct Collection {};
struct RefSpec {u64 mClassKey=0,mCollectionKey=0;void* mpCollection=nullptr;u64 GetCollectionKey()const{return mCollectionKey;}};
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
struct WorldEntityModule {Attrib::Gen::surfacelist mSurfaceList;void GetSurface(u8,Attrib::Gen::surface*)const;};
struct CollisionDebugComponent:CgsDev::DebugComponent {
    enum {E_DRAWMODE_SURFACES,E_DRAWMODE_FLAGS,E_DRAWMODE_ZONES,E_DRAWMODE_TRAFFIC,E_DRAWMODE_AISECTION,E_DRAWMODE_DRIVABLE,E_DRAWMODE_COUNT};
    CgsDev::DebugUI::StringList maDrawModeList[7];
    static const WorldEntityModule* spWorldEntityModule;static s32 siDrawMode,siSurfaceAlpha;static bool sbDrawTags,sbDrawNormals;
    void Construct(const WorldEntityModule*);void OnActivate();
    static u32 GetTrafficColour(f32);static u32 GetFlagsRenderColour(CollisionTag);static u32 GetSurfaceRenderColour(u8);static u32 GetPolyColour(CollisionTag,s32);
    static void DrawCollisionPolyCallback(CgsDev::Debug3DImmediateRender*,void*,const Vector3*,s32,Vector3,u32,s32);
};
}
namespace CgsSceneManager {
struct DrawCollisionPolyCallbackData {using DrawCollisionPolyCallback=void(*)(CgsDev::Debug3DImmediateRender*,void*,const Vector3*,s32,Vector3,u32,s32);DrawCollisionPolyCallback mpCallbackFunc;void* mpUserData;};
using DrawCollisionPolyCallback=DrawCollisionPolyCallbackData;
constexpr int KI_MAX_NUM_COLLISION_POLY_CALLBACKS=4;
struct Map {const CgsGeometric::PolygonSoupList* mpList=nullptr;int GetNumPolySoupLists()const{return mpList?1:0;}const CgsGeometric::PolygonSoupList* GetPolySoupList(s32)const{return mpList;}};
struct TriangleCollisionManager {Map mMap;const Map* GetPolySoupListSpacialMap()const{return &mMap;}};
struct TriangleCollisionDebugComponent {
    const TriangleCollisionManager* mpTriCollisionManager;rw::collision::Frustum mViewFrustum;
    bool mbDrawPolySoups=false,mbDrawSoupAabbs=false,mbUpdateViewFrustum=true;f32 mfSoupDrawDistance=27.5f;int miBuildCalls=0;
    static DrawCollisionPolyCallback siDrawPolyCallbacks[4];static s32 siNumDrawPolyCallbacks;
    static auto& GetDrawCollisionPolyCallback(s32 i){return siDrawPolyCallbacks[i];}
    static bool RegisterDrawCollisionPolyCallback(DrawCollisionPolyCallbackData::DrawCollisionPolyCallback,void*);
    static void DrawCollisionPoly(CgsDev::Debug3DImmediateRender*,const Vector3*,s32,Vector3,u32,s32);
    void UpdateViewFrustum(CgsDev::Debug3DImmediateRender*);bool IsPolyVisible(const Vector3*,s32,CgsDev::Debug3DImmediateRender*)const;
    void DrawPolySoupPoly(CgsDev::Debug3DImmediateRender*,const CgsGeometric::PolygonSoup*,s32,u8,bool);
    void DrawPolySoups(CgsDev::Debug3DImmediateRender*);void RenderWorld(CgsDev::Debug3DImmediateRender*);
    void BuildSpacialMap(){++miBuildCalls;}
};
}
#include "world_collision_visualizer.inc"
int main(){
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* n){++checks;if(!ok){++failures;std::printf("FAIL %s\n",n);}};
    using World=BrnWorld::CollisionDebugComponent;using Scene=CgsSceneManager::TriangleCollisionDebugComponent;
    BrnWorld::WorldEntityModule owner;World world;world.Construct(&owner);world.OnActivate();
    check(Scene::siNumDrawPolyCallbacks==1,"world construction registers the real polygon callback");
    check(world.maNames==std::vector<std::string>{"Draw mode","Draw normals","Surface alpha","Draw tags"},"original world draw-mode controls");
    check(world.maDrawModeList[5].miValue==5&&world.maDrawModeList[6].mpcName==nullptr,"six options and bounded native terminator");
    check(World::GetSurfaceRenderColour(1)==0x4bff3f7fu,"authored RGB truncation and alpha packing");
    owner.mSurfaceList.mRef.mCollectionKey=0x123456789abcdef0ull;
    Attrib::Gen::surface surface;owner.GetSurface(63,&surface);
    check(Attrib::suLastClassKey==0x68428A3C7836CF50ull&&Attrib::suLastCollectionKey==0x123456789abcdef0ull&&owner.mSurfaceList.muIndex==63,"surface lookup preserves both 64-bit keys and the surface index");
    owner.mSurfaceList.mbMissing=true;owner.GetSurface(2,&surface);
    check(Attrib::siDefaults==1&&Attrib::suLastCollectionKey==0,"missing surface reference uses the original default area");
    owner.mSurfaceList.mbMissing=false;
    CgsDev::Debug3DImmediateRender decodeRenderer;
    const Vector3 decodePoints[]={{0,0,1,0},{1,0,1,0},{0,1,1,0}};
    World::sbDrawTags=true;
    World::DrawCollisionPolyCallback(&decodeRenderer,nullptr,decodePoints,3,Vector3{0,0,1,0},0x00100003,0);
    check(owner.mSurfaceList.muIndex==1&&decodeRenderer.muLastColour==0x4bff3f7fu&&decodeRenderer.mLastText=="( 3)","raw polygon tag has material in the high half and group in the low half");
    World::sbDrawTags=false;
    BrnWorld::CollisionTag tag;tag.Construct(3,0x2000);
    check(World::GetFlagsRenderColour(tag)==0x80ffffffu,"driveable flag colour");
    tag.Construct(3,0x4000);check(World::GetFlagsRenderColour(tag)==0x800000ffu,"fatal flag colour");
    tag.Construct(3,0x5000);check(World::GetFlagsRenderColour(tag)==0xff00ffffu,"superfatal flag priority");
    World::siDrawMode=World::E_DRAWMODE_ZONES;check(World::GetPolyColour(tag,1)==0x2000ffffu,"original zone palette");
    CgsGeometric::PolygonSoupPoly polys[2]={{0x00100003,{0,1,2,3},{}},{0x00100003,{0,1,2,255},{}}};
    u16 vertices[4][3]={{0,0,0},{4,0,0},{0,4,0},{4,4,0}};
    CgsGeometric::PolygonSoup soup{};soup.miPosX=-2;soup.miPosY=-2;soup.miPosZ=5;soup.mfScale=1;
    soup.mpPolygons=reinterpret_cast<u8*>(polys);soup.mpVertices=reinterpret_cast<u8*>(vertices);soup.mu8NumPolygons=2;soup.mu8NumQuads=1;soup.mu8NumVertices=4;
    auto v=soup.GetVertexPosition(3);check(v.x==2&&v.y==2&&v.z==5&&v.w==0,"unsigned packed vertex plus signed origin and scale");
    const CgsGeometric::PolygonSoup* soups[]={&soup};CgsGeometric::AxisAlignedBox4 boxes{};
    boxes.mafMinX[0]=boxes.mafMinY[0]=-2;boxes.mafMinZ[0]=5;boxes.mafMaxX[0]=boxes.mafMaxY[0]=2;boxes.mafMaxZ[0]=5.1f;
    CgsGeometric::PolygonSoupList list{};for(int i=0;i<3;++i){list.mOverallAabb[i]=-100;list.mOverallAabb[i+4]=100;}
    list.mpapPolySoups=reinterpret_cast<uintptr_t>(soups);list.mpaPolySoupBoxes=reinterpret_cast<uintptr_t>(&boxes);list.miNumPolySoups=1;
    CgsSceneManager::TriangleCollisionManager manager;manager.mMap.mpList=&list;
    Scene scene;scene.mpTriCollisionManager=&manager;CgsDev::Debug3DImmediateRender renderer;renderer.mMatrix.SetIdentity();
    scene.RenderWorld(&renderer);check(renderer.miSolidTriangles==0&&renderer.miSolidQuads==0,"drawing is off by default");
    scene.mbDrawPolySoups=true;scene.mbDrawSoupAabbs=true;scene.RenderWorld(&renderer);
    check(renderer.miSolidQuads==1&&renderer.miSolidTriangles==1,"toggle traverses live soups and draws quads and triangles");
    check(renderer.miWireQuads==1&&renderer.miWireTriangles==1&&renderer.miBoxes==1,"wire outlines and optional soup bounds");
    check(renderer.maQuad[2].x==2&&renderer.maQuad[2].y==2&&renderer.maQuad[3].x==-2,"quad strip becomes perimeter order");
    check(std::fabs(renderer.maQuad[0].z-5.05f)<.00001f,"normal offsets collision geometry to avoid z fighting");
    check(renderer.muLastColour==0x208080ffu,"world callback supplies the authored zone colour");
    scene.mfSoupDrawDistance=.5f;scene.RenderWorld(&renderer);check(renderer.miSolidQuads==1,"distance control culls geometry");
    scene.mfSoupDrawDistance=27.5f;World::siSurfaceAlpha=0;scene.RenderWorld(&renderer);
    check(renderer.miSolidQuads==1&&renderer.miWireQuads==2,"zero alpha keeps the wireframe");
    World::sbDrawNormals=World::sbDrawTags=true;scene.RenderWorld(&renderer);
    check(renderer.miArrows==2&&renderer.miText==2,"normal and nearby tag controls have consumers");
    renderer.mCamera={200,0,0,0};scene.RenderWorld(&renderer);check(renderer.miWireQuads==3,"lists outside the camera are skipped");
    const auto& lrNearPlane=scene.mViewFrustum.GetPlane(0);check(lrNearPlane.x==0&&lrNearPlane.y==0&&lrNearPlane.z==1,"D3D near plane uses the Z column");
    check(scene.miBuildCalls==6,"spatial update remains in each active render call");
    std::printf("WorldCollisionVisualizer: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
