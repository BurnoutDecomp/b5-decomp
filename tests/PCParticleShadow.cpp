// Production effect-payload policy and published debris eligibility. The fixture
// supplies records, not another copy of the renderer's decision logic.
#include <array>
#include <vector>
#include <cstdio>
#include "types.hpp"
#include "pc/gcm/renderengine/shadows/SceneSettings.h"
static int siChecks=0,siFailures=0;
static void Check(bool pass,const char* name){++siChecks;if(!pass){++siFailures;std::printf("FAIL %s\n",name);}}
struct Vector {f32 x=0,y=0,z=0,w=0;};
struct Camera {Vector mEye;const Vector& GetPosition()const{return mEye;}};
namespace BrnParticle {
struct ParticleModule {
    struct ParticleRenderData {
        enum {eRenderDataFlagRenderDebris=1};
        ParticleModule* mpParticleModule=nullptr;
        Camera mCgsCamera;
        bool mbPlayingEffectsSuspendedPC=false;
        u32 muFlags=1;
    };
};
}
using Data=BrnParticle::ParticleModule::ParticleRenderData;
struct Particle {Vector mPositionPlusRotVel,mVelocityPlusScale;};
struct Bucket {u32 mu16NumberOfParticlesInBucket=0;std::array<Particle,32> maParticleData;};
namespace CgsPC::Reflections {
struct Snapshot {const BrnParticle::ParticleModule* mpOwner=nullptr;std::array<std::vector<Bucket>,5> maDebrisBuckets;} sPublished;
struct ParticleCapture {static bool HasDebrisShadow(const Data&,f32);};
static bool sabCameras[3]={true,true,true};
static const Camera* GetShadowCamera(u32 i){static Camera camera;return sabCameras[i]?&camera:nullptr;}
}
namespace CgsPC::Shadows {bool HasDebris(const Data*);}
#include "pc_particle_shadow.inc"
struct Input {
    bool mbEffects=true,mbStalled=false;
    bool GetCalibrationUnfriendlyEnablePostFx()const{return mbEffects;}
    bool GetIsStalled()const{return mbStalled;}
};
struct Renderer {
    bool mbRenderParticles=true;
    const Data* mpPublished=nullptr;
    const Data* GetPublishedParticleRenderDataPC()const{return mpPublished;}
    const Data* Select(const Input* lpDispatchThreadInputBuffer)const{
#include "pc_particle_policy.inc"
    }
};
int main(){
    using namespace CgsPC;
    BrnParticle::ParticleModule owner,other;Data data;data.mpParticleModule=&owner;
    Reflections::sPublished.mpOwner=&owner;
    auto& buckets=Reflections::sPublished.maDebrisBuckets;
    Shadows::Debris().mbEnabled=true;Shadows::Debris().miDistanceMode=Reflections::E_DISTANCE_RELATIVE;
    Shadows::Debris().mfDrawDistanceScale=0.5f;renderengine::GetGraphicsSettingsPC().mfShadowDistance=120;
    Check(!Shadows::HasDebris(&data),"empty published buckets do not admit a shadow pass");
    buckets[4].resize(1);buckets[4][0].mu16NumberOfParticlesInBucket=1;
    buckets[4][0].maParticleData[0].mVelocityPlusScale.w=1;
    Check(!Shadows::HasDebris(&data),"transparent glass debris is excluded from opaque caster eligibility");
    buckets[0]=buckets[4];auto& particle=buckets[0][0].maParticleData[0];
    particle.mPositionPlusRotVel.x=60;
    Check(Shadows::HasDebris(&data),"opaque debris on the relative cutoff admits a debris-only pass");
    particle.mPositionPlusRotVel.x=61;
    Check(!Shadows::HasDebris(&data),"opaque debris outside the relative cutoff cannot admit a pass");
    renderengine::GetGraphicsSettingsPC().mfShadowDistance=200;
    Check(Shadows::HasDebris(&data),"debris eligibility follows the live graphics shadow distance");
    data.mCgsCamera.mEye.x=100;
    Check(Shadows::HasDebris(&data),"debris cutoff uses the published main-camera eye");
    particle.mVelocityPlusScale.w=0;
    Check(!Shadows::HasDebris(&data),"zero-scale debris does not admit an empty pass");
    particle.mVelocityPlusScale.w=1;
    data.mpParticleModule=&other;
    Check(!Shadows::HasDebris(&data),"a payload from another module cannot consume retained buckets");
    data.mpParticleModule=&owner;data.muFlags=0;
    Check(!Shadows::HasDebris(&data),"original debris render flag remains authoritative");
    data.muFlags=1;data.mbPlayingEffectsSuspendedPC=true;
    Check(!Shadows::HasDebris(&data),"suspended effects cannot admit debris shadows");
    data.mbPlayingEffectsSuspendedPC=false;Shadows::Debris().mbEnabled=false;
    Check(!Shadows::HasDebris(&data),"the optional debris category remains opt-in");
    Shadows::Debris().mbEnabled=true;
    for(auto& valid:Reflections::sabCameras)valid=false;
    Check(!Shadows::HasDebris(&data),"unpublished cascade cameras cannot admit debris shadows");
    Reflections::sabCameras[1]=true;
    Check(Shadows::HasDebris(&data),"an available published cascade can receive opaque debris");
    Check(!Shadows::HasDebris(nullptr),"missing completed particle records cannot admit a pass");
    Renderer renderer;renderer.mpPublished=&data;Input input;
    for(int mask=0;mask<16;++mask){
        renderer.mbRenderParticles=(mask&1)!=0;input.mbEffects=(mask&2)!=0;input.mbStalled=(mask&4)!=0;
        const Input* source=(mask&8)?&input:nullptr;
        Check(renderer.Select(source)==((mask==11)?&data:nullptr),
            "production effect policy respects master, calibration, stall and input gates");
    }
    renderer.mbRenderParticles=true;input.mbEffects=true;input.mbStalled=false;renderer.mpPublished=nullptr;
    Check(renderer.Select(&input)==nullptr,"enabled captures still require a produced published particle record");
    std::printf("PCParticleShadow: %d checks, %d failures\n",siChecks,siFailures);return siFailures?1:0;
}
