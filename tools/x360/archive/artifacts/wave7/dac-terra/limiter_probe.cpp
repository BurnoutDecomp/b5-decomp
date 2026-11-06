#include "rw/audio/core/plugins/Limiter1.h"
#include <new>
void NativeLimiter1Lifetime(rw::audio::core::Limiter1* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Limiter1; if (del) delete p; else p->~Limiter1(); }
