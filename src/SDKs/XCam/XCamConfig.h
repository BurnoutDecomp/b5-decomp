#pragma once

// ===========================================================================
// XCAM config TU -- free functions of the X360 XCAM (USB camera / capture)
// SDK that program camera controls through the kernel USB-cam driver. This
// header is the canonical OWNING home for the reconstructed bodies in
// XCamConfig.cpp:
//
//     XCAM::WMVDecCBGetData_HANDLER      @ 0x82987F78
//     XCAM::XCamGetControlIDTerminalID   @ 0x82981EE8
//     XCAM::XCamHandleSetCompConfig      @ 0x829827D0
//     XCAM::XCamSetConfigInternal        @ 0x82980820
//     XCAM::XcamLookupFrameInterval      @ 0x82981FC0
//
// There is no reference source and no DWARF for this TU; the shapes below are
// reconstructed store-for-store from the X360 asm. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// CONFIG-DESCRIPTOR TABLES (attested by the asm strides):
//   Composite-profile table 300 (gXCamCompConfigTable300): base unk_82F3E238,
//   row stride 0x48 (72 bytes) = 9 x KXCamControlEntry(8 bytes).
//   Composite-profile table 301 (gXCamCompConfigTable301): base unk_82F3E310,
//   row stride 0x18 (24 bytes) = 3 x KXCamControlEntry(8 bytes).
//   Each entry is { s32 iCode; s32 iValue; }; a row is terminated by an entry
//   whose iCode == 0x7FFFFFFF. The raw payloads themselves are UNATTESTED rodata
//   (only their stride is recoverable), so they are declared extern here and
//   NOT defined -- do not fabricate the contents.
// ===========================================================================

#include "types.hpp"

namespace XCAM
{

// One (controlCode, value) pair inside a composite-config profile row.
// 8-byte stride is attested by the `i += 8` walk and the +0/+4 loads in
// XCamHandleSetCompConfig / HandleSetCompCompletion.
struct KXCamControlEntry
{
    s32 iCode;  // +0  control code (0x7FFFFFFF terminates the row)
    s32 iValue; // +4  value to program
};

// The variable-size config payload handed to XUsbcamSetConfig. The size class
// returned by XCamGetControlIDTerminalID selects whether 1, 2 or 4 bytes are
// written (stb / sth / stw into the same 4-byte slot).
union KXCamConfigPayload
{
    u8  uByte;
    u16 uHalf;
    u32 uWord;
};

// XCAM async I/O bookkeeping block returned by GetOverlappedInfo. Only the
// two fields these functions touch are named; the block is a fixed-size 28-byte
// slot in a 64-entry ring (28-byte-strided in GetOverlappedInfo). Offsets are
// byte offsets from the asm.
struct XCamOverlappedInfo
{
    u8  mOpaque0[0x0C]; // +0x00 .. +0x0B  (APC/handle/event bookkeeping)
    u32 uMode;          // +0x0C  set to 2 by XCamSetConfigInternal
    u8  mOpaque1[0x08]; // +0x10 .. +0x13  (+0x10 is the ring lock word)
    u32 uStateWord;     // +0x14  packed (table<<16)|index profile state word
};

// --- shared mutable SDK state (file-scope globals in the X360 image) ---
extern int gXCamDeviceHandle;         // dword_83222CE0 -- USB-cam device handle
extern int gXCamCompConfigApplied300; // dword_83223430 -- last applied 300 index
extern int gXCamCompConfigApplied301; // dword_83223434 -- last applied 301 index

// --- config-descriptor tables (contents unattested; extern-only) ---
extern const KXCamControlEntry gXCamCompConfigTable300[]; // unk_82F3E238
extern const KXCamControlEntry gXCamCompConfigTable301[]; // unk_82F3E310

// --- collaborators reconstructed in sibling TUs of the XCAM SDK ---
// NOTE: these X360 symbols carry NO "XCam" prefix (they already live in the
// XCAM namespace). Declared with their true names so the sibling TU links.
XCamOverlappedInfo* GetOverlappedInfo();                                 // 0x82981FE0
int  SetupOverlap(XCamOverlappedInfo* lpOverlap, void* lpUserOverlapped); // 0x82982340
int  ReturnOverlappedInfo(XCamOverlappedInfo* lpOverlap);                // 0x82982058
int  HandleCompletion(int a1, int a2, int a3);                          // async completion cb
int  HandleSetCompCompletion(int a1, int a2, int a3);                   // 0x82982628

// XUsbcamSetConfig -- kernel USB-cam driver export. Signature widened to the six
// PPC argument registers the callers populate (handle, terminalId, controlId,
// payload, completion callback, overlap context).
extern "C" int XUsbcamSetConfig(int iDeviceHandle, int iTerminalId, int iControlId,
                                void* lpPayload, void* lpCompletion, void* lpOverlap);

// --- reconstructed here (see XCamConfig.cpp) ---
int WMVDecCBGetData_HANDLER(void* a1, int a2, int a3, int a4, int a5, int a6); // 0x82987F78
int XCamGetControlIDTerminalID(int iCode, int* lpControlId, int* lpTerminalId, int* lpSize); // 0x82981EE8
int XCamHandleSetCompConfig(int a1, int a2, void* a3);                    // 0x829827D0
int XCamSetConfigInternal(int a1, int a2, int* a3);                       // 0x82980820
int XcamLookupFrameInterval(u32 uFramesPerSecond, u32* lpInterval);       // 0x82981FC0

} // namespace XCAM
