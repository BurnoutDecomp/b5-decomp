#include "GameSource/Director/Camera/Camera.h"
using namespace BrnDirector::Camera;
CGS_STATIC_ASSERT((sizeof(((Camera*)0)->mState_uFlags) == 8), "camera current flags are 64-bit");
CGS_STATIC_ASSERT((offsetof(Camera, mState_uFlags) == 0x140), "camera flag offset");
CGS_STATIC_ASSERT((offsetof(Camera, mState) + offsetof(CameraState, mCurrentFlags) == 0x140), "current flag identity");
extern "C" void SetValid(Camera* c) { c->mState_uFlags |= 2; }
extern "C" void ClearValid(Camera* c) { c->mState_uFlags &= ~2; }
extern "C" bool ReadValid(Camera* c) { return c->GetState().IsFlagSet(1); }
