#include "rw/audio/core/Iir2Filters.h"
#include <new>
#define LIFETIME(C) void Native##C##Lifetime(rw::audio::core::C* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::C; if (del) delete p; else p->~C(); }
LIFETIME(LowPassIir2)
LIFETIME(HighPassIir2)
LIFETIME(BandPassIir2)
LIFETIME(LowShelfIir2)
LIFETIME(HighShelfIir2)
LIFETIME(PeakingIir2)
