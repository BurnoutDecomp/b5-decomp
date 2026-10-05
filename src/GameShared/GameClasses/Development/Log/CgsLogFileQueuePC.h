#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace CgsDev { namespace Log {

// FLAG PC-platform leaf: bounded native file-output queue. The consumer never
// holds the queue lock during IO. Lifecycle/flush calls are serialized by the
// log front end; producers may enqueue concurrently. Stop must precede storage
// destruction. No allocation or file operation occurs in TryWrite.
template<size_t KU_CAPACITY = 256u * 1024u, size_t KU_BATCH = 32u * 1024u>
class LogFileQueuePC
{
public:
    using Sink = bool (*)(const char*, size_t);
    struct Statistics
    {
        unsigned long long muDroppedWrites = 0, muDroppedBytes = 0, muWriteErrors = 0;
    };
    static_assert(KU_CAPACITY > 0 && KU_BATCH > 0 && KU_BATCH <= KU_CAPACITY, "valid log queue sizes");

    bool Start(Sink lpSink)
    {
        if (!lpSink || mhThread || mbStopping) return false;
        mpSink = lpSink;
        mhWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        mhDrained = CreateEventW(nullptr, TRUE, TRUE, nullptr);
        if (!mhWake || !mhDrained) { CloseEvents(); return false; }
        mhThread = CreateThread(nullptr, 0, &ThreadMain, this, 0, &muThreadId);
        if (!mhThread) { CloseEvents(); return false; }
        return true;
    }

    bool TryWrite(const char* lpcText, size_t luBytes)
    {
        if (!lpcText && luBytes) return false;
        AcquireSRWLockExclusive(&mLock);
        if (mbStopping || !mhThread)
        {
            ReleaseSRWLockExclusive(&mLock);
            return false;
        }
        if (luBytes > KU_CAPACITY - muUsed)
        {
            ++mStatistics.muDroppedWrites;
            mStatistics.muDroppedBytes += luBytes;
            ReleaseSRWLockExclusive(&mLock);
            return false;
        }
        if (luBytes)
        {
            const size_t luTail = (muHead + muUsed) % KU_CAPACITY;
            const size_t luFirst = luBytes < KU_CAPACITY - luTail ? luBytes : KU_CAPACITY - luTail;
            std::memcpy(macQueue + luTail, lpcText, luFirst);
            std::memcpy(macQueue, lpcText + luFirst, luBytes - luFirst);
            muUsed += luBytes;
            ResetEvent(mhDrained);
            SetEvent(mhWake);
        }
        ReleaseSRWLockExclusive(&mLock);
        return true;
    }

    bool Flush()
    {
        if (!mhThread) return true;
        if (GetCurrentThreadId() == muThreadId) return false;
        return WaitForSingleObject(mhDrained, INFINITE) == WAIT_OBJECT_0;
    }

    void Stop()
    {
        if (!mhThread) return;
        AcquireSRWLockExclusive(&mLock);
        mbStopping = true;
        SetEvent(mhWake);
        ReleaseSRWLockExclusive(&mLock);
        WaitForSingleObject(mhThread, INFINITE);
        AcquireSRWLockExclusive(&mLock);
        CloseHandle(mhThread);
        mhThread = nullptr;
        CloseEvents();
        ReleaseSRWLockExclusive(&mLock);
    }

    Statistics GetStatistics()
    {
        AcquireSRWLockExclusive(&mLock);
        const Statistics lResult = mStatistics;
        ReleaseSRWLockExclusive(&mLock);
        return lResult;
    }

private:
    static DWORD WINAPI ThreadMain(void* lpContext)
    {
        static_cast<LogFileQueuePC*>(lpContext)->Run();
        return 0;
    }
    void Run()
    {
        for (;;)
        {
            WaitForSingleObject(mhWake, INFINITE);
            for (;;)
            {
                AcquireSRWLockExclusive(&mLock);
                const size_t luBytes = muUsed < KU_BATCH ? muUsed : KU_BATCH;
                if (!luBytes)
                {
                    const bool lbStop = mbStopping;
                    SetEvent(mhDrained);
                    ReleaseSRWLockExclusive(&mLock);
                    if (lbStop)
                    {
                        // Overflow never makes gameplay wait for a slow disk.
                        // Keep the loss explicit in the final log instead.
                        if (mStatistics.muDroppedWrites)
                        {
                            char lacNotice[160];
                            const int liBytes = std::snprintf(lacNotice, sizeof(lacNotice),
                                "\n[log] dropped %llu queued writes (%llu bytes) while the file writer was busy\n",
                                mStatistics.muDroppedWrites, mStatistics.muDroppedBytes);
                            if (!mpSink(lacNotice, static_cast<size_t>(liBytes)))
                            {
                                AcquireSRWLockExclusive(&mLock);
                                ++mStatistics.muWriteErrors;
                                ReleaseSRWLockExclusive(&mLock);
                            }
                        }
                        return;
                    }
                    break;
                }
                const size_t luFirst = luBytes < KU_CAPACITY - muHead ? luBytes : KU_CAPACITY - muHead;
                std::memcpy(macBatch, macQueue + muHead, luFirst);
                std::memcpy(macBatch + luFirst, macQueue, luBytes - luFirst);
                muHead = (muHead + luBytes) % KU_CAPACITY;
                muUsed -= luBytes;
                ReleaseSRWLockExclusive(&mLock);

                // This is the only call that can block on the file system.
                const bool lbWritten = mpSink(macBatch, luBytes);
                AcquireSRWLockExclusive(&mLock);
                if (!lbWritten) ++mStatistics.muWriteErrors;
                if (!muUsed) SetEvent(mhDrained);
                ReleaseSRWLockExclusive(&mLock);
            }
        }
    }
    void CloseEvents()
    {
        if (mhWake) CloseHandle(mhWake);
        if (mhDrained) CloseHandle(mhDrained);
        mhWake = mhDrained = nullptr;
    }

    SRWLOCK mLock = SRWLOCK_INIT;
    HANDLE mhWake = nullptr, mhDrained = nullptr, mhThread = nullptr;
    DWORD muThreadId = 0;
    Sink mpSink = nullptr;
    size_t muHead = 0, muUsed = 0;
    bool mbStopping = false;
    Statistics mStatistics;
    char macQueue[KU_CAPACITY]{}, macBatch[KU_BATCH]{};
};

} }
