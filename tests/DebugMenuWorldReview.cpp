// Complete production definitions are injected by the runner. These typed
// boundary spies expose state and draw calls, rather than native object layouts.
#include "BrnCommonTypes.h"
#include "rw/rwcore_structs.h"
#include "rw/math/vpu/vector3_operation.h"
#include "pc/math/VmxFloat.h"
#include "debug_menu_world_review_config.inc"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static unsigned checks=0, failures=0;
static void Check(bool ok,const char* name) {++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
static bool Near(f32 a,f32 b) {return std::fabs(a-b)<1e-4f;}
struct Assertion {std::string expression,file;unsigned line;};
static std::vector<Assertion> assertions;
namespace CgsDev {
using RGBA=u32;
namespace Assert {
int BeginAssert() {return 1;}
void* EndAssert() {return nullptr;}
int FireAssert(const char* text,const char* file,int line) {assertions.push_back({text,file,static_cast<unsigned>(line)});return 0;}
}
struct SimpleStrStream {
    std::string text;
    SimpleStrStream& operator<<(const char* s) {text+=s;return *this;}
    SimpleStrStream& operator<<(s32 n) {text+=std::to_string(n);return *this;}
    const char* GetBuffer() const {return text.c_str();}
};
struct Polygon {char kind;std::vector<Vector2> points;u32 colour;};
struct Circle {Vector2 point;f32 radius;s32 segments;u32 colour;};
struct Debug2DImmediateRender {
    Vector2 screen{1000,500,123,456};
    std::vector<Polygon> polygons;std::vector<Circle> circles;std::string order;
    Vector2 GetVirtualScreenSize() const {return screen;}
    void DrawSolidConvexPolygon(const Vector2* p,u32 n,u32 c) {polygons.push_back({'S',{p,p+n},c});order+='S';}
    void DrawWirePolygon(const Vector2* p,u32 n,u32 c) {polygons.push_back({'W',{p,p+n},c});order+='W';}
    void DrawCircle(Vector2 p,f32 r,s32 n,u32 c) {circles.push_back({p,r,n,c});order+='C';}
};
struct Quad {bool solid;Vector3 points[4];u32 colour;};
struct Debug3DImmediateRender {
    std::vector<Quad> quads;std::vector<std::string> labels;
    void DrawSolidQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA colour) {quads.push_back({true,{a,b,c,d},colour.m_rgba});}
    void DrawQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA colour) {quads.push_back({false,{a,b,c,d},colour.m_rgba});}
    void DrawSolidTriangle(Vector3,Vector3,Vector3,rw::RGBA) {}
    void DrawHollowTriangle(Vector3,Vector3,Vector3,rw::RGBA) {}
    void DrawArrow(Vector3,Vector3,rw::RGBA) {}
    Vector3 GetCameraPosition() const {return {0,0,0,0};}
    void DrawText(Vector3,const char* text,f32) {labels.emplace_back(text);}
};
struct DebugComponent {
    bool mbActive=true;void* mpNext=reinterpret_cast<void*>(1234);
    void Construct() {mbActive=false;mpNext=nullptr;}
    void Destruct() {mbActive=false;}
};
}
#ifdef CGS_ASSERT
#undef CGS_ASSERT
#endif
#define CGS_ASSERT(condition,message) do {if(!(condition)) {CgsDev::Assert::BeginAssert();CgsDev::Assert::FireAssert(message,__FILE__,__LINE__);CgsDev::Assert::EndAssert();}} while(0)

