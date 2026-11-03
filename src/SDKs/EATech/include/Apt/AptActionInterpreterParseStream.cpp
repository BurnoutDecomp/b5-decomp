// ARTIST 0x82AF3440: native four-byte action operand relocation.
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptConstFile.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction1.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction2.h"
#include "SDKs/EATech/include/Apt/AptString/StringPool.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"

extern AptValue* gpUndefinedValue;

namespace
{
    // The two-word operand records and constant table are identified by the
    // loads/stores in ARTIST 0x82AF3540. Names describe their observed roles.
    union AptParsedValueSlot { int32_t mnIndex; AptValue* mpValue; };
    struct AptParsedValueList { int32_t mnCount; AptParsedValueSlot* mpValues; };
    struct AptParsedConstant { int32_t mnType; union { int32_t mnValue; float mfValue; const char* mpString; }; };
    struct AptParsedUrl { const char* mpUrl; const char* mpTarget; };
    struct AptParsedTry { int32_t mnTryLength, mnCatchLength, mnFinallyLength; uint8_t maFlagsAndRegister[4]; const char* mpCatchName; };
    CGS_STATIC_ASSERT((sizeof(AptParsedValueSlot) == 4), "ARTIST value slot");
    CGS_STATIC_ASSERT((sizeof(AptParsedValueList) == 8), "ARTIST value list");
    CGS_STATIC_ASSERT((sizeof(AptParsedConstant) == 8), "ARTIST constant record");
    CGS_STATIC_ASSERT((sizeof(AptParsedUrl) == 8), "ARTIST URL record");
    CGS_STATIC_ASSERT((sizeof(AptParsedTry) == 20), "ARTIST try record");

    unsigned char* AlignActionOperand(unsigned char* p)
    { return reinterpret_cast<unsigned char*>((reinterpret_cast<uintptr_t>(p) + 3u) & ~static_cast<uintptr_t>(3u)); }

    template <class T> void Relocate(T*& p, uintptr_t base, bool unresolve)
    {
        if (p)
            p = reinterpret_cast<T*>(unresolve ? reinterpret_cast<uintptr_t>(p) - base : reinterpret_cast<uintptr_t>(p) + base);
    }

    AptValue* ResolveConstant(AptParsedConstant& c, AptConstFile* context)
    {
        switch (c.mnType)
        {
        case 1:
        {
            Relocate(c.mpString, reinterpret_cast<uintptr_t>(context), false);
            AptValue* result = StringPool::GetFromPool(c.mpString);
            Relocate(c.mpString, reinterpret_cast<uintptr_t>(context), true);
            return result;
        }
        case 6: return c.mfValue == 0.0f ? static_cast<AptValue*>(AptInteger::Create(0)) : static_cast<AptValue*>(AptFloat::Create(c.mfValue));
        case 7: return AptInteger::Create(c.mnValue);
        case 8: return AptLookup::PoolHolds(c.mnValue) ? AptLookup::GetPoolEntry(c.mnValue) : 0;
        case 5: return AptBoolean::Create(c.mnValue != 0);
        case 4: return AptRegister::FileHolds(c.mnValue) ? AptRegister::GetFileEntry(c.mnValue) : 0;
        case 3: return gpUndefinedValue;
        default: return 0;
        }
    }
}

