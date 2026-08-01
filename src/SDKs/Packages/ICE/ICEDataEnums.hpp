#ifndef SDKS_PACKAGES_ICE_ICEDATAENUMS_HPP
#define SDKS_PACKAGES_ICE_ICEDATAENUMS_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEDataEnums.hpp
//
// The ICE camera-system value/channel vocabulary: the quantized ICEParameter,
// the ICEValue scalar union, the per-channel ICEChannel, the per-element
// ICEElementDescription, plus the ICE enums (eICESpace / ICEInputType /
// ICEDataType) and category-name constants. This is the canonical mirrored home
// (the DecFIGS DWARF declares all of these in SDKs/Packages/ICE/ICEDataEnums.hpp,
// and the ledger maps ICE::ICEParameter::SetValue to ICEDataEnums.cpp).
//
// GROWN from the minimal ICEParameter-only slice to the full DWARF layout so the
// ICEData.cpp body phase can be reconstructed against a frozen layout. Member
// NAMES/ORDER/TYPES are authoritative from the DWARF; trivial inline-away
// accessors carry bodies, everything substantial is declaration-only (the bodies
// live in ICEData.cpp / sibling TUs, and the per-TU `cl /c` gate does not link).
//
// (DWARF spells the scalars float32_t / int32_t / short int; those are NOT project
// types -> f32 / s32 / s16 / u16, per the type vocabulary in types.hpp.)
// ============================================================================

namespace ICE
{

// ---------------------------------------------------------------------------
// Category-name counts (ICEDataEnums.hpp:26-32, DWARF).
// ---------------------------------------------------------------------------
static const s32 ICE_MAX_GENERIC_CATEGORY_NAMES = 1;
static const s32 ICE_MAX_WORLD_CATEGORY_NAMES   = 2;
static const s32 ICE_MAX_VEHICLE_CATEGORY_NAMES = 2;
static const s32 ICE_MAX_EVENTS_CATEGORY_NAMES  = 2;

// ICEDataEnums.hpp:34 (DWARF). Maximum packed value -- ICEParameter quantizes the
// unit interval [0,1] across the full u16 range, so value 1.0 -> ICE_PARAMETER_MAX.
static const u16 ICE_PARAMETER_MAX = 65535;

// ---------------------------------------------------------------------------
// eICESpace (ICEDataEnums.hpp:41, DWARF). The coordinate / reference space an ICE
// camera take is authored in. Legacy eCamelCase enumerators preserved verbatim
// from the DWARF (engine-facing enum; matching the original API).
// ---------------------------------------------------------------------------
enum eICESpace
{
    eICE_CAR_SPACE             = 0,
    eICE_WORLD_SPACE           = 1,
    eICE_HYBRID_SPACE          = 2,
    eICE_SCENE_SPACE           = 3,
    eICE_CAR2_SPACE            = 4,
    eICE_TRAFFIC_LIGHT_SPACE   = 5,
    eICE_TAKEDOWN_SPACE        = 6,
    eICE_IMPACT_SPACE          = 7,
    eICE_REVERSE_TAKEDOWN_SPACE = 8,
    eICE_GAMEPLAY_SPACE        = 9,
    eICE_HEADING_SPACE         = 10,
    eICE_BYSTANDER_SPACE       = 11,
    eICE_HEADING2_SPACE        = 12,
    eICE_LOOSE_HEADING_SPACE   = 13,
    eICE_NUM_SPACES            = 14
};

// ---------------------------------------------------------------------------
// ICEInputType (ICEDataEnums.hpp:214, DWARF). How an element is driven at edit
// time. Legacy eCamelCase enumerators preserved verbatim from the DWARF.
// ---------------------------------------------------------------------------
enum ICEInputType
{
    eICE_INPUT_BOOL        = 0,
    eICE_INPUT_ANALOG      = 1,
    eICE_INPUT_DISCREET    = 2,
    eICE_INPUT_ACCELERATED = 3,
    eICE_NUM_INPUT_TYPES   = 4
};

// ---------------------------------------------------------------------------
// ICEDataType (ICEDataEnums.hpp:225, DWARF). The stored representation of an
// element's value. Legacy eCamelCase enumerators preserved verbatim.
// ---------------------------------------------------------------------------
enum ICEDataType
{
    eICE_INT            = 0,
    eICE_UINT           = 1,
    eICE_HASH           = 2,
    eICE_FIXED          = 3,
    eICE_FLOAT          = 4,
    eICE_NUM_DATA_TYPES = 5
};

// ICEDataEnums.hpp:176 (DWARF). Free function -- is the given element index a valid
// ICE element. Declaration-only (out-of-line in ICEDataEnums.cpp / a sibling TU).
bool IsElementValid(s32 liElement);

// ---------------------------------------------------------------------------
// ICEParameter (ICEDataEnums.hpp:237, DWARF). A unit-interval scalar packed into a
// single u16. GetValue == muPacked / 65535; SetValue == round(clamp(v,0,1)*65535).
//
// Trivial members the compiler inlines away (and which are therefore not separate
// exported TUs) are provided inline here: the float ctor, GetValue, GetPackedPtr,
// the ordering/equality comparison operators, and operator=. Only SetValue(f32) is
// out-of-line, in ICEDataEnums.cpp (the one exported function, @0x8252ACE0).
// Because muPacked is monotonic in the stored value, the comparison operators
// compare muPacked directly.
// ---------------------------------------------------------------------------
struct ICEParameter
{
private:
    u16 muPacked;   // @0x00  quantized value in [0, ICE_PARAMETER_MAX]

public:
    ICEParameter(f32 lfValue)
    {
        SetValue(lfValue);
    }

