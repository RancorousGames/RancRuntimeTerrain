#pragma once

#include "RancRuntimeTerrain/Public/Struct/CGIntVector2.h"
#include "RancRuntimeTerrain/Public/Struct/CGTileHandle.h"
#include "RancRuntimeTerrain/Public/CGObjectPool.h"

#include "CGJob.generated.h"

struct FCGMeshData;

USTRUCT(BlueprintType)
struct FCGJob
{
	GENERATED_BODY()

	FCGJob()
		: mySector(0,0)
		, HeightmapGenerationDuration(0)
		, ErosionGenerationDuration(0)
		, LOD(0)
		, IsInPlaceUpdate(false)
	{
	}

	FCGIntVector2 mySector;
	FCGTileHandle myTileHandle;
	TCGBorrowedObject<FCGMeshData> Data;
	int32 HeightmapGenerationDuration;
	int32 ErosionGenerationDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	uint8 LOD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	bool IsInPlaceUpdate;
};