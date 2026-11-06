#include "types.hpp"
#define private public
#define protected public
#include "GameSource/World/BrnWorldModule.h"
#undef protected
#undef private
#include <new>
typedef BrnReplays::RaceCarEntitySerialiser Serialiser;
extern "C" const u32 gCandidateLayout[] = {
sizeof(Serialiser),__alignof(Serialiser),sizeof(BrnWorld::RaceCarEntityModule),__alignof(BrnWorld::RaceCarEntityModule),
offsetof(BrnWorld::RaceCarEntityModule,mReplaySerialiser),offsetof(BrnWorld::RaceCarEntityModule,mReplayTransform),
offsetof(BrnWorld::RaceCarEntityModule,mbInvalidatedRaceCars),offsetof(BrnWorld::RaceCarEntityModule,mabRaceCarsRendered),
offsetof(BrnWorld::RaceCarEntityModule,DEBUG_mbOverrideDamage),offsetof(BrnWorld::WorldModule,mRaceCarEntityModule),
offsetof(BrnWorld::WorldModule,mTrafficEntityModule),sizeof(BrnReplays::BaseSerialiser),__alignof(BrnReplays::BaseSerialiser)
};
extern "C" Serialiser* DiagnosticNext(Serialiser* p) { return p+1; }
extern "C" void DiagnosticDefault(Serialiser* p) { new(p) Serialiser; }
extern "C" void DiagnosticValueInit(Serialiser* p) { new(p) Serialiser(); }
extern "C" void DiagnosticCopy(Serialiser* to,const Serialiser* from) { new(to) Serialiser(*from); }
extern "C" void DiagnosticAssign(Serialiser* to,const Serialiser* from) { *to=*from; }
