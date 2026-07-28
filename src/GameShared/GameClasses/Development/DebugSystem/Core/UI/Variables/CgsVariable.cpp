#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"

#include <string.h>  // _stricmp - SetValueFromString matches option names case-insensitively

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// CgsDev::DebugUI::Variable - a registered debug variable: the value Variant, its display name,
// and the intrusive VariableMetadata attribute chain (min / max / step / read-only / save-enabled /
// visible / option list / change+select callbacks).
//
// Prepare is the register path (matching the X360 VariableManager::RegisterVariable direct fill
// 0x82829A80: copy the Variant, set the name, null the metadata list). AddMetadata / RemoveMetadata
// maintain the chain; FindMetadata walks it and every predicate/accessor below is expressed through
// it (the X360 inlines FindMetadata into all of them). Increment / Decrement / Select / OnChange are
// the menu-edit surface MenuItemVariable drives.

namespace CgsDev
{
    namespace DebugUI
    {
        bool Variable::Prepare(const Variant& lrVariant, const char* lpcName)
        {
            mVariant.Copy(lrVariant);
            mpcName    = lpcName;
            mpMetadata = nullptr;
            return true;
        }

        Variant&    Variable::GetValue()       { return mVariant; }
        const char* Variable::GetName() const  { return mpcName; }

        // Linear scan of the attribute chain for the first node of leType (DecFIGS CgsVariable.cpp:610,
        // local `lpMetadata` at :613). The X360 always inlines it; the walk is attested twice inside
        // ScriptInterface::SaveState -- 0x828328C4-0x828328E4 (leType == E_TYPE_READONLY) and
        // 0x8283290C-0x82832928 (leType == E_TYPE_SAVEENABLED). Both load meType at +4, compare against
        // the literal 3 / 4, and follow mpNextMetadata at +8 until null. Those are X360 offsets, so this
        // walks by named member instead.
        VariableMetadata* Variable::FindMetadata(VariableMetadata::Type leType) const
        {
            VariableMetadata* lpMetadata = mpMetadata;
            while (lpMetadata != nullptr)
            {
                if (lpMetadata->meType == leType)
                    return lpMetadata;

                lpMetadata = lpMetadata->mpNextMetadata;
            }

            return nullptr;
        }

        // Read-only flag (DecFIGS CgsVariable.cpp:262, local `lpReadOnly` at :264). Inlined in the X360;
        // attested at 0x828328B8-0x82832904 in ScriptInterface::SaveState: walk for E_TYPE_READONLY, and
        // on a hit `lbz r11,0(r11)` reads the attribute's stored bool out of mValue. When the attribute
        // is absent the result falls through to r31, which `li r31,0` @0x82832808 pins to FALSE.
        bool Variable::IsReadOnly()
        {
            VariableMetadata* lpReadOnly = FindMetadata(VariableMetadata::E_TYPE_READONLY);
            if (lpReadOnly != nullptr)
                return lpReadOnly->mValue.mbBool;

            return false;
        }

        // Save-enabled flag (DecFIGS CgsVariable.cpp:284, local `lpSaveEnabled` at :286). Same inlined
        // shape at 0x82832908-0x82832938, but the ABSENT default is the opposite one: the no-hit exit
        // 0x8283292C is `li r11,1`, i.e. a variable with no E_TYPE_SAVEENABLED attribute IS saved. On a
        // hit, 0x82832964 `lbz r11,0(r11)` returns the stored bool. The asymmetry with IsReadOnly above
        // is what the binary does, not a slip.
        bool Variable::IsSaveEnabled()
        {
            VariableMetadata* lpSaveEnabled = FindMetadata(VariableMetadata::E_TYPE_SAVEENABLED);
            if (lpSaveEnabled != nullptr)
                return lpSaveEnabled->mValue.mbBool;

            return true;
        }

