// HeightMapWorldHeight.cpp
#include "CGTextureHeightmap.h"
#include "Rendering/Texture2DResource.h"

bool UGCTextureHeightmap::Initialize()
{
	if (HeightMap)
	{
		FTexture2DMipMap* MyMipMap = &HeightMap->PlatformData->Mips[0];
		FByteBulkData* RawImageData = &MyMipMap->BulkData;

		TextureWidth = MyMipMap->SizeX;
		TextureHeight = MyMipMap->SizeY;
		if (RawImageData->IsLocked())
		{
			return false;
		}
		FormattedImageData = static_cast<FColor*>(RawImageData->Lock(LOCK_READ_ONLY));
		RawImageData->Unlock();
		return true;
	}
	else
	{
		FormattedImageData = nullptr;
		TextureWidth = 0;
		TextureHeight = 0;
	}

		return false;
}

float max = -10000;
float min = 10000;

float UGCTextureHeightmap::GetHeightAtPoint_Implementation(float x, float z)
{
	if (!FormattedImageData || TextureWidth <= 0 || TextureHeight <= 0)
	{
		return 0.0f;
	}

	int32 pixelX = FMath::Clamp(static_cast<int32>((x*TerrainSamplingScalar) + TextureWidth / 2), 0, TextureWidth - 1);
	int32 pixelY = FMath::Clamp(static_cast<int32>((z*TerrainSamplingScalar) + TextureWidth / 2), 0, TextureHeight - 1);

	FColor pixelColor = FormattedImageData[pixelY * TextureWidth + pixelX];
	float result = pixelColor.R / 255.0f;

	if (result > max)
	{
		max = result;
		UE_LOG(LogTemp, Warning, TEXT("new max height %f %d %d"), result, pixelX, pixelY);
	}
	if (result < min)
	{
		min = result;
		UE_LOG(LogTemp, Warning, TEXT("new min height %f %d %d"), result, pixelX, pixelY);
	}
	return result;
}