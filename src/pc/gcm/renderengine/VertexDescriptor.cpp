#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/RenderEngine.h"   // renderengine::VertexFormatGetStride

#include <cstring>   // memset

// X360 reconstruction of renderengine::VertexDescriptor. The bodies mirror the X360 image
// store-for-store: descriptor fields are written by name in the order the asm writes them, the
// D3DVERTEXELEMENT9 marshalling buffer is filled at the exact byte offsets the PPC sthx/stwx/stbx
// land on, the per-element 12-byte declaration stride and the 16-byte parsed-element stride match,
// and the bit masks match the PPC rlwinm/slw operations exactly.

namespace renderengine
{
    namespace
    {
        // The X360 lays each marshalled D3DVERTEXELEMENT9 down with a 12-byte stride into a 240-byte
        // stack buffer (v14[2] + v15[238]); the fields sit at record-relative offsets -2/0/+2/+6/+7/+8
        // off the cursor, so consecutive records overlap by two bytes. The cursor starts two bytes
        // into the buffer (at v15). Reproduced as raw bytes to preserve the exact store layout.
        inline void StoreU16(u8* lpBuffer, int liByteOffset, u16 luValue)
        {
            *reinterpret_cast<u16*>(lpBuffer + liByteOffset) = luValue;
        }
        inline void StoreU32(u8* lpBuffer, int liByteOffset, u32 luValue)
        {
            *reinterpret_cast<u32*>(lpBuffer + liByteOffset) = luValue;
        }
    }

    // 0x82B611A0 -- marshal the parsed element table into a D3DVERTEXELEMENT9 array (12-byte stride)
    // terminated by the D3D end sentinel, create the vertex declaration, and store it at +0x00.
    D3DVertexDeclaration* VertexDescriptor::CreateD3DObject(VertexDescriptorData* lpData)
    {
        u8 lauDecl[240];   // v14[2] + v15[238], one contiguous buffer

        u32 luIndex = 0;                                   // v2 = 0
        int liCursor = 2;                                  // v3 = v15 (= lauDecl + 2)
        const u32 luCount = lpData->muElementCount;        // v4 = *(a1 + 8)  (lhz, u16)
        const VertexElement* lpElement = lpData->maElements; // v5 = a1 + 0x10
        do
        {
            const u16 luStream     = lpElement->muStream;       // v6 = *(v5 - 2)  (rec +0x00)
            ++luIndex;
            const u16 luOffset     = lpElement->muOffset;       // v7 = *(v5 - 1)  (rec +0x02)
            const u32 luType       = lpElement->muType;         // v8 = *v5        (rec +0x04)
            const u8  luMethod     = lpElement->mauTail[0];     // v9  = *(v5 + 4) (rec +0x08)
            const u8  luUsage      = lpElement->mauTail[1];     // v10 = *(v5 + 5) (rec +0x09)
            const u8  luUsageIndex = lpElement->mauTail[2];     // v11 = *(v5 + 6) (rec +0x0A)
            ++lpElement;                                        // v5 += 4 (dwords) = +0x10

            StoreU16(lauDecl, liCursor - 2, luStream);          // sth r6, -2(r10)
            StoreU16(lauDecl, liCursor + 0, luOffset);          // sth r5,  0(r10)
            StoreU32(lauDecl, liCursor + 2, luType);            // stw r4,  2(r10)
            lauDecl[liCursor + 6] = luMethod;                   // stb r3,  6(r10)
            lauDecl[liCursor + 7] = luUsage;                    // stb r30, 7(r10)
            lauDecl[liCursor + 8] = luUsageIndex;               // stb r29, 8(r10)
            liCursor += 12;                                     // r10 += 0xC
        }
        while (luIndex < luCount);

        const int liEnd = 12 * static_cast<int>(luIndex);       // v12 = 12 * v2
        StoreU16(lauDecl, liEnd + 0, 0x00FFu);                  // sthx 0xFF, v12, &v14[0]
        StoreU16(lauDecl, liEnd + 2, 0x0000u);                  // sthx 0,    v12, &v14[2]
        StoreU32(lauDecl, liEnd + 4, 0xFFFFFFFFu);              // stwx -1,   v12, &v14[4]
        lauDecl[liEnd + 8]  = 0u;                               // stbx 0,    v12, &v14[8]
        lauDecl[liEnd + 9]  = 0u;                               // stbx 0,    v12, &v14[9]
        lauDecl[liEnd + 10] = 0u;                               // stbx 0,    v12, &v14[10]

        D3DVertexDeclaration* lpResult = D3DDevice_CreateVertexDeclaration(lauDecl);
        lpData->mpDeclaration = lpResult;                       // *a1 = result
        return lpResult;
    }