namespace BrnPhysics {namespace Props {
const int KI_WORLD_X_HALF_EXTENT=5000,KI_WORLD_X_EXTENSION=3000,KI_WORLD_Z_HALF_EXTENT=5000,KI_CELL_SIZE=100;
struct PropCellId {u16 muX=0,muZ=0;u16 GetX() const{return muX;}u16 GetZ() const{return muZ;}};
struct PropZoneData {
#if CELL_ID_INSTANCE
    PropCellId GetCellId(f32,f32) const;
#else
    static PropCellId GetCellId(f32,f32);
#endif
};
}}
namespace Attrib {namespace Gen {
struct surface {f32 colour[4]{1,.5f,0,1};const f32 (&DebugRenderColor() const)[4] {return colour;}};
}}
namespace BrnWorld {
const u32 KU_NUM_ZONE_SLOTS=9;const u8 KU_ADDED_TO_CONTACT_GEN_BIT=4;
struct PropCellRecord {
    BrnPhysics::Props::PropCellId id;bool active=false;
    BrnPhysics::Props::PropCellId GetId() const{return id;}bool IsActive() const{return active;}
};
struct PropCellManager {PropCellRecord maCells[150];s32 miNumLoadedCells=0;};
struct UsedSlots {
    bool bits[9]{};
    s32 GetFirstNonZeroBit() const {return GetNextNonZeroBit(-1);}
    s32 GetNextNonZeroBit(s32 n) const {for(++n;n<9;++n)if(bits[n])return n;return -1;}
};
struct PropEntityInstance {
    Matrix44Affine transform{};u8 mu8Flags=0;
    const Matrix44Affine& GetWorldTransform() const {return transform;}
};
struct PropZoneManager {PropCellManager mCellManager;PropEntityInstance maProps[5418];UsedSlots maUsedProps;};
struct PropEntityModule {PropZoneManager mZoneManager;Vector3 mPlayerPosition{};};
struct PropEntityDebugComponent {
    PropEntityModule* mpPropEntityModule=nullptr;f32 mfCellGridZoom=1,mfAspect=-1;
    Vector2 mMapDimensions{},mScreenSize{};
    void RenderCellGrid(CgsDev::Debug2DImmediateRender*);
    Vector2 ToCellGridScreenCoords(Vector3);
};
struct RaceCarEntityModule {};
struct BoostBurnout5 {
    bool mbAddRemoveChunkMode=false;int miBoostLevel=1,miChainSize=77,calls=0;bool last=true;
    void UpdateMaxBoost(bool b){++calls;last=b;}
};
using CgsDev::DebugComponent;
struct BoostDebugComponent:DebugComponent {
    RaceCarEntityModule* mpRaceCarEntityModule=nullptr;int miBoostRules=-1,miActionTextModifiedTime=-1;
    void Construct(RaceCarEntityModule*);void Destruct();static void OnAddRemoveChunkMode(void*,void*);
};
enum TrafficDirection {E_TRAFFIC_DIRECTION_VALID,E_TRAFFIC_DIRECTION_UNKNOWN,E_TRAFFIC_DIRECTION_NO_LANES};
const u16 KU_COLLISION_FLAG_DRIVEABLE=8192,KU_COLLISION_FLAG_FATAL=16384,KU_COLLISION_FLAG_SUPERFATAL=4096;
const u16 KU_COLLISION_MASK_SURFACE_ID=1008,KI_INVALID_SECTION_INDEX=32767;
struct CollisionTag {
    u16 mu16GroupTag=0,mu16MaterialTag=0;
    void Construct(u16 g,u16 m){mu16GroupTag=g;mu16MaterialTag=m;}
    TrafficDirection GetTrafficInfo(f32* f) const {*f=0;return E_TRAFFIC_DIRECTION_UNKNOWN;}
};
struct WorldEntityModule {
    void GetSurface(u8,Attrib::Gen::surface* out) const {*out=Attrib::Gen::surface();}
};
static const WorldEntityModule world;
static const WorldEntityModule* spWorldEntityModule=&world;
static int siSurfaceAlpha=75,siDrawMode=1;static bool sbDrawNormals=false,sbDrawTags=false;
static const u32 K_ZONE_COLOURS[16]{};
struct CollisionDebugComponent {
    enum {E_DRAWMODE_SURFACES,E_DRAWMODE_FLAGS,E_DRAWMODE_ZONES,E_DRAWMODE_TRAFFIC,E_DRAWMODE_AISECTION,E_DRAWMODE_DRIVABLE,E_DRAWMODE_COUNT};
    static u32 GetTrafficColour(f32);static u32 GetSurfaceRenderColour(u8);
    static u32 GetFlagsRenderColour(CollisionTag);static u32 GetPolyColour(CollisionTag,s32);
    static void DrawCollisionPolyCallback(CgsDev::Debug3DImmediateRender*,void*,const Vector3*,s32,Vector3,u32,s32);
};
}
#include "debug_menu_world_review.inc"

