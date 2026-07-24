#ifndef BRN_WORLD_SKY_DEBUG_COMPONENT_H
#define BRN_WORLD_SKY_DEBUG_COMPONENT_H

// =============================================================================
// BrnWorld::EnvironmentSettings::DebugComponent -- the "Sky" debug component
// (GameSource/World/DebugComponents/BrnSkyDebugComponent.{h,cpp} DWARF home,
// BrnSkyDebugComponent.h:55).
//
// The environment-settings tuning page: it mirrors the EnvironmentManager's
// sky / scattering / fill-light / cloud / junkyard-light state into debug-
// editable scalars (ManagerToDebug) and back (DebugToManager). The WorldModule
// owns one by value (mSkyDebugComponent, WorldModule::Prepare Construct+
// Registers it against the environment manager).
//
// Declaration shape: the DecFIGS DWARF (member list + method set). X360
// addresses: Construct @0x827C7668 (NOT in the function export set -- the
// address is attested by WorldModule::Prepare's call @0x827D5880; body lands
// with this TU's reconstruction), Destruct @?, Update @0x827C7760, RenderHUD
// @0x827C79A0, GetName @0x827B23E8, OnActivate @0x827B2408, DebugToManager
// @0x827B3B28, ManagerToDebug @0x827BFC80. All bodies are the (todo)
// BrnSkyDebugComponent.cpp TU's work -- none is bodied here.
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent base

namespace CgsDev { class Debug2DImmediateRender; }

namespace BrnWorld
{
namespace EnvironmentSettings
{
    class EnvironmentManager;

    // BrnSkyDebugComponent.h:55.
    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // BrnSkyDebugComponent.cpp:48 -- @0x827C7668 (see the file banner).
        void Construct( EnvironmentManager* lpEnvironmentManager );

        // BrnSkyDebugComponent.cpp:92.
        void Destruct();

        // BrnSkyDebugComponent.cpp:105 -- @0x827C7760.
        virtual void Update();

        // BrnSkyDebugComponent.cpp:176 -- @0x827C79A0.
        virtual void RenderHUD( CgsDev::Debug2DImmediateRender* lpRender );

    protected:
        // BrnSkyDebugComponent.cpp:253 -- @0x827B23E8.
        virtual const char* GetName() const;

        // BrnSkyDebugComponent.cpp:265.
        virtual const char* GetPath() const;

        // BrnSkyDebugComponent.cpp:277 -- @0x827B2408.
        virtual void OnActivate();

    private:
        // BrnSkyDebugComponent.cpp:658 -- @0x827B3B28.
        void DebugToManager();

        // BrnSkyDebugComponent.cpp:739 -- @0x827BFC80.
        void ManagerToDebug();

        // ---- members (DWARF order, BrnSkyDebugComponent.h:90..160) ---------
        EnvironmentManager* mpEnvironmentManager;               // :90

        bool  mbPrintDebugInfo;                                 // :95
        bool  mbSimulateTimeOfDay;                              // :97
        u32   muTimeOfDay_HH;                                   // :98
        u32   muTimeOfDay_MM;                                   // :98
        u32   muTimeOfDay_SS;                                   // :98
        f32   mfTimeOfDayDelta;                                 // :99
        f32   mfCloudDelta;                                     // :100

        f32   mfSkyTopColour_r, mfSkyTopColour_g, mfSkyTopColour_b;    // :102
        f32   mfSkyHorColour_r, mfSkyHorColour_g, mfSkyHorColour_b;    // :103
        f32   mfSkySunColour_r, mfSkySunColour_g, mfSkySunColour_b;    // :104
        f32   mfSkyHorPow;                                      // :105
        f32   mfSkySunPow;                                      // :106
        f32   mfSkyDrk;                                         // :107
        f32   mfSkyHorBleedScl;                                 // :108
        f32   mfSkyHorBleedPow;                                 // :109
        f32   mfSkySunBleedPow;                                 // :110

        f32   mfScattTopColour_r, mfScattTopColour_g, mfScattTopColour_b; // :112
        f32   mfScattHorColour_r, mfScattHorColour_g, mfScattHorColour_b; // :113
        f32   mfScattSunColour_r, mfScattSunColour_g, mfScattSunColour_b; // :114
        f32   mfScattHorPow;                                    // :115
        f32   mfScattSunPow;                                    // :116
        f32   mfScattDrk;                                       // :117
        f32   mfScattHorBleedScl;                               // :118
        f32   mfScattHorBleedPow;                               // :119
        f32   mfScattSunBleedPow;                               // :120
        f32   mafScattDist[2];                                  // :121
        f32   mfScattPow;                                       // :122
        f32   mfScattCap;                                       // :123

        f32   mfKeyFillColour_r, mfKeyFillColour_g, mfKeyFillColour_b;             // :125
        f32   mfShadowFillColour_r, mfShadowFillColour_g, mfShadowFillColour_b;    // :126
        f32   mfRightFillColour_r, mfRightFillColour_g, mfRightFillColour_b;       // :127
        f32   mfLeftFillColour_r, mfLeftFillColour_g, mfLeftFillColour_b;          // :128
        f32   mfUpFillColour_r, mfUpFillColour_g, mfUpFillColour_b;                // :129
        f32   mfDownFillColour_r, mfDownFillColour_g, mfDownFillColour_b;          // :130

        f32   mfKeyLightColour_r, mfKeyLightColour_g, mfKeyLightColour_b;          // :132
        f32   mfSpecularColour_r, mfSpecularColour_g, mfSpecularColour_b;          // :133
        f32   mfAmbientIrradianceScale;                         // :134

        f32   mafCloudLayerDensity[2];                          // :136
        f32   mafCloudLayerFeathering[2];                       // :137
        f32   mafCloudLayerOpacity[2];                          // :138
        f32   mfCloudLayerDarkColourR;                          // :139
        f32   mfCloudLayerDarkColourG;                          // :140
        f32   mfCloudLayerDarkColourB;                          // :141
        f32   mfCloudLayerLiteColourR;                          // :142
        f32   mfCloudLayerLiteColourG;                          // :143
        f32   mfCloudLayerLiteColourB;                          // :144

        f32   mfSunTiltAtHorizon;                               // :146
        f32   mfSunTiltAtMidday;                                // :147

        f32   mafJunkyardKeyLightDirection[3];                  // :149
        bool  mbCalculateJunkyardKeyLightDirectionFromTime;     // :150
        bool  mbOverrideJunkyardKeyLightDirection;              // :151
        bool  mbGetJunkyardKeyLightDirection;                   // :152

        u32   muSunElevTodLBoundHH;                             // :154
        u32   muSunElevTodLBoundMM;                             // :155
        u32   muSunElevTodLBoundSS;                             // :156
        u32   muSunElevTodUBoundHH;                             // :158
        u32   muSunElevTodUBoundMM;                             // :159
        u32   muSunElevTodUBoundSS;                             // :160
    };

} // namespace EnvironmentSettings
} // namespace BrnWorld

#endif // BRN_WORLD_SKY_DEBUG_COMPONENT_H
