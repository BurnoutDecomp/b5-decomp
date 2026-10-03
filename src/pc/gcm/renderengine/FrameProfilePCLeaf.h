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
// BRN_FRAME_TIMING_ONLY=1 keeps frame endpoints/counters but skips every section
// timer, including the ordinary per-draw timers, for frame-pacing measurements.
// BRN_FRAME_COARSE=1 keeps frame/update/render-stage attribution without per-draw
// clock reads. Timing-only still takes precedence over both diagnostic modes.
namespace renderengine
{
    namespace FrameProfile
    {
        enum Section { UPDATE, DISPATCH, GEOMETRY_PREPARE, GEOMETRY_LOCK,
                       GEOMETRY_CONVERT, GEOMETRY_UNLOCK, GEOMETRY_SUBMIT, PRESENT,
                       PRESENT_COPY, PRESENT_WAIT, DISPATCH_SORT,
                       UPDATE_DISPLAY, UPDATE_START, UPDATE_SIMULATION,
                       UPDATE_RESOURCE, UPDATE_PUBLISH, UPDATE_TIMING,
                       RESOURCE_POOL, RESOURCE_MEMORY, RESOURCE_LOAD, RESOURCE_UNLOAD,
                       RESOURCE_FILE, RESOURCE_ATTRIB, OBJECT_TO_MESH,
                       MESH_TECHNIQUE, MESH_CONSTANTS, MESH_BUFFERS,
                       GEOMETRY_LOOKUP, WORLD_DRAW, IMMEDIATE_DRAW, POSTFX,
                       DISPATCH_EFFECTS, RENDER_SETUP, RENDER_BUILD_LISTS, RENDER_TINT,
                       RENDER_SHADOWS, RENDER_ENVMAP, RENDER_PARTICLE_BUILD,
                       RENDER_WORLD, RENDER_PARTICLES, RENDER_COMPOSITE, RENDER_GUI,
                       RENDER_PRESENT, GEOMETRY_CREATE, GEOMETRY_COPY,
                       POOL_REQUESTS, POOL_STATES, POOL_RETIRE, TEXTURE_REALIZE, TEXTURE_GPU_CREATE, TEXTURE_CPU_CREATE, TEXTURE_UPLOAD, UPDATE_MESH_PREPARE, NUM_SECTIONS };
        struct Frame
        {
            LONGLONG miBegin = 0, miEnd = 0, miPreviousEnd = 0;
            LONGLONG maTicks[NUM_SECTIONS] = {};
            unsigned long long maCycles[NUM_SECTIONS] = {};
            unsigned muMeshPrepared = 0, muMeshRebuilt = 0;
            LONG muCycleReadFailures = 0;
            unsigned muRasterCreates = 0, muRasterReleases = 0;
            unsigned muStagingCreates = 0, muStagingReuses = 0;
            unsigned muVertexCreates = 0, muIndexCreates = 0, muEvictions = 0, muDraws = 0;
            unsigned long long muUploadedBytes = 0;
            int miCameraBegin = -1, miCameraEnd = -1;
            unsigned muCameraChanges = 0, muPresents = 0;
            unsigned long long muBehaviourBegin = 0, muBehaviourEnd = 0;
            unsigned long long muShotBegin = 0, muShotEnd = 0;
            unsigned muCameraPublications = 0, muNewCameraPublications = 0;
            unsigned muBehaviourChanges = 0, muShotChanges = 0;
            unsigned muNativeBuffers = 0;
            unsigned muPlayerTakedowns = 0, muTakedownVictims = 0;
            unsigned muRivals = 0, muCrashingRivals = 0, muAirborneRivals = 0;
            unsigned muInstancedDraws = 0, muInstances = 0;
            unsigned muPreZMeshes = 0, muWorldOpaqueMeshes = 0, muCarOpaqueMeshes = 0;
            unsigned muGpuStatus = 0;
            double mfGpuSceneMs = -1, mfGpuOutputMs = -1;
            unsigned muVertexBindRequests = 0, muVertexBindSkips = 0;
            unsigned muIndexBindRequests = 0, muIndexBindSkips = 0, muRebasedDraws = 0;
        };
        struct Capture
        {
            Frame* mpFrames = nullptr;
            Frame* mpCurrent = nullptr;
            unsigned muCount = 0;
            unsigned muDropped = 0;
            int miCamera = -1;
            unsigned long long muBehaviour = 0, muShot = 0;
            LONGLONG miFrequency = 0, miPreviousEnd = 0;
            bool mbInitialized = false;
            bool mbDetailed = false;
            bool mbTimingOnly = false;
            bool mbCoarse = false;
            bool mbCpuCycles = false;
            using ReadThreadCycles = BOOL(WINAPI*)(HANDLE, PULONG64);
            ReadThreadCycles mpReadThreadCycles = nullptr;
            bool mbGpuTiming = false;
            void (*mpFinishGpu)() = nullptr;
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
                const char* lpcDetail = std::getenv("BRN_FRAME_DETAIL");
                gCapture.mbDetailed = lpcDetail && lpcDetail[0] && lpcDetail[0] != '0';
                const char* lpcTimingOnly = std::getenv("BRN_FRAME_TIMING_ONLY");
                gCapture.mbTimingOnly = lpcTimingOnly && lpcTimingOnly[0] && lpcTimingOnly[0] != '0';
                const char* lpcCoarse = std::getenv("BRN_FRAME_COARSE");
                gCapture.mbCoarse = lpcCoarse && lpcCoarse[0] && lpcCoarse[0] != '0';
                const char* lpcCycles = std::getenv("BRN_FRAME_CPU_CYCLES");
                gCapture.mbCpuCycles = lpcCycles && lpcCycles[0] && lpcCycles[0] != '0';
                const char* lpcGpu = std::getenv("BRN_GPU_PROFILE");
                gCapture.mbGpuTiming = lpcGpu && lpcGpu[0] && lpcGpu[0] != '0';
                if (lpcEnable && lpcEnable[0] && lpcEnable[0] != '0')
                {
                    LARGE_INTEGER lFrequency;
                    if (!QueryPerformanceFrequency(&lFrequency) || lFrequency.QuadPart <= 0) return;
                    gCapture.miFrequency = lFrequency.QuadPart;
                    gCapture.mpFrames = new (std::nothrow) Frame[KU_CAPACITY];
                    if (gCapture.mpFrames && gCapture.mbCpuCycles && !gCapture.mbTimingOnly)
                        gCapture.mpReadThreadCycles = reinterpret_cast<Capture::ReadThreadCycles>(
                            GetProcAddress(GetModuleHandleA("kernel32.dll"), "QueryThreadCycleTime"));
                }
            }
            if (!gCapture.mpFrames) return;
            if (gCapture.muCount == KU_CAPACITY) { ++gCapture.muDropped; return; }
            Frame& lrFrame = gCapture.mpFrames[gCapture.muCount];
            lrFrame = Frame{};
            lrFrame.miPreviousEnd = gCapture.miPreviousEnd;
            lrFrame.miCameraBegin = gCapture.miCamera;
            lrFrame.muBehaviourBegin = lrFrame.muBehaviourEnd = gCapture.muBehaviour;
            lrFrame.muShotBegin = lrFrame.muShotEnd = gCapture.muShot;
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
            static bool IsCoarseSection(Section leSection)
            {
                return leSection == UPDATE || leSection == DISPATCH
                    // These run only on a new geometry mirror, not cache hits.
                    || (leSection >= GEOMETRY_PREPARE && leSection <= GEOMETRY_UNLOCK)
                    || (leSection >= PRESENT && leSection <= RESOURCE_ATTRIB)
                    || leSection >= DISPATCH_EFFECTS;
            }
            static Frame* FrameFor(Section leSection, bool lbEnabled)
            {
                return lbEnabled && !gCapture.mbTimingOnly
                    && (!gCapture.mbCoarse || IsCoarseSection(leSection))
                    ? gCapture.mpCurrent : nullptr;
            }
            Frame* mpFrame;
            Section meSection;
            Section meDetail;
            LONGLONG miBegin;
            explicit Scope(Section leSection, Section leDetail = NUM_SECTIONS, bool lbEnabled = true)
                : mpFrame(FrameFor(leSection, lbEnabled)),
                  meSection(leSection), meDetail(leDetail),
                  miBegin(mpFrame ? Now() : 0) {}
            ~Scope()
            {
                if (!mpFrame) return;
                const LONGLONG liElapsed = Now() - miBegin;
                mpFrame->maTicks[meSection] += liElapsed;
                if (meDetail != NUM_SECTIONS) mpFrame->maTicks[meDetail] += liElapsed;
            }
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
        };
        // Extra per-draw sections are separately enabled. Timing-only mode
        // suppresses both these and the ordinary section timers above.
        struct DetailScope : Scope
        {
            explicit DetailScope(Section leSection)
                : Scope(leSection, NUM_SECTIONS, gCapture.mbDetailed) {}
        };
        // Sparse diagnostic spans only. CPU cycles are not elapsed time and must
        // never be converted to milliseconds or GHz. Availability/failure is
        // recorded separately, so a failed query is not reported as zero work.
        struct CycleScope : Scope
        {
            ULONG64 muBeginCycles = 0;
            bool mbCyclesValid = false;
            bool Read(ULONG64& lruCycles)
            {
                if (gCapture.mpReadThreadCycles
                    && gCapture.mpReadThreadCycles(GetCurrentThread(), &lruCycles)) return true;
                InterlockedIncrement(&mpFrame->muCycleReadFailures);
                return false;
            }
            explicit CycleScope(Section leSection, Section leDetail = NUM_SECTIONS)
                : Scope(leSection, leDetail)
            {
                if (mpFrame && gCapture.mbCpuCycles) mbCyclesValid = Read(muBeginCycles);
            }
            ~CycleScope()
            {
                if (!mbCyclesValid) return;
                ULONG64 luEnd = 0;
                if (Read(luEnd))
                {
                    if (luEnd >= muBeginCycles)
                    {
                        mpFrame->maCycles[meSection] += luEnd - muBeginCycles;
                        if (meDetail != NUM_SECTIONS)
                            mpFrame->maCycles[meDetail] += luEnd - muBeginCycles;
                    }
                    else InterlockedIncrement(&mpFrame->muCycleReadFailures);
                }
            }
        };
        // Consecutive stages share boundary timestamps: each interval belongs
        // to exactly one stage, including early returns. Only the render owner
        // uses this object. It does not add clocks when recording is disabled.
        struct Stage
        {
            Frame* mpFrame;
            Section meSection;
            LONGLONG miBegin;
            explicit Stage(Section leSection)
                : mpFrame(Scope::FrameFor(leSection, true)), meSection(leSection),
                  miBegin(mpFrame ? Now() : 0) {}
            void Next(Section leSection)
            {
                if (!mpFrame) return;
                const LONGLONG liNow = Now();
                mpFrame->maTicks[meSection] += liNow - miBegin;
                meSection = leSection;
                miBegin = liNow;
            }
            ~Stage()
            {
                if (mpFrame) mpFrame->maTicks[meSection] += Now() - miBegin;
            }
            Stage(const Stage&) = delete;
            Stage& operator=(const Stage&) = delete;
        };
        inline void Camera(int liCamera)
        {
            if (gCapture.mpCurrent && gCapture.miCamera != liCamera)
                ++gCapture.mpCurrent->muCameraChanges;
            gCapture.miCamera = liCamera;
        }
        // The arbitrator state is not a shot identity: a takedown can publish a
        // new shot while staying in the same state. Record the actual published
        // camera's producers and native NEW_THIS_FRAME flag, without dereferencing
        // retained pointers or inferring cuts from camera motion. Several simulation
        // publications may occur in one rendered frame, so count publications.
        inline void CameraOutput(const void* lpBehaviour, const void* lpShot, bool lbNew)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                const auto luBehaviour = static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(lpBehaviour));
                const auto luShot = static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(lpShot));
                ++lpFrame->muCameraPublications;
                lpFrame->muNewCameraPublications += lbNew;
                lpFrame->muBehaviourChanges += gCapture.muBehaviour != luBehaviour;
                lpFrame->muShotChanges += gCapture.muShot != luShot;
                lpFrame->muBehaviourEnd = gCapture.muBehaviour = luBehaviour;
                lpFrame->muShotEnd = gCapture.muShot = luShot;
            }
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
        inline void TextureStaging(bool lbReused)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                if (lbReused) ++lpFrame->muStagingReuses;
                else ++lpFrame->muStagingCreates;
            }
        }
        inline void ResourceTexture(bool lbReleased)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                if (lbReleased) ++lpFrame->muRasterReleases;
                else ++lpFrame->muRasterCreates;
            }
        }
        inline void Retire() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muEvictions; }
        inline void Draw() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muDraws; }
        inline void Present() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muPresents; }
        inline void NativeBuffer() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muNativeBuffers; }
        inline void GeometryBinding(bool lbVertex, bool lbSkipped)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                if (lbVertex) { ++lpFrame->muVertexBindRequests; lpFrame->muVertexBindSkips += lbSkipped; }
                else { ++lpFrame->muIndexBindRequests; lpFrame->muIndexBindSkips += lbSkipped; }
            }
        }
        inline void RebasedDraw() { if (gCapture.mpCurrent) ++gCapture.mpCurrent->muRebasedDraws; }
        inline void Instanced(unsigned count) { if (gCapture.mpCurrent) {
            ++gCapture.mpCurrent->muInstancedDraws; gCapture.mpCurrent->muInstances += count; } }
        inline void SceneLists(unsigned preZ, unsigned world, unsigned cars) { if (gCapture.mpCurrent) {
            gCapture.mpCurrent->muPreZMeshes += preZ; gCapture.mpCurrent->muWorldOpaqueMeshes += world;
            gCapture.mpCurrent->muCarOpaqueMeshes += cars; } }
        inline bool Active() { return gCapture.mpCurrent != nullptr; }
        inline void PlayerTakedown(int liVictim)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                ++lpFrame->muPlayerTakedowns;
                if (liVictim >= 0 && liVictim < 32)
                    lpFrame->muTakedownVictims |= 1u << liVictim;
            }
        }
        inline void RivalSample(unsigned luRivals, unsigned luCrashing, unsigned luAirborne)
        {
            if (Frame* lpFrame = gCapture.mpCurrent)
            {
                // Keep the busiest simulation step when a render frame catches up.
                if (luRivals > lpFrame->muRivals) lpFrame->muRivals = luRivals;
                if (luCrashing > lpFrame->muCrashingRivals) lpFrame->muCrashingRivals = luCrashing;
                if (luAirborne > lpFrame->muAirborneRivals) lpFrame->muAirborneRivals = luAirborne;
            }
        }
        inline void Finish()
        {
            if (!gCapture.mpFrames) return;
            if (gCapture.mpFinishGpu) { gCapture.mpFinishGpu(); gCapture.mpFinishGpu = nullptr; }
            char lacPath[MAX_PATH];
            const DWORD luLength = GetModuleFileNameA(nullptr, lacPath, MAX_PATH);
            if (luLength && luLength + 18 < MAX_PATH)
            {
                std::snprintf(lacPath + luLength, MAX_PATH - luLength, ".frames.csv");
                if (FILE* lpFile = std::fopen(lacPath, "w"))
                {
                    std::fprintf(lpFile, "frame,time_s,interval_ms,active_ms,update_ms,dispatch_ms,geometry_prepare_ms,geometry_lock_ms,geometry_convert_ms,geometry_unlock_ms,geometry_submit_ms,present_ms,present_copy_ms,present_wait_ms,dispatch_sort_ms,update_display_ms,update_start_ms,update_simulation_ms,update_resource_ms,update_publish_ms,update_timing_ms,resource_pool_ms,resource_memory_ms,resource_load_ms,resource_unload_ms,resource_file_ms,resource_attrib_ms,object_to_mesh_ms,mesh_technique_ms,mesh_constants_ms,mesh_buffers_ms,geometry_lookup_ms,world_draw_ms,immediate_draw_ms,postfx_ms,dispatch_effects_ms,render_setup_ms,render_build_lists_ms,render_tint_ms,render_shadows_ms,render_envmap_ms,render_particle_build_ms,render_world_ms,render_particles_ms,render_composite_ms,render_gui_ms,render_present_ms,geometry_create_ms,geometry_copy_ms,pool_requests_ms,pool_states_ms,pool_retire_ms,texture_realize_ms,texture_gpu_create_ms,texture_cpu_create_ms,texture_upload_ms,update_mesh_prepare_ms,vb_creates,ib_creates,upload_bytes,evictions,draws,camera_begin,camera_end,camera_changes,presents,native_buffers,player_takedowns,takedown_victims,rivals,crashing_rivals,airborne_rivals,qpc_end,instanced_draws,instances,prez_meshes,world_opaque_meshes,car_opaque_meshes,gpu_status,gpu_scene_ms,gpu_output_ms,vb_bind_requests,vb_bind_skips,ib_bind_requests,ib_bind_skips,rebased_draws,geometry_create_cycles,geometry_lock_cycles,geometry_copy_cycles,resource_pool_cycles,pool_requests_cycles,pool_states_cycles,pool_retire_cycles,texture_realize_cycles,texture_gpu_create_cycles,texture_cpu_create_cycles,texture_upload_cycles,update_mesh_prepare_cycles,raster_creates,raster_releases,staging_creates,staging_reuses,cycle_read_failures,mesh_prepared,mesh_rebuilt,camera_behaviour_begin,camera_behaviour_end,camera_shot_begin,camera_shot_end,camera_publications,camera_new_publications,camera_behaviour_changes,camera_shot_changes,update_simulation_cycles,dispatch_cycles\n");
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
                        std::fprintf(lpFile, ",%u,%u,%llu,%u,%u,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%lld,%u,%u,%u,%u,%u", lr.muVertexCreates,
                            lr.muIndexCreates, lr.muUploadedBytes, lr.muEvictions, lr.muDraws,
                            lr.miCameraBegin, lr.miCameraEnd, lr.muCameraChanges, lr.muPresents, lr.muNativeBuffers,
                            lr.muPlayerTakedowns, lr.muTakedownVictims, lr.muRivals, lr.muCrashingRivals, lr.muAirborneRivals, lr.miEnd, lr.muInstancedDraws, lr.muInstances, lr.muPreZMeshes, lr.muWorldOpaqueMeshes, lr.muCarOpaqueMeshes);
                        std::fprintf(lpFile, ",%u,%.6f,%.6f,%u,%u,%u,%u,%u", lr.muGpuStatus, lr.mfGpuSceneMs, lr.mfGpuOutputMs,
                            lr.muVertexBindRequests, lr.muVertexBindSkips, lr.muIndexBindRequests, lr.muIndexBindSkips, lr.muRebasedDraws);
                        std::fprintf(lpFile, ",%llu,%llu,%llu,%llu", lr.maCycles[GEOMETRY_CREATE],
                            lr.maCycles[GEOMETRY_LOCK], lr.maCycles[GEOMETRY_COPY], lr.maCycles[RESOURCE_POOL]);
                        for (unsigned ls = POOL_REQUESTS; ls < NUM_SECTIONS; ++ls)
                            std::fprintf(lpFile, ",%llu", lr.maCycles[ls]);
                        std::fprintf(lpFile, ",%u,%u,%u,%u,%u,%u,%u", lr.muRasterCreates, lr.muRasterReleases, lr.muStagingCreates, lr.muStagingReuses, static_cast<unsigned>(lr.muCycleReadFailures), lr.muMeshPrepared, lr.muMeshRebuilt);
                        std::fprintf(lpFile, ",%llu,%llu,%llu,%llu,%u,%u,%u,%u,%llu,%llu\n",
                            lr.muBehaviourBegin, lr.muBehaviourEnd, lr.muShotBegin, lr.muShotEnd,
                            lr.muCameraPublications, lr.muNewCameraPublications, lr.muBehaviourChanges, lr.muShotChanges,
                            lr.maCycles[UPDATE_SIMULATION], lr.maCycles[DISPATCH]);
                    }
                    std::fclose(lpFile);
                }
                std::snprintf(lacPath + luLength, MAX_PATH - luLength, ".frames.json");
                if (FILE* lpFile = std::fopen(lacPath, "w"))
                {
                    std::fprintf(lpFile, "{\"frames\":%u,\"dropped_frames\":%u,\"counter_frequency\":%lld,\"timing_only\":%s,\"coarse\":%s,\"cpu_cycles\":%s,\"cpu_cycles_available\":%s}\n",
                        gCapture.muCount, gCapture.muDropped, gCapture.miFrequency,
                        gCapture.mbTimingOnly ? "true" : "false", gCapture.mbCoarse ? "true" : "false",
                        gCapture.mbCpuCycles ? "true" : "false", gCapture.mpReadThreadCycles ? "true" : "false");
                    std::fclose(lpFile);
                }
            }
            delete[] gCapture.mpFrames;
            gCapture.mpFrames = gCapture.mpCurrent = nullptr;
        }
    }
}
