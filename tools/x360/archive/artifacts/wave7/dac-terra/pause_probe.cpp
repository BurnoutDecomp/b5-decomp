#include "rw/audio/core/plugins/Pause.h"
#include <new>
void NativePauseLifetime(rw::audio::core::Pause* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Pause; if (del) delete p; else p->~Pause(); }
