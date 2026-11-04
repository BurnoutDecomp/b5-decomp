#include "GameSource/Resource/BrnGameDataModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"
#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceTypePS3.h"
#include "GameShared/GameClasses/RenderWare/CgsRwTextureStateResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsMaterialStateResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsRwColourCubeResourceType.h"
#include "GameShared/GameClasses/Fonts/Resources/CgsFontResourceType.h"
#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsModelResourceType.h"
#include "GameShared/GameClasses/Graphics/Instances/CgsInstanceListResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListResourceType.h"
#include "GameSource/Gui/PFX/Resource/BrnGuiPFXHooksResource.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchyResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistryResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacWaveContentResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacReverbIRContentResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/Plugins/Ginsu/CgsGinsuWaveContentResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsBankResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/CSIS/CgsCsisResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerResourceType.h"
#include "GameShared/GameClasses/Sound/Logic/CgsNicotineResourceType.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSnapshotDataResourceType.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSchemaResourceType.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultResourceType.h"
#include "GameShared/GameClasses/World/Resources/CgsWorldPainter2DResourceType.h"
#include "GameShared/GameClasses/SceneManager/Zones/Resources/ZoneListResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"
#include "SharedClasses/Graphics/VFXPropsResourceType.h"
#include "SharedClasses/Graphics/ParticleDescriptionResourceType.h"
#include "SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResourceType.h"
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"
#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsRwRenderableResourceType.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialResourceType.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.h"
#include "GameShared/GameClasses/Graphics/Resources/CgsShaderTechniqueResourceType.h"
#include "GameShared/GameClasses/RenderWare/x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsListResourceType.h"
#include "SharedClasses/Physics/Props/BrnPropGraphicsListResourceType.h"
#include "SharedClasses/Physics/Props/BrnPropInstanceDataResourceType.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMapResourceType.h"
#include "SharedClasses/Gui/Flapt/BrnFlaptFileResourceType.h"
#include "SharedClasses/Trigger/BrnTriggerResourceType.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.h"
#include "SharedClasses/Sound/Engines/BrnSoundLoopModelResourceType.h"
#include "SharedClasses/DataLists/VehicleListResourceType.h"
#include "SharedClasses/DataLists/WheelListResourceType.h"
#include "SharedClasses/DataLists/ChallengeListResourceType.h"
#include "SharedClasses/World/BrnVehicleGraphicsSpecResourceType.h"
#include "SharedClasses/World/BrnWheelGraphicsSpecResourceType.h"
#include "SharedClasses/World/BrnEnvironmentKeyframeResourceType.h"
#include "SharedClasses/World/BrnEnvironmentTimeLineResourceType.h"
#include "SharedClasses/World/BrnEnvironmentDictionaryResourceType.h"
#include "SharedClasses/Massive/Resources/MassiveLookupTableResourceType.h"
#include "SharedClasses/Graphics/PlayerCarColoursResourceType.h"
#include "SharedClasses/Progression/BrnProgressionResourceType.h"
#include "SharedClasses/Physics/Deformation/Resources/StreamedDeformationSpecResourceType.h"
#include "SharedClasses/StreetData/BrnStreetDataResourceType.h"
#include "GameShared/GameClasses/Containers/CgsDictionaryResourceType.h"
#include "SDKs/Packages/ICE/ICEData.hpp"
#include "GameShared/GameClasses/System/Resource/CgsTextFileResource.h"
#include "GameShared/GameClasses/RenderWare/CgsRwMaterialCRC32ResourceType.h"
#include "GameShared/GameClasses/RenderWare/PS3/materialstates/CgsRwShaderParameterResourceTypePS3.h"
#include "GameShared/GameClasses/RenderWare/CgsRwDebugResourceType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageListType.h"
#include "GameShared/GameClasses/Physics/CgsInstanceCollisionListResourceType.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsClusteredMeshResourceType.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsKdTreeResourceType.h"
#include "SharedClasses/Gui/SatNav/Resources/BrnSatNavTileResourceType.h"
#include "SharedClasses/Progression/BrnProfileUpgradeResourceType.h"

