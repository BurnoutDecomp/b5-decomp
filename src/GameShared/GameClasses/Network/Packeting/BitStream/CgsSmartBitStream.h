#pragma once

#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsBitStream.h"

// CgsNetwork::SmartBitStream
//
// Typed convenience layer over CgsNetwork::BitStream. It adds quantised
// float/int helpers, fixed-width byte/uint helpers, and a raw-byte block
// transfer (AddRawData / GetRawData) on top of the base AddBits/GetBits
// bit-packer.
//
// This is the canonical class home. The .cpp in this directory defines only the
// two reconstructed raw-data functions (AddRawData, GetRawData). The byte/uint
// helpers below are trivial inline wrappers over the inherited AddBits/GetBits
// (the X360 inlined them into AddRawData/GetRawData; KI_BITSIZE_BYTE/INT confirm
// the field widths). The Quantised methods live in the CgsFloatQuantiser /
// CgsIntQuantiser translation units and are only declared here.

namespace CgsNetwork
{
    struct SmartBitStream : public BitStream
    {
    private:
        static const s32 KI_BITSIZE_BYTE = 8;
        static const s32 KI_BITSIZE_INT  = 64;

    public:
        bool AddQuantisedFloat(float lfValue, float lfMin, float lfMax, s32 liNumBits);
        bool AddQuantisedFloat(float lfValue, float lfMin, float lfMax, float lfResolution);
        void GetQuantisedFloat(float* lpfValue, float lfMin, float lfMax, s32 liNumBits);
        void GetQuantisedFloat(float* lpfValue, float lfMin, float lfMax, float lfResolution);

        bool AddQuantisedInt(s32 liValue, s32 liMin, s32 liMax);
        void GetQuantisedInt(s32* lpiValue, s32 liMin, s32 liMax);
        s32  GetQuantisedInt(s32 liMin, s32 liMax);

        bool AddByte(u8 luByte)   { return AddBits(luByte, KI_BITSIZE_BYTE); }   // AddBits(b, 8)
        u8   GetByte()            { return static_cast<u8>(GetBits(KI_BITSIZE_BYTE)); }

        bool AddUInt(u64 luValue) { return AddBits(luValue, KI_BITSIZE_INT); }   // AddBits(u, 64)
        u64  GetUInt()            { return GetBits(KI_BITSIZE_INT); }

        bool AddRawData(const char* lpcData, s32 liNumBytes);
        void GetRawData(char* lpcBuffer, s32 liNumBytes);
    };
}
