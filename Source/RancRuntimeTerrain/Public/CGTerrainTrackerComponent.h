#pragma once

#include "RancRuntimeTerrain/Public/CGTerrainManager.h"

#include <Runtime/Engine/Classes/Components/ActorComponent.h>

#include "CGTerrainTrackerComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RANCRUNTIMETERRAIN_API UCGTerrainTrackerComponent : public UActorComponent
{
	GENERATED_BODY()

	bool isSetup = false;
public:	
	// Sets default values for this component's properties
	UCGTerrainTrackerComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;



	/* Sets actor invisible until inital terrain generation is complete */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	bool HideActorUntilTerrainComplete = false;

	/* Attempts to disable gravity on character until terrain generation is complete */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	bool DisableCharacterGravityUntilComplete = false;

	/* Attempts to teleport character to terrain surface when terrain generation is complete */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	bool TeleportToSurfaceOnTerrainComplete  = false;

	void OnTerrainComplete();

	FVector mySpawnLocation;
	ACGTerrainManager* MyTerrainManager;

	/* Attempts to teleport character to terrain surface when terrain generation is complete */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	int32 SpawnRayCastsPerFrame = 10;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void OnUnregister() override;

	bool isTerrainComplete = false;
	bool isSpawnPointFound = false;

};