void AptActionInterpreter::_parseStream(unsigned char* stream, uintptr_t base,
                                        AptConstFile* context, int32_t* valueCount)
{
    const bool unresolve = context == 0;
    if (!unresolve)
        gpValuesToRelease->ReleaseValues();
    unsigned char* pc = stream;
    for (unsigned int op = *pc++; op != 0; op = *pc++)
    {
        switch (op)
        {
        case 0x77: case 0xB4: case 0xB7: pc += 4; break;
        case 0xA2: case 0xAE: case 0xAF: case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB5: ++pc; break;
        case 0xA3: case 0xB6: pc += 2; break;
        case 0x81: case 0x87: case 0x99: case 0x9D: case 0x9F: case 0xB8:
            pc = AlignActionOperand(pc) + 4; break;
        case 0x8B: case 0x8C: case 0xA1: case 0xA4: case 0xA5: case 0xA6: case 0xA7:
        {
            const char** slot = reinterpret_cast<const char**>(AlignActionOperand(pc));
            pc = reinterpret_cast<unsigned char*>(slot + 1); Relocate(*slot, base, unresolve); break;
        }
        case 0x83:
        {
            AptParsedUrl* r = reinterpret_cast<AptParsedUrl*>(AlignActionOperand(pc));
            pc = reinterpret_cast<unsigned char*>(r + 1); Relocate(r->mpUrl, base, unresolve); Relocate(r->mpTarget, base, unresolve); break;
        }
        case 0x94:
        {
            uintptr_t* slot = reinterpret_cast<uintptr_t*>(AlignActionOperand(pc));
            pc = reinterpret_cast<unsigned char*>(slot + 1);
            *slot = unresolve ? *slot - reinterpret_cast<uintptr_t>(pc) : *slot + reinterpret_cast<uintptr_t>(pc); break;
        }
        case 0x8F:
        {
            AptParsedTry* r = reinterpret_cast<AptParsedTry*>(AlignActionOperand(pc));
            pc = reinterpret_cast<unsigned char*>(r + 1);
            if ((r->maFlagsAndRegister[0] & 4u) == 0) Relocate(r->mpCatchName, base, unresolve); break;
        }
        case 0x9B:
        {
            AptScriptFunction1ByteCode* r = reinterpret_cast<AptScriptFunction1ByteCode*>(AlignActionOperand(pc));
            pc = r->maByteCode;
            Relocate(r->mpName, base, unresolve);
            if (!unresolve) Relocate(r->mppArgumentNames, base, false);
            for (int32_t i = 0; i < r->mnNumArguments; ++i) Relocate(r->mppArgumentNames[i], base, unresolve);
            if (unresolve) { Relocate(r->mppArgumentNames, base, true); r->mnConstantPoolCount = static_cast<int32_t>(0x98765432u); r->mppConstantPool = reinterpret_cast<AptValue**>(0x12345678u); }
            break;
        }
        case 0x8E:
        {
            AptScriptFunction2ByteCode* r = reinterpret_cast<AptScriptFunction2ByteCode*>(AlignActionOperand(pc));
            pc = r->maByteCode;
            Relocate(r->mpName, base, unresolve);
            if (!unresolve) Relocate(r->mpArgTable, base, false);
            for (int32_t i = 0; i < r->mnNumArguments; ++i) Relocate(r->mpArgTable[i].mpName, base, unresolve);
            if (unresolve) { Relocate(r->mpArgTable, base, true); r->mnConstantPoolCount = static_cast<int32_t>(0x98765432u); r->mppConstantPool = reinterpret_cast<AptValue**>(0x12345678u); }
            break;
        }
        case 0x88: case 0x96:
        {
            AptParsedValueList* r = reinterpret_cast<AptParsedValueList*>(AlignActionOperand(pc));
            pc = reinterpret_cast<unsigned char*>(r + 1);
            if (!unresolve) Relocate(r->mpValues, base, false);
            for (int32_t i = 0; i < r->mnCount; ++i)
            {
                if (unresolve)
                {
                    AptValue* v = r->mpValues[i].mpValue;
                    if (v->isString())
                    {
                        AptString* str = v->getVtblIndex() == AptVFT_StringValue ? static_cast<AptString*>(v) : static_cast<AptString*>(static_cast<AptStringObject*>(v)->GetBoxedString());
                        StringPool::RemoveFromPool(str);
                    }
                    else v->Release();
                    r->mpValues[i].mnIndex = (*valueCount)++;
                }
                else
                {
                    AptParsedConstant* constants = reinterpret_cast<AptParsedConstant*>(context->mnSecondaryOffset);
                    int32_t index = r->mpValues[i].mnIndex;
                    ++*valueCount;
                    AptValue* v = ResolveConstant(constants[index], context);
                    r->mpValues[i].mpValue = v;
                    if (!v->isString()) v->AddRef();
                    if ((i % 16) == 0) gpValuesToRelease->ReleaseValues();
                }
            }
            if (unresolve) Relocate(r->mpValues, base, true);
            break;
        }
        default: break;
        }
        if (!unresolve) gpValuesToRelease->ReleaseValues();
    }
}
