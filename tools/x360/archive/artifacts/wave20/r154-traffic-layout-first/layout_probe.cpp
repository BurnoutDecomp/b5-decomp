#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
typedef char runtime_size[(sizeof(BrnTraffic::TrafficLightRuntimeState)==8)?1:-1];
typedef char runtime_align[(__alignof(BrnTraffic::TrafficLightRuntimeState)==4)?1:-1];
typedef char placeholder_align[(__alignof(BrnTraffic::TrafficLightState)==4)?1:-1];
typedef char manager_size[(sizeof(BrnTraffic::TrafficLightManager)==0x12cc)?1:-1];
typedef char manager_align[(__alignof(BrnTraffic::TrafficLightManager)==4)?1:-1];
typedef char array_size[(sizeof(Array<unsigned short,72>)==0x94)?1:-1];
typedef char array_align[(__alignof(Array<unsigned short,72>)==4)?1:-1];
struct WithDtor { float t; unsigned char a,b; ~WithDtor(); };
typedef char dtor_align[(__alignof(WithDtor)==4 && sizeof(WithDtor)==8)?1:-1];
