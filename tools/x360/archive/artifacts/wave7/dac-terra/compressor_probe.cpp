#include "rw/audio/core/plugins/Compressor1.h"
#include <new>
void NativeCompressor1Lifetime(rw::audio::core::Compressor1* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Compressor1; if (del) delete p; else p->~Compressor1(); }
