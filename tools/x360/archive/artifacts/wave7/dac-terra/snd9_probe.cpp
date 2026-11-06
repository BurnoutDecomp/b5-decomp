#include "rw/audio/core/Snd9Service.h"
#include <new>
void NativeSnd9ServiceLifetime(rw::audio::core::Snd9Service* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Snd9Service; if (del) delete p; else p->~Snd9Service(); }
__declspec(noinline) unsigned NativeSnd9TickCall(rw::audio::core::PlugIn* p) { return p->GetPpuTicksEvent(); }
unsigned NativeSnd9TickDispatch(rw::audio::core::Snd9Service* p, unsigned ticks) { ::new (static_cast<void*>(p)) rw::audio::core::Snd9Service; p->mTimerHandle.mCpuTicks = ticks; return NativeSnd9TickCall(p); }
