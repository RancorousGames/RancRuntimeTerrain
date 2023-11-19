#pragma once

#include "RancRuntimeTerrain/Public/CGMcQueue.h"
#include "RancRuntimeTerrain/Public/CGObjectPool.h"
#include "RancRuntimeTerrain/Public/CGSettings.h"
#include "RancRuntimeTerrain/Public/Struct/CGJob.h"
#include "RancRuntimeTerrain/Public/Struct/CGLODMeshData.h"
#include "RancRuntimeTerrain/Public/Struct/CGMeshData.h"
#include "RancRuntimeTerrain/Public/Struct/CGSector.h"
#include "RancRuntimeTerrain/Public/Struct/CGTerrainConfig.h"
#include "RancRuntimeTerrain/Public/Struct/CGTileHandle.h"
#include "RancRuntimeTerrain/Public/Struct/CGIntVector2.h"

#include "RancRuntimeTerrain/Public/WorldHeightInterface.h"
#include <Runtime/Engine/Classes/Components/HierarchicalInstancedStaticMeshComponent.h>
#include <Runtime/Engine/Classes/GameFramework/Actor.h>

#include "CGTerrainManager.generated.h"

class ACGTile;

UCLASS(BlueprintType, Blueprintable)
class RANCRUNTIMETERRAIN_API ACGTerrainManager : public AActor
{
	GENERATED_BODY()

public:
	ACGTerrainManager();
	~ACGTerrainManager();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RancRuntimeTerrain")
	UCGSettings* Settings = GetMutableDefault<UCGSettings>();

	/* Event called when initial terrain generation is complete */
	DECLARE_EVENT(ACGTerrainManager, FTerrainCompleteEvent)

	FTerrainCompleteEvent& OnTerrainComplete() { return TerrainCompleteEvent; }

	/* Returns true once terrain has been configured */
	bool isReady = false;

	/* Main entry point for starting terrain generation */
	UFUNCTION(BlueprintCallable, Category = "RancRuntimeTerrain")
	void SetupTerrainGeneratorHeightmap(TScriptInterface<IWorldHeightInterface> worldHeightInterface);

	/* Main entry point for starting terrain generation */
	UFUNCTION(BlueprintCallable, Category = "RancRuntimeTerrain")
	void SetupTerrainGeneratorFastNoise(UUFNNoiseGenerator* aHeightmapGenerator, UUFNNoiseGenerator* aBiomeGenerator /*FCGTerrainConfig aTerrainConfig*/);

	/* Add a new actor to track and generate terrain tiles around */
	UFUNCTION(BlueprintCallable, Category = "RancRuntimeTerrain")
	void AddActorToTrack(AActor* aActor);

	/* Add a new actor to track and generate terrain tiles around */
	UFUNCTION(BlueprintCallable, Category = "RancRuntimeTerrain")
	void RemoveActorToTrack(AActor* aActor);

	// Pending job queue, worker threads take jobs from here
	TCGSpmcQueue<FCGJob> myPendingJobQueue;

	// Update queue, jobs get sent here from the worker thread
	TQueue<FCGJob, EQueueMode::Mpsc> myUpdateJobQueue;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void BeginDestroy() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	UHierarchicalInstancedStaticMeshComponent* MyWaterMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RancRuntimeTerrain")
	FCGTerrainConfig myTerrainConfig;

	UFUNCTION(BlueprintImplementableEvent, Category = "RancRuntimeTerrain|Events")
	void OnAfterTileCreated(ACGTile* tile);

	UPROPERTY(BlueprintReadOnly, Category = "RancRuntimeTerrain")
	bool IsTerrainComplete = false;

protected:
	void BroadcastTerrainComplete()
	{
		TerrainCompleteEvent.Broadcast();
	}

private:
	void SetActorSector(const AActor* aActor, const FCGIntVector2& aNewSector);
	void AllocateAllMeshDataStructures();
	bool AllocateDataStructuresForLOD(FCGMeshData* aData, FCGTerrainConfig* aConfig, const uint8 aLOD);
	int GetLODForRange(const int32 aRange);
	void CreateTileRefreshJob(FCGJob aJob);
	void ProcessTilesForActor(const AActor* anActor);
	TPair<ACGTile*, int32> GetAvailableTile();
	void FreeTile(ACGTile* aTile, const int32& aWaterMeshIndex);
	FCGIntVector2 GetSector(const FVector& aLocation);
	TArray<FCGSector> GetRelevantSectorsForActor(const AActor* aActor);

	FTerrainCompleteEvent TerrainCompleteEvent;

	// Threads
	TArray<FRunnableThread*> myWorkerThreads;

	// Geometry data storage
	UPROPERTY()
	TArray<FCGLODMeshData> myMeshData;
	TArray<TCGObjectPool<FCGMeshData>> myFreeMeshData;

	// Tile/Sector tracking
	TArray<ACGTile*> myFreeTiles;
	TArray<int32> myFreeWaterMeshIndices;
	UPROPERTY()
	TMap<FCGIntVector2, FCGTileHandle> myTileHandleMap;
	TSet<FCGIntVector2> myQueuedSectors;

	// Actor tracking
	TArray<AActor*> myTrackedActors;
	TMap<AActor*, FCGIntVector2> myActorLocationMap;

	// Sweep tracking
	float myTimeSinceLastSweep = 0.0f;
	const float mySweepTime = 2.0f;
	uint8 myActorIndex = 0;
};