    f32 GetValue() const
    {
        return (f32)muPacked / (f32)ICE_PARAMETER_MAX;
    }

    // Pack value into [0,1]-quantized u16. Out-of-line in ICEDataEnums.cpp.
    void SetValue(f32 lfValue);

    u16* GetPackedPtr()
    {
        return &muPacked;
    }

    // muPacked is monotonic in the stored value, so ordering on the packed integer
    // is equivalent to ordering on the unit-interval value.
    bool operator<(const ICEParameter& lrOther) const  { return muPacked <  lrOther.muPacked; }
    bool operator>(const ICEParameter& lrOther) const  { return muPacked >  lrOther.muPacked; }
    bool operator>=(const ICEParameter& lrOther) const { return muPacked >= lrOther.muPacked; }
    bool operator<=(const ICEParameter& lrOther) const { return muPacked <= lrOther.muPacked; }
    bool operator==(const ICEParameter& lrOther) const { return muPacked == lrOther.muPacked; }
    bool operator!=(const ICEParameter& lrOther) const { return muPacked != lrOther.muPacked; }

    ICEParameter& operator=(const ICEParameter& lrOther) = default;
};

// ---------------------------------------------------------------------------
// ICEValue (ICEDataEnums.hpp:265, DWARF). A 4-byte scalar whose raw storage is a
// union reinterpreted as int or float -- the in-memory form an element's value
// takes once decoded. The single member is the union `Value { s32 liInt; f32
// lfFloat; }`. All members are trivial inline-away accessors (read or write one
// union field); none are separate exported TUs, so they carry inline bodies.
//
// GetBool() == (Value.liInt != 0): the bool query just tests the raw 4-byte word
// against zero (true for any non-zero int OR any non-zero-bit float). FLAG: this is
// the natural reading of a union-backed bool query and matches the DWARF's lack of
// any float-compare member; confirm against the body-phase pseudocode if a
// float == 0.0f form ever shows up.
// ---------------------------------------------------------------------------
struct ICEValue
{
private:
    union
    {
        s32 liInt;     // raw int view
        f32 lfFloat;   // raw float view
    } Value;

public:
    ICEValue()                  { Value.liInt = 0; }
    ICEValue(f32 lfValue)       { Value.lfFloat = lfValue; }
    ICEValue(s32 liValue)       { Value.liInt = liValue; }
    ICEValue(u32 luValue)       { Value.liInt = (s32)luValue; }

    void Set(s32 liValue)       { Value.liInt = liValue; }
    void Set(f32 lfValue)       { Value.lfFloat = lfValue; }

    bool GetBool() const        { return Value.liInt != 0; }
    f32  GetFloat() const       { return Value.lfFloat; }
    s32  GetSignedInt() const   { return Value.liInt; }
    u32  GetUnsignedInt() const { return (u32)Value.liInt; }

    ICEValue& operator=(f32 lfValue) { Value.lfFloat = lfValue; return *this; }
    ICEValue& operator=(s32 liValue) { Value.liInt = liValue; return *this; }
    ICEValue& operator=(const ICEValue& lrOther) { Value = lrOther.Value; return *this; }

