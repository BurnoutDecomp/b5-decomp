#pragma once

// CgsSystem::IThreadClass - the engine's thread-callback interface. A class that drives the
// update/dispatch/resource threads implements it; ThreadLayout calls these back on the
// appropriate threads. BrnGameModule is the concrete implementor. Interface recovered from
// the DecFIGS DWARF (System/Threads/CgsThreadLayout.h); only the interface is declared here
// (ThreadLayout itself is its own TU).

class Mutex;
struct AssertData;

namespace CgsSystem
{
    struct IThreadClass
    {
        virtual bool UpdateThread() = 0;
        virtual void DispatchThread() = 0;
        virtual void ResourceUpdateThread(Mutex* lpMutex) = 0;
        virtual void OnStartOfUpdateFrame() = 0;
        virtual void OnEndOfUpdateFrame() = 0;
        virtual void OnCompletionOfVsyncWait() = 0;
        virtual void RenderAssert(const AssertData* lpAssertData) = 0;
    };
}
