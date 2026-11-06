#include "rw/audio/core/plugins/HighPassButterworth.h"
#include "rw/audio/core/plugins/LowPassButterworth.h"
#include <new>
#define LIFE(C) void Native##C##Lifetime(rw::audio::core::C* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::C; if (del) delete p; else p->~C(); }
LIFE(HighPassButterworth)
LIFE(LowPassButterworth)
