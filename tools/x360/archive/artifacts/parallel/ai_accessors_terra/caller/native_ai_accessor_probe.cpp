#include "GameSource/World/AI/BrnAIModule.h"
BrnAI::AICar* ProbeCar(const BrnAI::AIModule* p, u32 i) { return p->GetAICar(i); }
BrnAI::AIDriver* ProbeDriver(BrnAI::AIModule* p, EActiveRaceCarIndex i) { return p->GetAIDriver(i); }
