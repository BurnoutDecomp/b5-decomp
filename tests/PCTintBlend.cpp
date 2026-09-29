#include <Windows.h>
#include <vector>
#include <cstring>
#include <chrono>
#include "GameShared/Jobs/TintBlend/TintBlend.cpp"
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
using rw::graphics::postfx::TintBlendParameters;
static int checks, failures;
static void Check(bool pass,const char* name) {
    ++checks; if (!pass) { ++failures; std::printf("FAIL %s\n",name); }
}
// Scalar numerical oracle, preserving source order, truncation and PC clamping.
__declspec(noinline) static void Reference(TintBlendParameters& p) {
    const unsigned width=p.size&~15u;
    for (unsigned z=0;z<p.size;++z) for (unsigned y=0;y<p.size;++y)
        for (unsigned x=0;x<width;++x) {
            auto dst=p.dst+z*p.dstSliceStride+y*p.dstStride+4*x;
            for (unsigned channel=0;channel<3;++channel) {
                float value=0;
                for (unsigned source=0;source<p.numSources;++source)
                    value+=p.src[source][((z*p.size+y)*width+x)*3+channel]*p.factor[source];
                dst[2-channel]=value<=0?0:value>=255?255:static_cast<unsigned char>(value);
            }
            dst[3]=255;
        }
}
int main() {
    unsigned random=0x19551996u;
    std::vector<unsigned char> sources[6];
    TintBlendParameters p={};
    const unsigned sizes[]={0,15,16,17,31,32};
    for (unsigned size:sizes) {
        p.size=size; p.dstStride=size*4+13; p.dstSliceStride=p.dstStride*size+29;
        const size_t sourceBytes=size_t(size)*size*(size&~15u)*3;
        for (unsigned s=0;s<6;++s) {
            sources[s].resize(sourceBytes+1);
            for (auto& byte:sources[s]) { random=random*1664525u+1013904223u; byte=static_cast<unsigned char>(random>>24); }
            p.src[s]=sources[s].data()+1; // deliberate unaligned RGB source
        }
        for (p.numSources=2;p.numSources<=6;++p.numSources) for (unsigned mode=0;mode<4;++mode) {
            for (unsigned s=0;s<6;++s)
                p.factor[s]=mode==0?1.0f/p.numSources:mode==1?(s==0?1.0f:0.0f):mode==2?(s&1?-0.7f:1.7f):float(s+1)/21.0f;
            std::vector<unsigned char> actual(p.dstSliceStride*size+64,0x5a),expected=actual;
            p.dst=expected.data()+1; Reference(p);
            p.dst=actual.data()+1; rw::graphics::postfx::TintBlend(p);
            Check(actual==expected,"SIMD matches scalar bytes including row/slice padding and dropped tail");
        }
    }
    // A final RGB block ending at a guard page catches speculative wide overreads.
    p.size=16; p.numSources=6; p.dstStride=64; p.dstSliceStride=1024;
    void* allocations[6]={}; const size_t bytes=16*16*16*3;
    for (unsigned s=0;s<6;++s) {
        allocations[s]=VirtualAlloc(nullptr,bytes+4096,MEM_RESERVE,PAGE_NOACCESS);
        auto source=static_cast<unsigned char*>(VirtualAlloc(allocations[s],bytes,MEM_COMMIT,PAGE_READWRITE));
        if (!source) return 2;
        std::memset(source,17*(s+1),bytes); p.src[s]=source; p.factor[s]=1.0f/6;
    }
    std::vector<unsigned char> actual(16384),expected(16384);
    p.dst=expected.data(); Reference(p); p.dst=actual.data(); rw::graphics::postfx::TintBlend(p);
    Check(actual==expected,"last source block never reads beyond the allocation");
    for (auto allocation:allocations) VirtualFree(allocation,0,MEM_RELEASE);
    p.size=32; p.numSources=4; p.dstStride=128; p.dstSliceStride=4096;
    for (unsigned s=0;s<6;++s) { p.src[s]=sources[s].data()+1; p.factor[s]=0.25f; }
    actual.resize(131072); p.dst=actual.data();
    auto start=std::chrono::steady_clock::now();
    for (unsigned i=0;i<300;++i) Reference(p);
    auto middle=std::chrono::steady_clock::now();
    for (unsigned i=0;i<300;++i) rw::graphics::postfx::TintBlend(p);
    auto end=std::chrono::steady_clock::now();
    std::printf("Tint 32-cube / four sources: scalar %.3f ms, SIMD %.3f ms per job (microbenchmark)\n",
        std::chrono::duration<double,std::milli>(middle-start).count()/300,
        std::chrono::duration<double,std::milli>(end-middle).count()/300);
    std::printf("PCTintBlend: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
