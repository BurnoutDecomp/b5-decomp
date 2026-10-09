// Real debug Variable/Variant metadata, callbacks and menu edits with the PC INI sidecar.
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <limits>
#include "pc/DebugIniPCLeaf.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"

static int giChecks = 0, giFailures = 0;
static void Check(bool lbValue, const char* lpcName)
{
    ++giChecks;
    if (!lbValue) { ++giFailures; std::printf("FAIL %s\n", lpcName); }
}
#undef CGS_ASSERT
#define CGS_ASSERT(value, message) Check(!!(value), message)
namespace CgsDev { namespace Log { void WriteToLog(const char* p) { std::printf("%s", p); } } }
#include "pc_debug_ini.inc"

using namespace CgsDev::DebugUI;
static char gacIni[MAX_PATH];
static void Set(const char* lpcPath, const char* lpcValue)
{ WritePrivateProfileStringA("Debug", lpcPath, lpcValue, gacIni); }
static void Metadata(Variable& lrVariable, VariableMetadata& lrMetadata,
                     const Variant& lrValue, VariableMetadata::Type leType)
{ lrMetadata.Prepare(lrValue, leType); lrVariable.AddMetadata(&lrMetadata); }

struct CallbackState
{
    int miCalls = 0;
    bool mbReady = false;
    VariableManager* mpManager = nullptr;
    Variable* mpForget = nullptr;
};
static void Changed(void* lpValue, void* lpData)
{
    auto& lrState = *static_cast<CallbackState*>(lpData);
    Check(lrState.mbReady && lpValue != nullptr, "callback sees completed registration and real value pointer");
    ++lrState.miCalls;
    if (lrState.mpForget) ForgetDebugIniPC(lrState.mpManager, lrState.mpForget);
    if (lrState.mpManager) lrState.mpManager->ApplyIniOverridesPC(); // nested application is a no-op
}

