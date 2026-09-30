#include <cstdio>
#include <cstring>
#include <vector>
#include "GameShared/Jobs/Relocator/RelocatorJob.h"

static int checks,failures,assertions;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
static void Check(bool ok,const char* message) {
    ++checks;
    if(!ok) { ++failures; std::printf("FAIL %s\n",message); }
}
static void CopyCase(size_t source,size_t dest,u32 size,int budget) {
    const size_t length=(source>dest ? source : dest)+size+64;
    std::vector<u8> actual(length),expected,bounce(budget+32,0xA7);
    for(size_t i=0;i<length;++i) actual[i]=static_cast<u8>((i*37+(i>>8)*11)^0x63);
    expected=actual;
    std::memmove(expected.data()+dest,expected.data()+source,size);
    CgsMemory::RelocateOp op = {actual.data()+source,actual.data()+dest,size,0};
    CgsMemory::RelocatorJobData data = {};
    data.mpOps=&op; data.miNumOps=1; data.mpBounceBuffer=bounce.data()+16; data.miBounceBufferSize=budget;
    RelocatorJob job;
    job.Execute(&data);
    char message[160];
    std::snprintf(message,sizeof(message),"copy %zu -> %zu, %u bytes, %d-byte budget preserves payload and neighboring bytes",source,dest,size,budget);
    bool guards=true;
    for(int i=0;i<16;++i) guards &= bounce[i]==0xA7 && bounce[16+budget+i]==0xA7;
    Check(actual==expected && guards,message);
}
int main() {
    for(int budget : {1,7,64,1024}) {
        CopyCase(64,32,257,budget); // pack toward lower addresses, overlapping
        CopyCase(32,64,257,budget); // overlapping upward move
        CopyCase(32,288,257,budget); // exactly one shared byte
        CopyCase(288,32,257,budget);
        CopyCase(32,289,257,budget); // adjacent, disjoint
    }
    CopyCase(256,128,1398128,1024*1024); // measured car texture size, original bounce capacity
    CopyCase(128,256,1398128,1024*1024);
    CopyCase(32,32,257,7); // identity copy
    Check(assertions==0,"valid copy operations satisfy all runtime assertions");
    std::printf("PCRelocatorCopy: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
