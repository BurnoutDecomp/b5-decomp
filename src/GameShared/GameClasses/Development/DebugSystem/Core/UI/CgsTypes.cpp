#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"

// CgsDev::DebugUI::Variant method bodies. The tagged-union value type every debug variable flows
// through. The type tags are verified against the X360 (DebugComponent builds Variant(bool*) as tag
// 8 == E_TYPE_PTR_BOOL, Variant(s32) as tag 2 == E_TYPE_INT32, ...; GetDereferenceType 0x... maps
// the by-pointer tags 5/6/7/8 back to the immediate tags 1/2/3/4). The constructors are the simple
// tag+value setters the rest of the debug UI relies on.
//
// Deferred (their own follow-on, needing the formatter + deref plumbing): ConvertToString /
// ConvertFromString / Dereference - declared in the header, reconstructed when the menu render +
// save/load paths are built.

namespace CgsDev
{
    namespace DebugUI
    {
        // X360 Metrics::DEFAULT - the built-in debug-UI screen metrics, carved verbatim from the X360
        // (unk_820DC138, the source DebugUI::Construct memcpy's into the UI). The render layout reads
        // mfScreenWidth/Height + the borders from here.
        const Metrics Metrics::DEFAULT =
        {
            16.0f,    // mfTextSize
            1.0f,     // mfWindowBorderSize
            400.0f,   // mfWindowMoveSpeed
            1280.0f,  // mfScreenWidth
            720.0f,   // mfScreenHeight
            64.0f,    // mfScreenBorderLeft
            64.0f,    // mfScreenBorderTop
            64.0f,    // mfScreenBorderRight
            64.0f,    // mfScreenBorderBottom
            20.0f,    // mfCascadeStep
            0.7f,     // mfBarHeightScale
            1.5f,     // mfBarHeightScaleBig
            8.0f,     // mfSubItemIndent
            2.0f,     // mfMenuGap
            4.0f,     // mfErrorWindowBorder
            0.3f,     // mfAutoRepeatDelay
            0.05f,    // mfAutoRepeatRate
            0.001f,   // mfAutoRepeatAcceleration
        };

        Variant::Variant()
        {
            Clear();
        }

        Variant::Variant(const Variant& lrOther)
        {
            Copy(lrOther);
        }

        Variant::Variant(f32 lfValue)   { meType = E_TYPE_FLOAT;   mValue.mfFloat   = lfValue; }
        Variant::Variant(s32 liValue)   { meType = E_TYPE_INT32;   mValue.miInt32   = liValue; }
        Variant::Variant(u32 luValue)   { meType = E_TYPE_UINT32;  mValue.muUInt32  = luValue; }
        Variant::Variant(bool lbValue)  { meType = E_TYPE_BOOL;    mValue.mbBool    = lbValue; }

        Variant::Variant(f32* lpfValue) { meType = E_TYPE_PTR_FLOAT;  mValue.mpfFloat  = lpfValue; }
        Variant::Variant(s32* lpiValue) { meType = E_TYPE_PTR_INT32;  mValue.mpiInt32  = lpiValue; }
        Variant::Variant(u32* lpuValue) { meType = E_TYPE_PTR_UINT32; mValue.mpuUInt32 = lpuValue; }
        Variant::Variant(bool* lpbValue){ meType = E_TYPE_PTR_BOOL;   mValue.mpbBool   = lpbValue; }

        Variant::Variant(const StringList* lpStringList)
        {
            meType = E_TYPE_UI_STRINGLISTINT32;
            mValue.mpInt32StringList = lpStringList;
        }

        Variant::Variant(UValue::VariableCallbackFunction lpfCallback)
        {
            meType = E_TYPE_UI_VARIABLECALLBACK;
            mValue.mpVariableCallbackFunction = lpfCallback;
        }

        void Variant::Copy(const Variant& lrOther)
        {
            meType = lrOther.meType;
            mValue = lrOther.mValue;
        }

        void Variant::Clear()
        {
            meType = E_TYPE_NONE;
            mValue.mpValue = nullptr;
        }

        void Variant::SetVoidPointerType(void* lpValue)
        {
            meType = E_TYPE_PTR_VOID;
            mValue.mpValue = lpValue;
        }

        // X360: maps the by-pointer tags to the immediate type they dereference to (PTR_FLOAT->FLOAT,
        // PTR_INT32->INT32, PTR_UINT32->UINT32, PTR_BOOL->BOOL); everything else -> NONE.
        Variant::Type Variant::GetDereferenceType() const
        {
            switch (meType)
            {
            case E_TYPE_PTR_FLOAT:  return E_TYPE_FLOAT;
            case E_TYPE_PTR_INT32:  return E_TYPE_INT32;
            case E_TYPE_PTR_UINT32: return E_TYPE_UINT32;
            case E_TYPE_PTR_BOOL:   return E_TYPE_BOOL;
            default:                return E_TYPE_NONE;
            }
        }
    }
}
