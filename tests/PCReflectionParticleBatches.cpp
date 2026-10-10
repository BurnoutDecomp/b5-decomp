// Run the production capture producer against controlled batch builders and a
// renderer boundary enforcing the original [first, last) dispatch contract.
#include <array>
#include <vector>
#include <cstdio>
#include "types.hpp"
struct Vector {f32 x=0,y=0,z=0,w=0;};
struct Matrix44 {};
using Matrix44Affine=Matrix44;
namespace rw::math::vpu {
Matrix44 InverseOfMatrixWithOrthonormal3x3(const Matrix44& value){return value;}
}
namespace renderengine {struct VertexBuffer {};}
namespace CgsGraphics {
struct Camera {
    Matrix44 mView,mProjection;f32 maProjectionScalars[9]={};Vector mPosition;
    const Matrix44& GetViewProjectionMatrix()const{return mProjection;}
    const Vector& GetPosition()const{return mPosition;}
};
struct ImRendererBase {inline static void* mgpActiveRenderer=nullptr;};
}
struct EffectsVertexBufferLocked {u32 muBytes=0;u32 GetBytesUsed()const{return muBytes;}};
static u32 suPre=0,suPost=0,suInvalid=0,suCalls=0;
static std::vector<u32> saDraws;
namespace BrnParticle::Native {
struct SimpleParticleBatchArray {
    std::vector<u32> maIds;
    void Construct(){}void Clear(){maIds.clear();}
    s32 GetCount()const{return static_cast<s32>(maIds.size());}
};
struct SimpleParticleVertexBufferBuilder {
    static void BuildDispatchData(EffectsVertexBufferLocked* writer,SimpleParticleBatchArray& batches,
        void*,const u32*,u32 types,f32,const CgsGraphics::Camera&,f32,bool)
    {
        const u32 count=types==10?suPre:suPost;
        for(u32 i=0;i<count;++i)batches.maIds.push_back(static_cast<u32>(batches.maIds.size()));
        writer->muBytes+=count*4;
    }
};
struct SimpleRenderer {
    void Dispatch(renderengine::VertexBuffer*,const SimpleParticleBatchArray& batches,u32 first,u32 last,
        void*,f32,f32,bool)
    {
        if(first==last)return;
        ++suCalls;
        if(first>last || last>static_cast<u32>(batches.GetCount())){++suInvalid;return;}
        for(u32 i=first;i<last;++i)saDraws.push_back(batches.maIds[i]);
    }
};
struct LionBatchArray {void Construct(){}void Clear(){}};
struct SparkBatchArray {void Construct(){}void Clear(){}};
struct SparkFrameDataSet {
    struct Frame {void Set(const Matrix44&,const Matrix44&,f32){}f32 mfTimeStamp=0;};
    std::array<Frame,4> maFrames;
};
struct SparkVertexBufferBuilder {template<class... T>static void BuildDispatchData(T&&...){}};
enum EDebrisArrayID {eDummyDebris};
}
namespace cLionFX {
template<class... T>void Render(T&&...){}
template<class... T>void Dispatch(T&&...){}
}
namespace BrnParticle {
u32 LionTimeFromSeconds(f32){return 0;}
struct ParticleModule {
    struct ParticleRenderData {
        enum {eRenderDataFlagRenderDebris=1,eRenderDataFlagRenderSimple=2,eRenderDataFlagRenderLion=4,
              eRenderDataFlagRenderSparks=8,eRenderDataFlagReducedFrameRate=16};
        bool mbPlayingEffectsSuspendedPC=false;u32 muFlags=2|8;
        f32 mfCurrentTime=0,mfWhiteLevel=1,mfTimeStepMultiplier=1;
        Vector mvSunDirection,mvSunColour,mvAmbientColour;
    };
    struct SimpleFrame {void* GetArrays(){return nullptr;}} mSimpleParticleFramePC;
    Native::SimpleRenderer mSimpleParticleRenderer;
    struct DebrisRenderer {
        template<class... T>void BeginRender(T&&...){}
        template<class... T>void RenderDebrisArray(T&&...){}
    } mDebrisRenderer;
    struct LionRenderer {
        Matrix44 mBackMat,mViewMat,mViewProjection,mPackedFrustumLrtb;
        template<class... T>void SetCameraData(T&&...){}
    } mLionRenderer;
    struct SparkRenderer {
        template<class... T>void Dispatch(T&&...){saDraws.push_back(1000);}
    } mSparkRenderer;
};
}
namespace CgsPC::Reflections {
struct Buffer {
    bool mbAvailable=true;EffectsVertexBufferLocked mWriter;renderengine::VertexBuffer mBuffer;
    EffectsVertexBufferLocked* Begin(u32){mWriter.muBytes=0;return mbAvailable?&mWriter:nullptr;}
    void End(){}renderengine::VertexBuffer* GetBuffer(){return &mBuffer;}
};
struct FaceBuffers {Buffer mSimple,mSparks,mLion;};
static std::array<FaceBuffers,6> saBuffers;
struct Published {
    const BrnParticle::ParticleModule* mpOwner=nullptr;
    std::array<int,5> maDebris;std::array<int,4> maSparks;
    BrnParticle::Native::SparkFrameDataSet mSparkFrames;
} sPublished;
Matrix44Affine AffineView(const CgsGraphics::Camera& camera){return camera.mView;}
Matrix44 PackedFrustum(const CgsGraphics::Camera&){return {};}
struct ParticleCapture {
    static u32 Render(u32,BrnParticle::ParticleModule&,
        const BrnParticle::ParticleModule::ParticleRenderData&,const CgsGraphics::Camera&);
};
}
#include "pc_reflection_particle_batches.inc"
static int siChecks=0,siFailures=0;
static void Check(bool value,const char* name)
{++siChecks;if(!value){++siFailures;if(siFailures<12)std::printf("FAIL %s (pre=%u post=%u)\n",name,suPre,suPost);}}
static void Reset(){saDraws.clear();suInvalid=0;suCalls=0;}
int main()
{
    using namespace CgsPC::Reflections;
    BrnParticle::ParticleModule owner,other;BrnParticle::ParticleModule::ParticleRenderData data;
    CgsGraphics::Camera camera;sPublished.mpOwner=&owner;
    // Includes pre>post (the assert), pre==post (silent loss), pre<post (partial
    // loss), empty groups and the native pre/sparks/post ordering on all faces.
    for(u32 face=0;face<6;++face)for(suPre=0;suPre<=10;++suPre)for(suPost=0;suPost<=2;++suPost){
        Reset();const u32 bytes=ParticleCapture::Render(face,owner,data,camera);
        Check(suInvalid==0,"all submitted ranges satisfy the original dispatch contract");
        Check(suCalls==(suPre?1u:0u)+(suPost?1u:0u),"each nonempty group is dispatched once");
        std::vector<u32> expected;
        for(u32 i=0;i<suPre;++i)expected.push_back(i);
        expected.push_back(1000);
        for(u32 i=suPre;i<suPre+suPost;++i)expected.push_back(i);
        Check(saDraws==expected,"every simple batch is drawn exactly once around the spark pass");
        Check(bytes==4*(suPre+suPost),"batch range selection preserves the built vertex payload");
    }
    suPre=4;suPost=2;Reset();data.mbPlayingEffectsSuspendedPC=true;
    Check(ParticleCapture::Render(0,owner,data,camera)==0 && saDraws.empty(),"suspended effects submit no batches");
    data.mbPlayingEffectsSuspendedPC=false;Reset();
    Check(ParticleCapture::Render(0,other,data,camera)==0 && saDraws.empty(),"another module cannot consume the published frame");
    Reset();saBuffers[0].mSimple.mbAvailable=false;
    ParticleCapture::Render(0,owner,data,camera);
    Check(suCalls==0 && suInvalid==0,"failed simple-buffer allocation cannot submit a stale range");
    saBuffers[0].mSimple.mbAvailable=true;Reset();data.muFlags=8;
    ParticleCapture::Render(0,owner,data,camera);
    Check(suCalls==0 && saDraws==std::vector<u32>{1000},"disabling simple particles leaves the spark pass intact");
    std::printf("PCReflectionParticleBatches: %d checks, %d failures\n",siChecks,siFailures);
    return siFailures?1:0;
}