namespace BrnResource
{
    // ARTIST 82667EA8: preserve allocation, cache initialization and insertion order.
    void GameDataModule::RegisterResourceTypes(rw::IResourceAllocator* lpAllocator)
    {
        miNumTypes = 0;
        {
            CgsResource::TextFileResourceType* lpType = new (lpAllocator) CgsResource::TextFileResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "TextFileResourceType";
        }
        {
            CgsResource::RwRasterResourceType* lpType = new (lpAllocator) CgsResource::RwRasterResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwRasterResourceType";
        }
        {
            CgsResource::MaterialResourceType* lpType = new (lpAllocator) CgsResource::MaterialResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "MaterialResourceType";
        }
        {
            CgsResource::RwVertexDescResourceType* lpType = new (lpAllocator) CgsResource::RwVertexDescResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwVertexDescResourceType";
        }
        {
            CgsResource::RwMaterialCRC32ResourceType* lpType = new (lpAllocator) CgsResource::RwMaterialCRC32ResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwMaterialCRC32ResourceType";
        }
        {
            CgsResource::RwTextureStateResourceType* lpType = new (lpAllocator) CgsResource::RwTextureStateResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwTextureStateResourceType";
        }
        {
            CgsResource::MaterialStateResourceType* lpType = new (lpAllocator) CgsResource::MaterialStateResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "MaterialStateResourceType";
        }
        {
            CgsResource::RwShaderProgramBufferResourceType* lpType = new (lpAllocator) CgsResource::RwShaderProgramBufferResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwShaderProgramBufferResourceType";
        }
        {
            CgsResource::RwShaderParameterResourceType* lpType = new (lpAllocator) CgsResource::RwShaderParameterResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwShaderParameterResourceType";
        }
        {
            CgsResource::ShaderTechniqueResourceType* lpType = new (lpAllocator) CgsResource::ShaderTechniqueResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ShaderTechniqueResourceType";
        }
        {
            CgsResource::RwDebugResourceType* lpType = new (lpAllocator) CgsResource::RwDebugResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwDebugResourceType";
        }
        {
            CgsResource::MaterialTechniqueResourceType* lpType = new (lpAllocator) CgsResource::MaterialTechniqueResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "MaterialTechniqueResourceType";
        }
        {
            CgsResource::RwRenderableResourceType* lpType = new (lpAllocator) CgsResource::RwRenderableResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwRenderableResourceType";
        }
        {
            CgsResource::AptDataHeaderType* lpType = new (lpAllocator) CgsResource::AptDataHeaderType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "AptDataHeaderType";
        }
        {
            CgsResource::HudMessageResourceType* lpType = new (lpAllocator) CgsResource::HudMessageResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "HudMessageResourceType";
        }
        {
            CgsResource::HudMessageListResourceType* lpType = new (lpAllocator) CgsResource::HudMessageListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "HudMessageListResourceType";
        }
        {
            CgsResource::GuiPopupResourceType* lpType = new (lpAllocator) CgsResource::GuiPopupResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GuiPopupResourceType";
        }
        {
            CgsResource::FontResourceType* lpType = new (lpAllocator) CgsResource::FontResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "FontResourceType";
        }
        {
            CgsResource::LuaCodeResourceType* lpType = new (lpAllocator) CgsResource::LuaCodeResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "LuaCodeResourceType";
        }
        {
            CgsResource::RegistryResourceType* lpType = new (lpAllocator) CgsResource::RegistryResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RegistryResourceType";
        }
        {
            CgsResource::GinsuWaveContentResourceType* lpType = new (lpAllocator) CgsResource::GinsuWaveContentResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GinsuWaveContentResourceType";
        }
        {
            CgsResource::GenericRwacWaveContentResourceType* lpType = new (lpAllocator) CgsResource::GenericRwacWaveContentResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GenericRwacWaveContentResourceType";
        }
        {
            CgsResource::GenericRwacReverbIRContentResourceType* lpType = new (lpAllocator) CgsResource::GenericRwacReverbIRContentResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GenericRwacReverbIRContentResourceType";
        }
        {
            CgsResource::AemsBankResourceType* lpType = new (lpAllocator) CgsResource::AemsBankResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "AemsBankResourceType";
        }
        {
            CgsResource::CsisResourceType* lpType = new (lpAllocator) CgsResource::CsisResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "CsisResourceType";
        }
        {
            CgsResource::NicotineResourceType* lpType = new (lpAllocator) CgsResource::NicotineResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "NicotineResourceType";
        }
        {
            CgsResource::VoiceHierarchyResourceType* lpType = new (lpAllocator) CgsResource::VoiceHierarchyResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "VoiceHierarchyResourceType";
        }
        {
            CgsResource::SplicerResourceType* lpType = new (lpAllocator) CgsResource::SplicerResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "SplicerResourceType";
        }
        {
            CgsResource::SnrResourceType* lpType = new (lpAllocator) CgsResource::SnrResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "SnrResourceType";
        }
        {
            CgsResource::SnapshotDataResourceType* lpType = new (lpAllocator) CgsResource::SnapshotDataResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "SnapshotDataResourceType";
        }
        {
            CgsResource::LanguageResourceType* lpType = new (lpAllocator) CgsResource::LanguageResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "LanguageResourceType";
        }
        {
            CgsResource::VideoDataResourceType* lpType = new (lpAllocator) CgsResource::VideoDataResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "VideoDataResourceType";
        }
        {
            CgsResource::ZoneListResourceType* lpType = new (lpAllocator) CgsResource::ZoneListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ZoneListResourceType";
        }
        {
            CgsGraphics::InstanceListResourceType* lpType = new (lpAllocator) CgsGraphics::InstanceListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "InstanceListResourceType";
        }
        {
            CgsPhysics::InstanceCollisionListResourceType* lpType = new (lpAllocator) CgsPhysics::InstanceCollisionListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "InstanceCollisionListResourceType";
        }
        {
            CgsResource::ClusteredMeshResourceType* lpType = new (lpAllocator) CgsResource::ClusteredMeshResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ClusteredMeshResourceType";
        }
        {
            CgsResource::KdTreeResourceType* lpType = new (lpAllocator) CgsResource::KdTreeResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "KdTreeResourceType";
        }
        {
            CgsResource::AttribSysSchemaResourceType* lpType = new (lpAllocator) CgsResource::AttribSysSchemaResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "AttribSysSchemaResourceType";
        }
        {
            CgsResource::AttribSysVaultResourceType* lpType = new (lpAllocator) CgsResource::AttribSysVaultResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "AttribSysVaultResourceType";
        }
        {
            CgsResource::ModelResourceType* lpType = new (lpAllocator) CgsResource::ModelResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ModelResourceType";
        }
        {
            CgsResource::RwColourCubeResourceType* lpType = new (lpAllocator) CgsResource::RwColourCubeResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "RwColourCubeResourceType";
        }
        {
            CgsResource::WorldPainter2DResourceType* lpType = new (lpAllocator) CgsResource::WorldPainter2DResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "WorldPainter2DResourceType";
        }
        {
            BrnAI::AISectionsResourceType* lpType = new (lpAllocator) BrnAI::AISectionsResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "AISectionsResourceType";
        }
        {
            BrnTrigger::TriggerResourceType* lpType = new (lpAllocator) BrnTrigger::TriggerResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "TriggerResourceType";
        }
        {
            BrnTraffic::TrafficDataResourceType* lpType = new (lpAllocator) BrnTraffic::TrafficDataResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "TrafficDataResourceType";
        }
        {
            BrnTraffic::GraphicsStubResourceType* lpType = new (lpAllocator) BrnTraffic::GraphicsStubResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GraphicsStubResourceType";
        }
        {
            BrnSound::Vehicles::Engines::LoopModelResourceType* lpType = new (lpAllocator) BrnSound::Vehicles::Engines::LoopModelResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "LoopModelResourceType";
        }
        {
            BrnResource::VehicleListResourceType* lpType = new (lpAllocator) BrnResource::VehicleListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "VehicleListResourceType";
        }
        {
            BrnResource::ChallengeListResourceType* lpType = new (lpAllocator) BrnResource::ChallengeListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ChallengeListResourceType";
        }
        {
            BrnResource::WheelListResourceType* lpType = new (lpAllocator) BrnResource::WheelListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "WheelListResourceType";
        }
        {
            BrnVehicle::GraphicsSpecResourceType* lpType = new (lpAllocator) BrnVehicle::GraphicsSpecResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GraphicsSpecResourceType";
        }
        {
            BrnWheel::GraphicsSpecResourceType* lpType = new (lpAllocator) BrnWheel::GraphicsSpecResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "GraphicsSpecResourceType";
        }
        {
            BrnParticle::ParticleDescriptionCollectionResourceType* lpType = new (lpAllocator) BrnParticle::ParticleDescriptionCollectionResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ParticleDescriptionCollectionResourceType";
        }
        {
            BrnParticle::ParticleDescriptionResourceType* lpType = new (lpAllocator) BrnParticle::ParticleDescriptionResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ParticleDescriptionResourceType";
        }
        {
            BrnParticle::TextureNameMapResourceType* lpType = new (lpAllocator) BrnParticle::TextureNameMapResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "TextureNameMapResourceType";
        }
        {
            BrnParticle::BrnVFXMeshCollectionResourceType* lpType = new (lpAllocator) BrnParticle::BrnVFXMeshCollectionResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "BrnVFXMeshCollectionResourceType";
        }
        {
            BrnParticle::VFXPropCollectionResourceType* lpType = new (lpAllocator) BrnParticle::VFXPropCollectionResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "VFXPropCollectionResourceType";
        }
        {
            BrnFlapt::FlaptFileResourceType* lpType = new (lpAllocator) BrnFlapt::FlaptFileResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "FlaptFileResourceType";
        }
        {
            CgsResource::SatNavTileDirectoryResourceType* lpType = new (lpAllocator) CgsResource::SatNavTileDirectoryResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "SatNavTileDirectoryResourceType";
        }
        {
            CgsResource::SatNavTileResourceType* lpType = new (lpAllocator) CgsResource::SatNavTileResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "SatNavTileResourceType";
        }
        {
            CgsResource::PFXHookBundleResourceType* lpType = new (lpAllocator) CgsResource::PFXHookBundleResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "PFXHookBundleResourceType";
        }
        {
            BrnProgression::ProgressionResourceType* lpType = new (lpAllocator) BrnProgression::ProgressionResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ProgressionResourceType";
        }
        {
            BrnPhysics::Props::PropPhysicsResourceType* lpType = new (lpAllocator) BrnPhysics::Props::PropPhysicsResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "PropPhysicsResourceType";
        }
        {
            BrnPhysics::Props::PropGraphicsListResourceType* lpType = new (lpAllocator) BrnPhysics::Props::PropGraphicsListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "PropGraphicsListResourceType";
        }
        {
            BrnPhysics::Props::PropInstanceDataResourceType* lpType = new (lpAllocator) BrnPhysics::Props::PropInstanceDataResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "PropInstanceDataResourceType";
        }
        {
            BrnWorld::EnvironmentSettings::KeyframeResourceType* lpType = new (lpAllocator) BrnWorld::EnvironmentSettings::KeyframeResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "BrnEnvironmentKeyframeResourceType";
        }
        {
            BrnWorld::EnvironmentSettings::TimeLineResourceType* lpType = new (lpAllocator) BrnWorld::EnvironmentSettings::TimeLineResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "BrnEnvironmentTimeLineResourceType";
        }
        {
            BrnWorld::EnvironmentSettings::DictionaryResourceType* lpType = new (lpAllocator) BrnWorld::EnvironmentSettings::DictionaryResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "BrnEnvironmentDictionaryResourceType";
        }
        {
            BrnSound::World::StaticSoundMapResourceType* lpType = new (lpAllocator) BrnSound::World::StaticSoundMapResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "StaticSoundMapResourceType";
        }
        {
            BrnStreetData::StreetDataResourceType* lpType = new (lpAllocator) BrnStreetData::StreetDataResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "StreetDataResourceType";
        }
        {
            CgsContainers::DictionaryResourceType<ICE::ICETakeData>* lpType = new (lpAllocator) CgsContainers::DictionaryResourceType<ICE::ICETakeData>;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ICETakeDictionaryResourceType";
        }
        {
            BrnResource::StreamedDeformationSpecResourceType* lpType = new (lpAllocator) BrnResource::StreamedDeformationSpecResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "StreamedDeformationSpecResourceType";
        }
        {
            CgsResource::PolygonSoupListResourceType* lpType = new (lpAllocator) CgsResource::PolygonSoupListResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "PolygonSoupListResourceType";
        }
        {
            CgsResource::MassiveLookupTableResourceType* lpType = new (lpAllocator) CgsResource::MassiveLookupTableResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "MassiveLookupTableResourceType";
        }
        {
            CgsResource::PlayerCarColoursResourceType* lpType = new (lpAllocator) CgsResource::PlayerCarColoursResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "PlayerCarColoursResourceType";
        }
        {
            BrnProgression::ProfileUpgradeResourceType* lpType = new (lpAllocator) BrnProgression::ProfileUpgradeResourceType;
            lpType->InitCachedValues();
            maTypes[miNumTypes].mpType = lpType;
            maTypes[miNumTypes++].mpcName = "ProfileUpgradeResourceType";
        }
    }
}
