#pragma once

#include <d3d9.h>
#include <cstdint>

// FLAG PC-platform leaf: D3D9 timestamp queries replace the X360 command-buffer
// timing callbacks. These are elapsed GPU timeline spans, including any GPU
// starvation. Older packets are read once per frame without FLUSH or a wait.
// https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries
namespace renderengine::GpuMonitorTimingPC
{
    constexpr unsigned MaxMonitors = 32;
    constexpr unsigned FrameTimer = MaxMonitors;
    constexpr unsigned NumTimers = MaxMonitors + 1;
    constexpr unsigned FrequencyQuery = NumTimers * 2;
    constexpr unsigned DisjointQuery = FrequencyQuery + 1;
    constexpr unsigned NumQueries = DisjointQuery + 1;
    enum Status { Pending, Valid, Invalid, Unavailable, RingFull, Reset };
    struct Result
    {
        std::uint64_t frame = 0, frequency = 0;
        std::uint64_t ticks[NumTimers] = {};
        std::uint32_t used = 0;
        Status status = Pending;
    };

    template<class Backend> class Recorder
    {
        using Query = typename Backend::Query;
        struct Packet
        {
            Query* queries[NumQueries] = {};
            std::uint64_t frame = 0;
            std::uint32_t used = 0, stopped = 0;
            int monitor = -1;
            bool pending = false, discarded = false;
        };
        static constexpr unsigned Capacity = 5;
        Backend& mBackend;
        void (*mpReport)(const Result&) = nullptr;
        Packet maPackets[Capacity];
        unsigned muNext = 0;
        int miActive = -1;
        bool mbTried = false, mbAvailable = false;

        void Report(std::uint64_t frame, Status status)
        {
            Result result;
            result.frame = frame;
            result.status = status;
            if (mpReport) mpReport(result);
        }
        bool Initialize()
        {
            if (mbTried) return mbAvailable;
            mbTried = true;
            for (Packet& packet : maPackets)
                for (unsigned i = 0; i < NumQueries; ++i)
                    if (FAILED(mBackend.Create(i, &packet.queries[i])) || !packet.queries[i])
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
            if (miActive >= 0) Report(maPackets[miActive].frame, Invalid);
            miActive = -1;
            // An incomplete Issue sequence is not reused until device reset.
            mbAvailable = false;
        }
    public:
        explicit Recorder(Backend& backend) : mBackend(backend) {}
        Recorder(const Recorder&) = delete;
        Recorder& operator=(const Recorder&) = delete;
        ~Recorder() { ResetQueries(false); }
        void SetReport(void (*report)(const Result&)) { mpReport = report; }

        void Poll()
        {
            for (Packet& packet : maPackets)
            {
                if (!packet.pending) continue;
                BOOL disjoint = TRUE;
                HRESULT hr = mBackend.Read(packet.queries[DisjointQuery], &disjoint, sizeof(disjoint));
                if (hr == S_FALSE) continue;
                if (FAILED(hr))
                {
                    packet.pending = false;
                    mbAvailable = false;
                    Report(packet.frame, Invalid);
                    continue;
                }
                if (packet.discarded)
                {
                    packet.pending = false;
                    continue;
                }
                Result result;
                result.frame = packet.frame;
                result.used = packet.used;
                bool ready = true, failed = false;
                auto Read = [&](unsigned index, std::uint64_t& value)
                {
                    const HRESULT read = mBackend.Read(packet.queries[index], &value, sizeof(value));
                    ready &= read == S_OK;
                    failed |= FAILED(read);
                };
                Read(FrequencyQuery, result.frequency);
                std::uint64_t begin[NumTimers] = {}, end[NumTimers] = {};
                for (unsigned i = 0; i < NumTimers; ++i)
                    if (i == FrameTimer || (packet.used & (std::uint32_t(1) << i)))
                    {
                        Read(NumTimers + i, begin[i]);
                        Read(i, end[i]);
                    }
                if (failed)
                {
                    packet.pending = false;
                    mbAvailable = false;
                    Report(packet.frame, Invalid);
                    continue;
                }
                if (!ready) continue;
                bool valid = !disjoint && result.frequency && end[FrameTimer] >= begin[FrameTimer];
                for (unsigned i = 0; i < NumTimers; ++i)
                    if (i == FrameTimer || (packet.used & (std::uint32_t(1) << i)))
                    {
                        valid &= end[i] >= begin[i] && begin[i] >= begin[FrameTimer]
                            && end[i] <= end[FrameTimer];
                        result.ticks[i] = end[i] - begin[i];
                    }
                packet.pending = false;
                result.status = valid ? Valid : Invalid;
                if (mpReport) mpReport(result);
            }
        }
        void Begin(std::uint64_t frame)
        {
            AbandonActive();
            Poll();
            if (!Initialize()) { Report(frame, Unavailable); return; }
            Packet& packet = maPackets[muNext];
            if (packet.pending) { Report(frame, RingFull); return; }
            packet.frame = frame;
            packet.used = packet.stopped = 0;
            packet.monitor = -1;
            packet.discarded = false;
            miActive = static_cast<int>(muNext);
            muNext = (muNext + 1) % Capacity;
            if (FAILED(mBackend.Issue(packet.queries[DisjointQuery], D3DISSUE_BEGIN)) ||
                FAILED(mBackend.Issue(packet.queries[NumTimers + FrameTimer], D3DISSUE_END)))
                FailActive();
        }
        void Start(unsigned monitor)
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            if (monitor >= MaxMonitors || packet.monitor >= 0 ||
                (packet.used & (std::uint32_t(1) << monitor))) { FailActive(); return; }
            packet.monitor = static_cast<int>(monitor);
            packet.used |= std::uint32_t(1) << monitor;
            if (FAILED(mBackend.Issue(packet.queries[NumTimers + monitor], D3DISSUE_END))) FailActive();
        }
        void Stop(unsigned monitor)
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            if (monitor >= MaxMonitors || packet.monitor != static_cast<int>(monitor))
            { FailActive(); return; }
            if (FAILED(mBackend.Issue(packet.queries[monitor], D3DISSUE_END))) { FailActive(); return; }
            packet.stopped |= std::uint32_t(1) << monitor;
            packet.monitor = -1;
        }
        void End()
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            if (packet.monitor >= 0 || packet.used != packet.stopped ||
                FAILED(mBackend.Issue(packet.queries[FrameTimer], D3DISSUE_END)) ||
                FAILED(mBackend.Issue(packet.queries[FrequencyQuery], D3DISSUE_END)) ||
                FAILED(mBackend.Issue(packet.queries[DisjointQuery], D3DISSUE_END)))
            { FailActive(); return; }
            packet.pending = true;
            Report(packet.frame, Pending);
            miActive = -1;
        }
        void AbandonActive()
        {
            if (miActive < 0) return;
            Packet& packet = maPackets[miActive];
            if (FAILED(mBackend.Issue(packet.queries[DisjointQuery], D3DISSUE_END)))
            { FailActive(); return; }
            // Drain the last marker before reusing an interrupted packet. It
            // must never be reported as the timing of a complete game frame.
            packet.pending = packet.discarded = true;
            Report(packet.frame, Invalid);
            miActive = -1;
        }
        void ResetQueries(bool report = true)
        {
            miActive = -1;
            for (Packet& packet : maPackets)
            {
                for (Query*& query : packet.queries) mBackend.Release(query);
                packet.pending = packet.discarded = false;
            }
            muNext = 0;
            mbTried = mbAvailable = false;
            if (report) Report(0, Reset);
        }
    };

    struct NativeBackend
    {
        using Query = IDirect3DQuery9;
        IDirect3DDevice9* device = nullptr;
        HRESULT Create(unsigned index, Query** query)
        {
            const D3DQUERYTYPE type = index == FrequencyQuery ? D3DQUERYTYPE_TIMESTAMPFREQ
                : index == DisjointQuery ? D3DQUERYTYPE_TIMESTAMPDISJOINT : D3DQUERYTYPE_TIMESTAMP;
            return device ? device->CreateQuery(type, query) : D3DERR_INVALIDCALL;
        }
        HRESULT Issue(Query* query, DWORD flags) { return query->Issue(flags); }
        HRESULT Read(Query* query, void* data, DWORD bytes) { return query->GetData(data, bytes, 0); }
        void Release(Query*& query) { if (query) query->Release(); query = nullptr; }
    };
    inline NativeBackend gBackend;
    inline Recorder<NativeBackend> gRecorder(gBackend);
    inline std::uint64_t guFrame = 0;
    inline void Begin(IDirect3DDevice9* device, void (*report)(const Result&))
    {
        gRecorder.SetReport(report);
        if (gBackend.device != device) { gRecorder.ResetQueries(); gBackend.device = device; }
        gRecorder.Begin(++guFrame);
    }
    inline void DeviceReset()
    {
        gRecorder.ResetQueries();
        gBackend.device = nullptr;
    }
    inline void AbandonCurrentFrame() { gRecorder.AbandonActive(); }
}
