// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript comparison opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionEquals   @0x7FD938
//     _FunctionAptActionLessThan @0x7FD714
//     _FunctionAptActionGreater  @0x7FCF30
//
// Each compares the top two operands and pushes an AptBoolean; the result is
// `under OP top` (under = pushed first, top = pushed last), matching the stack
// convention. Same frame as the logic/arithmetic ops: SWF-v7 undefined rule
// (an undefined operand yields `undefined` unless gpUndefinedValue is null), an
// integer/numeric fast path, then the inline two-operand collapse.
//
//   Equals   : both defined ints -> exact ==, else |toFloat(under)-toFloat(top)| < 0.001.
//   LessThan : both defined ints -> under < top (asm: top > under), else toFloat.
//   Greater  : type-aware -- both defined strings -> strcmp(top,under) < 0 (i.e.
//              under > top lexically); else if either operand is a defined float ->
//              toFloat(under) > toFloat(top); else integer -> toInteger(top) <
//              toInteger(under). (Greater's v7-undefined-fail path pushes
//              gpUndefinedValue rather than skipping the collapse.)
//
// The console selects AptBoolean::spBooleanTrue/spBooleanFalse directly; here that
// is AptBoolean::Create(b) (same singletons). The stack collapse is inlined to
// mirror the asm.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // c_integer()->GetInt()
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // c_string()->GetInternalString()->GetBuffer()
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH::GetCharacterInst (CIH->undefined redirect)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"      // AptCharacterInst::GetTypeTag

#include <cmath>     // fabsf, fabs
#include <cstring>   // strcmp

// gpUndefinedValue: AptGlobals.cpp (built at AptInit). AptGetSwfVersion: AptLinker.cpp.
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();
// The value NaN test (console isNaN @0x82AF9768); homed in
// AptActionInterpreterBuiltins.cpp.
extern bool         isNaN(AptValue* pValue);

namespace
{
    inline bool BothDefinedInts(const AptValue* pTop, const AptValue* pUnder)
    {
        return pTop->getVtblIndex()   == AptVFT_Integer && pTop->getIsDefined()
            && pUnder->getVtblIndex() == AptVFT_Integer && pUnder->getIsDefined();
    }

    inline bool ShouldCompute(const AptValue* pTop, const AptValue* pUnder)
    {
        if (AptGetSwfVersion() == 7 &&
            !(pTop->getIsDefined() && pUnder->getIsDefined()) &&
            gpUndefinedValue)
            return false;
        return true;
    }

    inline bool IsDefinedString(const AptValue* pV)
    {
        AptVirtualFunctionTable_Indices t = pV->getVtblIndex();
        return (t == AptVFT_StringValue || t == AptVFT_StringObject) && pV->getIsDefined();
    }

    // The shared two-operand collapse: release the operands, push the result (AddRef).
    inline void CollapseTwo(AptActionInterpreter* pInterp, AptValue* pResult)
    {
        int top = pInterp->mnStackTop;
        if (top >= 2)
        {
            pInterp->mpStack[top - 1]->Release();
            pInterp->mpStack[top - 2]->Release();
            top -= 2;
            pInterp->mnStackTop = top;
        }
        pInterp->mpStack[top] = pResult;
        pInterp->mnStackTop = top + 1;
        pResult->AddRef();
    }

    // ---- Equals2 (ECMA-262 abstract equality) helpers --------------------

    // A live CharacterInstHandle (type 12 defined) or its "none" sentinel (type 37);
    // the leak collapses a destroyed CIH (its linked character's type tag == 0xF) to
    // the shared `undefined` singleton before the comparison runs.
    inline bool IsCIHForRedirect(const AptValue* pV)
    {
        AptVirtualFunctionTable_Indices t = pV->getVtblIndex();
        return (t == AptVFT_CharacterInstHandle && pV->getIsDefined())
            || t == AptVFT_CIHNone;
    }

    // Resolve a CIH operand to gpUndefinedValue when its character instance is the
    // destroyed/movie-end placeholder (mTypeFlags & 0xFC000000) == 0x3C000000, i.e.
    // GetTypeTag() == 0xF.  (asm: lwz mpCharacterInst[+0x20]; lwz mTypeFlags[+8];
    // clrrwi 26; cmpw 0x3C000000.)
    inline AptValue* ResolveCIHToUndefined(AptValue* pV)
    {
        if (IsCIHForRedirect(pV))
        {
            const AptCharacterInst* pInst =
                static_cast<const AptCIH*>(pV)->GetCharacterInst();
            if ((pInst->mTypeFlags & 0x3Fu) == 15u)   // x64 low-6-bit tag (X360 form 0x3C000000)
                return gpUndefinedValue;
        }
        return pV;
    }

