#include "rw/audio/core/plugins/Pan2D.h"
#include <new>
void NativePan2DLifetime(rw::audio::core::Pan2D* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Pan2D; if (del) delete p; else p->~Pan2D(); }
