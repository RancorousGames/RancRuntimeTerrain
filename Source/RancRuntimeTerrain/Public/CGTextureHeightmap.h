// HeightMapWorldHeight.h
#pragma once

#include "CoreMinimal.h"
#include "WorldHeightInterface.h"
#include "CGTextureHeightmap.generated.h"

UCLASS(BlueprintType)
class RANCRUNTIMETERRAIN_API UGCTextureHeightmap : public UObject, public IWorldHeightInterface
{
	GENERATED_BODY()

public:
	/** TerrainSamplingScalar: higher value = smaller map, SmoothingLevel: flattens peaks & edges. Higher value increases initialization time**/
	UFUNCTION(BlueprintCallable, Category = "Data Source")
	bool Initialize(UTexture2D* HeightMapTexture, float TerrainSamplingScalar, int32 SmoothingLevel = 1);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Data Source")
	float GetHeightAtPoint(float x, float y);
	virtual float GetHeightAtPoint_Implementation(float x, float y) override;

private:
	float TerrainSamplingScalarValue = 1.0f;

	uint8* FormattedGreyscaleData;
	int32 TextureWidth;
	int32 TextureHeight;
};