// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RancRuntimeTerrain/Public/RancRuntimeTerrain.h"
#include "RancRuntimeTerrain/Public/CGSettings.h"
#include <Developer/Settings/Public/ISettingsModule.h>
#include <Developer/Settings/Public/ISettingsSection.h>

//#include "RancRuntimeTerrainPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FRancRuntimeTerrain"

void FRancRuntimeTerrain::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "RancRuntimeTerrain",
			LOCTEXT("RancRuntimeTerrainSettingsName", "RancRuntimeTerrain"),
			LOCTEXT("RancRuntimeTerrainSettingsDescription", "Configure the RancRuntimeTerrain Plugin"),
			GetMutableDefault<UCGSettings>()
		);

		if (SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FRancRuntimeTerrain::HandleSettingsSaved);
		}
	}
}

void FRancRuntimeTerrain::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "RancRuntimeTerrain");
	}
}

bool FRancRuntimeTerrain::HandleSettingsSaved()
{
	//RestartServices();

	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRancRuntimeTerrain, RancRuntimeTerrain)