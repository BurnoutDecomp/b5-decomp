#pragma once

#include <d3d9.h>
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"

// FLAG PC-platform leaf: optional D3D9 timestamp attribution. Results describe
// elapsed GPU-timeline spans (which can include starvation), not GPU busy time.
// Read older queries without FLUSH; pending results never delay rendering.
// Query types and result widths: https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries
namespace renderengine::GpuFrameTimingPC
{
    enum Status : unsigned { Off, Pending, Valid, Invalid, Unavailable, RingFull };
    enum Point : unsigned { BeginStamp, SceneStamp, OutputStamp, Frequency, Disjoint, NumQueries };
    struct Result
    {
        unsigned frame = 0;
        Status status = Off;
        double sceneMs = -1, outputMs = -1;
    };

    template<class Backend> class Recorder
    {
        using Query = typename Backend::Query;
        struct Packet
        {
            Query* queries[NumQueries] = {};
            unsigned frame = 0;
            bool pending = false, sceneMarked = false;
        };
        static constexpr unsigned Capacity = 8;
        Backend& mBackend;
        void (*mpReport)(const Result&);
        Packet maPackets[Capacity];
        unsigned muNext = 0;
        int miActive = -1;
        bool mbTried = false, mbAvailable = false;

        void Report(Packet& packet, Status status, double scene = -1, double output = -1)
        {
            if (mpReport) mpReport(Result{packet.frame, status, scene, output});
            packet.pending = false;
        }
        bool Initialize()
        {
            if (mbTried) return mbAvailable;
            mbTried = true;
            for (Packet& packet : maPackets)
                for (unsigned i = 0; i < NumQueries; ++i)
                    if (FAILED(mBackend.Create(static_cast<Point>(i), &packet.queries[i])) || !packet.queries[i])
                    {
                        for (Packet& cleanup : maPackets)
                            for (Query*& query : cleanup.queries) mBackend.Release(query);
                        return false;
                    }
            mbAvailable = true;
            return true;
        }
        void FailActive()
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            // An incomplete Issue sequence must not be reused as a fresh sample.
            // Disable this recorder until Reset; all later frames report unavailable.
            Report(packet, Invalid);
            miActive = -1;
            mbAvailable = false;
        }
    public:
        Recorder(Backend& backend, void (*report)(const Result&)) : mBackend(backend), mpReport(report) {}
        Recorder(const Recorder&) = delete;
        Recorder& operator=(const Recorder&) = delete;
        ~Recorder() { Reset(); }

