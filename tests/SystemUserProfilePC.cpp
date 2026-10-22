// Real PC profile API + complete SystemUserProfile TU contract.
// Win32 file operations touch only config.ini beside this fixture executable.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/PC/CgsXUserProfileSettings.h"
#include "GameShared/GameClasses/System/CgsXOverlapped.h"
extern "C" u32 XUserReadProfileSettingsByXuid(u32, u32, u32, const u64*, u32, const u32*, u32*, void*, void*);
#define private public
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"
#undef private

namespace
{
    unsigned guChecks = 0, guFailures = 0, guAssertions = 0;
    const char* gpcAssert = nullptr;
    struct AssertObserved {};
    std::string gsConfig;
    constexpr u32 KU_CAMERA = XPROFILE_GAMER_RACE_CAMERA_LOCATION;
    constexpr u32 KU_RUMBLE = XPROFILE_OPTION_CONTROLLER_VIBRATION;
    constexpr u32 KU_REQUIRED = sizeof(XUSER_READ_PROFILE_SETTING_RESULT) + 2 * sizeof(XUSER_PROFILE_SETTING);
    const u32 KAU_IDS[2] = { KU_CAMERA, KU_RUMBLE };
    struct Buffer128
    {
        XUSER_READ_PROFILE_SETTING_RESULT result;
        XUSER_PROFILE_SETTING settings[2];
        u8 tail[128 - sizeof(XUSER_READ_PROFILE_SETTING_RESULT) - 2 * sizeof(XUSER_PROFILE_SETTING)];
    };
    struct Guarded
    {
        u64 before[4];
        Buffer128 buffer;
        u64 after[4];
    };
    void Check(const char* lpcId, bool lbPass)
    {
        ++guChecks;
        std::printf("CHECK %s %s\n", lpcId, lbPass ? "PASS" : "FAIL");
        if (!lbPass) ++guFailures;
    }
    int Finish()
    {
        std::printf("SYSTEM_USER_PROFILE checks=%u failures=%u assertions=%u\n", guChecks, guFailures, guAssertions);
        return guFailures ? 1 : 0;
    }
    bool Bytes(const void* lpData, size_t luCount, u8 luValue = 0xA5)
    {
        const auto* lpBytes = static_cast<const u8*>(lpData);
        for (size_t lu = 0; lu < luCount; ++lu) if (lpBytes[lu] != luValue) return false;
        return true;
    }
    void Fill(Guarded& lr) { std::memset(&lr, 0xA5, sizeof(lr)); }
    u32 Read(Guarded& lr, u32& rcb, const u32* lpIds = KAU_IDS, u32 luCount = 2, u32 luUser = 0, u32 luTitle = 0)
    {
        return XUserReadProfileSettings(luTitle, luUser, luCount, lpIds, &rcb, &lr.buffer.result, nullptr);
    }
    void Ini(const char* lpcText)
    {
        std::ofstream lFile(gsConfig, std::ios::binary | std::ios::trunc);
        lFile << lpcText;
        if (!lFile) throw std::runtime_error("owned config write failed");
        lFile.close();
        // Successful cache flushing returns zero (WritePrivateProfileStringA).
        // Keep this exact owned-path flush; subsequent actual API reads verify
        // the updated preferences instead of applying the normal write-BOOL rule.
        (void)WritePrivateProfileStringA(nullptr, nullptr, nullptr, gsConfig.c_str());
    }
    struct Listener final : CgsGui::SystemUserProfile::Listener
    {
        std::string order, name;
        CgsGui::SystemUserProfile::ProfileSettings settings{};
        unsigned settingCalls = 0;
        void UserNameChanged(const char* lpName) override { order += 'N'; name = lpName; }
        void ProfileSettingsChanged(const CgsGui::SystemUserProfile::ProfileSettings& lr) override
        { order += 'P'; settings = lr; ++settingCalls; }
        void SigninStateChanged(CgsGui::SystemUserProfile::EUserSigninState le) override
        { order += le == CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN ? 'S' : 'O'; }
        void UIClosed() override { order += 'U'; }
    };
    // The only replaced boundary is assert reporting, which throws immediately
    // so the retained legacy consumer never dereferences its unwritten buffer.
    // Profile API, provider, watcher, Win32 IO and listener virtual dispatch are real.
}
namespace CgsDev { namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char* lpExpression, const char*, int)
    { ++guAssertions; gpcAssert = lpExpression; throw AssertObserved{}; }
    void* EndAssert() { return nullptr; }
} }

