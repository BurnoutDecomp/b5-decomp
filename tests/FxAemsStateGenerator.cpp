#include <cstdint>
#include <cstring>
#include <cstdio>
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t; using s32=int32_t;
#include "fx_aems_state_generator.inc"
int main()
{
    // Native scalar layout attested at ARTIST82B70730..778: count byte+2,
    // full signed state+4, state table+8, trigger table at u16+0.
    // Extra backing makes the old wrong-count walk deterministic and in bounds.
    alignas(8) u8 data[262176] = {};
    int total=0,failures=0;
    auto word=[&](int at,s32 value){std::memcpy(data+at,&value,4);};
    auto check=[&](bool ok,const char* name){++total;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    auto init=[&](u8 count,s32 current){
        std::memset(data,0,sizeof(data));u16 offset=20;std::memcpy(data,&offset,2);
        data[2]=count;word(4,current);word(8,70000);word(12,-40000);word(16,0);
    };
    auto stored=[&](){s32 value;std::memcpy(&value,data+4,4);return value;};
    init(0,123456);check(UpdateStateGenerator(data)==123456,"zero-count retains full state");
    init(3,0);check(UpdateStateGenerator(data)==0,"no triggers retains zero");
    word(20,1);check(UpdateStateGenerator(data)==70000,"first trigger selects full positive state");
    check(stored()==70000,"selected state stored at4");
    check(data[2]==3,"state write preserves byte count");
    word(20,0);word(24,-1);check(UpdateStateGenerator(data)==-40000,"negative trigger selects signed state");
    check(stored()==-40000,"negative state stored without truncation");
    word(20,1);word(28,1);check(UpdateStateGenerator(data)==70000,"first active trigger has priority");
    word(20,0);word(24,0);word(28,0);check(UpdateStateGenerator(data)==70000,"state persists after triggers clear");
    word(28,1);check(UpdateStateGenerator(data)==0,"third trigger selects zero state");
    std::printf("FxAemsStateGenerator: %d checks, %d failures\n",total,failures);
    return failures?1:0;
}
