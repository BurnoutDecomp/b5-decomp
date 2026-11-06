#include "rw/audio/core/PlugIn.h"
#include "rw/audio/core/Voice.h"
#include <eathread/eathread_mutex.h>
#include <eathread/eathread_thread.h>
#include <stddef.h>
typedef rw::audio::core::System AudioSystem;
extern "C" const unsigned native_audio_layout[] = {
 sizeof(AudioSystem),__alignof(AudioSystem),sizeof(EA::Thread::Mutex),__alignof(EA::Thread::Mutex),sizeof(EA::Thread::ThreadId),
 sizeof(rw::audio::core::TimerManager),sizeof(rw::audio::core::VoiceActiveNode),
 offsetof(AudioSystem,mpObjectTable),offsetof(AudioSystem,mfSystemTime),offsetof(AudioSystem,mpExpelledVoiceList),
 offsetof(AudioSystem,mpAllocator),offsetof(AudioSystem,mpCommandMutex),offsetof(AudioSystem,mpSystemMutex),
 offsetof(AudioSystem,mpLockThreadId),offsetof(AudioSystem,mppThreadId),offsetof(AudioSystem,mppVoiceListNodes),
 offsetof(AudioSystem,mpTimerListHead),offsetof(AudioSystem,mTimerManager),offsetof(AudioSystem,mpExpelAfterDecayList),
 offsetof(AudioSystem,muExpelAfterDecayCount),offsetof(AudioSystem,muCommandBufferSize),
 offsetof(AudioSystem,muDeferredRingCursor),offsetof(AudioSystem,muDeferredRingHighWater),offsetof(AudioSystem,mfSystemTimerPeriod),
 offsetof(AudioSystem,mfCpuClockRate),offsetof(AudioSystem,muMixerCpuTicks),offsetof(AudioSystem,muFrameCounter),
 offsetof(AudioSystem,miRefCount),offsetof(AudioSystem,mucThreadProcessor),offsetof(AudioSystem,maucDebugFeatures)
};
