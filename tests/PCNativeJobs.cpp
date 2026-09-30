#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <thread>
#include <array>
#include <utility>
#include <malloc.h>
#include <windows.h>
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/local_backend.h"
#include "SDKs/EATech/eajobs/job.h"
#include "SDKs/EATech/eajobs/jobs.h"
// Include the production helper so this fixture can install its otherwise
// private watchdog predicate; the helper body itself is not substituted.
#include "SDKs/EATech/eajobs/detail.cpp"

using namespace EA::Jobs;
using Backend=EA::Jobs::LocalBackend::LocalBackend;
static int checks,failures;
static std::atomic<int> badArguments{0};
static void Check(bool ok,const char* what){++checks;if(!ok){++failures;std::printf("FAIL %s\n",what);}}

// Allocation boundary only; scheduler, worker threads, events and job bodies
// below execute the production implementation. Guards detect undersized records.
struct GuardAllocator : EA::Allocator::ICoreAllocator {
    struct Block{void* address;size_t size;};
    std::mutex lock;std::vector<Block> blocks;bool intact=true;
    void* Alloc(size_t n,const char*,unsigned,unsigned alignment,unsigned offset) override {
        if(offset)std::abort();void* p=_aligned_malloc(n+32,std::max<unsigned>(alignment,16));
        if(!p)std::abort();std::memset(static_cast<char*>(p)+n,0xCE,32);
        std::lock_guard<std::mutex> guard(lock);blocks.push_back({p,n});return p;
    }
    void* Alloc(size_t n,const char* name,unsigned flags) override{return Alloc(n,name,flags,16,0);}
    void Free(void* p,size_t) override {
        if(!p)return;std::lock_guard<std::mutex> guard(lock);
        auto it=std::find_if(blocks.begin(),blocks.end(),[p](const Block& b){return b.address==p;});
        if(it==blocks.end())std::abort();
        for(int i=0;i<32;++i)if(static_cast<u8*>(p)[it->size+i]!=0xCE)intact=false;
        blocks.erase(it);_aligned_free(p);
    }
    bool Guards(){std::lock_guard<std::mutex> guard(lock);
        for(const auto& b:blocks)for(int i=0;i<32;++i)if(static_cast<u8*>(b.address)[b.size+i]!=0xCE)intact=false;return intact;}
};
struct Work {
    std::atomic<int> calls{0};
    std::atomic<int>* active=nullptr;std::atomic<int>* peak=nullptr;
    HANDLE entered=nullptr,release=nullptr;
    std::atomic<int>* prerequisite=nullptr;
};
static void Body(Param a,Param b,Param c,Param d){
    if(a.mpValue!=reinterpret_cast<void*>(0x12345678ABCDEF00ull)||!b.mpValue
        ||c.muValue!=sizeof(Work)||d.muValue!=0xF1234567u){++badArguments;return;}
    auto& w=*static_cast<Work*>(b.mpValue);
    if(w.prerequisite && w.prerequisite->load()!=1)++badArguments;
    if(w.active){int n=++*w.active,old=w.peak->load();while(old<n&&!w.peak->compare_exchange_weak(old,n)){} }
    if(w.entered)SetEvent(w.entered);
    if(w.release)WaitForSingleObject(w.release,3000);
    ++w.calls;
    if(w.active)--*w.active;
}
static void Configure(Job& job,Work& work){
    job.Clear();job.SetCode(JOB_ENVIRONMENT_LOCAL,reinterpret_cast<const void*>(&Body),0);
    job.mParams[0]=Param(reinterpret_cast<void*>(0x12345678ABCDEF00ull));
    job.SetData(&work,sizeof(work));job.mParams[3]=Param(0xF1234567u);
}
static bool Done(Job& job){const auto until=GetTickCount64()+4000;
    while(!job.IsDone()&&GetTickCount64()<until)Sleep(1);return job.IsDone();}
