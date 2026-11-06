#include "rw/audio/core/plugins/SinePlayer.h"
#include <new>
void NativeSinePlayerLifetime(rw::audio::core::SinePlayer* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::SinePlayer; if (del) delete p; else p->~SinePlayer(); }
__declspec(noinline) void NativeSineEventCall(rw::audio::core::PlugIn* p, int id, void* payload) { p->EventEvent(id,payload); }
void NativeSineEventDispatch(rw::audio::core::SinePlayer* p, int id, void* payload) { ::new (static_cast<void*>(p)) rw::audio::core::SinePlayer; NativeSineEventCall(p,id,payload); }
