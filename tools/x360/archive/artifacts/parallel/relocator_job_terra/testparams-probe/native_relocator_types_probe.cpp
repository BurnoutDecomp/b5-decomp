#include "GameShared/Jobs/Relocator/CgsRelocator.h"
typedef CgsMemory::RelocationTestParams T;
CGS_STATIC_ASSERT(sizeof(T)==0x10, "test params size");
CGS_STATIC_ASSERT(sizeof(T::Command)==0x30 && __alignof(T::Command)==16, "command stride and alignment");
CGS_STATIC_ASSERT(sizeof(T::Result)==0x80 && __alignof(T::Result)==128, "result stride and alignment");
CGS_STATIC_ASSERT(offsetof(T,mpCommandPoster)==4 && offsetof(T,mpResultReader)==8 && offsetof(T,miCounter)==12, "params fields");
CGS_STATIC_ASSERT(offsetof(CgsMemory::RelocatorJobData,mpTestParams)==16, "test params pointer");