static void Boost() {
    using namespace BrnWorld;
    BoostBurnout5 b;BoostDebugComponent::OnAddRemoveChunkMode(nullptr,&b);
    Check(b.miBoostLevel==4 && b.miChainSize==77 && b.calls==1 && !b.last,"F01 chunk disable updates boost level, retaining chain size");
    b.mbAddRemoveChunkMode=true;BoostDebugComponent::OnAddRemoveChunkMode(nullptr,&b);
    Check(b.calls==1,"chunk enabled leaves boost untouched");
    RaceCarEntityModule m;BoostDebugComponent d;d.Construct(&m);
    Check(d.mbActive && d.mpNext==reinterpret_cast<void*>(1234) && d.mpRaceCarEntityModule==&m && d.miBoostRules==1 && d.miActionTextModifiedTime==0,"F02 Construct preserves untouched base state");
    d.Destruct();Check(d.mpRaceCarEntityModule==nullptr && d.mbActive,"F02 Destruct clears owner while preserving active state");
    assertions.clear();d.Destruct();Check(assertions.size()==1 && assertions[0].line==68 && assertions[0].expression=="mpRaceCarEntityModule != NULL","F02 null-owner destruction keeps original assertion");
}
static void Cells() {
    using namespace BrnWorld;
    static PropEntityModule m;PropEntityDebugComponent d;d.mpPropEntityModule=&m;m.mPlayerPosition={25,10,-30,0};
    m.mZoneManager.mCellManager.miNumLoadedCells=2;
    m.mZoneManager.mCellManager.maCells[0]={{50,49},true};
    m.mZoneManager.mCellManager.maCells[1]={{51,50},false};
    m.mZoneManager.maUsedProps.bits[2]=m.mZoneManager.maUsedProps.bits[8]=true;
    m.mZoneManager.maProps[2].transform.Pos()={125,0,70,0};m.mZoneManager.maProps[2].mu8Flags=4;
    m.mZoneManager.maProps[8].transform.Pos()={-75,0,-130,0};
    m.mZoneManager.maProps[1204].transform.Pos()={9999,0,9999,0};
    CgsDev::Debug2DImmediateRender render;assertions.clear();d.RenderCellGrid(&render);
    Check(render.order=="SWCCC","F13 loaded cells precede two used-slot markers and player");
    Check(render.polygons.size()==2 && render.polygons[0].colour==0x3200FF00 && render.polygons[1].colour==0xFFFFFFFF,"F13 active solid and inactive wire colours");
    if(render.polygons.size()==2) {
        const auto& p=render.polygons[0].points;
        Check(p.size()==4 && Near(p[0].x,475) && Near(p[0].y,215) && Near(p[1].x,575) && Near(p[1].y,215) && Near(p[2].x,575) && Near(p[2].y,265) && Near(p[3].x,475) && Near(p[3].y,265),"F13 original cell corner order and 100-unit extent");
    } else Check(false,"F13 original cell corner order and 100-unit extent");
    Check(render.circles.size()==3,"F13 sparse used slots generate only occupied markers");
    if(render.circles.size()==3) {
        const auto& a=render.circles[0];const auto& b=render.circles[1];
        Check(Near(a.point.x,600) && Near(a.point.y,300) && a.radius==2 && a.segments==3 && a.colour==0xFF0000FF,"F13 index2 uses prop2, contact colour and three segments");
        Check(Near(b.point.x,400) && Near(b.point.y,200) && b.radius==2 && b.colour==0xFFFFFFFF,"F13 final used slot8 retains white marker");
    } else {Check(false,"F13 occupied prop2 marker");Check(false,"F13 occupied prop8 marker");}
    const bool playerCorrect=!render.circles.empty() && Near(render.circles.back().point.x,500) && Near(render.circles.back().point.y,250) && render.circles.back().radius==5 && render.circles.back().segments==5;
    Check(playerCorrect,"F14 final player marker projects actual position, not cell origin");
    Check(d.mMapDimensions.x==1000 && d.mMapDimensions.y==1000 && d.mMapDimensions.z==0 && d.mMapDimensions.w==0 && d.mScreenSize.z==0 && d.mScreenSize.w==0 && d.mfAspect==2,"F13 complete map/screen cache setup");
    Check(assertions.empty(),"valid player position has no cell range assertion");
    const Vector2 projected=d.ToCellGridScreenCoords({35,20,70,5});
    Check(Near(projected.x,510) && Near(projected.y,300),"F13 projector selects player-relative XZ lanes");
    Check(projected.z==10 && projected.w==10,"F13 raw permutation retains relative X in unused lanes");
    d.mMapDimensions.x=0;const Vector2 zero=d.ToCellGridScreenCoords({35,20,70,5});
    Check(std::isnan(zero.x),"F13 zero map axis retains unconditional reciprocal result");
    render.screen.y=0;d.RenderCellGrid(&render);Check(std::isnan(d.mfAspect),"F13 zero screen height retains unconditional aspect calculation");
    m.mZoneManager.mCellManager.miNumLoadedCells=0;m.mZoneManager.maUsedProps=UsedSlots{};
    CgsDev::Debug2DImmediateRender empty;d.RenderCellGrid(&empty);
    Check(empty.order=="C" && empty.circles.size()==1,"empty cells and slots still draw player marker");
    m.mPlayerPosition.x=-5001;assertions.clear();CgsDev::Debug2DImmediateRender invalid;d.RenderCellGrid(&invalid);
    Check(assertions.size()==1 && assertions[0].line==515,"F13 unused GetCellId call preserves range assertion side effect");
    BrnPhysics::Props::PropZoneData zone;assertions.clear();const auto id=zone.GetCellId(35000,0);
    Check(id.muX==400 && id.muZ==50,"cell helper takes unsigned low16 before division after assertion");
    Check(assertions.size()==1 && assertions[0].line==516 && assertions[0].file=="..\\..\\..\\SharedClasses\\Physics/Props/BrnPhysicsPropZoneData.h","cell helper retains exact non-gating assertion location");
    assertions.clear();Check(zone.GetCellId(-5001,0).muX==655,"negative biased coordinate wraps low16 before division");
}
static void Collision() {
    using namespace BrnWorld;
    Check(CollisionDebugComponent::GetSurfaceRenderColour(1)==0x4B007FFF,"F09 surface channels pack red into low byte");
    BrnWorld::CollisionTag t;t.Construct(0,0x1000);Check(CollisionDebugComponent::GetFlagsRenderColour(t)==0x80000000,"F12 non-drive non-fatal precedes superfatal");
    t.mu16MaterialTag=KU_COLLISION_FLAG_FATAL|KU_COLLISION_FLAG_SUPERFATAL;
    Check(CollisionDebugComponent::GetFlagsRenderColour(t)==0xFF00FFFF,"fatal superfatal retains yellow");
    Vector3 points[4]={{0,0,0,0},{2,0,0,0},{0,2,0,0},{2,2,0,0}};
    CgsDev::Debug3DImmediateRender r;sbDrawTags=true;CollisionDebugComponent::DrawCollisionPolyCallback(&r,nullptr,points,4,{0,0,1,0},(0x1000u<<16)|0x8123u,7);
    Check(r.quads.size()==2,"F10 quad retains solid and wire passes");
    if(r.quads.size()==2) {
        const auto& q=r.quads[0];Check(q.points[0].x==0 && q.points[1].x==2 && q.points[2].x==2 && q.points[3].x==0 && q.points[2].y==2 && Near(q.points[0].z,.05f),"F10 both quad passes use p0 p1 p3 p2 and normal offset");
        Check(r.quads[1].points[2].x==2 && r.quads[1].points[3].x==0 && r.quads[1].colour==0xFFFFFFFF,"F10 wire shares vertex order and white colour");
    } else {Check(false,"F10 solid vertex order");Check(false,"F10 wire vertex order");}
    Check(r.labels==std::vector<std::string>({"( 291)"}),"F11 label uses masked group tag, not polygon index");
    siSurfaceAlpha=0;CgsDev::Debug3DImmediateRender wire;CollisionDebugComponent::DrawCollisionPolyCallback(&wire,nullptr,points,4,{0,0,1,0},0,7);
    Check(wire.quads.size()==1 && !wire.quads[0].solid,"zero alpha retains wire-only quad pass");
}
static void Scale() {
    Matrix44Affine m;m.SetIdentity();Check(Near(GetMaximumScale(m),std::sqrt(3.0f)),"F17 identity component maximum has length sqrt3");
    m.Right()={-2,0,0,0};m.Up()={0,-3,0,0};m.At()={0,0,-4,0};Check(GetMaximumScale(m)==0,"F17 negative axes select componentwise zero");
    m.Right()={2,-1,.5f,0};m.Up()={1,3,-4,0};m.At()={-2,2,4,0};Check(Near(GetMaximumScale(m),std::sqrt(29.0f)),"F17 shear uses max vector, not maximum axis length");
}
int main() {Boost();Cells();Collision();Scale();std::printf("DebugMenuWorldReview: %u checks, %u failures\n",checks,failures);return failures?1:0;}
