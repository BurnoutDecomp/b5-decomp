#pragma once
namespace CgsPC::Debug
{
    // The early INI registrations and the original component share these globals.
    // Keep one set of rows and one metadata record for each original variable.
    inline bool BeginVehicleLodRegistration()
    {
        static bool sbRegistered = false;
        if (sbRegistered) return false;
        sbRegistered = true;
        return true;
    }
}
