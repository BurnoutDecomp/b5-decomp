// Production prop RenderModel selection and packet routing with observable draw sinks.
#define NOMINMAX
#include <cstdio>
#include <initializer_list>
#include "types.hpp"
#include "pc/gcm/renderengine/reflections/PropReflection.h"
#include "pc/gcm/renderengine/reflections/ReflectionDistance.h"
#include "pc/gcm/renderengine/reflections/ReflectionLod.h"
static int giChecks=0,giFailures=0;
static void Check(bool b,const char* s){++giChecks;if(!b){++giFailures;std::printf("FAIL %s\n",s);}}
#define CGS_ASSERT(b,s) Check(!!(b),s)
namespace rw { namespace math { namespace vpu {
struct Vector3 { using InParam=const Vector3&; f32 x=0,y=0,z=0; };
static Vector3 operator-(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static f32 MagnitudeSquared(Vector3 v){return v.x*v.x+v.y*v.y+v.z*v.z;}
struct Matrix44 { using InParam=const Matrix44&; };
struct Matrix44Affine { Vector3 mPosition; const Vector3& Pos() const {return mPosition;} };
} } }
using namespace rw::math::vpu;
namespace CgsGraphics {
struct Renderable { s32 miState; struct {f32 w=1;} mBoundingSphere; };
struct Model {
 enum State {E_STATE_LOD_0,E_STATE_LOD_1,E_STATE_LOD_2,E_STATE_COUNT=16};
 enum {E_FLAG_MODEL_USES_INSTANCE_SHADER=1,KU_OBJECTS_PER_JOB_BLOCK=128};
 bool mabExists[3]={true,true,true}; bool mbInstanced=false; u32 muNumLods=3;
 Renderable maRenderables[3]={{0},{1},{2}};
 u32 GetNumLods() const {return muNumLods;}
 f32 GetLodDistance(u32 i) const {Check(i<muNumLods,"authored LOD access stays within the model's actual state count");static f32 d[3]={10,20,100};return i<muNumLods?d[i]:0;}
 bool DoesStateExist(State s) const {return s>=0 && u32(s)<muNumLods && mabExists[s];}
 bool GetFlag(int) const {return mbInstanced;}
 const Renderable* GetRenderable(State s) const {return &maRenderables[s];}
};
struct DispatchBin {void BeginPacket(){} const void* EndPacket(){return this;}};
struct DispatchList {u32 muCount=0;u32 GetCount()const{return muCount;}void Submit(int,const void*){++muCount;}};
struct DispatchFrame {DispatchBin mBin;DispatchList maLists[32];DispatchList*GetList(u32 i){return &maLists[i];}DispatchBin&GetBin(){return mBin;}};
static s32 giDrawState=-1,giInstanceState=-1,giOpaque=-1,giTransparent=-1;
static u32 guTechnique=0;static bool gbZOnly=false;
struct DrawRenderable {static void AddToBin(const Renderable*r,DispatchFrame*,bool,s8 a,s8 b,u32,u8 t,bool z,u8,u8,int,u32)
{giDrawState=r->miState;giOpaque=a;giTransparent=b;guTechnique=t;gbZOnly=z;}};
struct ModelInstanceCollector {static void AddInstance(Model*,const Matrix44Affine*,Model::State s){giInstanceState=s;}};
struct Constants {void SetShaderConstantData(int,const Matrix44Affine&) {}};
static Constants mShaderConstantTable;
}
namespace renderengine {
enum GraphicsModelCategoryPC{E_GRAPHICS_PROP,E_GRAPHICS_PROP_ENVMAP};
struct Diagnostics {u32 muPropBase=0,muPropEnvExtended=0;};
static bool GraphicsDiagnosticsEnabledPC(){return false;}
static Diagnostics&GetGraphicsDiagnosticsPC(){static Diagnostics d;return d;}
static void RecordGraphicsModelPC(GraphicsModelCategoryPC,const CgsGraphics::Model*,u32){}
}
namespace BrnWorld {
using namespace CgsGraphics;
enum{E_TECHNIQUE_ZONLY=9};
struct ShaderLodInfo {u8 GetEnvMapTechnique()const{return 7;}template<class Bounds>u8 GetLodTechnique(const Bounds&,const Matrix44Affine&,const Vector3&)const{return 3;}};
struct PropEntityModule {
 bool mbOverrideLod=false,mbOverrideLodDistances=false;s32 miLodOverrideValue=0; s32 mauOverrideLodDistances[3]={100,200,300};
 bool RenderModel(Model*,const Matrix44Affine*,DispatchFrame*,Matrix44::InParam,Vector3::InParam,f32,s32,s32,s32,bool,bool,const ShaderLodInfo*);
};
}
#include "pc/gcm/renderengine/shadows/SceneSettings.h"
#include "pc_prop_reflection_lod.inc"
int main()
{
 using namespace CgsGraphics;using namespace BrnWorld;
 PropEntityModule lModule;Model lModel;DispatchFrame lFrame;Matrix44 lView;Matrix44Affine lTransform;ShaderLodInfo lShader;
 auto draw=[&](bool env,bool shadow,Vector3 camera=Vector3{}){giDrawState=-1;giInstanceState=-1;
  return lModule.RenderModel(&lModel,&lTransform,&lFrame,lView,camera,1,5,6,7,env,shadow,&lShader);};
 Check(renderengine::PropEnvironmentMapLodPC()==2,"missing setting defaults to original prop reflection LOD2");
 Check(draw(true,false)&&giDrawState==2&&guTechnique==7&&giOpaque==6&&giTransparent==7,
       "default capture submits original LOD2 with its reflection technique and routing");
 renderengine::PropEnvironmentMapLodPC()=0;
 Check(draw(true,false)&&giDrawState==0,"LOD0 control reaches the actual reflected prop draw packet");
 renderengine::PropEnvironmentMapLodPC()=1;
 Check(draw(true,false)&&giDrawState==1,"LOD1 control reaches the reflected prop draw packet");
 lModel.mbInstanced=true;renderengine::PropEnvironmentMapLodPC()=0;
 Check(draw(true,false)&&giInstanceState==0&&giDrawState==-1,"instanced props receive the same reflection LOD control");
 lModel.mbInstanced=false;lModel.mabExists[0]=false;const u32 luBefore=lFrame.maLists[5].muCount;
 Check(!draw(true,false)&&lFrame.maLists[5].muCount==luBefore,"missing requested state emits no invalid packet");
 lModel.mabExists[0]=true;
 Check(draw(false,false,{15,0,0})&&giDrawState==1&&guTechnique==3,"reflection option does not force main-view prop LOD or technique");
 Check(draw(false,true,{15,0,0})&&giDrawState==1&&guTechnique==9&&gbZOnly&&giOpaque==giTransparent,
       "shadow selection and depth-only packet routing stay original");
 Check(!draw(true,false,{101,0,0}),"reflection LOD cannot bypass the original distance cull");
 renderengine::EnvironmentMapDrawDistancePC()=150;
 Check(draw(true,false,{101,0,0})&&giDrawState==0,"extended distance reaches reflected prop draw packets with the requested LOD");
 Check(!draw(false,false,{101,0,0})&&!draw(false,true,{101,0,0}),"extended reflections preserve main-view and shadow prop culls");
 Check(!draw(true,false,{150,0,0}),"extended prop distance keeps the original exclusive cutoff");
 lModel.mbInstanced=true;
 Check(draw(true,false,{101,0,0})&&giInstanceState==0,"extended distance reaches the reflected instance collector");
 lModel.mbInstanced=false;lModule.mbOverrideLodDistances=true;
 Check(draw(true,false,{200,0,0}),"longer original prop override distances are retained");
 lModule.mbOverrideLodDistances=false;renderengine::EnvironmentMapDrawDistancePC()=0;
 Check(!draw(true,false,{101,0,0}),"restoring zero immediately restores the original prop cutoff");
 for(s32 i:{-1,3,100}){renderengine::PropEnvironmentMapLodPC()=i;Check(draw(true,false)&&giDrawState==2,"invalid script LOD falls back to original state");}
 lModule.mbOverrideLod=true;lModule.miLodOverrideValue=1;renderengine::PropEnvironmentMapLodPC()=0;
 Check(draw(true,false)&&giDrawState==1,"original forced prop LOD retains its higher precedence");
 lModule.mbOverrideLod=false;renderengine::PropEnvironmentMapLodPC()=2;
 auto& lrSettings=renderengine::PropEnvironmentMapLodSettingsPC();
 Check(!lrSettings.IsDistanceBased(),"new prop reflection distance policy is disabled by default");
 Check(draw(true,false,{1,0,0})&&giDrawState==2&&draw(true,false,{70,0,0})&&giDrawState==2,
       "untuned prop reflections stay fixed LOD2 at near and far positions");
 lrSettings.miMode=renderengine::E_ENVIRONMENT_MAP_LOD_RELATIVE;
 Check(draw(true,false,{5,0,0})&&giDrawState==0&&draw(true,false,{5.1f,0,0})&&giDrawState==1,
       "relative prop LOD0 boundary uses half the authored distance in metres");
 Check(draw(true,false,{10,0,0})&&giDrawState==1&&draw(true,false,{10.1f,0,0})&&giDrawState==2,
       "relative props transition to LOD2 after the scaled authored LOD1 distance");
 Check(draw(true,false,{99,0,0})&&!draw(true,false,{100,0,0}),"prop relative scale leaves the final cull independent");
 lModule.mbOverrideLodDistances=true;
 lModule.mauOverrideLodDistances[0]=40;lModule.mauOverrideLodDistances[1]=80;lModule.mauOverrideLodDistances[2]=120;
 Check(draw(true,false,{15,0,0})&&giDrawState==0&&draw(true,false,{30,0,0})&&giDrawState==1
       &&draw(true,false,{50,0,0})&&giDrawState==2,"relative prop policy follows the active prop distance overrides");
 renderengine::WorldEnvironmentMapLodSettingsPC().miMode=renderengine::E_ENVIRONMENT_MAP_LOD_RELATIVE;
 renderengine::WorldEnvironmentMapLodSettingsPC().mfDistanceScale=10;
 Check(draw(true,false,{30,0,0})&&giDrawState==1,"world reflection scale cannot change prop selection");
 lModule.mauOverrideLodDistances[0]=80;lModule.mauOverrideLodDistances[1]=160;
 Check(draw(true,false,{30,0,0})&&giDrawState==0,"live prop distance changes move relative transitions immediately");
 lrSettings.mfDistanceScale=0.25f;
 Check(draw(true,false,{30,0,0})&&giDrawState==1,"live prop reflection scale changes move transitions immediately");
 lrSettings.miMode=renderengine::E_ENVIRONMENT_MAP_LOD_CUSTOM;
 lrSettings.mafTransitionDistances[0]=3;lrSettings.mafTransitionDistances[1]=6;
 Check(draw(true,false,{3,0,0})&&giDrawState==0&&draw(true,false,{4,0,0})&&giDrawState==1
       &&draw(true,false,{7,0,0})&&giDrawState==2,"custom prop distances take precedence over scaled main-view distances");
 lModel.mabExists[1]=false;
 Check(draw(true,false,{4,0,0})&&giDrawState==2,"adaptive props use a coarser available mesh for missing LOD1");
 lModel.mabExists[2]=false;
 Check(draw(true,false,{7,0,0})&&giDrawState==0,"adaptive props retain a finer mesh when coarser states are absent");
 lModel.mabExists[0]=false;
 Check(!draw(true,false,{4,0,0}),"adaptive props with no available state never emit an invalid packet");
 lModel.mabExists[0]=lModel.mabExists[1]=lModel.mabExists[2]=true;
 lModel.mbInstanced=true;
 Check(draw(true,false,{4,0,0})&&giInstanceState==1,"instanced props use the same adaptive policy");
 lModel.mbInstanced=false;lModule.mbOverrideLodDistances=false;
 Check(draw(false,false,{15,0,0})&&giDrawState==1&&draw(false,true,{15,0,0})&&giDrawState==1&&gbZOnly,
       "adaptive reflection settings leave main-view and shadow LOD selection unchanged");
 lModule.mbOverrideLod=true;lModule.miLodOverrideValue=0;
 Check(draw(true,false,{7,0,0})&&giDrawState==0,"original forced prop LOD still takes precedence over adaptive reflections");
 lModule.mbOverrideLod=false;renderengine::EnvironmentMapDrawDistancePC()=150;
 Check(draw(true,false,{125,0,0})&&giDrawState==2&&!draw(true,false,{150,0,0}),
       "adaptive prop LOD2 survives to the independent extended cutoff");
 renderengine::EnvironmentMapDrawDistancePC()=0;lrSettings=renderengine::EnvironmentMapLodSettingsPC();
 Check(draw(true,false,{1,0,0})&&giDrawState==2&&!draw(true,false,{101,0,0}),
       "resetting prop mode restores fixed LOD2 and the original cutoff");
 auto& lrSmall=CgsPC::Shadows::SmallObjects();
 lrSmall.mbEnabled=true;lrSmall.miDistanceMode=CgsPC::Reflections::E_DISTANCE_RELATIVE;
 lrSmall.mfDrawDistanceScale=.5f;renderengine::GetGraphicsSettingsPC().mfShadowDistance=120;
 Check(draw(false,true,{59,0,0}) && !draw(false,true,{60,0,0}),
       "small-caster cutoff follows half the live shadow distance with the original exclusive boundary");
 renderengine::GetGraphicsSettingsPC().mfShadowDistance=300;
 Check(draw(false,true,{125,0,0}) && gbZOnly && guTechnique==9,
       "raising shadow distance extends small-object depth packets without colour geometry");
 Check(!draw(false,false,{125,0,0}) && !draw(true,false,{125,0,0}),
       "small-object shadows cannot extend main-view or reflection visibility");
 for(auto& lrRenderable:lModel.maRenderables) lrRenderable.mBoundingSphere.w=3;
 Check(!draw(false,true,{125,0,0}),"larger props retain the original shadow policy");
 for(auto& lrRenderable:lModel.maRenderables) lrRenderable.mBoundingSphere.w=1;
 lrSmall.miDistanceMode=CgsPC::Reflections::E_DISTANCE_FIXED;lrSmall.mfDrawDistance=80;
 Check(draw(false,true,{79,0,0}) && !draw(false,true,{80,0,0}),"small-object fixed cutoff is independent from the main shadow distance");
 lrSmall.mbEnabled=false;
 Check(!draw(false,true,{125,0,0}),"disabling the override restores original caster distances");
 lrSmall.mbEnabled=true;lModule.mbOverrideLodDistances=false;
 for(u32 count:{1u,2u}){
  lModel.muNumLods=count;
  Check(draw(false,true,{5,0,0})&&giDrawState==0,"single/two-LOD small props submit valid near shadow packets");
  Check(draw(false,true,{79,0,0})&&u32(giDrawState)<count,"small shadow extension falls back to an existing sparse model state");
 }
 lModel.muNumLods=3;lrSmall.mbEnabled=false;
 std::printf("PCPropReflectionLod: %d checks, %d failures\n",giChecks,giFailures);return giFailures?1:0;
}
