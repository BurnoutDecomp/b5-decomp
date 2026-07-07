#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT, Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (dynamic assert message)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent::GetName
#include "SDKs/EATech/include/Apt/AptString/EAString.h"       // EAStringC (AptNativeString) temporary
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"      // AptBoolean::Create

// CgsGui::ObjectController - the apt display-object control bridge.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsAptObjectController.cpp):
//   ~ObjectController        @ 0x8284A5F0
//   Register                 @ 0x8284A610
//   UnRegister               @ 0x8284A728
//   GetName                  @ 0x8284A838
//   AttachController         @ 0x8284A8E0
//   GetObjectValue           @ 0x8284A9F0
//   SetObjectVariableBoolean @ 0x8284ADD8
//
// Notes on the X360 idioms de-optimized here:
//   - The Begin/Fire/EndAssert triplet + the streamed/plain assert string folds to
//     CGS_ASSERT(cond,"msg"); the one assert that streams the component name is kept
//     as an explicit StrStream build (the same dynamic-message form the X360 emits).
//   - The X360 builds an EAStringC from the key on the stack, calls the static
//     AptExtObject::Get/SetVariable, then drops the temporary's internal refcount.
//     That whole sequence is the codegen of a plain `EAStringC lKey(lpacKey)` local;
//     it is reconstructed as such (the ctor InitFromBuffer / dtor DecreaseInternalRef
//     are the inlined temporary lifetime).
//   - The "component is on the stage" assert reads the bound AptValue's packed type/
//     flag word (mnValueData) and tests its display-object bit. The bit is accessed
//     through the named union member, not a raw offset poke.

namespace CgsGui
{
    // CgsAptObjectController.h:199 - apt extension hash-table sizing (the iNumMembers
    // an ObjectController reserves when it constructs its AptExtObject base).
    const s32 ObjectController::KI_NUM_HASHTABLE_ENTRIES = 1;

    // The bound apt reference's packed type/flag word bit the X360 tests to confirm
    // the component is a live on-stage display object before driving it. The X360
    // reads `mnValueData >> 27 & 1`; expressed here as a named mask on the AptValue
    // type/flag word so no raw offset is used.
    static const u32 KU_APT_ONSTAGE_FLAG_MASK = 1u << 27;

    // X360 0x8284A5F0. Clear the controller state, then the AptExtObject base
    // destructor runs (emitted by the compiler as the base-dtor tail call).
    ObjectController::~ObjectController()
    {
        mbRegistered         = false;
        mpControlledObject   = 0;
        mpComponentReference = 0;
    }

    // X360 0x8284A838. Return the controlled component's name.
    const char* ObjectController::GetName()
    {
        CGS_ASSERT(mpControlledObject != 0, "Invalid component in GetName");
        return mpControlledObject->GetName();
    }

    // X360 0x8284A610. Register this object as an apt extension.
    void ObjectController::Register()
    {
        CGS_ASSERT(mpControlledObject != 0,
                   "Cannot register ourselves without knowing which GuiComponent we're attached to");
        CGS_ASSERT(!mbRegistered, "This object is already registered. Please UnRegister() first");

        AptRegisterExtension(this);
        mbRegistered = true;
    }

    // X360 0x8284A728. Tear down the apt-extension registration.
    void ObjectController::UnRegister()
    {
        CGS_ASSERT(mpControlledObject != 0, "Not target object");
        CGS_ASSERT(mbRegistered, "This object is not registered.");

        mbRegistered = false;
        AptUnRegisterExtension(this);
    }

    // X360 0x8284A8E0. Bind the apt-side component reference.
    void ObjectController::AttachController(AptValue* lpComponentReference)
    {
        CGS_ASSERT(lpComponentReference != 0, "Invalid reference");
        CGS_ASSERT(lpComponentReference != 0 &&
                       (lpComponentReference->mnValueData & KU_APT_ONSTAGE_FLAG_MASK) != 0,
                   "Invalid AptValue");

        mpComponentReference = lpComponentReference;
    }

    // X360 0x8284A9F0. Look the named ActionScript variable up on the bound reference.
    AptValue* ObjectController::GetObjectValue(const char* lpacKey)
    {
        CGS_ASSERT(lpacKey != 0, "Invalid key");

        if (mpComponentReference == 0)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "This controller has not been set up yet (Key : "
                       << (lpacKey ? lpacKey : "<NULLSTRING>") << ")";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        if ((mpComponentReference->mnValueData & KU_APT_ONSTAGE_FLAG_MASK) == 0)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            if (mpControlledObject != 0)
            {
                lStrStream << "Component is not on the stage so can't be controlled : "
                           << mpControlledObject->GetName() << "\n";
            }
            else
            {
                lStrStream << "Component is not on the stage so can't be controlled : Name Unavailable\n";
            }
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        EAStringC lKey(lpacKey);
        return GetVariable(mpComponentReference, &lKey);
    }

    // X360 0x824E23E0. Read the named ActionScript float variable from the bound
    // reference: assert the key, look up its AptValue (GetObjectValue), assert the
    // lookup succeeded (streamed message naming the key), then coerce it to a float.
    f32 ObjectController::GetObjectVariableFloat(const char* lpacVariable)
    {
        CGS_ASSERT(lpacVariable != 0, "Invalid key");

        AptValue* lpObjectValue = GetObjectValue(lpacVariable);
        if (lpObjectValue == 0)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid value with key "
                       << (lpacVariable ? lpacVariable : "<NULLSTRING>");
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        return lpObjectValue->toFloat();
    }

    // X360 0x8284ADD8. Set the named ActionScript boolean variable on the reference.
    void ObjectController::SetObjectVariableBoolean(const char* lpacVariable, bool lbValue)
    {
        CGS_ASSERT(lpacVariable != 0, "Invalid variable information");
        CGS_ASSERT(mpComponentReference != 0, "This controller has not been set up yet");
        CGS_ASSERT(mpComponentReference != 0 &&
                       (mpComponentReference->mnValueData & KU_APT_ONSTAGE_FLAG_MASK) != 0,
                   "Invalid AptValue -- trying to set a reference that isn't defined");

        EAStringC lVariable(lpacVariable);
        AptBoolean* lpBooleanValue = AptBoolean::Create(lbValue);
        SetVariable(mpComponentReference, &lVariable, lpBooleanValue);
    }
}