    bool operator==(const ICEValue& lrOther) const { return Value.liInt == lrOther.Value.liInt; }
    bool operator!=(const ICEValue& lrOther) const { return Value.liInt != lrOther.Value.liInt; }
};

// ---------------------------------------------------------------------------
// ICEChannel (ICEDataEnums.hpp:371, DWARF). One animatable channel of an ICE take:
// a run of keys (indices into the take's key table) and the parameters that drive
// the intervals between them. ICETake embeds an ICEChannel[12] by value.
//
// LAYOUT (DWARF ICEDataEnums.hpp:417-421):
//   @0x00  u16 mu16Keys
//   @0x02  u16 mu16Intervals
//   @0x04  u16 mu16CurrentInterval
//   @0x08  u16* mpKeyIndices         (pointer -- aligned to 4)
//   @0x0C  ICEParameter* mpParameters
//
// Trivial accessors that read/write a member directly carry inline bodies; the
// substantial methods (curve evaluation, spacing enforcement, key/interval
// arithmetic) are declaration-only -- their bodies live in ICEData.cpp / sibling
// TUs. In particular the three ICEData.cpp ICEChannel funcs --
// GetIntervalBracket(u16,...), GetIntervalSize(u16), IsHardCut(u16,s32) -- stay
// declaration-only here; the body phase defines them out-of-line.
// ---------------------------------------------------------------------------
struct ICEChannel
{
private:
    u16           mu16Keys;            // @0x00  number of keys
    u16           mu16Intervals;       // @0x02  number of intervals (keys - 1)
    u16           mu16CurrentInterval; // @0x04  cached current interval
    u16*          mpKeyIndices;        // @0x08  -> key index table
    ICEParameter* mpParameters;        // @0x0C  -> per-interval parameters

public:
    // --- DECLARE-ONLY (bodies in ICEData.cpp / sibling TUs) ---
    void Construct();
    void Prepare();

    // --- Trivial inline-away accessors ---
    s16 GetNumKeys() const            { return (s16)mu16Keys; }
    void SetNumKeys(s16 li16Keys)     { mu16Keys = (u16)li16Keys; }

    s16 GetNumIntervals() const       { return (s16)mu16Intervals; }
    void SetNumIntervals(s16 li16Int) { mu16Intervals = (u16)li16Int; }

    s16 GetCurrentInterval() const          { return (s16)mu16CurrentInterval; }
    void SetCurrentInterval(u16 lu16Interval) { mu16CurrentInterval = lu16Interval; }

    bool HasData() const              { return mpKeyIndices != 0; }

    void SetKeyData(u16* lpKeyIndices)              { mpKeyIndices = lpKeyIndices; }
    u16* GetKeyData() const                         { return mpKeyIndices; }
    void SetParameterData(ICEParameter* lpParams)   { mpParameters = lpParams; }
    ICEParameter* GetParameterData() const          { return mpParameters; }

    // The stored key index at an interval boundary. No out-of-line X360 symbol --
    // the console inlines it; the read is attested identically in ICETake::GetSlope
    // @0x825303C0 (`v16 = *(2 * a4 + *(v13 + 220) - 2)`, i.e. mpKeyIndices[a4 - 1]
    // for a call spelled GetKeyIndex(interval - 1)), in the private
    // ICETake::SetParameter @0x82530668, and in ICETakeData::SaveData @0x82532CF8.
    // Signed return per the DWARF (ICEDataEnums.hpp:387 `int16_t GetKeyIndex(uint16_t)`).
    s16  GetKeyIndex(u16 lu16Interval) const        { return (s16)mpKeyIndices[lu16Interval]; }
    void SetIntervalKey(u16 lu16Interval, s16 li16Key);

    f32  GetIntervalParameter(u16 lu16Interval) const;
    void SetIntervalParameter(u16 lu16Interval, f32 lfParameter);

    bool IsHardCut(u16 lu16Interval, s32 liElement) const;   // ICEData.cpp
    bool IsHardCut(s32 liElement) const;

    f32  GetIntervalSize(u16 lu16Interval) const;            // ICEData.cpp
    void GetIntervalBracket(u16 lu16Interval, f32* lpfStart, f32* lpfEnd) const; // ICEData.cpp
    f32  GetIntervalSize() const;
    void GetIntervalBracket(f32* lpfStart, f32* lpfEnd) const;

    f32  GetIntervalEnd() const;
    f32  GetIntervalStart() const;
    f32  GetIntervalStart(u16 lu16Interval) const;
    f32  GetIntervalEnd(u16 lu16Interval) const;