        // Visible flag (DecFIGS CgsVariable.cpp:307, local `lpVisible` at :309). Inlined; the whole
        // body is attested standalone by MenuItemVariable::IsVisible @0x8281A0C0, which is nothing but
        // `mpVariable->IsVisible()` with this walk folded in: load the variable's metadata head, scan
        // for meType == 5 (E_TYPE_VISIBLE) following mpNextMetadata, `lbz r3,0(r11)` the stored bool on
        // a hit (0x8281A0F0), and `li r3,1` on both no-hit exits (0x8281A0E8) -- so an unattributed
        // variable IS visible, the same absent-default polarity as IsSaveEnabled.
        bool Variable::IsVisible()
        {
            VariableMetadata* lpVisible = FindMetadata(VariableMetadata::E_TYPE_VISIBLE);
            if (lpVisible != nullptr)
                return lpVisible->mValue.mbBool;

            return true;
        }

        // The option table for an enum-style variable (DecFIGS CgsVariable.cpp:332, local
        // `lpStringList` at :334). Inlined into both string paths below -- 0x82822FD8-0x8282301C
        // (GetValueAsString) and 0x82823110-0x82823158 (SetValueFromString) -- which each walk for
        // meType == 6 (E_TYPE_STRINGLIST), read the table pointer out of the attribute's mValue, and
        // treat "attribute absent" and "attribute present but null table" identically.
        const StringList* Variable::GetStringList()
        {
            const VariableMetadata* lpStringList = FindMetadata(VariableMetadata::E_TYPE_STRINGLIST);
            if (lpStringList != nullptr)
                return lpStringList->mValue.mpInt32StringList;

            return nullptr;
        }

        // X360 0x82823208 (DecFIGS CgsVariable.cpp:548). Push the attribute onto the front of the
        // chain. The two asserts are the X360's inlined ASSERT sequences (Enter(gAssertMutex) /
        // FireAssert(expr, file, line) / Leave) at CgsVariable.cpp:551 and :552; the second one's
        // condition is the FindMetadata walk at 0x82823264-0x8282327C, which compares each existing
        // node's meType against the incoming node's. Both assert paths fall through to the same link
        // (0x828232B8 and 0x82823280 are identical), so a duplicate is reported and still linked.
        void Variable::AddMetadata(VariableMetadata* lpMetadata)
        {
            CGS_ASSERT(lpMetadata, "lpMetadata");
            CGS_ASSERT(FindMetadata(lpMetadata->meType) == nullptr, "same metadata added twice");

            lpMetadata->mpNextMetadata = mpMetadata;
            mpMetadata                 = lpMetadata;
        }

        // X360 0x828232D0 (DecFIGS CgsVariable.cpp:568, local `lpPrevMetadata` at :583). Unlink the
        // attribute and null its link field. The head case (0x82823328) and the walk case (0x82823368)
        // both clear lpMetadata->mpNextMetadata after re-pointing the predecessor; a node that is not
        // in this chain at all falls off the end (0x82823360) and is left alone. The assert is the
        // inlined sequence at CgsVariable.cpp:571.
        void Variable::RemoveMetadata(VariableMetadata* lpMetadata)
        {
            CGS_ASSERT(lpMetadata, "lpMetadata");

            if (lpMetadata == mpMetadata)
            {
                mpMetadata                 = mpMetadata->mpNextMetadata;
                lpMetadata->mpNextMetadata = nullptr;
                return;
            }

            for (VariableMetadata* lpPrevMetadata = mpMetadata;
                 lpPrevMetadata != nullptr;
                 lpPrevMetadata = lpPrevMetadata->mpNextMetadata)
            {
                if (lpPrevMetadata->mpNextMetadata == lpMetadata)
                {
                    lpPrevMetadata->mpNextMetadata = lpMetadata->mpNextMetadata;
                    lpMetadata->mpNextMetadata     = nullptr;
                    return;
                }
            }
        }

