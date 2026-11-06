#include "rw/audio/core/Rechannel.h"
#include <new>
void NativeRechannelLifetime(rw::audio::core::Rechannel* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::Rechannel; if (del) delete p; else p->~Rechannel(); }
