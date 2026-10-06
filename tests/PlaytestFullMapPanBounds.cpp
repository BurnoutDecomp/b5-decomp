#include <cmath>
#include <cstdio>
#include <limits>
#include <cstdint>
using f32=float; using s32=int32_t; using u32=uint32_t;
static int checks=0, failures=0;
#define CHECK(c) do { ++checks; if(!(c)) { ++failures; std::printf("FAIL line%d: %s\n",__LINE__,#c); } } while(0)
#define CGS_ASSERT(c,m) do { if(!(c)) { ++failures; std::printf("ASSERT: %s\n",m); } } while(0)
struct Vector2 {float x=0,y=0,z=0,w=0;void SetZero(){x=y=z=w=0;}};
struct Vector3 {float x=0,y=0,z=0,w=0;};
struct Vector4 {float x,y,z,w;};
namespace rw::math::vpu {static float Dot(Vector2 a,Vector2 b){return a.x*b.x+a.y*b.y;}}
namespace CgsModule {struct Event{};}
namespace BrnGui {
struct ControllerAxisPayload {s32 miAxis;f32 mfXAxis,mfYAxis;};
constexpr s32 KI_AXIS_LEFT_STICK=0,KI_AXIS_RIGHT_STICK=1,KI_AXIS_ALTERNATE_STICK=2;
constexpr f32 KF_PAN_INPUT_FILTER_FACTOR=.4f,KF_PAN_Y_BORDER=200,FK_PAN_CURSOR_MOVEMENT_RADIUS=60;
constexpr f32 KF_PANNING_STOP_RESETTIME=.2f,KF_MAX_SNAP_SCREEN_DISTANCE=2500;
constexpr s32 KI_SNAP_LIST_SIZE=289;
struct GuiEventUpdateSatNav {struct SatNavIconInfo {enum {E_SATNAVICON_TIRE_SHOP=12};};};
struct LockedIcon {int type=0;void SetIconType(int v){type=v;}};
struct GuiCache {f32 time=1;f32 GetTime()const{return time;}};
struct IconManager {void GetSatNavIconPositions(Vector2* p,s32* n){p[0]={};*n=1;}};
struct Cursor {
 Vector2 position;int deltaCalls=0;bool lastFlag=true;
 Vector2 GetPosition()const{return position;}
 void SetPosition(Vector2 p,bool flag){position=p;lastFlag=flag;}
 void SetDelta(f32,f32,f32){++deltaCalls;}
 u32 FindClosestSnapIndex(Vector2*,u32){return 0;}
};
// Terminal world/device I/O is deliberately identity in the XZ plane. It
// isolates the actual production pan/clamp while the view rectangle varies.
struct MapTransform {
 static const Vector4& GetWorldRect(){static const Vector4 r={-4375.419921875f,-5842.419921875f,5363.14990234375f,3904.739990234375f};return r;}
 static Vector3 Unflatten(Vector2 p){return {p.x,0,p.y,0};}
 static Vector2 WorldToDevice(Vector3 p,bool){return {p.x,p.z,0,0};}
 static Vector3 DeviceToWorld(Vector2 p){return {p.x,0,p.y,0};}
};
struct MainMap {Vector4 view={};Vector4 GetWorldRect()const{return view;}};
struct CrashNavMap {
 enum {E_CURSORMODE_NONE=0,E_CURSORMODE_SELECTING_ICONS=1,E_CURSORMODE_INSPECTING_ICONS=2,E_CURSORMODE_ZOOMEDOUT=3,E_CURSORMODE_PANNING=4};
 s32 meCursorMode=1;float mfMapPanningStopTime=0;u32 muHoveredEventID=55;
 uint64_t mHoveredDriveThruID=123,mHoveringRivalId=99;const char* mpLockedIconName="hover";
 bool mbLocalPlayerSelected=true;Vector2 mv2WorldCentrePoint;GuiCache cache;GuiCache* mpGuiCache=&cache;
 IconManager icons;IconManager* mpIconManager=&icons;Cursor mCursor;MainMap mMainMapComponent;
 LockedIcon mLockedIconInfo;int promptCalls=0;
 void UpdateButtonPrompts(){++promptCalls;}
 void MoveCursor(const CgsModule::Event*);
};
}
#include "playtest_full_map_pan_bounds_bodies.inc"
static bool near(float a,float b){return std::fabs(a-b)<.002f;}
static void input(BrnGui::CrashNavMap& m,float x,float y){BrnGui::ControllerAxisPayload p{1,x,y};m.MoveCursor(reinterpret_cast<const CgsModule::Event*>(&p));}
int main(){
 using namespace BrnGui;const auto fixed=MapTransform::GetWorldRect();
 // Every world edge must be invariant under a moving/zoomed rectangle. Four
 // deliberately incompatible view rectangles catch the old instance accessor.
 const Vector4 views[]={{-100,-100,100,100},{-9000,-9000,9000,9000},{3000,1000,7000,6000},{-7000,-7000,-3000,-2000}};
 for(auto view:views)for(int edge=0;edge<4;++edge){
  CrashNavMap m;m.mMainMapComponent.view=view;
  Vector2 centre{};float x=0,y=0;
  if(edge==0){centre.x=fixed.x+1;x=-1;}
  if(edge==1){centre.x=fixed.z-1;x=1;}
  if(edge==2){centre.y=fixed.y+200+1;y=1;}
  if(edge==3){centre.y=fixed.w-200-1;y=-1;}
  m.mv2WorldCentrePoint=centre;m.mCursor.position=centre;input(m,x,y);
  const float expectX=edge==0?fixed.x:(edge==1?fixed.z:centre.x);
  const float expectY=edge==2?fixed.y+200:(edge==3?fixed.w-200:centre.y);
  CHECK(near(m.mCursor.position.x,expectX));CHECK(near(m.mCursor.position.y,expectY));
 }
 // The original pan integration remains .4 of a60-unit stick offset while
 // inside city bounds; both axes, hover clearing and supplied set-position flag.
 {CrashNavMap m;m.mv2WorldCentrePoint={100,100,0,0};m.mCursor.position={90,110,0,0};m.mMainMapComponent.view={5000,5000,6000,6000};
  input(m,1,-1);CHECK(near(m.mCursor.position.x,118));CHECK(near(m.mCursor.position.y,130));
  CHECK(m.meCursorMode==4);CHECK(m.muHoveredEventID==0&&m.mHoveredDriveThruID==0&&m.mHoveringRivalId==0);
  CHECK(m.mpLockedIconName==nullptr&&!m.mbLocalPlayerSelected);CHECK(m.mLockedIconInfo.type==12);
  CHECK(m.promptCalls==1&&!m.mCursor.lastFlag);
 }
 // Retargeting a moving view near the boundary cannot carry the cursor out of
 // the fixed city. This catches the same held-stick feedback seen in live25.
 for(int edge=0;edge<4;++edge){CrashNavMap m;
  m.mv2WorldCentrePoint={edge<2?(edge==0?fixed.x+50:fixed.z-50):0,edge>=2?(edge==2?fixed.y+250:fixed.w-250):0,0,0};
  m.mCursor.position=m.mv2WorldCentrePoint;
  for(int i=0;i<120;++i){auto c=m.mv2WorldCentrePoint;m.mMainMapComponent.view={c.x-5000,c.y-5000,c.x+5000,c.y+5000};
   input(m,edge==0?-1.f:(edge==1?1.f:0.f),edge==2?1.f:(edge==3?-1.f:0.f));
   m.mv2WorldCentrePoint=m.mCursor.position;
  }
  CHECK(m.mCursor.position.x>=fixed.x-.002f&&m.mCursor.position.x<=fixed.z+.002f);
  CHECK(m.mCursor.position.y>=fixed.y+200-.002f&&m.mCursor.position.y<=fixed.w-200+.002f);
 }
 // Inspection freezes input before dereferencing the event, and ordinary LS
 // selection remains on the original cursor-delta route.
 {CrashNavMap m;m.meCursorMode=2;m.MoveCursor(nullptr);CHECK(m.promptCalls==0&&m.mCursor.deltaCalls==0);}
 {CrashNavMap m;ControllerAxisPayload p{0,1,0};m.MoveCursor(reinterpret_cast<const CgsModule::Event*>(&p));CHECK(m.mCursor.deltaCalls==1);}
 std::printf("PlaytestFullMapPanBounds: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