        // X360 0x82817D10 (DecFIGS CgsVariable.cpp:357, local `lpStep` at :366). Read-only variables
        // ignore the input entirely (0x82817D50-0x82817D58 is the inlined IsReadOnly early-out). The
        // step attribute supplies the delta; with no E_TYPE_STEP attribute the float path adds
        // flt_82001C98 (== 1.0f, 0x82817DE0) and the integer path uses `addi r10,r10,1`.
        //
        // The X360 folds E_TYPE_PTR_INT32 and E_TYPE_PTR_UINT32 into one jump-table target
        // (0x82817DF8, "cases 1,2") because `add` is sign- and width-agnostic; OnChange below keeps
        // them apart (it needs cmpw vs cmplw), so the source had two cases and the compiler merged
        // the identical code. They are written out separately here.
        //
        // E_TYPE_PTR_BOOL does not step at all -- 0x82817E2C `lbz` / `cntlzw` / `extrwi` stores
        // (value == 0), i.e. it TOGGLES. Decrement does exactly the same thing.
        void Variable::Increment()
        {
            if (IsReadOnly())
                return;

            VariableMetadata* lpStep = FindMetadata(VariableMetadata::E_TYPE_STEP);

            switch (mVariant.meType)
            {
            case Variant::E_TYPE_PTR_FLOAT:
                if (lpStep != nullptr)
                    *mVariant.mValue.mpfFloat += lpStep->mValue.mfFloat;
                else
                    *mVariant.mValue.mpfFloat += 1.0f;
                break;

            case Variant::E_TYPE_PTR_INT32:
                if (lpStep != nullptr)
                    *mVariant.mValue.mpiInt32 += lpStep->mValue.miInt32;
                else
                    ++*mVariant.mValue.mpiInt32;
                break;

            case Variant::E_TYPE_PTR_UINT32:
                if (lpStep != nullptr)
                    *mVariant.mValue.mpuUInt32 += lpStep->mValue.muUInt32;
                else
                    ++*mVariant.mValue.mpuUInt32;
                break;

            case Variant::E_TYPE_PTR_BOOL:
                *mVariant.mValue.mpbBool = !*mVariant.mValue.mpbBool;
                break;

            default:
                break;
            }

            OnChange();
        }

        // X360 0x82817E48 (DecFIGS CgsVariable.cpp:437, local `lpStep` at :446). Increment's mirror:
        // same read-only early-out (0x82817E88), same step lookup, `fsubs` / `subf` / `addi r10,r10,-1`
        // instead, and the same flt_82001C98 (1.0f) default at 0x82817F18. The bool case at 0x82817F64
        // is byte-for-byte Increment's -- a bool variable toggles whichever way the menu is nudged.
        // Every arm falls into the shared OnChange tail-call (0x82817F78).
        void Variable::Decrement()
        {
            if (IsReadOnly())
                return;

            VariableMetadata* lpStep = FindMetadata(VariableMetadata::E_TYPE_STEP);

            switch (mVariant.meType)
            {
            case Variant::E_TYPE_PTR_FLOAT:
                if (lpStep != nullptr)
                    *mVariant.mValue.mpfFloat -= lpStep->mValue.mfFloat;
                else
                    *mVariant.mValue.mpfFloat -= 1.0f;
                break;

            case Variant::E_TYPE_PTR_INT32:
                if (lpStep != nullptr)
                    *mVariant.mValue.mpiInt32 -= lpStep->mValue.miInt32;
                else
                    --*mVariant.mValue.mpiInt32;
                break;

            case Variant::E_TYPE_PTR_UINT32:
                if (lpStep != nullptr)
                    *mVariant.mValue.mpuUInt32 -= lpStep->mValue.muUInt32;
                else
                    --*mVariant.mValue.mpuUInt32;
                break;

            case Variant::E_TYPE_PTR_BOOL:
                *mVariant.mValue.mpbBool = !*mVariant.mValue.mpbBool;
                break;

            default:
                break;
            }

            OnChange();
        }

