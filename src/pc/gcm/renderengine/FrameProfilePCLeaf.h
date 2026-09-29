#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <new>

// FLAG PC-platform leaf: opt-in CPU frame tracing. No file I/O occurs inside a
// measured frame. BRN_FRAME_PROFILE=1 collects bounded records and writes them
// beside the executable on orderly shutdown (private harness slots stay private).
namespace renderengine
{
    namespace FrameProfile
    {
        enum Section { UPDATE, DISPATCH, GEOMETRY_PREPARE, GEOMETRY_LOCK,
                       GEOMETRY_CONVERT, GEOMETRY_UNLOCK, GEOMETRY_SUBMIT, PRESENT,
                       PRESENT_COPY, PRESENT_WAIT, DISPATCH_SORT, NUM_SECTIONS };
        struct Frame
        {
            LONGLONG miBegin = 0, miEnd = 0, miPreviousEnd = 0;
            LONGLONG maTicks[NUM_SECTIONS] = {};
            unsigned muVertexCreates = 0, muIndexCreates = 0, muEvictions = 0, muDraws = 0;
            unsigned long long muUploadedBytes = 0;
            int miCameraBegin = -1, miCameraEnd = -1;
            unsigned muCameraChanges = 0, muPresents = 0;
            unsigned muNativeBuffers = 0;
        };
        struct Capture
        {
            Frame* mpFrames = nullptr;
            Frame* mpCurrent = nullptr;
            unsigned muCount = 0;
            unsigned muDropped = 0;
            int miCamera = -1;
            LONGLONG miFrequency = 0, miPreviousEnd = 0;
            bool mbInitialized = false;
        };
        inline Capture gCapture;
        constexpr unsigned KU_CAPACITY = 65536;

        inline LONGLONG Now()
        {
            LARGE_INTEGER lValue;
            QueryPerformanceCounter(&lValue);
            return lValue.QuadPart;
        }
        inline void Begin()
        {
            if (!gCapture.mbInitialized)
            {
                gCapture.mbInitialized = true;
                const char* lpcEnable = std::getenv("BRN_FRAME_PROFILE");
                if (lpcEnable && lpcEnable[0] && lpcEnable[0] != '0')
                {
                    LARGE_INTEGER lFrequency;
                    if (!QueryPerformanceFrequency(&lFrequency) || lFrequency.QuadPart <= 0) return;
                    gCapture.miFrequency = lFrequency.QuadPart;
                    gCapture.mpFrames = new (std::nothrow) Frame[KU_CAPACITY];
                }
            }
            if (!gCapture.mpFrames) return;
            if (gCapture.muCount == KU_CAPACITY) { ++gCapture.muDropped; return; }
            Frame& lrFrame = gCapture.mpFrames[gCapture.muCount];
            lrFrame = Frame{};
            lrFrame.miPreviousEnd = gCapture.miPreviousEnd;
            lrFrame.miCameraBegin = gCapture.miCamera;
            lrFrame.miBegin = Now();
            gCapture.mpCurrent = &lrFrame;
        }
        inline void End()
        {
            Frame* lpFrame = gCapture.mpCurrent;
            if (!lpFrame) return;
            lpFrame->miEnd = Now();
            lpFrame->miCameraEnd = gCapture.miCamera;
            gCapture.miPreviousEnd = lpFrame->miEnd;
            gCapture.mpCurrent = nullptr;
            ++gCapture.muCount;
        }
        struct Scope
        {
            Frame* mpFrame;
            Section meSection;
            LONGLONG miBegin;
            explicit Scope(Section leSection)
                : mpFrame(gCapture.mpCurrent), meSection(leSection), miBegin(mpFrame ? Now() : 0) {}
            ~Scope() { if (mpFrame) mpFrame->maTicks[meSection] += Now() - miBegin; }
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
        };
        inline void Camera(int liCamera)
        {
            if (gCapture.mpCurrent && gCapture.miCamera != liCamera)
                ++gCapture.mpCurrent->muCameraChanges;
            gCapture.miCamera = liCamera;
        }
        inline void Geometry(bool lbVertex, unsigned luBytes)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                if (lbVertex) ++lpFrame->muVertexCreates;
                else ++lpFrame->muIndexCreates;
                lpFrame->muUploadedBytes += luBytes;
            }
        }
        inline void Retire() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muEvictions; }
        inline void Draw() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muDraws; }
        inline void Present() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muPresents; }
        inline void NativeBuffer() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muNativeBuffers; }
        inline void Finish()
        {
            if (!gCapture.mpFrames) return;
            char lacPath[MAX_PATH];
            const DWORD luLength = GetModuleFileNameA(nullptr, lacPath, MAX_PATH);
            if (luLength && luLength + 18 < MAX_PATH)
            {
                std::snprintf(lacPath + luLength, MAX_PATH - luLength, ".frames.csv");
                if (FILE* lpFile = std::fopen(lacPath, "w"))
                {
                    std::fprintf(lpFile, "frame,time_s,interval_ms,active_ms,update_ms,dispatch_ms,geometry_prepare_ms,geometry_lock_ms,geometry_convert_ms,geometry_unlock_ms,geometry_submit_ms,present_ms,present_copy_ms,present_wait_ms,dispatch_sort_ms,vb_creates,ib_creates,upload_bytes,evictions,draws,camera_begin,camera_end,camera_changes,presents,native_buffers\n");
                    const double lfMs = 1000.0 / static_cast<double>(gCapture.miFrequency);
                    for (unsigned lu = 0; lu < gCapture.muCount; ++lu)
                    {
                        const Frame& lr = gCapture.mpFrames[lu];
                        std::fprintf(lpFile, "%u,%.6f,%.6f,%.6f", lu,
                            (lr.miEnd - gCapture.mpFrames[0].miBegin) * lfMs / 1000.0,
                            (lr.miEnd - (lr.miPreviousEnd ? lr.miPreviousEnd : lr.miBegin)) * lfMs,
                            (lr.miEnd - lr.miBegin) * lfMs);
                        for (unsigned ls = 0; ls < NUM_SECTIONS; ++ls)
                            std::fprintf(lpFile, ",%.6f", lr.maTicks[ls] * lfMs);
                        std::fprintf(lpFile, ",%u,%u,%llu,%u,%u,%d,%d,%u,%u,%u\n", lr.muVertexCreates,
                            lr.muIndexCreates, lr.muUploadedBytes, lr.muEvictions, lr.muDraws,
                            lr.miCameraBegin, lr.miCameraEnd, lr.muCameraChanges, lr.muPresents, lr.muNativeBuffers);
                    }
                    std::fclose(lpFile);
                }
                std::snprintf(lacPath + luLength, MAX_PATH - luLength, ".frames.json");
                if (FILE* lpFile = std::fopen(lacPath, "w"))
                {
                    std::fprintf(lpFile, "{\"frames\":%u,\"dropped_frames\":%u,\"counter_frequency\":%lld}\n",
                        gCapture.muCount, gCapture.muDropped, gCapture.miFrequency);
                    std::fclose(lpFile);
                }
            }
            delete[] gCapture.mpFrames;
            gCapture.mpFrames = gCapture.mpCurrent = nullptr;
        }
    }
}