    // 0x82B61260 -- read the element table back out into a 16-record buffer (16-byte records), then
    // pad the unused tail records with the D3D end sentinel byte pattern. Returns lpData.
    VertexDescriptorData* VertexDescriptor::GetParameters(VertexDescriptorData* lpData, u32* lpParamsOut)
    {
        u32 luIndex = 0;                                        // v2 = 0
        const u32 luCount = lpData->muElementCount;             // *(result + 8)
        if (luCount != 0u)
        {
            u32* lpOut = lpParamsOut;                           // v3 = a2
            const u32* lpSrc = reinterpret_cast<const u32*>(lpData->maElements); // v4 = result + 0x10
            do
            {
                ++luIndex;
                lpOut[0] = lpSrc[0];                            // *v3 = *v4
                lpOut[1] = lpSrc[1];                            // v3[1] = v4[1]
                lpOut[2] = lpSrc[2];                            // v3[2] = v4[2]
                const u32 luWord3 = lpSrc[3];                   // v5 = v4[3]
                lpSrc += 4;                                     // v4 += 4 (= +0x10)
                lpOut[3] = luWord3;                             // v3[3] = v5
                lpOut += 4;                                     // v3 += 4 (= +0x10)
            }
            while (luIndex < lpData->muElementCount);

            if (luIndex >= 0x10u)
            {
                return lpData;                                 // bgelr cr6
            }
        }

        // Pad the remaining (16 - count) records with the D3D end sentinel, 16-byte stride. The byte
        // cursor lands two bytes into each record (r11 = (count<<4) + a2 + 4) and writes the sentinel
        // fields at the exact relative offsets the asm uses.
        u32 luRemaining = 0x10u - luIndex;                      // v6 = 16 - v2
        u8* lpByte = reinterpret_cast<u8*>(lpParamsOut) + (luIndex << 4) + 4; // v7 = &a2[4*v2+1]
        do
        {
            StoreU16(lpByte, -4, 0u);          // *(v7 - 4)  = 0   (sth  r6 -4(r11))
            lpByte[4] = 0u;                    // *(v7 + 4)  = 0   (stb  r6  4(r11))
            lpByte[5] = 0xFFu;                 //                  (stb  r9  5(r11))
            --luRemaining;
            lpByte[6] = 0xFFu;                 // *(v7 + 6)  = 255 (stb  r9  6(r11))
            lpByte[7] = 0u;                    // *(v7 + 7)  = 0   (stb  r6  7(r11))
            StoreU16(lpByte, -2, 0xFFFFu);     // *(v7 - 2)  = -1  (sth  r8 -2(r11))
            StoreU32(lpByte, 0, 0xFFFFFFFFu);  // *v7        = -1  (stw  r7  0(r11))
            StoreU32(lpByte, 8, 0u);           // *(v7 + 8)  = 0   (stw  r6  8(r11))
            lpByte += 16;                      // r11 += 0x10
        }
        while (luRemaining != 0u);

        return lpData;
    }

    // 0x82B637D0 -- build the rw resource descriptor: four {size=0, align=1} entries seeded (plus one
    // extra pass), then slot0 = {size = 0x11 * validElements + 0x10, align = 4}. The valid count is
    // the number of element records at a2 whose first dword (a2+4 +0x10*i) is not -1, over 16 records.
    u64* VertexDescriptor::GetResourceDescriptor(u64* lpDescriptorOut, int a2)
    {
        int liValid = 0;                                        // v2 = 0
        const u32* lpRecord = reinterpret_cast<const u32*>(    // v3 = a2 + 4
            reinterpret_cast<const u8*>(static_cast<usize>(static_cast<u32>(a2))) + 4);
        int liRecordsLeft = 16;                                 // v4 = 16
        do
        {
            if (*lpRecord != 0xFFFFFFFFu)                       // if (*v3 != -1)
            {
                ++liValid;                                      // ++v2
            }
            --liRecordsLeft;                                    // --v4
            lpRecord += 4;                                      // v3 += 4 (= +0x10)
        }
        while (liRecordsLeft != 0);

        const u32 luAlign = static_cast<u32>(17 * liValid + 16); // v5 = 17 * v2 + 16

        u32* lpDwords = reinterpret_cast<u32*>(lpDescriptorOut); // v6 = result
        int liSeed = 4;                                          // v7 = 4
        do
        {
            --liSeed;
            lpDwords[1] = 1u;     // v6[1] = 1
            lpDwords[0] = 0u;     // *v6   = 0
            lpDwords += 2;        // v6 += 2
        }
        while (liSeed >= 0);

        // back_chain = {LODWORD = 4 (size), HIDWORD = luAlign}; *result = back_chain.
        lpDescriptorOut[0] = (static_cast<u64>(luAlign) << 32) | 4ULL;
        return lpDescriptorOut;
    }

