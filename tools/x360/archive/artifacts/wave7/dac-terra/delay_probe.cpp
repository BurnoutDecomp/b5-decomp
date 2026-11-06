#include "rw/audio/core/plugins/Delay.h"
#include <new>
void NativeDelayLifetime(rw::audio::core::Delay* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Delay; if (del) delete p; else p->~Delay(); }
__declspec(noinline) unsigned NativeDelayTickCall(rw::audio::core::PlugIn* p) { return p->GetPpuTicksEvent(); }
unsigned NativeDelayTickDispatch(rw::audio::core::Delay* p, unsigned ticks) { ::new (static_cast<void*>(p)) rw::audio::core::Delay; p->mTimer.mCpuTicks = ticks; return NativeDelayTickCall(p); }
#include "rw/audio/core/Voice.h"
void NativeVoiceDelayRelease(rw::audio::core::Voice* voice, rw::audio::core::Delay* p) { ::new (static_cast<void*>(p)) rw::audio::core::Delay; p->mbActive = 0; rw::audio::core::Voice::ReleaseImmediate(voice, 1); }
