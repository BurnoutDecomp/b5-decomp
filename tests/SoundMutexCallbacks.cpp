// Production callbacks against a recursive mutex. Check the ownership count
// at the release seam, then exercise two contending native threads.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
using s32=int32_t;
namespace BrnSound {namespace Module {
struct RootSoundModule {
    static s32 msiMutexLockCount;
    static void MutexLockFn();static void MutexUnlockFn();static bool MutexIsLockedFn();
};
s32 RootSoundModule::msiMutexLockCount=0;
}}
static std::recursive_mutex mutex;
static thread_local int depth=0;
static std::atomic<unsigned> violations{0};
namespace rw {namespace audio {namespace core {
void CsisMutexLock(){mutex.lock();++depth;}
void CsisMutexUnlock(){
    // The callback still owns the mutex here. Its count must already describe
    // the nesting depth that will remain after this release.
    if(BrnSound::Module::RootSoundModule::msiMutexLockCount!=depth-1)++violations;
    --depth;mutex.unlock();
}
}}}
namespace BrnSound {namespace Module {
#include "sound_mutex_callbacks.inc"
}}
int main(){
    using Root=BrnSound::Module::RootSoundModule;
    unsigned checks=0,failures=0;
    auto expect=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL %s\n",msg);}};
    expect(!Root::MutexIsLockedFn(),"initially unlocked");
    Root::MutexLockFn();expect(Root::MutexIsLockedFn()&&Root::msiMutexLockCount==1,"first acquisition counted");
    Root::MutexLockFn();expect(Root::msiMutexLockCount==2,"nested acquisition counted");
    Root::MutexUnlockFn();expect(Root::msiMutexLockCount==1,"nested release preserves ownership");
    Root::MutexUnlockFn();expect(!Root::MutexIsLockedFn()&&Root::msiMutexLockCount==0,"balanced final release");
    expect(violations==0,"count updated before the mutex can change owner");
    // An old revision already failed at the deterministic release seam. Avoid
    // running its known unprotected counter concurrently (a C++ data race).
    if(!failures){
        auto work=[](){for(unsigned i=0;i<25000;++i){
            Root::MutexLockFn();if(!Root::MutexIsLockedFn()||Root::msiMutexLockCount!=1)++violations;
            Root::MutexUnlockFn();
        }};
        std::thread first(work),second(work);first.join();second.join();
        expect(violations==0,"50,000 contended lock cycles preserve ownership");
        expect(Root::msiMutexLockCount==0,"contended cycles leave a balanced counter");
    }
    std::printf("SoundMutexCallbacks: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