    bool SetParameter(f32 lfParameter, f32* lpfStart, f32* lpfEnd);
    void EnforceSpacing(u16 lu16Interval, f32 lfMinSpacing);
};

// ---------------------------------------------------------------------------
// ICEElementDescription (ICEDataEnums.hpp:293, DWARF). The static schema for one
// ICE element: its tag/display name, channel slot, data type & bit width, default/
// min/max, quantization ranges, edit-input behaviour, and (for discreet types) the
// token table. Not a runtime value -- a description of how to encode/decode one.
//
// Fields IN ORDER (DWARF ICEDataEnums.hpp:313-347). All-public (the DWARF lists no
// access specifier before the members; matched as a plain struct).
//
// Trivial type-predicate / default accessors carry inline bodies; the encode/decode
// /raw machinery is declaration-only (bodies in a sibling TU).
// ---------------------------------------------------------------------------
struct ICEElementDescription
{
    char*        mpTag;            // @0x00  element tag string
    char*        mpDisplayName;    // @0x04  human-readable name
    s32          miChannelNumber;  // @0x08  channel slot this element drives
    ICEDataType  mDataType;        // @0x0C  stored representation
    s32          miDataBits;       // @0x10  bit width of the stored value
    ICEValue     mDefault;         // @0x14  default value
    ICEValue     mMin;             // @0x18  minimum value
    ICEValue     mMax;             // @0x1C  maximum value
    f32          mfQuantSlotsLo;   // @0x20  quantization slot count (low range)
    f32          mfQuantRangeLo;   // @0x24  quantization value span (low range)
    f32          mfQuantSlotsHi;   // @0x28  quantization slot count (high range)
    f32          mfQuantRangeHi;   // @0x2C  quantization value span (high range)
    s32          miCubicLinear;    // @0x30  cubic vs linear interpolation flag
    s32          miTangentScale;   // @0x34  tangent scaling mode
    ICEInputType mInputType;       // @0x38  edit-time input behaviour
    s32          miIncButton;      // @0x3C  increment button id
    s32          miDecButton;      // @0x40  decrement button id
    f32          mfIncDecSpeed;    // @0x44  inc/dec base speed
    f32          mfIncDecAccel;    // @0x48  inc/dec acceleration
    f32          mfIncDecDecel;    // @0x4C  inc/dec deceleration
    char**       mpTokens;         // @0x50  discreet-value token table
    s32          miTokens;         // @0x54  number of tokens

public:
    // --- DECLARE-ONLY (bodies in a sibling TU) ---
    void Prepare();

    // --- Type predicates ---
    // IsFloat is the one X360-attested export (0x8252AD60): returns true for the
    // float-decoding types eICE_FIXED (3) AND eICE_FLOAT (4) -- the asm tests
    // (mDataType==3) || (mDataType==4), NOT just FLOAT. The other two are
    // inline-away (no export); modeled as the clean partition of the 5 data types
    // {INT,UINT}=int, {HASH}=hash, {FIXED,FLOAT}=float. FLAG: IsInt/IsHash grouping
    // is inferred (only IsFloat is asm-verified).
    bool IsFloat() const { return mDataType == eICE_FIXED || mDataType == eICE_FLOAT; }
    bool IsInt() const   { return mDataType == eICE_INT   || mDataType == eICE_UINT; }
    bool IsHash() const  { return mDataType == eICE_HASH; }

    // --- DECLARE-ONLY (encode/decode machinery; bodies elsewhere) ---
    u32  Encode(f32 lfValue) const;
    f32  Decode(u32 luEncoded) const;

    // --- Trivial inline-away default accessors ---
    f32 GetDefaultFloat() const     { return mDefault.GetFloat(); }
    s32 GetDefaultSignedInt() const { return mDefault.GetSignedInt(); }

    // --- DECLARE-ONLY (raw element-data access / value get-set; bodies elsewhere) ---
    u32      GetRawInt(u8* lpElementData, s32 liIndex) const;
    void     SetRawInt(u8* lpElementData, s32 liIndex, u32 luValue) const;
    ICEValue GetValue(u8* lpElementData, s32 liIndex) const;
    void     SetValue(u8* lpElementData, s32 liIndex, const ICEValue& lrValue) const;
    u32      GetDataSize(s32 liNumElements) const;
};

// The element schema: eICE_NUM_ELEMENTS descriptions, one per element index. The
// table is statically initialised in ICEData.cpp; it is declared here so every ICE
// TU that walks the schema (the codec, the editor) shares the one definition.
const s32 eICE_NUM_ELEMENTS = 48;
extern ICEElementDescription ICEElementDescriptions[eICE_NUM_ELEMENTS];

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEDATAENUMS_HPP