static void RegistryChecks()
{
    Set("/TEST\\Gain", "8.5");
    Set("Test/Count", "-2147483648");
    Set("Test/Mask", "4294967295");
    Set("Test/Enabled", "1");
    Set("Lazy Component/Controls/Late", "17");
    Set("Test/Read Only", "9");
    Set("Test/Option", "uLtRa");
    Set("Test/Unknown Future", "1");
    LoadDebugIniPC(gacIni);
    Check(DebugIniNeedsComponentPC("/Lazy Component"), "lazy component with configured children is detected");
    Check(!DebugIniNeedsComponentPC("Lazy Component Else"), "component prefix must end at a path separator");
    Check(!DebugIniNeedsComponentPC("Unconfigured"), "unconfigured components remain lazy");

    VariableManager lManager, lOtherManager;
    f32 lfGain = 1.0f;
    s32 liCount = 0, liReadOnly = 2, liOption = 0, liLate = 0;
    u32 luMask = 0;
    bool lbEnabled = false;
    Variable lGain, lCount, lMask, lEnabled, lReadOnly, lOption, lLate;
    lGain.Prepare(Variant(&lfGain), "Gain");
    lCount.Prepare(Variant(&liCount), "Count");
    lMask.Prepare(Variant(&luMask), "Mask");
    lEnabled.Prepare(Variant(&lbEnabled), "Enabled");
    lReadOnly.Prepare(Variant(&liReadOnly), "Read Only");
    lOption.Prepare(Variant(&liOption), "Option");
    QueueDebugIniPC(&lManager, &lGain, "test", "gain");
    QueueDebugIniPC(&lManager, &lCount, "Test", "Count");
    QueueDebugIniPC(&lManager, &lMask, "Test", "Mask");
    QueueDebugIniPC(&lOtherManager, &lEnabled, "Test", "Enabled");
    QueueDebugIniPC(&lManager, &lReadOnly, "Test", "Read Only");
    QueueDebugIniPC(&lManager, &lOption, "Test", "Option");
    Check(lfGain == 1.0f && liCount == 0 && liOption == 0,
          "registration queues values without applying before metadata");

    VariableMetadata lMin, lMax, lStep, lCallback, lParameter, lReadOnlyMetadata, lOptions, lOptionCallback, lOptionParameter;
    Metadata(lGain, lMin, Variant(0.0f), VariableMetadata::E_TYPE_MIN);
    Metadata(lGain, lMax, Variant(5.0f), VariableMetadata::E_TYPE_MAX);
    Metadata(lGain, lStep, Variant(0.5f), VariableMetadata::E_TYPE_STEP);
    CallbackState lState; lState.mpManager = &lManager;
    Metadata(lGain, lCallback, Variant(&Changed), VariableMetadata::E_TYPE_CHANGE_CALLBACK);
    Variant lUserData; lUserData.SetVoidPointerType(&lState);
    Metadata(lGain, lParameter, lUserData, VariableMetadata::E_TYPE_CHANGE_CALLBACK_PARAM);
    Metadata(lReadOnly, lReadOnlyMetadata, Variant(true), VariableMetadata::E_TYPE_READONLY);
    static const StringList kaOptions[] = { {10,"High"}, {20,"Ultra"}, {0,nullptr} };
    Metadata(lOption, lOptions, Variant(kaOptions), VariableMetadata::E_TYPE_STRINGLIST);
    CallbackState lOptionState; lOptionState.mbReady = true;
    Metadata(lOption, lOptionCallback, Variant(&Changed), VariableMetadata::E_TYPE_CHANGE_CALLBACK);
    Variant lOptionData; lOptionData.SetVoidPointerType(&lOptionState);
    Metadata(lOption, lOptionParameter, lOptionData, VariableMetadata::E_TYPE_CHANGE_CALLBACK_PARAM);
    lState.mbReady = true;
    lManager.ApplyIniOverridesPC();
    Check(lfGain == 5.0f && lState.miCalls == 1, "initial override uses original range clamp and callback once");
    Check(liCount == (std::numeric_limits<s32>::min)(), "full signed 32-bit minimum is preserved");
    Check(luMask == (std::numeric_limits<u32>::max)(), "full unsigned 32-bit maximum is preserved without atoi overflow");
    Check(!lbEnabled, "pending application is isolated by registry owner");
    lOtherManager.ApplyIniOverridesPC();
    Check(lbEnabled, "boolean 1 sets the actual engine boolean");
    Check(liReadOnly == 2, "read-only metadata attached after registration is honored");
    Check(liOption == 20, "option names use the completed engine option table");
    Check(lOptionState.miCalls == 1, "named options fire the real change callback once");

    lGain.SetValueFromString("2.0");
    lGain.Increment();
    Check(lfGain == 2.5f && lState.miCalls == 3, "normal menu increment uses the same value, step and callback");
    lManager.ApplyIniOverridesPC();
    Check(lfGain == 2.5f && lState.miCalls == 3, "later flushes do not overwrite menu or console edits");

    lLate.Prepare(Variant(&liLate), "Late");
    QueueDebugIniPC(&lManager, &lLate, "Lazy Component/Controls", "Late");
    lManager.ApplyIniOverridesPC();
    Check(liLate == 17 && lfGain == 2.5f, "late registration receives its initial value without resetting earlier edits");

    // A callback can unregister a queued variable before its turn. No saved batch of raw pointers.
    lfGain = 1.0f; liLate = 0; lState.mpForget = &lLate;
    QueueDebugIniPC(&lManager, &lGain, "Test", "Gain");
    QueueDebugIniPC(&lManager, &lLate, "Lazy Component/Controls", "Late");
    lManager.ApplyIniOverridesPC();
    Check(lfGain == 5.0f && liLate == 0 && PendingDebugIniCountPC(&lManager) == 0,
          "callback removal cancels another pending value safely");
    lState.mpForget = nullptr;
    QueueDebugIniPC(&lManager, &lLate, "Lazy Component/Controls", "Late");
    ForgetDebugIniPC(&lManager, &lLate);
    lLate.Release();
    lLate.Prepare(Variant(&liLate), "Unconfigured Reuse");
    lManager.ApplyIniOverridesPC();
    Check(liLate == 0, "pool-address reuse cannot apply a stale registration");

    QueueDebugIniPC(&lManager, &lGain, "Test", "Gain");
    lManager.Destruct();
    Check(PendingDebugIniCountPC(&lManager) == 0, "registry teardown drops pending pointers");
    ReportUnmatchedDebugIniPC();
}

