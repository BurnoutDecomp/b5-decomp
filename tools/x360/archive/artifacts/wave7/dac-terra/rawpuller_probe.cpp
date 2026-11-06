#include "rw/audio/core/RawPuller2.h"
#include <new>
void NativeRawPuller2Lifetime(rw::audio::core::RawPuller2* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::RawPuller2; if (del) delete p; else p->~RawPuller2(); }
__declspec(noinline) void NativeRawEventCall(rw::audio::core::PlugIn* p, int id, void* payload) { p->EventEvent(id,payload); }
void NativeRawEventDispatch(rw::audio::core::RawPuller2* p, int id, void* payload) { ::new (static_cast<void*>(p)) rw::audio::core::RawPuller2; NativeRawEventCall(p,id,payload); }
