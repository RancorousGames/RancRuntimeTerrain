// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include <Runtime/Core/Public/Modules/ModuleManager.h>

#define Msg(Text) if(GEngine) GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Green, TEXT(Text));


DECLARE_STATS_GROUP(TEXT("RancRuntimeTerrain"), STATGROUP_RancRuntimeTerrainStat, STATCAT_Advanced);

class RANCRUNTIMETERRAIN_API FRancRuntimeTerrain : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	bool HandleSettingsSaved();
};
