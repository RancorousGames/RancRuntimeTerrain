// HeightMapWorldHeight.h
#pragma once

#include "CoreMinimal.h"
#include "WorldHeightInterface.h"
#include "Engine/Texture2D.h"
#include "CGTextureHeightmap.generated.h"

UCLASS(BlueprintType)
class RANCRUNTIMETERRAIN_API UGCTextureHeightmap : public UObject, public IWorldHeightInterface
{
	GENERATED_BODY()


	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Map")
	UTexture2D* HeightMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category = "Data Source")
	float TerrainSamplingScalar = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Data Source")
	bool Initialize();
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Data Source")
	float GetHeightAtPoint(float x, float z);
	virtual float GetHeightAtPoint_Implementation(float x, float z) override;

private:
	FColor* FormattedImageData;
	uint32 TextureWidth;
	uint32 TextureHeight;
};