int main(int argc, char** argv)
{
    try
    {
        char laExe[MAX_PATH];
        const DWORD luLen = GetModuleFileNameA(nullptr, laExe, MAX_PATH);
        if (!luLen || luLen >= MAX_PATH) throw std::runtime_error("fixture executable path unavailable");
        std::string lsExe(laExe, luLen);
        const auto luSlash = lsExe.find_last_of("/\\");
        if (luSlash == std::string::npos) throw std::runtime_error("fixture directory unavailable");
        const std::string lsDirectory = lsExe.substr(0, luSlash + 1);
        gsConfig = lsDirectory + "config.ini";
        if (GetFileAttributesA(gsConfig.c_str()) != INVALID_FILE_ATTRIBUTES)
            throw std::runtime_error("fixture refuses existing sibling config.ini");
        // The real identity provider reads std::getenv's CRT environment table.
        // Win32 SetEnvironmentVariableA does not update that separate table.
        if (_putenv_s("BP_LAN", "1") != 0 ||
            _putenv_s("BP_LAN_NAME", "ProfileFixture") != 0)
            throw std::runtime_error("fixture persona environment setup failed");
        const bool lbConsumerControl = argc == 2 && std::strcmp(argv[1], "--baseline-consumer") == 0;
        if (lbConsumerControl)
        {
            CgsGui::SystemUserProfile lUser{};
            lUser.Construct(); lUser.Prepare();
            Listener lListener;
            lUser.AttachListener(lListener); lUser.SetUserIndex(0);
            try { lUser.UpdateUserSigninState(); }
            catch (const AssertObserved&)
            { std::printf("LEGACY_ASSERT %s\n", gpcAssert ? gpcAssert : "(null)"); }
            Check("legacy_consumer_selected_user_settings_callback", lListener.settingCalls == 1);
            Check("legacy_consumer_preserves_signin_name_before_settings", lListener.order == "SN");
            Check("legacy_consumer_actual_error87_assert", guAssertions == 1 && gpcAssert &&
                std::strstr(gpcAssert, "problem reading user settings") != nullptr);
            lUser.DetachListener(lListener); lUser.Release();
            return Finish();
        }

        Check("native_pointer_width", sizeof(void*) == 8);
        Check("native_result_extent", sizeof(XUSER_READ_PROFILE_SETTING_RESULT) == 16);
        Check("native_result_pointer_offset", offsetof(XUSER_READ_PROFILE_SETTING_RESULT, pSettings) == 8);
        Check("native_complete_data_extent", sizeof(XUSER_DATA) == 24);
        Check("native_record_stride", sizeof(XUSER_PROFILE_SETTING) == 48);
        Check("native_data_integer_offset", offsetof(XUSER_DATA, nData) == 8);
        Check("native_required_two_records", KU_REQUIRED == 112);
        Check("original_capacity_preserved", sizeof(Buffer128) == 128);
        Guarded lRead; Fill(lRead);
        u32 lcb = 128;
        const u32 luFirst = Read(lRead, lcb);
        Check("default_actual_api_success", luFirst == ERROR_SUCCESS);
        // Exact baseline87 is the intended failure control, never read as records.
        if (luFirst != ERROR_SUCCESS) return Finish();
        const auto& r = lRead.buffer.result;
        Check("default_required_extent", lcb == KU_REQUIRED);
        Check("default_result_count", r.dwSettingsLen == 2);
        Check("default_native_pointer_targets_owned_records", r.pSettings == lRead.buffer.settings);
        Check("default_native_pointer_not_guest32", reinterpret_cast<size_t>(r.pSettings) > 0xFFFFFFFFull);
        Check("default_requested_order", r.pSettings[0].dwSettingId == KU_CAMERA && r.pSettings[1].dwSettingId == KU_RUMBLE);
        Check("default_integer_types", r.pSettings[0].data.type == 1 && r.pSettings[1].data.type == 1);
        Check("default_policy_values", r.pSettings[0].data.nData == 0 && r.pSettings[1].data.nData == 3);
        Check("default_provenance", r.pSettings[0].source == XSOURCE_DEFAULT && r.pSettings[1].source == XSOURCE_DEFAULT);
        Check("default_local_user", r.pSettings[0].user.dwUserIndex == 0 && r.pSettings[1].user.dwUserIndex == 0);
        Check("default_outer_canaries", Bytes(lRead.before, sizeof(lRead.before)) && Bytes(lRead.after, sizeof(lRead.after)));
        Check("default_unused_original_capacity_untouched", Bytes(lRead.buffer.tail, sizeof(lRead.buffer.tail)));

        lcb = 0;
        Check("null_query_reports_insufficient", XUserReadProfileSettings(0, 0, 2, KAU_IDS, &lcb, nullptr, nullptr) == ERROR_INSUFFICIENT_BUFFER);
        Check("null_query_reports_exact_extent", lcb == KU_REQUIRED);
        Fill(lRead); lcb = KU_REQUIRED - 1;
        Check("short_buffer_rejected", Read(lRead, lcb) == ERROR_INSUFFICIENT_BUFFER);
        Check("short_buffer_reports_exact_extent", lcb == KU_REQUIRED);
        Check("short_buffer_entire_destination_untouched", Bytes(&lRead, sizeof(lRead)));
        Fill(lRead); lcb = KU_REQUIRED;
        Check("exact_extent_succeeds", Read(lRead, lcb) == ERROR_SUCCESS);
        Check("exact_extent_bounded", Bytes(lRead.before, sizeof(lRead.before)) && Bytes(lRead.after, sizeof(lRead.after)) && Bytes(lRead.buffer.tail, sizeof(lRead.buffer.tail)));
        const u32 laReverse[2] = { KU_RUMBLE, KU_CAMERA };
        Fill(lRead); lcb = 128;
        Check("reversed_request_succeeds", Read(lRead, lcb, laReverse) == ERROR_SUCCESS);
        Check("reversed_request_values_and_ids", lRead.buffer.settings[0].dwSettingId == KU_RUMBLE && lRead.buffer.settings[0].data.nData == 3 && lRead.buffer.settings[1].dwSettingId == KU_CAMERA && lRead.buffer.settings[1].data.nData == 0);
        Fill(lRead); lcb = 128;
        Check("one_setting_succeeds", Read(lRead, lcb, KAU_IDS, 1) == ERROR_SUCCESS);
        Check("one_setting_exact_extent", lcb == sizeof(XUSER_READ_PROFILE_SETTING_RESULT) + sizeof(XUSER_PROFILE_SETTING) && lRead.buffer.result.dwSettingsLen == 1);
        Check("one_setting_unused_record_untouched", Bytes(&lRead.buffer.settings[1], sizeof(lRead.buffer.settings[1])));
        const u32 laDuplicate[2] = { KU_RUMBLE, KU_RUMBLE };
        Fill(lRead); lcb = 128;
        Check("duplicates_preserved", Read(lRead, lcb, laDuplicate) == ERROR_SUCCESS && lRead.buffer.settings[0].dwSettingId == KU_RUMBLE && lRead.buffer.settings[1].dwSettingId == KU_RUMBLE);

        std::array<u32, 32> laMaximumIds; laMaximumIds.fill(KU_RUMBLE);
        const u32 luMaximumBytes = sizeof(XUSER_READ_PROFILE_SETTING_RESULT) + 32 * sizeof(XUSER_PROFILE_SETTING);
        std::vector<u64> laMaximum((luMaximumBytes + 2 * sizeof(u64)) / sizeof(u64), 0xA5A5A5A5A5A5A5A5ull);
        auto* lpMaximum = reinterpret_cast<XUSER_READ_PROFILE_SETTING_RESULT*>(laMaximum.data() + 1);
        lcb = luMaximumBytes;
        const u32 luMaximumError = XUserReadProfileSettings(0, 0, 32, laMaximumIds.data(), &lcb, lpMaximum, nullptr);
        Check("maximum_count_actual_api_success", luMaximumError == ERROR_SUCCESS && lcb == luMaximumBytes);
        bool lbMaximumRecords = luMaximumError == ERROR_SUCCESS && lpMaximum->dwSettingsLen == 32 &&
            lpMaximum->pSettings == reinterpret_cast<XUSER_PROFILE_SETTING*>(lpMaximum + 1);
        if (lbMaximumRecords)
            for (u32 lu = 0; lu < 32; ++lu)
                lbMaximumRecords = lbMaximumRecords && lpMaximum->pSettings[lu].dwSettingId == KU_RUMBLE &&
                    lpMaximum->pSettings[lu].data.type == 1 && lpMaximum->pSettings[lu].data.nData == 3;
        Check("maximum_count_all_records_populated", lbMaximumRecords);
        Check("maximum_count_outer_canaries", laMaximum.front() == 0xA5A5A5A5A5A5A5A5ull && laMaximum.back() == 0xA5A5A5A5A5A5A5A5ull);

        std::array<bool, 3> laLocalPorts{};
        for (u32 luPort = 1; luPort < 4; ++luPort)
        {
            Fill(lRead); lcb = 128;
            const u32 luPortError = Read(lRead, lcb, KAU_IDS, 2, luPort);
            laLocalPorts[luPort - 1] = luPortError == ERROR_SUCCESS &&
                lRead.buffer.settings[0].user.dwUserIndex == luPort &&
                lRead.buffer.settings[1].user.dwUserIndex == luPort &&
                lRead.buffer.settings[0].data.nData == 0 && lRead.buffer.settings[1].data.nData == 3 &&
                Bytes(lRead.before, sizeof(lRead.before)) && Bytes(lRead.after, sizeof(lRead.after));
        }
        Check("local_port1_shared_policy", laLocalPorts[0]);
        Check("local_port2_shared_policy", laLocalPorts[1]);
        Check("local_port3_shared_policy", laLocalPorts[2]);

        Fill(lRead); lcb = 128;
        Check("null_id_array_rejected", Read(lRead, lcb, nullptr) == ERROR_INVALID_PARAMETER);
        Check("zero_count_rejected", Read(lRead, lcb, KAU_IDS, 0) == ERROR_INVALID_PARAMETER);
        Check("too_many_settings_rejected_before_array_read", Read(lRead, lcb, KAU_IDS, 33) == ERROR_INVALID_PARAMETER);
        Check("foreign_title_rejected", Read(lRead, lcb, KAU_IDS, 2, 0, 1) == ERROR_INVALID_PARAMETER);
        Check("invalid_user_rejected", Read(lRead, lcb, KAU_IDS, 2, 4) == ERROR_INVALID_PARAMETER);
        Check("invalid_maximum_user_rejected", Read(lRead, lcb, KAU_IDS, 2, 0xFFFFFFFFu) == ERROR_INVALID_PARAMETER);
        const u32 laUnknown[2] = { KU_CAMERA, 0x10040002u };
        Check("unsupported_setting_rejected", Read(lRead, lcb, laUnknown) == ERROR_INVALID_PARAMETER);
        Check("null_size_pointer_rejected", XUserReadProfileSettings(0, 0, 2, KAU_IDS, nullptr, &lRead.buffer.result, nullptr) == ERROR_INVALID_PARAMETER);
        Check("nonzero_size_null_destination_rejected", XUserReadProfileSettings(0, 0, 2, KAU_IDS, &lcb, nullptr, nullptr) == ERROR_INVALID_PARAMETER);
        Check("unsupported_async_rejected", XUserReadProfileSettings(0, 0, 2, KAU_IDS, &lcb, &lRead.buffer.result, reinterpret_cast<XOVERLAPPED*>(1)) == ERROR_NOT_SUPPORTED);
        Check("invalid_requests_no_destination_write", Bytes(&lRead, sizeof(lRead)));
        Check("invalid_requests_no_size_change", lcb == 128);
        alignas(XUSER_PROFILE_SETTING) u8 laMisaligned[136]; std::memset(laMisaligned, 0xA5, sizeof(laMisaligned));
        lcb = 128;
        Check("misalignment_rejected", XUserReadProfileSettings(0, 0, 2, KAU_IDS, &lcb, reinterpret_cast<XUSER_READ_PROFILE_SETTING_RESULT*>(laMisaligned + 1), nullptr) == ERROR_INVALID_PARAMETER);
        Check("misalignment_no_write", Bytes(laMisaligned, sizeof(laMisaligned)));
        Fill(lRead); lcb = 128;
        XOVERLAPPED lRemote{};
        const u64 luForeignXuid = 0x123456789ABCDEF0ull;
        Check("remote_profile_request_started_only_for_failure", XUserReadProfileSettingsByXuid(
            0, 0, 1, &luForeignXuid, 2, KAU_IDS, &lcb, &lRead.buffer.result, &lRemote) == ERROR_IO_PENDING);
        Check("remote_profile_completion_remains_unsupported", XGetOverlappedResult(&lRemote, nullptr, 0) == ERROR_FUNCTION_FAILED);
        Check("remote_profile_no_fabricated_result", Bytes(&lRead, sizeof(lRead)));


        // Deliberately conflict with the owned executable-adjacent INI from another CWD.
        Ini("[SystemProfile]\nRaceCameraLocation=2\nControllerVibration=0\n");
        const std::string lsOther = lsDirectory + "other_cwd";
        if (!CreateDirectoryA(lsOther.c_str(), nullptr)) throw std::runtime_error("owned cwd create failed");
        { std::ofstream lOther(lsOther + "\\config.ini"); lOther << "[SystemProfile]\nRaceCameraLocation=1\nControllerVibration=3\n"; }
        if (!SetCurrentDirectoryA(lsOther.c_str())) throw std::runtime_error("owned cwd select failed");
        Fill(lRead); lcb = 128;
        Check("configured_actual_api_success", Read(lRead, lcb) == ERROR_SUCCESS);
        Check("configured_values_ignore_working_directory", lRead.buffer.settings[0].data.nData == 2 && lRead.buffer.settings[1].data.nData == 0);
        Check("configured_provenance", lRead.buffer.settings[0].source == XSOURCE_TITLE && lRead.buffer.settings[1].source == XSOURCE_TITLE);
        Ini("[SystemProfile]\nRaceCameraLocation=1\nControllerVibration=2\n");
        Fill(lRead); lcb = 128;
        Check("second_configured_pair", Read(lRead, lcb) == ERROR_SUCCESS && lRead.buffer.settings[0].data.nData == 1 && lRead.buffer.settings[1].data.nData == 2);
        Ini("[SystemProfile]\nRaceCameraLocation=0\nControllerVibration=1\n");
        Fill(lRead); lcb = 128;
        Check("third_configured_pair", Read(lRead, lcb) == ERROR_SUCCESS && lRead.buffer.settings[0].data.nData == 0 && lRead.buffer.settings[1].data.nData == 1);
        Ini("[SystemProfile]\nRaceCameraLocation=\nControllerVibration=\n");
        Fill(lRead); lcb = 128;
        Check("empty_keys_default_policy", Read(lRead, lcb) == ERROR_SUCCESS && lRead.buffer.settings[0].data.nData == 0 && lRead.buffer.settings[1].data.nData == 3 && lRead.buffer.settings[0].source == XSOURCE_DEFAULT);
        const char* lapInvalid[] = {
            "[SystemProfile]\nRaceCameraLocation=3\nControllerVibration=0\n",
            "[SystemProfile]\nRaceCameraLocation=-1\nControllerVibration=0\n",
            "[SystemProfile]\nRaceCameraLocation=x\nControllerVibration=0\n",
            "[SystemProfile]\nRaceCameraLocation=00\nControllerVibration=0\n",
            "[SystemProfile]\nRaceCameraLocation=2\nControllerVibration=4\n",
            "[SystemProfile]\nRaceCameraLocation=2\nControllerVibration=123456789\n",
        };
        bool lbErrors = true, lbNoWrites = true;
        for (const char* lpInvalid : lapInvalid)
        {
            Ini(lpInvalid); Fill(lRead); lcb = 128;
            const u32 luInvalidResult = Read(lRead, lcb);
            lbErrors = lbErrors && luInvalidResult == ERROR_INVALID_DATA;
            lbNoWrites = lbNoWrites && Bytes(&lRead, sizeof(lRead)) && lcb == 128;
        }
        Check("invalid_configurations_rejected", lbErrors);
        Check("invalid_configurations_publish_no_partial_records", lbNoWrites);

        Ini("[SystemProfile]\nRaceCameraLocation=2\nControllerVibration=0\n");
        CgsGui::SystemUserProfile lUser{};
        lUser.Construct();
        Check("watcher_real_prepare", lUser.Prepare());
        Listener lListener;
        lUser.AttachListener(lListener);
        Check("watcher_no_user_no_callback", lListener.order.empty());
        lUser.SetUserIndex(0); lUser.UpdateUserSigninState();
        Check("watcher_original_callback_order", lListener.order == "SNP");
        Check("watcher_real_user_name", lListener.name == "ProfileFixture");
        Check("watcher_typed_camera2_false", !lListener.settings.mbExternalCamera);
        Check("watcher_typed_rumble0", lListener.settings.miRumble == 0);
        Check("watcher_one_settings_callback", lListener.settingCalls == 1);
        lUser.UpdateUserSigninState();
        Check("watcher_unchanged_user_no_repeated_callbacks", lListener.order == "SNP");
        Ini("[SystemProfile]\nRaceCameraLocation=0\nControllerVibration=3\n");
        lUser.UpdateProfileSettings();
        Check("watcher_camera0_true_and_rumble3", lListener.settings.mbExternalCamera && lListener.settings.miRumble == 3);
        Check("watcher_refresh_settings_only", lListener.order == "SNPP");
        Ini("[SystemProfile]\nRaceCameraLocation=1\nControllerVibration=1\n");
        lUser.UpdateProfileSettings();
        Check("watcher_camera1_false_raw_rumble1", !lListener.settings.mbExternalCamera && lListener.settings.miRumble == 1);
        const unsigned luBeforePortRefresh = lListener.settingCalls;
        bool lbPortRefreshValues = true;
        for (u32 luPort = 1; luPort < 4; ++luPort)
        {
            lUser.SetUserIndex(luPort); lUser.UpdateProfileSettings();
            lbPortRefreshValues = lbPortRefreshValues && !lListener.settings.mbExternalCamera && lListener.settings.miRumble == 1;
        }
        Check("watcher_all_valid_local_ports_read_settings", lListener.settingCalls == luBeforePortRefresh + 3);
        Check("watcher_all_valid_local_ports_same_pc_preferences", lbPortRefreshValues);
        lUser.DetachListener(lListener);
        const unsigned luCalls = lListener.settingCalls;
        lUser.UpdateProfileSettings();
        Check("watcher_detached_listener_not_called", lListener.settingCalls == luCalls);
        Listener lLate;
        lUser.AttachListener(lLate);
        Check("watcher_live_attach_original_callback_order", lLate.order == "SNP");
        Check("watcher_live_attach_actual_preferences", !lLate.settings.mbExternalCamera && lLate.settings.miRumble == 1);
        lUser.SetUserIndex(4);
        lUser.UpdateProfileSettings();
        Check("watcher_no_user_does_not_read_settings", lLate.order == "SNP");
        lUser.DetachListener(lLate); lUser.Release();
        Check("valid_paths_no_asserts", guAssertions == 0);
        return Finish();
    }
    catch (const AssertObserved&) { Check("unexpected_production_assert", false); std::printf("ASSERT %s\n", gpcAssert ? gpcAssert : "(null)"); }
    catch (const std::exception& e) { Check("fixture_environment_or_boundary", false); std::printf("ERROR %s\n", e.what()); }
    return Finish();
}