static void ParsingChecks()
{
    f32 lfValue = 1.0f; s32 liValue = 7; u32 luValue = 7; bool lbValue = false;
    Variable lFloat, lInt, lUnsigned, lBool, lOpaque, lNull;
    lFloat.Prepare(Variant(&lfValue), "f"); lInt.Prepare(Variant(&liValue), "i");
    lUnsigned.Prepare(Variant(&luValue), "u"); lBool.Prepare(Variant(&lbValue), "b");
    Variant lVoid; lVoid.SetVoidPointerType(&liValue); lOpaque.Prepare(lVoid,"internal");
    lNull.Prepare(Variant(static_cast<f32*>(nullptr)), "unavailable");
    Variant lResult;
    for (const char* p : {"NaN","inf","-inf","1e99","1e-99","4oops",""," "})
        Check(!PrepareDebugIniValuePC(lFloat,p,lResult), "malformed or non-finite float is rejected before touching engine state");
    for (const char* p : {"2147483648","-2147483649","999999999999999999999","1.5","","abc"})
        Check(!PrepareDebugIniValuePC(lInt,p,lResult), "signed overflow and partial integers are rejected");
    for (const char* p : {"4294967296","-1","999999999999999999999","1.5","abc"})
        Check(!PrepareDebugIniValuePC(lUnsigned,p,lResult), "unsigned overflow and negative values are rejected");
    for (const char* p : {"2","yes","truthy","","false trailing"})
        Check(!PrepareDebugIniValuePC(lBool,p,lResult), "unknown boolean text is rejected");
    Check(PrepareDebugIniValuePC(lBool," FALSE ",lResult) && !lResult.mValue.mbBool, "case-insensitive false is typed correctly");
    Check(PrepareDebugIniValuePC(lBool,"TrUe",lResult) && lResult.mValue.mbBool, "case-insensitive true is typed correctly");
    Check(PrepareDebugIniValuePC(lFloat,"1.25e1",lResult) && lResult.mValue.mfFloat == 12.5f, "finite scientific notation is accepted");
    Check(!PrepareDebugIniValuePC(lOpaque,"99",lResult), "opaque pointers and function-like controls cannot be set as scalar values");
    Check(!PrepareDebugIniValuePC(lNull,"1",lResult), "a registered unavailable pointer is rejected without dereferencing it");
    Check(lfValue==1 && liValue==7 && luValue==7 && !lbValue, "validation never writes an engine variable");
    static const StringList kaOptions[]={{3,"Third"},{7,"Seventh"},{0,nullptr}};
    VariableMetadata lOptions; Metadata(lInt,lOptions,Variant(kaOptions),VariableMetadata::E_TYPE_STRINGLIST);
    Check(PrepareDebugIniValuePC(lInt,"7",lResult) && lResult.mValue.miInt32==7,"numeric enum entries must exist in the option list");
    Check(!PrepareDebugIniValuePC(lInt,"8",lResult),"unknown numeric enum entry is rejected");
    Check(!PrepareDebugIniValuePC(lInt,"Eighth",lResult),"unknown named enum entry is rejected");
}

int main()
{
    char lacTemp[MAX_PATH]; GetTempPathA(MAX_PATH,lacTemp); GetTempFileNameA(lacTemp,"bdi",0,gacIni);
    RegistryChecks(); ParsingChecks(); DeleteFileA(gacIni);
    std::printf("PCDebugIni: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures ? 1 : 0;
}
