#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WorldHeightInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWorldHeightInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * An interface for providing height data for terrain
 */
class RANCRUNTIMETERRAIN_API IWorldHeightInterface
{
	GENERATED_BODY()

public:
	/** Get the height at given coordinates */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Data Source")
	float GetHeightAtPoint(float x, float z);
};