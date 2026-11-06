#include "types.hpp"
#define private public
#define protected public
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#undef protected
#undef private
struct __declspec(align(64)) Base64 : BrnReplays::BaseSerialiser {};
struct __declspec(align(128)) Base128 : BrnReplays::BaseSerialiser {};
struct Derived64 : Base64 { bool flag; };
struct Derived128 : Base128 { bool flag; };
struct __declspec(align(64)) Direct64 : BrnReplays::BaseSerialiser { bool flag; };
struct __declspec(align(128)) Direct128 : BrnReplays::BaseSerialiser { bool flag; };
extern "C" const u32 gControls[] = {
 sizeof(Base64),sizeof(Base128),sizeof(Derived64),offsetof(Derived64,flag),sizeof(Derived128),offsetof(Derived128,flag),sizeof(Direct64),offsetof(Direct64,flag),sizeof(Direct128),offsetof(Direct128,flag)
};
