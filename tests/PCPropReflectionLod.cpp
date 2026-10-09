// Production prop RenderModel selection and packet routing with observable draw sinks.
#include <cstdio>
#include <initializer_list>
#include "types.hpp"
#include "pc/gcm/renderengine/PropReflectionPCLeaf.h"
#include "pc/gcm/renderengine/ReflectionDistancePCLeaf.h"
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
struct Renderable { s32 miState; int mBoundingSphere=0; };
struct Model {
 enum State {E_STATE_LOD_0,E_STATE_LOD_1,E_STATE_LOD_2,E_STATE_COUNT=16};
 enum {E_FLAG_MODEL_USES_INSTANCE_SHADER=1,KU_OBJECTS_PER_JOB_BLOCK=128};
 bool mabExists[3]={true,true,true}; bool mbInstanced=false;
 Renderable maRenderables[3]={{0},{1},{2}};
 u32 GetNumLods() const {return 3;}
 f32 GetLodDistance(u32 i) const {static f32 d[3]={10,20,100};return d[i];}
 bool DoesStateExist(State s) const {return s>=0 && s<3 && mabExists[s];}
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
struct ShaderLodInfo {u8 GetEnvMapTechnique()const{return 7;}u8 GetLodTechnique(int,const Matrix44Affine&,const Vector3&)const{return 3;}};
struct PropEntityModule {
 bool mbOverrideLod=false,mbOverrideLodDistances=false;s32 miLodOverrideValue=0; s32 mauOverrideLodDistances[3]={100,200,300};
 bool RenderModel(Model*,const Matrix44Affine*,DispatchFrame*,Matrix44::InParam,Vector3::InParam,f32,s32,s32,s32,bool,bool,const ShaderLodInfo*);
};
}
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
 std::printf("PCPropReflectionLod: %d checks, %d failures\n",giChecks,giFailures);return giFailures?1:0;
}
