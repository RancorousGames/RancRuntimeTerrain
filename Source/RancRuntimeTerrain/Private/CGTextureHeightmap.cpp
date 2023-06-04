// HeightMapWorldHeight.cpp

#include "CGTextureHeightmap.h"
#include "Rendering/Texture2DResource.h"

bool UGCTextureHeightmap::Initialize(UTexture2D* HeightMap, float TerrainSamplingScalar, int32 SmoothingLevel)
{	
	this->TerrainSamplingScalarValue = TerrainSamplingScalar;
	if (HeightMap)
	{
		FTexture2DMipMap* MyMipMap = &HeightMap->GetPlatformData()->Mips[0];
		FByteBulkData* RawImageData = &MyMipMap->BulkData;

		TextureWidth = MyMipMap->SizeX;
		TextureHeight = MyMipMap->SizeY;
		if (RawImageData->IsLocked()) return false;
		
		FormattedGreyscaleData = new uint8[TextureWidth*SmoothingLevel*TextureHeight*SmoothingLevel];

		UE_LOG(LogTemp, Warning, TEXT("Heightmap format: %d"), HeightMap->GetPixelFormat());
		if (HeightMap->GetPixelFormat() == PF_B8G8R8A8)
		{
			const FColor* ImageData = static_cast<FColor*>(RawImageData->Lock(LOCK_READ_ONLY));
			if (!ImageData) return false;
			
			const float smoothingValueCount = (2 * SmoothingLevel + 1) * (2 * SmoothingLevel + 1);
			
			for (int32 y = 0; y < TextureHeight; y++)
			{				
				//UE_LOG(LogTemp, Warning, TEXT("Pixel Y %f at %d"), pixelY, y);
				for (int32 x = 0; x < TextureWidth; x++)
				{
					float value = 0;
					float baseValue =  ImageData[y*TextureWidth + x].R;

					if (SmoothingLevel > 1 && x >= SmoothingLevel && y >= SmoothingLevel)
					{

						// sample all values in a grid around the value we wanna set
						for (int32 yp = -SmoothingLevel; yp <= SmoothingLevel; yp++)
						{
							for (int32 xp = -SmoothingLevel; xp <= SmoothingLevel; xp++)
							{
								value += ImageData[(y + yp)*TextureWidth + x + xp].R;
							}
						}

						// Set the final value to an average of the sampled values
						value /= smoothingValueCount;
					}
					else
					{
						value = ImageData[y*TextureWidth + x].R;
					}
				
					FormattedGreyscaleData[y*TextureWidth + x] = value;
				}
			}		
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("HeightMap is not in a supported color format, please use RGBA (PF_B8G8R8A8)"));
			return false;
		}
		
		RawImageData->Unlock();
		return true;
	}

	
	
	FormattedGreyscaleData = nullptr;
	TextureWidth = 0;
	TextureHeight = 0;

	return false;
}

float UGCTextureHeightmap::GetHeightAtPoint_Implementation(float x, float y)
{
	if (!FormattedGreyscaleData || TextureWidth <= 0 || TextureHeight <= 0)
	{
		return 0.0f;
	}

	const uint32 pixelX = FMath::Clamp(static_cast<int32>((x*TerrainSamplingScalarValue) + TextureWidth / 2), 0, TextureWidth - 1);
	const uint32 pixelY = FMath::Clamp(static_cast<int32>((y*TerrainSamplingScalarValue) + TextureHeight / 2), 0, TextureHeight - 1);

	return FormattedGreyscaleData[pixelY * TextureWidth + pixelX] / 255.0f;
}