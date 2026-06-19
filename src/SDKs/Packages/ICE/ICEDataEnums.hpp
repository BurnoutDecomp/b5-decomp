#ifndef SDKS_PACKAGES_ICE_ICEDATAENUMS_HPP
#define SDKS_PACKAGES_ICE_ICEDATAENUMS_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEDataEnums.hpp
//
// ICE::ICEParameter -- a quantized [0,1] scalar stored as a single u16. This is
// the canonical mirrored home (the DecFIGS DWARF declares ICEParameter and the
// ICE_PARAMETER_MAX constant in SDKs/Packages/ICE/ICEDataEnums.hpp, and the
// ledger maps ICE::ICEParameter::SetValue to ICEDataEnums.cpp).
//
// This file models a MINIMAL but FAITHFUL slice: just ICEParameter and
// ICE_PARAMETER_MAX. The future full `SDKs/Packages/ICE/ICEDataEnums.hpp` TU
// (which also holds the eICESpace enum, the ICE category-name constants, and the
// other ICE enums/types) EXTENDS this header in place -- it must NOT re-declare
// ICEParameter or ICE_PARAMETER_MAX in a parallel header.
//
// The trivial members that the compiler inlines away (and which are therefore not
// separate exported TUs) are provided inline here: the float ctor, GetValue,
// GetPackedPtr, the ordering/equality comparison operators, and operator=. Only
// SetValue(f32) is out-of-line, in ICEDataEnums.cpp (it is the one exported
// function, @0x8252ACE0). Because muPacked is monotonic in the stored value, the
// comparison operators compare muPacked directly.
// ============================================================================

namespace ICE
{

// ICEDataEnums.hpp:34 (DWARF). Maximum packed value -- ICEParameter quantizes the
// unit interval [0,1] across the full u16 range, so value 1.0 -> ICE_PARAMETER_MAX.
static const u16 ICE_PARAMETER_MAX = 65535;

// ICEDataEnums.hpp:237 (DWARF). A unit-interval scalar packed into a single u16.
// GetValue == muPacked / 65535; SetValue == round(clamp(value, 0, 1) * 65535).
struct ICEParameter
{
private:
    u16 muPacked;   // @0x00  quantized value in [0, ICE_PARAMETER_MAX]

public:
    // Ctor from a float just packs the value (== SetValue). Declared inline since
    // it is not a separate exported TU.
    ICEParameter(f32 lfValue)
    {
        SetValue(lfValue);
    }

    // Unpack: cast muPacked to float and divide by the full u16 range. Inverse of
    // SetValue's pack step.
    f32 GetValue() const
    {
        return (f32)muPacked / (f32)ICE_PARAMETER_MAX;
    }

    // Pack value into [0,1]-quantized u16. Out-of-line in ICEDataEnums.cpp.
    void SetValue(f32 lfValue);

    // Raw access to the packed storage.
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

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEDATAENUMS_HPP