    // 0x82B63060 -- parse the RenderWare vertex-format parameters at a2 into the element table,
    // compute per-stream strides, set the stream / type-2 masks, then CreateD3DObject.
    VertexDescriptorData* VertexDescriptor::Initialize(VertexDescriptorWrapper* lpWrapper, int a2)
    {
        VertexDescriptorData* lpData = lpWrapper->mpData;       // v2 = *a1
        std::memset(lpData, 0, 0x20);                           // memset(*a1, 0, 32)

        int laiStreamStride[16];                                // v26[96] -> 16 dwords + slack used
        // The X360 frame reserves 0x60 (96) bytes and zero-fills the first 16 dwords (the per-stream
        // accumulators indexed by stream<<2).
        for (int liStream = 0; liStream < 16; ++liStream)
        {
            laiStreamStride[liStream] = 0;                      // *v4++ = 0 (16 times)
        }
        lpData->muType2Mask = 0u;                               // *(v2 + 12) = 0 (sth 0xC)

        int liValidCount = 0;                                   // v5 = 0
        // v7 = a2 + 9 (cursor into the parameter records); v8 = v2 + 0x12 (element table write cursor
        // at +0x12, i.e. element record +0x02). v9 = 16 records; v10 = the defaults table.
        const u8* lpParam = reinterpret_cast<const u8*>(       // v7 = a2 + 9
            reinterpret_cast<const u8*>(static_cast<usize>(static_cast<u32>(a2))) + 9);
        u8* lpElemByte = reinterpret_cast<u8*>(lpData) + 0x12;  // v8 = v2 + 0x12
        int liParamLeft = 16;                                   // v9 = 16
        const u8* lpDefaults = gauVertexFormatDefaults;         // v10 = &unk_8214AD50

        do
        {
            const u32 luType = *reinterpret_cast<const u32*>(lpParam - 5);  // *(v7 - 5)
            if (luType != 0xFFFFFFFFu)
            {
                // Copy the four parameter dwords into the element record (writes land at element
                // record +0x00/+0x04/+0x08/+0x0C, i.e. v8-2 .. v8+0xC).
                u8* lpRec = lpElemByte - 2;                                 // v11/r11 = v8 - 2
                *reinterpret_cast<u32*>(lpRec + 0x00) = *reinterpret_cast<const u32*>(lpParam - 9);  // *(v7-9)
                *reinterpret_cast<u32*>(lpRec + 0x04) = *reinterpret_cast<const u32*>(lpParam - 5);  // *(v7-5)
                *reinterpret_cast<u32*>(lpRec + 0x08) = *reinterpret_cast<const u32*>(lpParam - 1);  // *(v7-1)
                *reinterpret_cast<u32*>(lpRec + 0x0C) = *reinterpret_cast<const u32*>(lpParam + 3);  // *(v7+3)

                const u8 luUsage = lpParam[2];                             // *(v7 + 2)
                lpData->muStreamMask |= (1u << luUsage);                   // *(v2+4) |= 1<<v7[2]

                const int liStride = VertexFormatGetStride(
                    static_cast<int>(*reinterpret_cast<const u32*>(lpParam - 5)));  // *(v7-5)

                const u16 luStream = *reinterpret_cast<const u16*>(lpElemByte);     // v14 = *v11 (u16 @ v8)
                const u32 luStreamIdx = static_cast<u32>(
                    *reinterpret_cast<const u16*>(lpParam - 9)) << 2;              // ROL4(*(v7-9),2)
                if (luStream < 0x100u)
                {
                    const u32 luNew = luStream + static_cast<u32>(liStride);       // v16 = v14 + Stride
                    int& lriAcc = *reinterpret_cast<int*>(
                        reinterpret_cast<u8*>(laiStreamStride) + luStreamIdx);     // v26[ROL4(*(v7-9),2)]
                    if (static_cast<u32>(lriAcc) < luNew)
                    {
                        lriAcc = static_cast<int>(luNew);
                    }
                }
                else
                {
                    int& lriAcc = *reinterpret_cast<int*>(
                        reinterpret_cast<u8*>(laiStreamStride) + luStreamIdx);     // v26[ROL4(*(v7-9),2)]
                    *reinterpret_cast<u16*>(lpElemByte) = static_cast<u16>(lriAcc); // *v11 = v26[...]
                    lriAcc += liStride;                                            // v26[...] += Stride
                }

                int liMethod = lpParam[0];                                // v18 = *v7
                if (liMethod == 0xFF)
                {
                    liMethod = (liMethod & ~0xFF) |
                        lpDefaults[static_cast<u32>(luUsage) << 1];       // LOBYTE = v10[ROL4(v7[2],1)]
                }
                lpElemByte[7] = static_cast<u8>(liMethod);                // *(v11 + 7)

                int liUsage = lpParam[1];                                 // v19 = v7[1]
                if (liUsage == 0xFF)
                {
                    liUsage = (liUsage & ~0xFF) |
                        lpDefaults[(static_cast<u32>(luUsage) << 1) + 1]; // LOBYTE = v10[ROL4(v7[2],1)+1]
                }
                lpElemByte[8] = static_cast<u8>(liUsage);                 // *(v11 + 8)

                if (*reinterpret_cast<const u32*>(lpParam + 3) == 2u)     // *(v7 + 3) == 2
                {
                    lpData->muType2Mask |= static_cast<u16>(
                        1u << *reinterpret_cast<const u16*>(lpParam - 9)); // *(v2+12) |= 1<<*(v7-9)
                }

                ++liValidCount;                                           // v5 = v12 + 1
                lpElemByte = lpElemByte + 0x10;                           // v8 = v11 + 8 (dwords) = +0x10
            }
            --liParamLeft;                                                // --v9
            lpParam += 16;                                                // v7 += 16
        }
        while (liParamLeft != 0);

        lpData->muElementCount = static_cast<u16>(liValidCount);          // *(v2 + 8) = v5

        // Per-stream stride table base = +0x10 * (count + 1) bytes into the object.
        u8* lpStrideOut = reinterpret_cast<u8*>(lpData) + 0x10 * (liValidCount + 1); // v20
        if (liValidCount != 0)
        {
            const u8* lpStrideParam = reinterpret_cast<const u8*>(        // v21 = a2 + 0x100
                reinterpret_cast<const u8*>(static_cast<usize>(static_cast<u32>(a2))) + 0x100);
            const u16* lpStreamOfElem = reinterpret_cast<const u16*>(    // v22 = v2 + 0x10
                reinterpret_cast<u8*>(lpData) + 0x10);
            int liLeft = liValidCount;                                    // v23 = v5
            do
            {
                int liByte = *lpStrideParam;                              // LOBYTE(v24) = *v21
                if (*lpStrideParam == 0)
                {
                    liByte = *reinterpret_cast<const int*>(               // v24 = v26[ROL4(*v22,2)]
                        reinterpret_cast<const u8*>(laiStreamStride) +
                        (static_cast<u32>(*lpStreamOfElem) << 2));
                }
                *lpStrideOut = static_cast<u8>(liByte);                   // *v20 = v24
                --liLeft;
                lpStreamOfElem += 8;                                      // v22 += 8 (u16) = +0x10
                ++lpStrideParam;                                          // ++v21
                ++lpStrideOut;                                            // ++v20
            }
            while (liLeft != 0);
        }

        lpData->muInitialised = 1u;                                       // *(v2 + 10) = 1 (sth r29)
        VertexDescriptor::CreateD3DObject(lpData);
        return lpData;
    }

    // 0x82B63250 -- release the D3D declaration if present, then clear the pointer. Returns lpData.
    VertexDescriptorData* VertexDescriptor::Release(VertexDescriptorData* lpData)
    {
        VertexDescriptorData* lpResult = lpData;                // v1 = result
        if (lpData->mpDeclaration != nullptr)                   // if (*result)
        {
            VertexDescriptor_PreRelease(lpData->mpDeclaration); // sub_82B61F90
            D3DResource_Release(lpData->mpDeclaration);         // result = D3DResource_Release(*v1)
            lpData->mpDeclaration = nullptr;                    // *v1 = 0
        }
        return lpResult;
    }
}