        void Poll()
        {
            for (Packet& packet : maPackets)
            {
                if (!packet.pending) continue;
                unsigned long long begin = 0, scene = 0, output = 0, frequency = 0;
                BOOL disjoint = TRUE;
                // Check the last marker first. Never spin, flush, or reinterpret
                // S_FALSE (a successful HRESULT) as a completed result.
                const HRESULT hrDisjoint = mBackend.Read(packet.queries[Disjoint], &disjoint, sizeof(disjoint));
                if (hrDisjoint == S_FALSE) continue;
                if (FAILED(hrDisjoint)) { Report(packet, Invalid); mbAvailable = false; continue; }
                const HRESULT reads[] = {
                    mBackend.Read(packet.queries[BeginStamp], &begin, sizeof(begin)),
                    mBackend.Read(packet.queries[SceneStamp], &scene, sizeof(scene)),
                    mBackend.Read(packet.queries[OutputStamp], &output, sizeof(output)),
                    mBackend.Read(packet.queries[Frequency], &frequency, sizeof(frequency))};
                bool ready = true, failed = false;
                for (HRESULT hr : reads) { ready &= hr == S_OK; failed |= FAILED(hr); }
                if (failed) { Report(packet, Invalid); mbAvailable = false; continue; }
                if (!ready) continue;
                if (disjoint || !frequency || scene < begin || output < scene)
                    Report(packet, Invalid);
                else
                    Report(packet, Valid, double(scene - begin) * 1000.0 / double(frequency),
                        double(output - scene) * 1000.0 / double(frequency));
            }
        }
        void Begin(unsigned frame)
        {
            if (miActive >= 0) FailActive();
            Poll();
            Packet& packet = maPackets[muNext];
            if (!Initialize()) { if (mpReport) mpReport(Result{frame, Unavailable}); return; }
            if (packet.pending) { if (mpReport) mpReport(Result{frame, RingFull}); return; }
            packet.frame = frame;
            packet.sceneMarked = false;
            miActive = static_cast<int>(muNext);
            muNext = (muNext + 1) % Capacity;
            if (FAILED(mBackend.Issue(packet.queries[Disjoint], D3DISSUE_BEGIN)) ||
                FAILED(mBackend.Issue(packet.queries[BeginStamp], D3DISSUE_END)))
                FailActive();
        }
        void SceneComplete()
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            if (packet.sceneMarked) return;
            if (FAILED(mBackend.Issue(packet.queries[SceneStamp], D3DISSUE_END))) FailActive();
            else packet.sceneMarked = true;
        }
        void OutputComplete()
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            if (!packet.sceneMarked ||
                FAILED(mBackend.Issue(packet.queries[OutputStamp], D3DISSUE_END)) ||
                FAILED(mBackend.Issue(packet.queries[Frequency], D3DISSUE_END)) ||
                FAILED(mBackend.Issue(packet.queries[Disjoint], D3DISSUE_END)))
            { FailActive(); return; }
            packet.pending = true;
            if (mpReport) mpReport(Result{packet.frame, Pending});
            miActive = -1;
        }
        void Abandon(unsigned frame)
        {
            for (unsigned i = 0; i < Capacity; ++i)
            {
                Packet& packet = maPackets[i];
                if (packet.frame != frame) continue;
                if (miActive == static_cast<int>(i))
                {
                    if (FAILED(mBackend.Issue(packet.queries[Disjoint], D3DISSUE_END))) mbAvailable = false;
                    miActive = -1;
                }
                packet.pending = false;
            }
            // An assert can present an interrupted prefix before opening its
            // no-clear overlay. Invalidate even an already completed prefix.
            if (mpReport) mpReport(Result{frame, Invalid});
        }
        void Reset()
        {
            if (miActive >= 0) FailActive();
            for (Packet& packet : maPackets)
            {
                if (packet.pending) Report(packet, Invalid);
                for (Query*& query : packet.queries) mBackend.Release(query);
            }
            muNext = 0;
            miActive = -1;
            mbTried = mbAvailable = false;
        }
    };

    struct NativeBackend
    {
        using Query = IDirect3DQuery9;
        IDirect3DDevice9* device = nullptr;
        HRESULT Create(Point point, Query** query)
        {
            const D3DQUERYTYPE type = point == Frequency ? D3DQUERYTYPE_TIMESTAMPFREQ
                : point == Disjoint ? D3DQUERYTYPE_TIMESTAMPDISJOINT : D3DQUERYTYPE_TIMESTAMP;
            return device ? device->CreateQuery(type, query) : D3DERR_INVALIDCALL;
        }
        HRESULT Issue(Query* query, DWORD flags) { return query->Issue(flags); }
        HRESULT Read(Query* query, void* data, DWORD bytes) { return query->GetData(data, bytes, 0); }
        void Release(Query*& query) { if (query) query->Release(); query = nullptr; }
    };
    inline void Report(const Result& result)
    {
        if (!FrameProfile::gCapture.mpFrames || result.frame >= FrameProfile::KU_CAPACITY) return;
        FrameProfile::Frame& frame = FrameProfile::gCapture.mpFrames[result.frame];
        frame.muGpuStatus = result.status;
        frame.mfGpuSceneMs = result.sceneMs;
        frame.mfGpuOutputMs = result.outputMs;
    }
    inline NativeBackend gBackend;
    inline Recorder<NativeBackend> gRecorder(gBackend, Report);
    inline void Finish()
    {
        gRecorder.Poll();
        gRecorder.Reset(); // unfinished queries remain explicitly invalid, no shutdown wait
        gBackend.device = nullptr;
    }
    inline void Begin(IDirect3DDevice9* device)
    {
        if (!FrameProfile::gCapture.mbGpuTiming || !FrameProfile::Active()) return;
        if (gBackend.device != device) { gRecorder.Reset(); gBackend.device = device; }
        FrameProfile::gCapture.mpFinishGpu = Finish;
        gRecorder.Begin(FrameProfile::gCapture.muCount);
    }
    inline void SceneComplete() { gRecorder.SceneComplete(); }
    inline void DeviceReset()
    {
        // Reset/lost-device transitions invalidate outstanding timestamp spans.
        // Release is safe even while lost; do not poll the device here.
        gRecorder.Reset();
        gBackend.device=nullptr;
    }
    inline void OutputComplete() { gRecorder.OutputComplete(); }
    inline void AbandonCurrentFrame()
    {
        if (FrameProfile::gCapture.mbGpuTiming && FrameProfile::Active())
            gRecorder.Abandon(FrameProfile::gCapture.muCount);
    }
}
