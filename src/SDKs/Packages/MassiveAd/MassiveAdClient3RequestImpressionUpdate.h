#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestImpressionUpdate -- the "/impsrv/4/activity"
// impression-update request (vendor middleware). A concrete CRequestObject:
// the MassiveAd asset/zone layer builds one impression-report block per media
// asset (model / texture / video / audio) or interaction through the Add*Report
// helpers, Finish() seals the block, and Parse() consumes the server response.
// Sibling of CRequestBuilder / CRequestManager / CRequestObject (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestImpressionUpdate::CRequestImpressionUpdate @ 0x82BD9538
//     MassiveAdClient3::CRequestImpressionUpdate::`vector deleting destructor' @ 0x82BD9E88
//     MassiveAdClient3::CRequestImpressionUpdate::GetRequestURL          @ 0x82BD9590
//     MassiveAdClient3::CRequestImpressionUpdate::AddModelReport         @ 0x82BD9AC8
//     MassiveAdClient3::CRequestImpressionUpdate::AddTextureRepor        @ 0x82BD9640
//     MassiveAdClient3::CRequestImpressionUpdate::AddVideoReport         @ 0x82BD97D0
//     MassiveAdClient3::CRequestImpressionUpdate::AddAudioReport         @ 0x82BD9960
//     MassiveAdClient3::CRequestImpressionUpdate::AddInteractionR        @ 0x82BD9C28
//     MassiveAdClient3::CRequestImpressionUpdate::Finish                 @ 0x82BD9D20
//     MassiveAdClient3::CRequestImpressionUpdate::Parse                  @ 0x82BD9DA8 (BLOCKED)
//
// Parse @ 0x82BD9DA8 is NOT reconstructed here: its body issues a `bl STUB`
// (Hex-Rays `STUB(this, mpSignature@+0x30, 20)`) to a function that is not homed
// or even named in this TU's dossier, and it also calls CRequestObject::
// ReadRemoveSignature, whose signature the committed base header deliberately
// leaves undeclared as un-attested. The Parse override is DECLARED (so the class
// stays concrete / instantiable and the compile gate is clean) but its body is
// left for the ledger slice that homes those two collaborators. Reproducing the
// STUB side-effect without a real symbol would be fabrication.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestImpressionUpdate class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// Layout over the committed CRequestObject base (members BY NAME; the two u16
// report counters are the only fields the derived class adds -- the ctor stores
// them at the X360-absolute +0x50 / +0x52, reproduced by member NAME because the
// base subobject is wider on the 64-bit host):
//   +0x00  vftable            (off_82186F84; slot 0 = vector deleting destructor,
//                              the Parse override, GetRequestURL)
//   ...    CRequestObject base body
//   +0x50  mnReportCount       (u16, 0 at ctor; ++ by AddModel/Texture/Video/
//                              AudioReport, written by Finish under tag 33)
//   +0x52  mnInteractionCount  (u16, 0 at ctor; ++ by AddInteractionR, written by
//                              Finish under tag 41)
//
// The Add*Report parameter names encode the exact wire FIELD TAG each value is
// written under (e.g. nTag38 -> WriteU8(38) then WriteU32(nTag38)); the tag
// numbers are the one attested fact about each field, so they name the params.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestImpressionUpdate : public CRequestObject
{
public:
    // @ 0x82BD9538. Chains CRequestObject(101, "RequestImpressionUpdate"),
    // installs this class's vftable (off_82186F84 -- modelled by the virtuals),
    // and zeroes both report counters.
    CRequestImpressionUpdate();

    // @ 0x82BD9E88 (vector deleting destructor). No own teardown: the X360 thunk
    // rewrites the vftable, chains ~CRequestObject, and conditionally frees the
    // object through CMassiveBaseObject::operator delete when its low bit is set.
    virtual ~CRequestImpressionUpdate();

    // @ 0x82BD9590. Returns the impression-update server endpoint. The X360 loads
    // it through the .data pointer off_82F91D34 -> "/impsrv/4/activity".
    // FLAG: introduced as a virtual on this class (the per-request-type endpoint
    // getter of vftable off_82186F84); the base CRequestObject declaration is not
    // in this TU's ledger slice, so it is not marked `override`.
    virtual const char* GetRequestURL();

    // @ 0x82BD9DA8 (BLOCKED -- see the file header). Vftable Parse override the
    // response path dispatches; body lives in the ReadRemoveSignature/STUB slice.
    int Parse() override;

    // @ 0x82BD9AC8. Appends a model impression-report block (block type 41) and
    // bumps mnReportCount. Returns 0.
    int AddModelReport(unsigned char uTag54, unsigned int nTag38,
                       unsigned int nTag21, unsigned int nTag50,
                       unsigned long long nTag51, unsigned long long nTag52,
                       unsigned short sTag53, unsigned short sTag70);

    // @ 0x82BD9640. Appends a texture impression-report block (block type 46,
    // float field under tag 48) and bumps mnReportCount. Returns 0.
    int AddTextureRepor(unsigned char uTag54, unsigned int nTag38,
                        unsigned int nTag21, unsigned int nTag50,
                        unsigned long long nTag51, unsigned long long nTag52,
                        unsigned short sTag53, float fTag48,
                        unsigned short sTag70);

    // @ 0x82BD97D0. Appends a video impression-report block (byte-identical wire
    // shape to AddTextureRepor: block type 46, float under tag 48) and bumps
    // mnReportCount. Returns 0.
    int AddVideoReport(unsigned char uTag54, unsigned int nTag38,
                       unsigned int nTag21, unsigned int nTag50,
                       unsigned long long nTag51, unsigned long long nTag52,
                       unsigned short sTag53, float fTag48,
                       unsigned short sTag70);

    // @ 0x82BD9960. Appends an audio impression-report block (block type 43,
    // float field under tag 49, no tag-53 field) and bumps mnReportCount.
    // Returns 0.
    int AddAudioReport(unsigned char uTag54, unsigned int nTag38,
                       unsigned int nTag21, unsigned int nTag50,
                       unsigned long long nTag51, unsigned long long nTag52,
                       float fTag49, unsigned short sTag70);

    // @ 0x82BD9C28. Appends an interaction-report block (leading byte 224, block
    // type 24) and bumps mnInteractionCount. Returns 0.
    int AddInteractionR(unsigned char uTag31, unsigned int nTag38,
                        unsigned int nTag21, unsigned long long nTag51,
                        unsigned short sTag32);

    // @ 0x82BD9D20. Seals the request: writes the report count (tag 33) and the
    // interaction count (tag 41), FinishBaseBlock(211, timestamp, sign), then
    // SetStatus(2). Returns 0.
    int Finish();

private:
    unsigned short mnReportCount;      // +0x50 (model/texture/video/audio reports)
    unsigned short mnInteractionCount; // +0x52 (interaction reports)
};

} // namespace MassiveAdClient3
