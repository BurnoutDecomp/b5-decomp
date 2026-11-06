#include "rw/audio/core/plugins/VuMeter.h"
#include <new>
void NativeVuMeterLifetime(rw::audio::core::VuMeter* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::VuMeter; if (del) delete p; else p->~VuMeter(); }
__declspec(noinline) unsigned NativeVuMeterTickCall(rw::audio::core::PlugIn* p) { return p->GetPpuTicksEvent(); }
unsigned NativeVuMeterTickDispatch(rw::audio::core::VuMeter* p, unsigned ticks) { ::new (static_cast<void*>(p)) rw::audio::core::VuMeter; p->mTimer.mCpuTicks = ticks; return NativeVuMeterTickCall(p); }
__declspec(noinline) void NativeVuMeterEventCall(rw::audio::core::PlugIn* p, int id, void* payload) { p->EventEvent(id,payload); }
void NativeVuMeterEventDispatch(rw::audio::core::VuMeter* p, int id, void* payload) { ::new (static_cast<void*>(p)) rw::audio::core::VuMeter; NativeVuMeterEventCall(p,id,payload); }