    // type is one of the "primitive" comparison types (number/string/bool/string-obj)
    // AND the value is defined -- the leak's repeated `(type==N) && isDefined` guard.
    inline bool IsDefinedType(const AptValue* pV, int nType)
    {
        return static_cast<int>(pV->getVtblIndex()) == nType && pV->getIsDefined();
    }

    inline bool IsPrimitiveCompareType(const AptValue* pV)
    {
        return IsDefinedType(pV, AptVFT_Integer)        // 7
            || IsDefinedType(pV, AptVFT_Float)          // 6
            || IsDefinedType(pV, AptVFT_Boolean)        // 5
            || IsDefinedType(pV, AptVFT_StringValue)    // 1
            || IsDefinedType(pV, AptVFT_StringObject);  // 33
    }

    // A defined string value (primitive type 1 or boxed type 33).
    inline bool IsDefinedStringType(const AptValue* pV)
    {
        return IsDefinedType(pV, AptVFT_StringValue)
            || IsDefinedType(pV, AptVFT_StringObject);
    }

    // toFloat of a value that is "really" an integer (the leak widens via fcfid).
    inline float IntToFloat(int n) { return static_cast<float>(n); }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionEquals @0x7FD938 -- under == top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionEquals(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
        {
            pResult = AptBoolean::Create(pTop->c_integer()->GetInt() == pUnder->c_integer()->GetInt());
        }
        else
        {
            float ftop   = pTop->toFloat();
            float funder = pUnder->toFloat();
            pResult = AptBoolean::Create(fabsf(ftop - funder) < 0.001f);
        }
    }