static Backend& Back(JobScheduler& s){return *static_cast<Backend*>(s.mSchedulers[0]);}
static void Count(void* p){++*static_cast<std::atomic<int>*>(p);}
static int watchdogCalls,waitCallbackCalls;
static void* expectedWaitContext;
static bool waitContextMatched;
static bool StopWatchdog(){++watchdogCalls;return false;}
static WaitOnControl CancelWait(void* context){
    ++waitCallbackCalls;waitContextMatched=context==expectedWaitContext;return WAIT_ON_CANCEL;
}
struct ProfileCapture { std::vector<JobMetrics> records;std::vector<int> batches; };
static void CaptureProfile(const JobMetrics* records,int count,void* context){
    auto& capture=*static_cast<ProfileCapture*>(context);
    capture.batches.push_back(count);capture.records.insert(capture.records.end(),records,records+count);
}
template<size_t... I> static std::array<Job,sizeof...(I)> ProfileJobs(std::index_sequence<I...>){
    return {{((void)I,Job("profile"))...}};
}

int main(){
    GuardAllocator allocator;SetAllocator(&allocator);
    void* target=nullptr;void* high=reinterpret_cast<void*>(0x1234567887654321ull);
    Event pointer(Event::EVENT_TYPE_WRITE_PTR,high,&target,nullptr);pointer.Run();
    Check(target==high,"pointer-write events preserve the full native address");
    {
        LARGE_INTEGER now,frequency;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
        Check(TicksToSeconds(static_cast<u64>(frequency.QuadPart))==1.0f,"job timestamps use the native QPC frequency");
        Detail::spWaitWatchdog=StopWatchdog;u8 done=0;
        Check(Detail::WaitOnYieldHelper(nullptr,nullptr,-1,static_cast<u64>(now.QuadPart),&done)==1&&watchdogCalls==0,
            "a current full-width start timestamp does not trip the eight-second watchdog");
        const u64 old=static_cast<u64>(now.QuadPart-frequency.QuadPart*9);
        Check(Detail::WaitOnYieldHelper(nullptr,nullptr,-1,old,&done)==0&&watchdogCalls==1,
            "an expired wait invokes the watchdog and honors cancellation");
        done=1;
        Check(Detail::WaitOnYieldHelper(nullptr,nullptr,-1,old,&done)==1&&watchdogCalls==1,
            "an already-completed wait skips the watchdog");
        Detail::spWaitWatchdog=nullptr;
    }
    {
        JobScheduler scheduler;scheduler.Initialize(24,128);auto& backend=Back(scheduler);
        Check(backend.mNumSlots==24,"the first Initialize argument sets local job capacity");
        Check(allocator.Guards(),"native job-instance array allocation fits all constructed records");
        Work work;Job job("payload");Configure(job,work);scheduler.AddJobs(&job,1);
        Check(!job.IsDone()&&work.calls==0,"submitted job stays pending until a worker executes it");
        expectedWaitContext=&work;job.WaitOn(CancelWait,expectedWaitContext,-1);
        Check(waitCallbackCalls==1&&waitContextMatched&&!job.IsDone(),
            "Job WaitOn forwards the full native callback context and honors cancel");
        Check(backend.ExecuteReadyJobs(JOB_AFFINITY_ANY)==1&&work.calls==1&&badArguments==0,
            "scheduler invokes a full-width code pointer with all four original job arguments");
        Check(job.IsDone()&&backend.GetNumJobsInFlight()==0,"completion uses the high packed state bits");
        job.WaitOn(CancelWait,expectedWaitContext,-1);
        Check(waitCallbackCalls==1,"completed jobs return without invoking the wait callback");
        Job waiting("barrier");Work pending;Configure(waiting,pending);
        waiting.INTERNAL_AddNotReady(scheduler.mSchedulers);
        backend.TryToRunJobInstanceGarbageCollector(1);
        Check(!waiting.IsDone()&&backend.ExecuteReadyJobs(JOB_AFFINITY_ANY)==0,
            "garbage collection neither reclaims nor executes a not-ready job");
        waiting.mStartEvent.Run();
        Check(backend.ExecuteReadyJobs(JOB_AFFINITY_ANY)==1&&pending.calls==1,
            "the final start barrier makes the same pending generation ready");
        backend.TryToRunJobInstanceGarbageCollector(1);
        Check(job.IsDone()&&waiting.IsDone()&&allocator.Guards(),"completed generations remain done after reclamation");

        Work parentWork,childWork;childWork.prerequisite=&parentWork.calls;
        Job group[2]={Job("parent"),Job("child")};Configure(group[0],parentWork);Configure(group[1],childWork);
        group[0].mEntryPoint.mPriority=JOB_PRIORITY_LOW;group[1].mEntryPoint.mPriority=JOB_PRIORITY_HIGH;
        Job::Dependency dep{};dep.mJob=&group[0];dep.mTrigger=Event::EVENT_WHEN_JOB_END;
        group[1].mDependencies.Add(dep);scheduler.AddJobs(group,2);
        Check(backend.ExecuteReadyJobs(JOB_AFFINITY_ANY)==0 || parentWork.calls==1,
            "a higher-priority dependent cannot execute before its prerequisite");
        for(int n=0;n<4&&!group[1].IsDone();++n)backend.ExecuteReadyJobs(JOB_AFFINITY_ANY);
        Check(parentWork.calls==1&&childWork.calls==1&&badArguments==0,"dependency end events release the dependent exactly once");

        Job affinity("affinity");Work affinityWork;Configure(affinity,affinityWork);
        affinity.mEntryPoint.SetAffinity(JOB_AFFINITY_1);scheduler.AddJobs(&affinity,1);
        Check(backend.ExecuteReadyJobs(JOB_AFFINITY_0)==0&&affinityWork.calls==0,
            "worker affinity filters the high-word affinity field");
        Check(backend.ExecuteReadyJobs(JOB_AFFINITY_1)==1&&affinityWork.calls==1,"eligible worker executes the affinity-restricted job");

        std::atomic<int> eventCount{0};Job fanout("events");Work fanoutWork;Configure(fanout,fanoutWork);
        for(int i=0;i<40;++i)fanout.mEvents[Event::EVENT_WHEN_JOB_END].Add(Event(Event::EVENT_TYPE_CALLBACK,Count,&eventCount,nullptr));
        scheduler.AddJobs(&fanout,1);backend.ExecuteReadyJobs(JOB_AFFINITY_ANY);
        Check(eventCount==40&&allocator.Guards(),"overflow event lists use the installed jobs allocator and preserve every callback");
        Job eventOnly("barrier-only");std::atomic<int> phaseEvents{0};
        eventOnly.mEvents[Event::EVENT_WHEN_JOB_BEGIN].Add(Event(Event::EVENT_TYPE_CALLBACK,Count,&phaseEvents,nullptr));
        eventOnly.mEvents[Event::EVENT_WHEN_JOB_END].Add(Event(Event::EVENT_TYPE_CALLBACK,Count,&phaseEvents,nullptr));
        scheduler.AddJobs(&eventOnly,1);
        Check(backend.ExecuteReadyJobs(JOB_AFFINITY_ANY)==1&&eventOnly.IsDone()&&phaseEvents==2,
            "a job with no code still runs both event phases and completes");
    }
    Check(allocator.blocks.empty()&&allocator.Guards(),"backend teardown releases every owned allocation without guard damage");
    {
        ProfileCapture capture;
        JobScheduler scheduler;scheduler.Initialize(40,128);auto& backend=Back(scheduler);
        backend.SetProfilingCallback(CaptureProfile);backend.SetProfilingContext(&capture);
        Work work[30];auto jobs=ProfileJobs(std::make_index_sequence<30>{});
        for(int i=0;i<30;++i){Configure(jobs[i],work[i]);jobs[i].SetName("profile payload");}
        scheduler.AddJobs(jobs.data(),30);bool ran=true;
        for(int i=0;i<30;++i)ran=(backend.ExecuteReadyJobs(JOB_AFFINITY_ANY)==1)&&ran;
        backend.TryToRunJobInstanceGarbageCollector(1);
        Check(ran&&capture.records.size()==30&&capture.batches==std::vector<int>({23,7}),
            "profiling retains completed jobs until GC and publishes full batches plus remainder");
        bool valid=capture.records.size()==30;
        for(const auto& record:capture.records)valid=valid&&record.ticksAtSubmission<=record.ticksAtBegin
            &&record.ticksAtBegin<=record.ticksAtEnd&&record.threadId==GetCurrentThreadId()
            &&record.entryPoint.mpfnLocalJob==Body&&std::strcmp(record.entryPoint.GetName(),"profile payload")==0;
        Check(valid,"profiling exports native EntryPoint and ordered full-width timestamps after arguments are consumed");
        backend.TryToRunJobInstanceGarbageCollector(1);
        Check(capture.records.size()==30,"repeated GC does not republish completed profiling records");
    }
    {
        JobScheduler scheduler;scheduler.Initialize(32,128);scheduler.SetProfiling(false);
        JobThreadParameters parameters;
        scheduler.AddThread(parameters);scheduler.AddThread(parameters);scheduler.AddThread(parameters);
        Check(Back(scheduler).GetNumThreads()==3,"native EAThread starts three real job workers");
        HANDLE release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        std::atomic<int> active{0},peak{0};Work work[3];Job jobs[3]={Job("one"),Job("two"),Job("three")};
        for(int i=0;i<3;++i){work[i].active=&active;work[i].peak=&peak;work[i].release=release;Configure(jobs[i],work[i]);}
        scheduler.AddJobs(jobs,3);const auto until=GetTickCount64()+3000;
        while(peak<3&&GetTickCount64()<until)Sleep(1);
        SetEvent(release);bool finished=true;for(auto& job:jobs)finished=Done(job)&&finished;
        Check(finished&&peak==3&&work[0].calls==1&&work[1].calls==1&&work[2].calls==1&&badArguments==0,
            "three workers overlap execution and preserve payloads through real completion");
        CloseHandle(release);
        bool reused=true;
        for(int i=0;i<100&&reused;++i){Configure(jobs[0],work[0]);work[0].release=nullptr;scheduler.AddJobs(jobs,1);reused=Done(jobs[0]);}
        Check(reused&&work[0].calls==101,"job slots can be reused across more submissions than capacity");
        ProfileCapture workerProfile;scheduler.SetProfiling(true);
        Back(scheduler).SetProfilingCallback(CaptureProfile);Back(scheduler).SetProfilingContext(&workerProfile);
        Work profiledWork;Job profiled("worker profile");Configure(profiled,profiledWork);
        scheduler.AddJobs(&profiled,1);const bool profiledDone=Done(profiled);
        Back(scheduler).TryToRunJobInstanceGarbageCollector(1);
        bool knownWorker=false;
        if(workerProfile.records.size()==1)for(int i=0;i<3;++i)
            knownWorker=knownWorker||(workerProfile.records[0].threadId==Back(scheduler).GetThreadId(i));
        Check(profiledDone&&knownWorker&&profiledWork.calls==1,
            "profiling identifies an actual worker using the same IDs exposed by the backend");
        Work sleeping;Job sleepJob("sleepable");Configure(sleepJob,sleeping);sleepJob.mEntryPoint.mAllowSleepOn=true;
        sleeping.entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        sleeping.release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        scheduler.AddJobs(&sleepJob,1);
        bool entered=WaitForSingleObject(sleeping.entered,4000)==WAIT_OBJECT_0;
        std::atomic<bool> waiterReturned{false};
        std::thread waiter([&]{sleepJob.SleepOn();waiterReturned=true;});
        const auto waitDeadline=GetTickCount64()+2000;
        auto& instance=Back(scheduler).mpJobInstances[sleepJob.mJobInstanceHandle.mIndex];
        while(InterlockedCompareExchange(reinterpret_cast<volatile long*>(&instance.mGarbageCollectorLock),0,0)<2
            &&GetTickCount64()<waitDeadline)Sleep(1);
        Check(entered&&!waiterReturned,"sleepable jobs hold the native completion semaphore while their body is active");
        bool retained=instance.RefCount()->Increment();
        SetEvent(sleeping.release);waiter.join();
        Check(Done(sleepJob)&&waiterReturned&&sleeping.calls==1,"native completion wakes the sleeping caller");
        Back(scheduler).TryToRunJobInstanceGarbageCollector(1);
        Check(retained&&instance.mGarbageCollectorLock==2,"GC keeps a completed sleepable job while a reference is outstanding");
        instance.RefCount()->Decrement();Back(scheduler).TryToRunJobInstanceGarbageCollector(1);
        Check(instance.mGarbageCollectorLock==0&&sleepJob.IsDone(),"GC reclaims the semaphore after the final reference is released");
        CloseHandle(sleeping.entered);CloseHandle(sleeping.release);
        scheduler.Destroy();
    }
    Check(allocator.blocks.empty()&&allocator.Guards()&&badArguments==0,"joined workers leave no scheduler allocations or argument corruption");
    SetAllocator(nullptr);
    std::printf("PCNativeJobs: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
