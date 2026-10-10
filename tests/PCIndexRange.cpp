#define NOMINMAX
#include <Windows.h>
#include "pc/gcm/renderengine/IndexRange.h"
#include <cstdio>
#include <vector>
#include <limits>
#include <algorithm>
using renderengine::IndexRangePC::Range;
using renderengine::IndexRangePC::Scan;
static unsigned checks=0,failures=0;
static void Check(bool pass,const char* name) {
    ++checks;if(!pass){++failures;if(failures<=12)std::printf("FAIL %s\n",name);}
}
template<class T> static Range Reference(const void* data,size_t count) {
    if(!count)return {};
    std::vector<T> values(count);std::memcpy(values.data(),data,count*sizeof(T));
    std::sort(values.begin(),values.end());return {values.front(),values.back()};
}
static bool Same(Range a,Range b){return a.muMin==b.muMin&&a.muMax==b.muMax;}
template<class T> static bool Guarded(const void* data,size_t count,Range* out) {
    __try{*out=Scan<T>(data,count);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T> static void Cases() {
    unsigned random=0x82647a25u;
    const T edges[]={0,1,static_cast<T>((std::numeric_limits<T>::max)()/2),
        static_cast<T>((std::numeric_limits<T>::max)()/2+1),static_cast<T>((std::numeric_limits<T>::max)()-1),
        (std::numeric_limits<T>::max)()};
    std::vector<unsigned char> data(1040*sizeof(T)+16);
    for(unsigned alignment=0;alignment<16;++alignment)for(unsigned count=0;count<257;++count) {
        auto* base=data.data()+alignment;
        for(unsigned i=0;i<count;++i){random=random*1664525u+1013904223u;
            const T value=i%5?static_cast<T>(random):edges[(random>>8)%6];std::memcpy(base+i*sizeof(T),&value,sizeof(T));}
        Check(Same(Scan<T>(base,count),Reference<T>(base,count)),"unaligned unsigned ranges and every reduction tail match the reference");
    }
    for(T value:edges)for(unsigned count:{1u,7u,8u,15u,16u,31u,32u,33u,1024u}) {
        for(unsigned i=0;i<count;++i)std::memcpy(data.data()+i*sizeof(T),&value,sizeof(T));
        Check(Same(Scan<T>(data.data(),count),Range{value,value}),"uniform signed-boundary inputs retain their unsigned range");
    }
    SYSTEM_INFO system{};GetSystemInfo(&system);const size_t page=system.dwPageSize;
    auto* guard=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    DWORD previous=0;
    Check(guard&&VirtualProtect(guard+page,page,PAGE_NOACCESS,&previous),"guard page is available");
    if(guard) {
        for(unsigned count=0;count<65;++count) {
            auto* base=guard+page-count*sizeof(T);
            for(unsigned i=0;i<count;++i){T value=static_cast<T>(i*17u+128u);std::memcpy(base+i*sizeof(T),&value,sizeof(T));}
            Range actual{};const auto expected=Reference<T>(base,count);
            Check(Guarded<T>(base,count,&actual)&&Same(actual,expected),"zero length and vector tails never touch the guard page");
        }
        VirtualFree(guard,0,MEM_RELEASE);
    }
}
template<class T> __declspec(noinline) static Range Kernel(const void* data,size_t count){return Scan<T>(data,count);}
__declspec(noinline) static Range Legacy(const void* data,size_t count,bool wide) {
    Range result{count?0xffffffffu:0u,0};
    for(std::uint32_t i=0;i<count;++i) {
        const std::uint32_t value=wide?static_cast<const std::uint32_t*>(data)[i]:static_cast<const std::uint16_t*>(data)[i];
        if(value<result.muMin)result.muMin=value;
        if(value>result.muMax)result.muMax=value;
    }
    return result;
}
template<class T> static void Benchmark() {
    std::vector<T> values(4096);unsigned random=0x7253abu;
    for(auto& value:values){random=random*1664525u+1013904223u;value=static_cast<T>(random);}
    LARGE_INTEGER hz{};QueryPerformanceFrequency(&hz);
    for(unsigned count:{8u,31u,128u,1024u,4096u}) {
        const unsigned iterations=20000000u/count;
        double ms[4]{};
        const auto expected=Reference<T>(values.data(),count);
        const unsigned long long checksum=(expected.muMin+static_cast<unsigned long long>(expected.muMax))*iterations;
        for(unsigned phase=0;phase<4;++phase) {
            Range(*volatile legacy)(const void*,size_t,bool)=&Legacy;
            Range(*volatile typed)(const void*,size_t)=&Kernel<T>;
            LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);unsigned long long sum=0;
            if(phase==1||phase==2)for(unsigned i=0;i<iterations;++i){const auto range=typed(values.data(),count);sum+=range.muMin+static_cast<unsigned long long>(range.muMax);}
            else for(unsigned i=0;i<iterations;++i){const auto range=legacy(values.data(),count,sizeof(T)==4);sum+=range.muMin+static_cast<unsigned long long>(range.muMax);}
            QueryPerformanceCounter(&end);ms[phase]=1000.*(end.QuadPart-begin.QuadPart)/hz.QuadPart;
            Check(sum==checksum,"legacy runtime-width reduction agrees with typed scan");
        }
        std::printf("WIDTH width=%zu count=%u legacy_a=%.4f typed_a=%.4f typed_b=%.4f legacy_b=%.4f ms\n",sizeof(T)*8,count,ms[0],ms[1],ms[2],ms[3]);
    }
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    Cases<std::uint16_t>();Cases<std::uint32_t>();
    if(!failures){Benchmark<std::uint16_t>();Benchmark<std::uint32_t>();}
    std::printf("PCIndexRange: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
