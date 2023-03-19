#pragma once

#include "CGSettings.generated.h"

/**
* Implements the settings for the RancRuntimeTerrain Plugin
*/
UCLASS(config = RancRuntimeTerrain, BlueprintType)
class RANCRUNTIMETERRAIN_API UCGSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = Debug)
	bool ShowTimings = false;
};
