#include "rw/audio/core/Voice.h"
#include "rw/audio/core/plugins/ReverbModel1.h"
#include <new>
void NativeReverbModel1Lifetime(rw::audio::core::ReverbModel1* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::ReverbModel1; if (del) delete p; else p->~ReverbModel1(); }
#include "rw/audio/core/plugins/ReverbModel1.h"
#include <new>
__declspec(noinline) unsigned NativeReverbTickCall(rw::audio::core::PlugIn* p) { return p->GetPpuTicksEvent(); }
unsigned NativeReverbTickDispatch(rw::audio::core::ReverbModel1* p, unsigned ticks) { ::new (static_cast<void*>(p)) rw::audio::core::ReverbModel1; p->mTimer.mCpuTicks = ticks; return NativeReverbTickCall(p); }

void NativeVoiceReverbRelease(rw::audio::core::Voice* voice, rw::audio::core::ReverbModel1* p, unsigned mode) { ::new (static_cast<void*>(p)) rw::audio::core::ReverbModel1; p->mbAllPassCount = static_cast<unsigned char>(mode); p->mbTimerAdded = 0; rw::audio::core::Voice::ReleaseImmediate(voice, 1); }