    CollapseTwo(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionLessThan @0x7FD714 -- under < top (asm computes top > under).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionLessThan(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
        {
            pResult = AptBoolean::Create(pTop->c_integer()->GetInt() > pUnder->c_integer()->GetInt());
        }
        else
        {
            float ftop   = pTop->toFloat();
            float funder = pUnder->toFloat();
            pResult = AptBoolean::Create(ftop > funder);
        }
    }

    CollapseTwo(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionGreater @0x7FCF30 -- under > top (type-aware).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGreater(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (IsDefinedString(pTop) && IsDefinedString(pUnder))
        {
            // Lexical: strcmp(top, under) < 0  <=>  under > top.
            const char* sTop   = pTop->c_string()->GetInternalString()->GetBuffer();
            const char* sUnder = pUnder->c_string()->GetInternalString()->GetBuffer();
            pResult = AptBoolean::Create(strcmp(sTop, sUnder) < 0);
        }
        else if ((pTop->getVtblIndex()   == AptVFT_Float && pTop->getIsDefined()) ||
                 (pUnder->getVtblIndex() == AptVFT_Float && pUnder->getIsDefined()))
        {
            pResult = AptBoolean::Create(pUnder->toFloat() > pTop->toFloat());
        }
        else
        {
            pResult = AptBoolean::Create(pTop->toInteger() < pUnder->toInteger());
        }
    }

    CollapseTwo(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionLessThan2 (0x48; PS3 @0x801A28) -- ECMA `under < top`,
// type-aware. SWF v7: any undefined operand -> undefined result (ShouldCompute).
// Both defined strings -> strcmp(under, top) < 0; else a NaN operand -> undefined;
// else either defined Float -> toFloat under < top; else toInteger under < top.
// Result collapses the two operands (the console's inline pop-2 + push + AddRef).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionLessThan2(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];   // v4
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];   // v5

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (IsDefinedString(pTop) && IsDefinedString(pUnder))
        {
            // Lexical: strcmp(under, top) < 0  <=>  under < top.
            const char* sUnder = pUnder->c_string()->GetInternalString()->GetBuffer();
            const char* sTop   = pTop->c_string()->GetInternalString()->GetBuffer();
            pResult = AptBoolean::Create(strcmp(sUnder, sTop) < 0);
        }
        else if (isNaN(pTop) || isNaN(pUnder))   // isNaN @0x82AF9768 (Builtins.cpp)
        {
            pResult = gpUndefinedValue;
        }
        else if ((pTop->getVtblIndex()   == AptVFT_Float && pTop->getIsDefined()) ||
                 (pUnder->getVtblIndex() == AptVFT_Float && pUnder->getIsDefined()))
        {
            pResult = AptBoolean::Create(pUnder->toFloat() < pTop->toFloat());
        }
        else
        {
            pResult = AptBoolean::Create(pUnder->toInteger() < pTop->toInteger());
        }
    }

    CollapseTwo(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionEquals2 @0x82AFA368 (0x49) -- the SWF-v7 ECMA-262 abstract
// equality `under == top`.
//
// A type-aware equality (the ActionScript ECMA AbstractEquality, ECMA-262 11.9.3):
//   * CIH operands whose linked character is the destroyed/movie-end placeholder
//     (type tag 0xF) are first collapsed to `undefined`.
//   * SWF v7: when at least one operand is `undefined`, the result is whether BOTH
//     are undefined (true) or not (false); the typed comparison is skipped.
//   * Same primitive number/string/bool type -> the natural compare (int ==, float
//     within 0.001, string EAStringC ==, bool int ==).
//   * Mixed types -> the ECMA coercion ladder: object-vs-string -> string compare;
//     numeric-vs-string when neither is a parseable float -> integer compare,
//     otherwise float compare within 0.001; one side numeric the other not ->
//     numeric coercion (int/float) within 0.001.
//
// The console pushes AptBoolean::spBooleanTrue/spBooleanFalse directly (here
// AptBoolean::Create(b)); the result `bEqual` mirrors the asm's r31 latch and the
// two operands are collapsed via the shared pop-2 + push helper.
//
// Faithfully transliterated from the X360 ARTIST pseudocode/asm; the string
// equality the console performs through Burnout_X360_Artist_0040_0 on the embedded
// EAStringC is the EAStringC operator==, and the '.' scan (sub_82AD8B80) is
// EAStringC::UTF8_Find(".") -- both reached via the established c_string() path.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionEquals2(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];   // v4
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];   // v5

    // CIH -> undefined redirect (both operands).
    pTop   = ResolveCIHToUndefined(pTop);
    pUnder = ResolveCIHToUndefined(pUnder);

    // SWF v7 undefined rule: count the undefined operands.
    int v2 = 0;
    if (AptGetSwfVersion() == 7)
    {
        if (!pTop->getIsDefined())
            v2 = 1;
        if (!pUnder->getIsDefined())
            ++v2;
        if (v2 > 0)
        {
            // At least one operand undefined: equal only when BOTH are undefined.
            CollapseTwo(pInterp, AptBoolean::Create(v2 == 2));
            return;
        }
    }

    // ---- LABEL_24: the type-aware comparison; v2 latches the result (1 == equal).
    const int nTop   = static_cast<int>(pTop->getVtblIndex());     // v15
    const int nUnder = static_cast<int>(pUnder->getVtblIndex());   // v21

    if (IsPrimitiveCompareType(pTop) && IsPrimitiveCompareType(pUnder))
    {
        // Both operands are defined primitive number/string/bool values.
        if (!pTop->getIsDefined())
        {
            v2 = 1;   // LABEL_187 (defensive; pTop is defined here)
        }
        else if (nTop == AptVFT_Integer && IsDefinedType(pUnder, AptVFT_Integer))
        {
            // LABEL_133/134/135: integer == integer.
            v2 = (pUnder->toInteger() - pTop->toInteger()) == 0;
        }
        else if (nTop == AptVFT_Float && IsDefinedType(pUnder, AptVFT_Float))
        {
            // float == float (exact, the asm's fcmpu equal test).
            v2 = (pTop->toFloat() == pUnder->toFloat()) ? 1 : 0;
        }
        else if ((nTop == AptVFT_StringValue || nTop == AptVFT_StringObject)
              && (IsDefinedType(pUnder, AptVFT_StringValue)
               || IsDefinedType(pUnder, AptVFT_StringObject)))
        {
            // String == String: compare the embedded EAStringC values.
            const EAStringC& sTop   = *pTop->c_string()->GetInternalString();
            const EAStringC& sUnder = *pUnder->c_string()->GetInternalString();
            v2 = (sTop == sUnder) ? 1 : 0;
        }
        else
        {
            // Mixed primitive types -> the ECMA coercion ladder below.
            goto MixedCompare;
        }
        CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
        return;
    }

    // ---- LABEL_190: at least one operand is NOT a defined primitive type.
    if (nTop != nUnder)
    {
        if (!pTop->getIsDefined() && !pUnder->getIsDefined())
            v2 = 1;   // both undefined-ish, same-shaped -> equal
        // else: different shapes -> not equal (v2 stays 0)
        CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
        return;
    }
    // nTop == nUnder (same non-primitive type) -> fall through to the ladder.

MixedCompare:
    {
        // asm: if ( (v15 != 7 && v15 != 6) || isNaN(v5) ) { ...maybe LABEL_105... }
        // The string/identity ladder (LABEL_105) is entered when BOTH:
        //   (top is not a number)  OR (under is NaN)        -- the outer guard
        //   (under is not a number) OR (top is NaN)          -- the inner guard
        // Otherwise execution falls through to the numeric coercion (LABEL_964).
        const bool bTopIsNumberType   = (nTop   == AptVFT_Integer || nTop   == AptVFT_Float);
        const bool bUnderIsNumberType = (nUnder == AptVFT_Integer || nUnder == AptVFT_Float);

        bool bStringLadder = false;
        if (!bTopIsNumberType || isNaN(pUnder))   // isNaN -- outer guard
        {
            if (!bUnderIsNumberType || isNaN(pTop))   // isNaN -- inner guard
                bStringLadder = true;
        }

        if (bStringLadder)
        {
            // ---- LABEL_105: string-typed / object coercion sub-ladder. The named
            // predicates mirror the asm's v39/v40/v43/v46/v47/v48 type-tag tests.
            const bool bTopDefStr    = IsDefinedStringType(pTop);            // v39 / v48
            const bool bUnderDefBool = IsDefinedType(pUnder, AptVFT_Boolean);// v40 / v47
            const bool bUnderDefStr  = IsDefinedStringType(pUnder);          // v46

            // v39 && !v40: both render to strings -> EAStringC string compare.
            if (bTopDefStr && !bUnderDefBool)
            {
                EAStringC scratchTop;
                EAStringC scratchUnder;
                const EAStringC* pStrTop   = AptValue::Get_ToString(pTop,   &scratchTop);
                const EAStringC* pStrUnder = AptValue::Get_ToString(pUnder, &scratchUnder);
                v2 = (*pStrTop == *pStrUnder) ? 1 : 0;
                // scratchTop/scratchUnder destructors == the asm's two
                // EAStringC::DecreaseInternalRefCount calls.
                CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
                return;
            }

            // asm: if (!v43) goto LABEL_132; else if (v46) -> LABEL_132 else LABEL_133.
            //      LABEL_132: if (!v47) goto LABEL_136; else if (v48) goto LABEL_136.
            //      LABEL_136: v51 = (v5 - v4); LABEL_135: v2 = (v51 == 0).
            bool bLabel136 = false;
            if (!bTopDefStr || bUnderDefStr)         // reach LABEL_132 (asm v43 = top-is-string-defined, == bTopDefStr)
            {
                if (!bUnderDefBool || bTopDefStr)    // !v47, or (v47 && v48)
                    bLabel136 = true;
            }

            if (bLabel136)
            {
                v2 = (pUnder == pTop) ? 1 : 0;       // LABEL_136: (v5 - v4) == 0
                CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
                return;
            }

            // LABEL_133/134/135: integer-coerced compare.
            v2 = (pUnder->toInteger() - pTop->toInteger()) == 0;
            CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
            return;
        }

        // ---- LABEL_964 region: numeric/string coercion with float tolerance.
        // Decide per operand whether its string form parses as a float (contains '.').
        bool bTopHasDot   = false;   // v53
        bool bUnderHasDot = false;   // v54

        if (IsDefinedType(pTop, AptVFT_StringValue) || IsDefinedType(pTop, AptVFT_StringObject)
            || IsDefinedType(pTop, AptVFT_Float))
        {
            if (!IsDefinedType(pTop, AptVFT_Float)
                && pTop->c_string()->GetInternalString()->UTF8_Find(".", 0) != -1)
                bTopHasDot = true;
        }
        if (IsDefinedType(pUnder, AptVFT_StringValue) || IsDefinedType(pUnder, AptVFT_StringObject)
            || IsDefinedType(pUnder, AptVFT_Float))
        {
            if (!IsDefinedType(pUnder, AptVFT_Float)
                && pUnder->c_string()->GetInternalString()->UTF8_Find(".", 0) != -1)
                bUnderHasDot = true;
        }

        float fTop;     // the top side's numeric form
        float fUnder;   // the under side's numeric form

        if (IsDefinedType(pTop, AptVFT_Integer))
        {
            const int nTopInt = pTop->toInteger();
            if (!bUnderHasDot)
            {
                // both integral -> integer compare.
                v2 = (pUnder->toInteger() - nTopInt) == 0;
                CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
                return;
            }
            fTop = IntToFloat(nTopInt);          // widened via fcfid
            fUnder = pUnder->toFloat();
        }
        else if (IsDefinedType(pUnder, AptVFT_Integer))
        {
            const int nUnderInt = pUnder->toInteger();
            if (!bTopHasDot)
            {
                // both integral -> integer compare (v5 side fixed).
                v2 = (nUnderInt - pTop->toInteger()) == 0;
                CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
                return;
            }
            fTop   = pTop->toFloat();
            fUnder = IntToFloat(nUnderInt);      // widened via fcfid
        }
        else
        {
            fTop   = pTop->toFloat();
            fUnder = pUnder->toFloat();
        }

        // float tolerance: |fTop - fUnder| < 0.001 (flt_82013F90).
        v2 = (fabsf(fTop - fUnder) < 0.001f) ? 1 : 0;
        CollapseTwo(pInterp, AptBoolean::Create(v2 != 0));
        return;
    }
}
