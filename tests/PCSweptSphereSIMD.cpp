#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"
#include "pc/geometric/SweptSphereSIMD.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "swept_scalar_reference.inc"

using CgsGeometric::Triangle4;
using CgsGeometric::SweptSphere;
struct Case {SweptSphere sphere;Triangle4 triangles;};
struct Output {Vector3 contact[4],normal[4];Vector3Plus sphere[4],triangle[4];Triangle4::Mask4 mask;};
static unsigned checks,failures;
#ifdef PC_SWEPT_SCALAR_FALLBACK
static constexpr bool kScalarFallback = true;
#else
static constexpr bool kScalarFallback = false;
#endif
static void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
template<class V> static u32 Bits(const V& v,unsigned i){u32 x;std::memcpy(&x,reinterpret_cast<const char*>(&v)+4*i,4);return x;}
static float Float(u32 bits){float f;std::memcpy(&f,&bits,4);return f;}
static void SetLane(Vector4& v,unsigned i,float x){std::memcpy(reinterpret_cast<char*>(&v)+4*i,&x,4);}
static void Mask(Triangle4& t,unsigned i,u32 value){std::memcpy(reinterpret_cast<char*>(&t.mValidMasks)+4*i,&value,4);}
static Vector3Plus Pack(float x,float y,float z,float w){Vector3Plus v;v.x=x;v.y=y;v.z=z;v.w=w;return v;}
static void Triangle(Triangle4& t,unsigned lane,const float* a,const float* b,const float* c){
    Vector4* v[]={&t.mVertex0X,&t.mVertex0Y,&t.mVertex0Z,&t.mVertex1X,&t.mVertex1Y,&t.mVertex1Z,&t.mVertex2X,&t.mVertex2Y,&t.mVertex2Z};
    const float* p[]={a,b,c};for(unsigned k=0;k<9;++k)SetLane(*v[k],lane,p[k/3][k%3]);
}
#define OUTPUT_ARGS(o) (o).contact[0],(o).normal[0],(o).sphere[0],(o).triangle[0], \
 (o).contact[1],(o).normal[1],(o).sphere[1],(o).triangle[1], \
 (o).contact[2],(o).normal[2],(o).sphere[2],(o).triangle[2], \
 (o).contact[3],(o).normal[3],(o).sphere[3],(o).triangle[3]
__declspec(noinline) static void Scalar(const Case& c,Output& o){o.mask=CgsGeometric::Reference::IntersectTriangle4SweptSphere(c.sphere,c.triangles,OUTPUT_ARGS(o));}
__declspec(noinline) static void Simd(const Case& c,Output& o){o.mask=CgsGeometric::SweptSpherePC::Run(c.sphere,c.triangles,OUTPUT_ARGS(o));}
__declspec(noinline) static void Public(const Case& c,Output& o){o.mask=CgsGeometric::IntersectTriangle4SweptSphere(c.sphere,c.triangles,OUTPUT_ARGS(o));}

