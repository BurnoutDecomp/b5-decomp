#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"

#include <windows.h>
#include <cstdio>

// CgsMemory::LowMemory -- see the header for WHY this exists (the shipped serialised
// resource format stores pointers in 4-byte slots; on x64 the pointed-to memory must
// therefore live below 4 GB or the PointerFromU32 round-trip truncates).
//
// FLAG PC-platform leaf: no X360 counterpart -- the 360 is a 32-bit address space, so
// its allocator layer needs no address-range constraint at all. This whole TU is a
// Windows-x64 port mechanism.

namespace CgsMemory
{
    namespace LowMemory
    {
        namespace
        {
            const ULONG_PTR KU_LOW_LIMIT = 0x0000000100000000ull;   // 4 GB, exclusive

            // The largest recorded reservation and whether it satisfied the low limit;
            // used by IsLowAddress + the diagnostic string the caller logs.
            bool s_bLastWasLow = false;

            bool BlockIsLow(const void* lpBlock, size_t lnBytes)
            {
                const ULONG_PTR luBase = reinterpret_cast<ULONG_PTR>(lpBlock);
                return luBase != 0 && (luBase + lnBytes) <= KU_LOW_LIMIT;
            }

            // Win10 1803+ VirtualAlloc2 with an explicit MEM_ADDRESS_REQUIREMENTS
            // ceiling: the OS picks a base for us and guarantees the whole extent ends
            // below 4 GB. Resolved dynamically so the exe still starts on hosts without
            // it (the scan fallback below covers those).
            typedef PVOID(WINAPI* PfnVirtualAlloc2)(HANDLE, PVOID, SIZE_T, ULONG, ULONG,
                                                    MEM_EXTENDED_PARAMETER*, ULONG);

            void* ReserveViaVirtualAlloc2(size_t lnBytes)
            {
                static PfnVirtualAlloc2 s_pfn      = 0;
                static bool             s_bResolved = false;
                if (!s_bResolved)
                {
                    s_bResolved = true;
                    HMODULE lhBase = GetModuleHandleW(L"kernelbase.dll");
                    if (lhBase == 0)
                        lhBase = GetModuleHandleW(L"kernel32.dll");
                    if (lhBase != 0)
                        s_pfn = reinterpret_cast<PfnVirtualAlloc2>(
                            reinterpret_cast<void*>(GetProcAddress(lhBase, "VirtualAlloc2")));
                }
                if (s_pfn == 0)
                    return 0;

                MEM_ADDRESS_REQUIREMENTS lReq;
                ZeroMemory(&lReq, sizeof(lReq));
                lReq.LowestStartingAddress = reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(0x00010000ull));
                lReq.HighestEndingAddress  = reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(KU_LOW_LIMIT - 1));
                lReq.Alignment             = 0;   // natural (64 KB allocation granularity)

                MEM_EXTENDED_PARAMETER lParam;
                ZeroMemory(&lParam, sizeof(lParam));
                lParam.Type    = MemExtendedParameterAddressRequirements;
                lParam.Pointer = &lReq;

                return s_pfn(GetCurrentProcess(), 0, lnBytes, MEM_RESERVE | MEM_COMMIT,
                             PAGE_READWRITE, &lParam, 1);
            }

            // Fallback for hosts without VirtualAlloc2: walk candidate bases upward on
            // the 64 KB allocation granularity (stepped 16 MB to keep the scan short)
            // and take the first that accepts the whole block. Stops at the 4 GB line.
            void* ReserveViaScan(size_t lnBytes)
            {
                const ULONG_PTR KU_FIRST = 0x0000000010000000ull;   // 256 MB: past the low clutter
                const ULONG_PTR KU_STEP  = 0x0000000001000000ull;   // 16 MB
                for (ULONG_PTR luBase = KU_FIRST;
                     luBase + lnBytes <= KU_LOW_LIMIT;
                     luBase += KU_STEP)
                {
                    void* lpBlock = VirtualAlloc(reinterpret_cast<PVOID>(luBase), lnBytes,
                                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                    if (lpBlock != 0)
                        return lpBlock;
                }
                return 0;
            }
        }

        void* Reserve(size_t lnBytes)
        {
            void* lpBlock = ReserveViaVirtualAlloc2(lnBytes);
            if (lpBlock == 0 || !BlockIsLow(lpBlock, lnBytes))
            {
                if (lpBlock != 0)
                {
                    VirtualFree(lpBlock, 0, MEM_RELEASE);
                    lpBlock = 0;
                }
                lpBlock = ReserveViaScan(lnBytes);
            }

            if (lpBlock != 0 && BlockIsLow(lpBlock, lnBytes))
            {
                s_bLastWasLow = true;
                return lpBlock;
            }

            // No low region available. Serve the request anyway (the engine cannot run
            // without its root heap) but make the consequence loud: every serialised
            // pointer slot the resource system writes into this memory WILL truncate.
            // CgsLog is not necessarily up when the root heap is first backed, so this
            // goes straight to stderr.
            if (lpBlock == 0)
                lpBlock = VirtualAlloc(0, lnBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            s_bLastWasLow = false;
            std::fprintf(stderr,
                         "[LowMemory] FAILED to reserve %llu MB below 4GB -- got %p. "
                         "Serialised resource pointer slots (PointerFromU32) WILL TRUNCATE.\n",
                         static_cast<unsigned long long>(lnBytes >> 20), lpBlock);
            return lpBlock;
        }

        void Release(void* lpBlock)
        {
            if (lpBlock != 0)
                VirtualFree(lpBlock, 0, MEM_RELEASE);
        }

        bool IsLowAddress(const void* lpBlock)
        {
            if (lpBlock == 0)
                return s_bLastWasLow;
            return reinterpret_cast<ULONG_PTR>(lpBlock) < KU_LOW_LIMIT;
        }
    }
}
