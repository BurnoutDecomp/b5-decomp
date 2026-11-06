#include "rw/audio/core/AiffWriter.h"
#include <new>
void NativeAiffWriterLifetime(rw::audio::core::AiffWriter* p, int del) { ::new (static_cast<void*>(p)) rw::audio::core::AiffWriter; if (del) delete p; else p->~AiffWriter(); }
#include "rw/audio/core/AiffWriter.h"
#include <new>
void NativeAiffWriterDispatch(rw::audio::core::AiffWriter* p, int id, void* payload) { ::new (static_cast<void*>(p)) rw::audio::core::AiffWriter; rw::audio::core::PlugIn::Event(p,id,payload); }
