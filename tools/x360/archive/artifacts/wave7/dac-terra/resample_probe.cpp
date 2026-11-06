#include "rw/audio/core/Resample.h"
#include <new>
void NativeResampleLifetime(rw::audio::core::Resample* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Resample; if (del) delete p; else p->~Resample(); }