template<class V> static bool EqualVector(const V& a,const V& b){
    for(unsigned i=0;i<4;++i){const u32 x=Bits(a,i),y=Bits(b,i);if(x!=y&&!(std::isnan(Float(x))&&std::isnan(Float(y))))return false;}
    return true;
}
static bool Equal(const Output& a,const Output& b){
    if(std::memcmp(&a.mask,&b.mask,16))return false;
    for(unsigned i=0;i<4;++i)if(!EqualVector(a.contact[i],b.contact[i])||!EqualVector(a.normal[i],b.normal[i])
        ||!EqualVector(a.sphere[i],b.sphere[i])||!EqualVector(a.triangle[i],b.triangle[i]))return false;
    return true;
}
static Case Face(){
    Case c{};const float a[]={0,0,0},b[]={0,0,4},d[]={4,0,0};
    for(unsigned i=0;i<4;++i){Triangle(c.triangles,i,a,b,d);Mask(c.triangles,i,~0u);}
    c.sphere.Set(Pack(1,2,1,.5f),Pack(0,-1,0,4));return c;
}
static u32 state=0x5eeda123;
static u32 Next(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static float Random(float range){return (float(Next()&65535)/32768.0f-1.0f)*range;}

static void EdgeCases(){
    Case c=Face();Output out;
    Simd(c,out);
    bool face=true;for(unsigned i=0;i<4;++i)face &= Bits(out.mask,i)==~0u&&out.contact[i].y==-1&&out.normal[i].y==1
        &&out.triangle[i].x==1&&out.triangle[i].y==0&&out.triangle[i].z==1&&out.triangle[i].w==.375f
        &&out.sphere[i].x==1&&out.sphere[i].y==1.5f&&out.sphere[i].z==1&&out.sphere[i].w==.375f;
    Check(face,"analytic front-face hit preserves normals, start-frame point and contact time");
    const u32 masks[]={~0u,0,0x80000000u,0x1234u};
    for(unsigned i=0;i<4;++i)Mask(c.triangles,i,masks[i]);Simd(c,out);
    bool masking=true;for(unsigned i=0;i<4;++i)masking &= Bits(out.mask,i)==masks[i]&&out.triangle[i].w==.375f;
    Check(masking,"partial valid masks are bitwise ANDed and disabled lanes still receive outputs");
    c=Face();c.sphere.Set(Pack(1,-2,1,.5f),Pack(0,1,0,4));Simd(c,out);
    Check(Bits(out.mask,0)==0,"back-face sweep is rejected");
    c.sphere.Set(Pack(1,.5f,1,.5f),Pack(1,0,0,4));Simd(c,out);
    Check(Bits(out.mask,0)==0,"parallel sweep exactly on the slab boundary is rejected");
    c.sphere.Set(Pack(1,1.5f,1,.5f),Pack(0,-1,0,1));Simd(c,out);
    Check(Bits(out.mask,0)==0,"entry exactly at the sweep endpoint is rejected");
    c=Face();c.sphere.Set(Pack(1,.25f,1,.5f),Pack(0,-1,0,1));Simd(c,out);
    Check(Bits(out.mask,0)==~0u&&out.triangle[0].w==0&&out.sphere[0].y==-.25f,"face result wins over swept features for an initial overlap");
    c=Face();
    auto& o=out;
    CgsGeometric::SweptSpherePC::Run(c.sphere,c.triangles,
        o.contact[0],o.normal[0],o.sphere[0],o.triangle[0],o.contact[1],o.normal[1],o.sphere[1],o.triangle[1],
        o.contact[2],o.normal[2],o.sphere[2],o.triangle[2],o.normal[0],o.normal[3],o.sphere[3],o.triangle[3]);
    const bool directStoreOrder = o.normal[0].y == 1;
    o.normal[0].y = 0;
    CgsGeometric::IntersectTriangle4SweptSphere(c.sphere,c.triangles,
        o.contact[0],o.normal[0],o.sphere[0],o.triangle[0],o.contact[1],o.normal[1],o.sphere[1],o.triangle[1],
        o.contact[2],o.normal[2],o.sphere[2],o.triangle[2],o.normal[0],o.normal[3],o.sphere[3],o.triangle[3]);
    Check(directStoreOrder && o.normal[0].y==(kScalarFallback ? -1.0f : 1.0f),
          "SIMD preserves grouped stores; public dispatch selects the requested implementation");
    for(unsigned variant=0;variant<9;++variant){
        c=Face();float a[]={0,0,0},b[]={0,0,4},d[]={4,0,0};
        if(variant==0){b[2]=0;d[0]=0;}
        if(variant==1){d[0]=0;d[2]=2;}
        if(variant==2)c.sphere.Set(Pack(1,.25f,1,.5f),Pack(0,0,0,0));
        if(variant==3)c.sphere.Set(Pack(1,200,1,.0001f),Pack(0,-2,0,800));
        if(variant==4)a[0]=std::numeric_limits<float>::quiet_NaN();
        if(variant==5)b[2]=std::numeric_limits<float>::infinity();
        if(variant==6){a[0]=-0.0f;b[0]=-0.0f;}
        if(variant==7){b[2]=1e-20f;d[0]=1e-20f;}
        if(variant==8)c.sphere.Set(Pack(4.1f,.1f,-.1f,.25f),Pack(0,-1,0,.2f));
        Triangle(c.triangles,variant%4,a,b,d);
        Output scalar,simd;Scalar(c,scalar);Simd(c,simd);
        Check(Equal(scalar,simd),"degenerate, stationary, long/tiny, unordered and signed-zero cases match PC scalar outputs");
    }
}

int main(int argc,char**){
    // An empty value removes the variable, exercising the shipping default.
    _putenv_s("BRN_SWEPT_SPHERE_SIMD",kScalarFallback ? "0" : "");
    EdgeCases();
    std::vector<Case> cases;cases.reserve(20000);
    for(unsigned n=0;n<20000;++n){
        Case c{};
        c.sphere.Set(Pack(Random(20),Random(20),Random(20),.001f+std::fabs(Random(4))),
                     Pack(Random(2),Random(2),Random(2),std::fabs(Random(40))));
        for(unsigned i=0;i<4;++i){float a[3],b[3],d[3];for(unsigned j=0;j<3;++j){a[j]=Random(20);b[j]=a[j]+Random(10);d[j]=a[j]+Random(10);}
            Triangle(c.triangles,i,a,b,d);Mask(c.triangles,i,(Next()%5)==0?0:~0u);}
        if(n&1){
            // Contact-rich road-like geometry, including large world coordinates.
            const float x=Random(2000),y=Random(100),z=Random(2000),radius=.001f+std::fabs(Random(3));
            const float length=.1f+std::fabs(Random(20)),width=3+std::fabs(Random(8));
            c.sphere.Set(Pack(x,y,z,radius),Pack(Random(.1f),-1,Random(.1f),length));
            for(unsigned i=0;i<4;++i){
                const float plane=y-radius-(.1f+.3f*i)*length,slope=Random(.2f);
                const float a[]={x-width,plane-width*slope,z-width};
                const float b[]={x-width,plane-width*slope,z+3*width};
                const float d[]={x+3*width,plane+3*width*slope,z-width};
                Triangle(c.triangles,i,a,b,d);
            }
        }
        cases.push_back(c);
    }
    const unsigned savedCsr=_mm_getcsr();
    for(unsigned mode : {0x1f80u,0x9fc0u}){
        _mm_setcsr(mode);unsigned mismatch=0,hitLanes=0;
        for(const auto& c:cases){Output scalar,simd,publicResult;Scalar(c,scalar);Simd(c,simd);Public(c,publicResult);
            if(!Equal(scalar,simd)||!Equal(simd,publicResult))++mismatch;
            for(unsigned i=0;i<4;++i)hitLanes += Bits(scalar.mask,i)!=0;
        }
        std::printf("fuzz mxcsr=%04x batches=%zu hit_lanes=%u mismatches=%u\n",mode,cases.size(),hitLanes,mismatch);
        Check(mismatch==0,"four distinct random triangles preserve all masks and finite output bits");
        Check(hitLanes>100,"random corpus exercises accepted contacts as well as misses");
    }
    _mm_setcsr(savedCsr);
#ifdef PC_SWEPT_BENCHMARK
    {
        for(unsigned repeat=0;repeat<3;++repeat)for(auto run:{Scalar,Simd}){
            volatile u32 sink=2166136261u;
            const auto begin=std::chrono::steady_clock::now();Output o;
            for(unsigned i=0;i<200000;++i){run(cases[i%cases.size()],o);sink=(sink^Bits(o.triangle[i&3],(i>>2)&3)^Bits(o.mask,i&3))*16777619u;}
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            std::printf("kernel %s repeat=%u ms=%.3f checksum=%u\n",run==Scalar?"scalar":"simd",repeat,ms,unsigned(sink));
        }
    }
#endif
    std::printf("PCSweptSphereSIMD: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
