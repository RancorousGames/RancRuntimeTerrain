#pragma once

#include "RancRuntimeTerrain/Public/Struct/CGMeshData.h"

#include "CGLODMeshData.generated.h"

USTRUCT(BlueprintType)
struct FCGLODMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	TArray<FCGMeshData> Data;

};