        // X360 0x82817F80 (DecFIGS CgsVariable.cpp:518, locals `lpSelectCallback` /
        // `lpSelectCallbackParam` at :521-:522, `lpParameter` at :528). Fire the select callback with
        // the variable's value pointer and the callback's own parameter attribute. Both the attribute
        // and the stored function pointer are checked (0x82817FD0 and 0x82817FDC each `blr` on null);
        // a missing E_TYPE_SELECT_CALLBACK_PARAM passes null (`li r4,0` @0x82817FE4). The X360 makes
        // it a tail-call through ctr, so Select does nothing else afterwards.
        void Variable::Select()
        {
            VariableMetadata* lpSelectCallback      = FindMetadata(VariableMetadata::E_TYPE_SELECT_CALLBACK);
            VariableMetadata* lpSelectCallbackParam = FindMetadata(VariableMetadata::E_TYPE_SELECT_CALLBACK_PARAM);

            if (lpSelectCallback != nullptr && lpSelectCallback->mValue.mpVariableCallbackFunction != nullptr)
            {
                void* lpParameter = nullptr;
                if (lpSelectCallbackParam != nullptr)
                    lpParameter = lpSelectCallbackParam->mValue.mpValue;

                lpSelectCallback->mValue.mpVariableCallbackFunction(mVariant.mValue.mpValue, lpParameter);
            }
        }

        // X360 0x82816600 (DecFIGS CgsVariable.cpp:708, locals `lpMin` / `lpMax` /
        // `lpChangeCallback` / `lpChangeCallbackParam` at :711-:714, `lpParameter` at :775).
        // Post-edit fixup: clamp the pointed-at value to the min/max attributes, then fire the change
        // callback. All four attribute lookups happen up front (0x82816600-0x8281669C) whatever the
        // variant type is.
        //
        // Only the BY-POINTER variants clamp (0x828166A0 dispatches on meType 5/6/7 and everything
        // else skips straight to the callback) -- there is nothing to write through for an immediate.
        // The three arms differ only in the comparison: `fcmpu` for float (0x82816760), signed `cmpw`
        // for int32 (0x8281671C), unsigned `cmplw` for uint32 (0x828166D0). The callback tail-call at
        // 0x828167B0 passes the variable's value pointer and the param attribute's value (null when
        // the attribute is absent), exactly like Select above.
        void Variable::OnChange()
        {
            VariableMetadata* lpMin                 = FindMetadata(VariableMetadata::E_TYPE_MIN);
            VariableMetadata* lpMax                 = FindMetadata(VariableMetadata::E_TYPE_MAX);
            VariableMetadata* lpChangeCallback      = FindMetadata(VariableMetadata::E_TYPE_CHANGE_CALLBACK);
            VariableMetadata* lpChangeCallbackParam = FindMetadata(VariableMetadata::E_TYPE_CHANGE_CALLBACK_PARAM);

            switch (mVariant.meType)
            {
            case Variant::E_TYPE_PTR_FLOAT:
                if (lpMin != nullptr && *mVariant.mValue.mpfFloat < lpMin->mValue.mfFloat)
                    *mVariant.mValue.mpfFloat = lpMin->mValue.mfFloat;

                if (lpMax != nullptr && *mVariant.mValue.mpfFloat > lpMax->mValue.mfFloat)
                    *mVariant.mValue.mpfFloat = lpMax->mValue.mfFloat;
                break;

            case Variant::E_TYPE_PTR_INT32:
                if (lpMin != nullptr && *mVariant.mValue.mpiInt32 < lpMin->mValue.miInt32)
                    *mVariant.mValue.mpiInt32 = lpMin->mValue.miInt32;

                if (lpMax != nullptr && *mVariant.mValue.mpiInt32 > lpMax->mValue.miInt32)
                    *mVariant.mValue.mpiInt32 = lpMax->mValue.miInt32;
                break;

            case Variant::E_TYPE_PTR_UINT32:
                if (lpMin != nullptr && *mVariant.mValue.mpuUInt32 < lpMin->mValue.muUInt32)
                    *mVariant.mValue.mpuUInt32 = lpMin->mValue.muUInt32;

                if (lpMax != nullptr && *mVariant.mValue.mpuUInt32 > lpMax->mValue.muUInt32)
                    *mVariant.mValue.mpuUInt32 = lpMax->mValue.muUInt32;
                break;

            default:
                break;
            }

            if (lpChangeCallback != nullptr && lpChangeCallback->mValue.mpVariableCallbackFunction != nullptr)
            {
                void* lpParameter = nullptr;
                if (lpChangeCallbackParam != nullptr)
                    lpParameter = lpChangeCallbackParam->mValue.mpValue;

                lpChangeCallback->mValue.mpVariableCallbackFunction(mVariant.mValue.mpValue, lpParameter);
            }
        }

