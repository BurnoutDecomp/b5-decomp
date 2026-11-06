#include "rw/audio/core/plugins/Pan2D1.h"
#include <new>
void NativePan2D1Lifetime(rw::audio::core::Pan2D1* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Pan2D1; if (del) delete p; else p->~Pan2D1(); }