        // X360 0x828230B0 (DecFIGS CgsVariable.cpp:209, local `lpStringList` at :218). Read-only
        // variables ignore the string (0x82823108 is the inlined IsReadOnly early-out). An option
        // table turns the string into one of its int values by case-insensitive name (`_stricmp`
        // @0x828231A8), walking entries until the terminating null-name row; otherwise the Variant's
        // own parser handles it.
        //
        // Two details the asm is explicit about: the option store is split by variant type -- meType
        // 6 writes THROUGH the pointer (0x828231DC) while meType 2 (E_TYPE_INT32) overwrites the
        // immediate (0x828231F8), and any other type stores nothing -- and the option path does NOT
        // call OnChange (0x828231E8 / 0x82823200 return directly). Only the ConvertFromString
        // fallback runs it (0x82823144). The assert at CgsVariable.cpp:221 fires before the walk when
        // an option table is attached to something that is not an int32 pointer.
        void Variable::SetValueFromString(const char* lpcValue)
        {
            if (IsReadOnly())
                return;

            const StringList* lpStringList = GetStringList();
            if (lpStringList != nullptr)
            {
                CGS_ASSERT(mVariant.meType == Variant::E_TYPE_PTR_INT32,
                           "mVariant.meType == Variant::E_TYPE_PTR_INT32");

                while (lpStringList->mpcName != nullptr && _stricmp(lpStringList->mpcName, lpcValue) != 0)
                    ++lpStringList;

                if (lpStringList->mpcName != nullptr)
                {
                    if (mVariant.meType == Variant::E_TYPE_PTR_INT32)
                        *mVariant.mValue.mpiInt32 = lpStringList->miValue;
                    else if (mVariant.meType == Variant::E_TYPE_INT32)
                        mVariant.mValue.miInt32 = lpStringList->miValue;

                    return;
                }
            }

            mVariant.ConvertFromString(lpcValue);
            OnChange();
        }

        // X360 0x82822FC0 (DecFIGS CgsVariable.cpp:169, local `lpStringList` at :172). NOT
        // RECONSTRUCTED -- see the report note. The real body is SetValueFromString's mirror: look up
        // the option table, assert meType == E_TYPE_PTR_INT32 (CgsVariable.cpp:175), scan the table
        // for the entry whose miValue equals the pointed-at int (0x8282305C-0x82823088), and on a hit
        // copy that entry's name into the caller's buffer with DebugUI::SafeStringCopy (0x828230A0);
        // every other exit formats the value with Variant::ConvertToString (0x82823008).
        //
        // The blocker is the call at 0x828230A0: it passes r3 = the destination buffer, r4 = the
        // source string, r5 = the length -- there is no `this`, so DebugUI::SafeStringCopy is a STATIC
        // member (SafeStringCat is the same, see DebugComponent::MakeFullPath 0x82822350). CgsDebugUI.h
        // currently declares both as ordinary non-static members, and Variable derives from nothing
        // (DWARF CgsVariable.h:94), so it has no DebugUI instance to call one through and cannot reach
        // the function at all. Adding `static` to those two declarations unblocks this body; that
        // header is outside this agent's file list, so the value path below is left as it was found.
        void Variable::GetValueAsString(char* lpcValue, int liLength)
        {
            if (liLength > 0)
                lpcValue[0] = '\0';
        }
    }
}
