#include "RancRuntimeTerrain/Public/CGTerrainGeneratorWorker.h"
#include "RancRuntimeTerrain/Public/CGTile.h"

#include <ProceduralMeshComponent/Public/ProceduralMeshComponent.h>

#include "RancRuntimeTerrain/Public/WorldHeightInterface.h"
#include <UnrealFastNoisePlugin/Public/UFNNoiseGenerator.h>

#include <chrono>

DECLARE_CYCLE_STAT(TEXT("RancRuntimeTerrainStat ~ HeightMap"), STAT_HeightMap, STATGROUP_RancRuntimeTerrainStat);
DECLARE_CYCLE_STAT(TEXT("RancRuntimeTerrainStat ~ Normals"), STAT_Normals, STATGROUP_RancRuntimeTerrainStat);
DECLARE_CYCLE_STAT(TEXT("RancRuntimeTerrainStat ~ Erosion"), STAT_Erosion, STATGROUP_RancRuntimeTerrainStat);

FCGTerrainGeneratorWorker::FCGTerrainGeneratorWorker(ACGTerrainManager& aTerrainManager, FCGTerrainConfig& aTerrainConfig, TArray<TCGObjectPool<FCGMeshData>>& meshDataPoolPerLOD) 
	: pTerrainManager(aTerrainManager)
	, pTerrainConfig(aTerrainConfig)
	, pMeshDataPoolsPerLOD(meshDataPoolPerLOD)
{
}

FCGTerrainGeneratorWorker::~FCGTerrainGeneratorWorker()
{
}

bool FCGTerrainGeneratorWorker::Init()
{
	IsThreadFinished = false;
	return true;
}

uint32 FCGTerrainGeneratorWorker::Run()
{
	// Here's the loop
	while (!IsThreadFinished)
	{
		if (pTerrainManager.myPendingJobQueue.Dequeue(workJob))
		{
			workLOD = workJob.LOD;

			workJob.Data = pMeshDataPoolsPerLOD[workLOD].Borrow([&] { return !IsThreadFinished; });
			if (!workJob.Data.IsValid() && IsThreadFinished)
			{
				// seems borrowing aborted because IsThreadFinished got true. Let's just return
				return 1;
			}
			
			pMeshData = workJob.Data.Get();

			std::chrono::milliseconds startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch());

			prepMaps();
			ProcessTerrainMap();

			workJob.HeightmapGenerationDuration = (std::chrono::duration_cast<std::chrono::milliseconds>(
													   std::chrono::system_clock::now().time_since_epoch()) -
												   startMs)
													  .count();

			startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch());

			if (workLOD == 0)
			{
				{
					SCOPE_CYCLE_COUNTER(STAT_Erosion);

					for (int32 i = 0; i < pTerrainConfig.DropletAmount; ++i)
					{
						ProcessSingleDropletErosion();
					}
				}
			}

			workJob.ErosionGenerationDuration = (std::chrono::duration_cast<std::chrono::milliseconds>(
													 std::chrono::system_clock::now().time_since_epoch()) -
												 startMs)
													.count();

			ProcessPerBlockGeometry();
			ProcessPerVertexTasks();
			ProcessSkirtGeometry();

			pTerrainManager.myUpdateJobQueue.Enqueue(workJob);
		}
		// Otherwise, take a nap
		else
		{
			FPlatformProcess::Sleep(0.01f);
		}
	}

	return 1;
}

void FCGTerrainGeneratorWorker::Stop()
{
	IsThreadFinished = true;
}

void FCGTerrainGeneratorWorker::Exit()
{
}

void FCGTerrainGeneratorWorker::prepMaps()
{
	// TODO : VERTEX COLORS
	for (int32 i = 0; i < pMeshData->MyColours.Num(); ++i)
	{
		pMeshData->MyColours[i].R = 0;
		pMeshData->MyColours[i].G = 0;
		pMeshData->MyColours[i].B = 0;
		pMeshData->MyColours[i].A = 0;
	}
}

void FCGTerrainGeneratorWorker::ProcessTerrainMap()
{
	SCOPE_CYCLE_COUNTER(STAT_HeightMap);
	// Size of the noise sampling (larger than the actual mesh so we can have seamless normals)
	int32 exX = GetNumberOfNoiseSamplePoints();
	int32 exY = exX;

	const int32 BlocksPerSector = workLOD == 0 ? pTerrainConfig.BlocksPerSector : pTerrainConfig.BlocksPerSector / pTerrainConfig.LODs[workLOD].ResolutionDivisor;
	const int32 currentBlockSize = workLOD == 0 ? pTerrainConfig.BlockSize : pTerrainConfig.BlockSize * pTerrainConfig.LODs[workLOD].ResolutionDivisor;


	UObject* WorldInterfaceObject = pTerrainConfig.AlternateWorldHeightInterface.GetObject();

	int32 SectorCenterOffset = BlocksPerSector / 2;
	
	// Calculate the new noisemap
	for (int y = 0; y < exY; ++y)
	{
		for (int x = 0; x < exX; ++x)
		{
			const int32 worldX = (((workJob.mySector.X * BlocksPerSector) + x)) - SectorCenterOffset;
			const int32 worldY = (((workJob.mySector.Y * BlocksPerSector) + y)) - SectorCenterOffset;

			float height = WorldInterfaceObject ? IWorldHeightInterface::Execute_GetHeightAtPoint(WorldInterfaceObject, worldX, worldY) : pTerrainConfig.NoiseGenerator->GetNoise2D(worldX, worldY);
			pMeshData->HeightMap[x + (exX * y)] = height - pTerrainConfig.NoiseWaterLevel;
		}
	}
	// Put heightmap into Red channel

	if (pTerrainConfig.GenerateSplatMap && workLOD == 0)
	{
		int i = 0;
		for (int y = 0; y < pTerrainConfig.BlocksPerSector; ++y)
		{
			for (int x = 0; x < pTerrainConfig.BlocksPerSector; ++x)
			{
				const float& noiseValue = pMeshData->HeightMap[(x + 1) + (exX * (y + 1))];

				pMeshData->myTextureData[i].R = (uint8)FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 1.0f), FVector2D(0.0f, 255.0f), noiseValue);

				pMeshData->myTextureData[i].G = (uint8)FMath::GetMappedRangeValueClamped(FVector2D(-1.0f, 0.0f), FVector2D(0.0f, 255.0f), noiseValue);

				pMeshData->myTextureData[i].B = i;

				pMeshData->myTextureData[i].A = 0;

				i++;
			}
		}
	}

	// Then put the biome map into the Green vertex colour channel
	if (pTerrainConfig.BiomeBlendGenerator)
	{
		exX -= 2;
		exY -= 2;
		for (int y = 0; y < exY; ++y)
		{
			for (int x = 0; x < exX; ++x)
			{
				int32 worldX = (((workJob.mySector.X * (exX - 1)) + x));
				int32 worldY = (((workJob.mySector.Y * (exX - 1)) + y));
				float val = pTerrainConfig.BiomeBlendGenerator->GetNoise2D(worldX, worldY);

				pMeshData->MyColours[x + (exX * y)].G = FMath::Clamp(FMath::RoundToInt(((val + 1.0f) / 2.0f) * 256), 0, 255);
			}
		}
	}
}

void FCGTerrainGeneratorWorker::AddDepositionToHeightMap()
{
	int32 index = 0;
	for (float& heightPoint : pMeshData->HeightMap)
	{
		//heightPoint.Z += (*pDepositionMap)[index];
		++index;
	}
}

void FCGTerrainGeneratorWorker::erodeHeightMapAtIndex(int32 aX, int32 aY, float aAmount)
{
	int32 XUnits = GetNumberOfNoiseSamplePoints();
	float mod1 = 0.5f;
	float mod2 = 0.4f;

	pMeshData->HeightMap[aX + (XUnits * aY)] -= aAmount;
	pMeshData->HeightMap[aX + (XUnits * (aY + 1))] -= aAmount * mod1;
	pMeshData->HeightMap[aX + (XUnits * (aY - 1))] -= aAmount * mod1;
	pMeshData->HeightMap[aX + 1 + (XUnits * (aY))] -= aAmount * mod1;
	pMeshData->HeightMap[aX - 1 + (XUnits * (aY))] -= aAmount * mod1;

	pMeshData->HeightMap[aX + 1 + (XUnits * (aY + 1))] -= aAmount * mod1;
	pMeshData->HeightMap[aX + 1 + (XUnits * (aY - 1))] -= aAmount * mod1;
	pMeshData->HeightMap[aX - 1 + (XUnits * (aY + 1))] -= aAmount * mod1;
	pMeshData->HeightMap[aX - 1 + (XUnits * (aY - 1))] -= aAmount * mod1;

	// Add to the Red channel for deposition
	if (aAmount > 0.0f)
	{
		//pMeshData->MyVertexData[aX - 1 + ((XUnits - 2) * (aY - 1))].Color.R = FMath::Clamp(pMeshData->MyVertexData[aX - 1 + ((XUnits - 2) * (aY - 1))].Color.R + FMath::RoundToInt(aAmount), 0, 255);
	}
	// Add to the blue channel for erosion
	if (aAmount <= 0.0f)
	{
		//pMeshData->MyVertexData[aX - 1 + ((XUnits - 2) * (aY - 1))].Color.B = FMath::Clamp(pMeshData->MyVertexData[aX - 1 + ((XUnits - 2) * (aY - 1))].Color.B + FMath::RoundToInt(aAmount * 0.01f), 0, 255);
	}
}

void FCGTerrainGeneratorWorker::ProcessSingleDropletErosion()
{
	int32 XUnits = GetNumberOfNoiseSamplePoints();
	int32 YUnits = XUnits;

	// Pick a random start point that isn't on an edge
	int32 cX = FMath::RandRange(1, XUnits - 1);
	int32 cY = FMath::RandRange(1, YUnits - 1);

	float sedimentAmount = 0.0f;
	float waterAmount = 1.0f;
	FVector velocity = FVector(0.0f, 0.0f, 1.0f);

	//while (waterAmount > 0.0f && cX > 0 && cX < XUnits - 1 && cY > 0 && cY < YUnits - 1)
	//{
	//	FVector origin = pMeshData->HeightMap[cX + (XUnits * cY)];
	//	if (origin.Z < pTerrainConfig->DropletErosionFloor)
	//	{
	//		// Don't care about underwater erosion
	//		break;
	//	}
	//	FVector up = (pMeshData->HeightMap[cX + (XUnits * (cY + 1))] - origin).GetSafeNormal();
	//	FVector down = (pMeshData->HeightMap[cX + (XUnits * (cY - 1))] - origin).GetSafeNormal();
	//	FVector left = (pMeshData->HeightMap[cX + 1 + (XUnits * (cY))] - origin).GetSafeNormal();
	//	FVector right = (pMeshData->HeightMap[cX - 1 + (XUnits * (cY))] - origin).GetSafeNormal();

	//	FVector upleft = (pMeshData->HeightMap[cX + 1 + (XUnits * (cY + 1))] - origin).GetSafeNormal();
	//	FVector downleft = (pMeshData->HeightMap[cX + 1 + (XUnits * (cY - 1))] - origin).GetSafeNormal();
	//	FVector upright = (pMeshData->HeightMap[cX - 1 + (XUnits * (cY + 1))] - origin).GetSafeNormal();
	//	FVector downright = (pMeshData->HeightMap[cX - 1 + (XUnits * (cY - 1))] - origin).GetSafeNormal();

	//	FVector lowestRoute = FVector(0.0f);

	//	int32 newCx = cX;
	//	int32 newCy = cY;

	//	if (up.Z < lowestRoute.Z) { lowestRoute = up; newCy++; }
	//	if (down.Z < lowestRoute.Z) { lowestRoute = down; newCy--; }
	//	if (left.Z < lowestRoute.Z) { lowestRoute = left; newCx++; }
	//	if (right.Z < lowestRoute.Z) { lowestRoute = right; newCx--; }
	//	if (upleft.Z < lowestRoute.Z) { lowestRoute = upleft; newCy++; newCx++; }
	//	if (upright.Z < lowestRoute.Z) { lowestRoute = upright; newCy++; newCx--; }
	//	if (downleft.Z < lowestRoute.Z) { lowestRoute = downleft; newCy--; newCx++; }
	//	if (downright.Z < lowestRoute.Z) { lowestRoute = downright; newCy--; newCx--; }

	//	// The amount of sediment to pick up depends on if we are hitting an obstacle
	//	float sedimentUptake = pTerrainConfig->DropletErosionMultiplier * FVector::DotProduct(velocity, lowestRoute);
	//	if (sedimentUptake < 0.0f) { sedimentUptake = 0.0f; }

	//	sedimentAmount += sedimentUptake;

	//	float sedimentDeposit = 0.0f;
	//	// Deposit sediment if we are carrying too much
	//	if (sedimentAmount > pTerrainConfig->DropletSedimentCapacity)
	//	{
	//		sedimentDeposit = (sedimentAmount - pTerrainConfig->DropletSedimentCapacity) * pTerrainConfig->DropletDespositionMultiplier;
	//	}

	//	// Deposit based on slope
	//	sedimentDeposit += sedimentAmount * FMath::Clamp(1.0f + lowestRoute.Z, 0.0f, 1.0f);

	//	sedimentAmount -= sedimentDeposit;

	//	velocity = lowestRoute;

	//	erodeHeightMapAtIndex(cX, cY, (sedimentUptake + (sedimentDeposit * -1.0f)));

	//	waterAmount -= pTerrainConfig->DropletEvaporationRate;

	//	cX = newCx;
	//	cY = newCy;
}

void FCGTerrainGeneratorWorker::ProcessPerBlockGeometry()
{
	int32 vertCounter = 0;
	int32 triCounter = 0;

	int32 unitCount = workLOD == 0 ? pTerrainConfig.BlocksPerSector : (pTerrainConfig.BlocksPerSector / pTerrainConfig.LODs[workLOD].ResolutionDivisor);

	// Generate the mesh data for each block
	for (int32 y = 0; y < unitCount; ++y)
	{
		for (int32 x = 0; x < unitCount; ++x)
		{
			UpdateOneBlockGeometry(x, y, vertCounter, triCounter);
		}
	}
}

void FCGTerrainGeneratorWorker::ProcessPerVertexTasks()
{
	SCOPE_CYCLE_COUNTER(STAT_Normals);
	int32 unitCount = workLOD == 0 ? pTerrainConfig.BlocksPerSector : (pTerrainConfig.BlocksPerSector / pTerrainConfig.LODs[workLOD].ResolutionDivisor);

	int32 rowLength = workLOD == 0 ? pTerrainConfig.BlocksPerSector + 1 : (pTerrainConfig.BlocksPerSector / (pTerrainConfig.LODs[workLOD].ResolutionDivisor) + 1);

	for (int32 y = 0; y < unitCount + 1; ++y)
	{
		for (int32 x = 0; x < unitCount + 1; ++x)
		{
			FVector normal;
			FProcMeshTangent tangent(0.0f, 1.0f, 0.f);

			GetNormalFromHeightMapForVertex(x, y, normal);

			uint8 slopeChan = FMath::RoundToInt((1.0f - FMath::Abs(FVector::DotProduct(normal, FVector::UpVector))) * 256);
			pMeshData->MyColours[x + (y * rowLength)].R = slopeChan;
			pMeshData->MyNormals[x + (y * rowLength)] = normal;
			pMeshData->MyTangents[x + (y * rowLength)] = tangent;
		}
	}
}

// Generates the 'skirt' geometry that falls down from the edges of each tile
void FCGTerrainGeneratorWorker::ProcessSkirtGeometry()
{
	// Going to do this the simple way, keep code easy to understand!

	int32 sectorSideVertCount = workLOD == 0 ? pTerrainConfig.BlocksPerSector + 1 : (pTerrainConfig.BlocksPerSector / pTerrainConfig.LODs[workLOD].ResolutionDivisor) + 1;

	int32 startIndex = sectorSideVertCount * sectorSideVertCount;
	int32 triStartIndex = ((sectorSideVertCount - 1) * (sectorSideVertCount - 1) * 6);

	// Bottom Edge verts
	for (int i = 0; i < sectorSideVertCount; ++i)
	{
		pMeshData->MyPositions[startIndex + i].X = pMeshData->MyPositions[i].X;
		pMeshData->MyPositions[startIndex + i].Y = pMeshData->MyPositions[i].Y;
		pMeshData->MyPositions[startIndex + i].Z = -30000.0f;

		pMeshData->MyNormals[startIndex + i] = pMeshData->MyNormals[i];
	}
	// bottom edge triangles
	for (int i = 0; i < ((sectorSideVertCount - 1)); ++i)
	{
		pMeshData->MyTriangles[triStartIndex + (i * 6)] = i;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 1] = startIndex + i + 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 2] = startIndex + i;

		pMeshData->MyTriangles[triStartIndex + (i * 6) + 3] = i + 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 4] = startIndex + i + 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 5] = i;
	}
	triStartIndex += ((sectorSideVertCount - 1) * 6);

	startIndex = ((sectorSideVertCount) * (sectorSideVertCount + 1));
	// Top Edge verts
	for (int i = 0; i < sectorSideVertCount; ++i)
	{
		pMeshData->MyPositions[startIndex + i].X = pMeshData->MyPositions[i + startIndex - (sectorSideVertCount * 2)].X;
		pMeshData->MyPositions[startIndex + i].Y = pMeshData->MyPositions[i + startIndex - (sectorSideVertCount * 2)].Y;
		pMeshData->MyPositions[startIndex + i].Z = -30000.0f;

		pMeshData->MyNormals[startIndex + i] = pMeshData->MyNormals[i + startIndex - (sectorSideVertCount * 2)];
	}
	// top edge triangles

	for (int i = 0; i < ((sectorSideVertCount - 1)); ++i)
	{
		pMeshData->MyTriangles[triStartIndex + (i * 6)] = i + startIndex - (sectorSideVertCount * 2);
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 1] = startIndex + i;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 2] = i + startIndex - (sectorSideVertCount * 2) + 1;

		pMeshData->MyTriangles[triStartIndex + (i * 6) + 3] = i + startIndex - (sectorSideVertCount * 2) + 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 4] = startIndex + i;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 5] = startIndex + i + 1;
	}
	triStartIndex += ((sectorSideVertCount - 1) * 6);

	startIndex = sectorSideVertCount * (sectorSideVertCount + 2);
	// Right edge - bit different
	for (int i = 0; i < sectorSideVertCount - 2; ++i)
	{
		pMeshData->MyPositions[startIndex + i].X = pMeshData->MyPositions[(i + 1) * sectorSideVertCount].X;
		pMeshData->MyPositions[startIndex + i].Y = pMeshData->MyPositions[(i + 1) * sectorSideVertCount].Y;
		pMeshData->MyPositions[startIndex + i].Z = -30000.0f;

		pMeshData->MyNormals[startIndex + i] = pMeshData->MyNormals[(i + 1) * sectorSideVertCount];
	}
	// Bottom right corner

	pMeshData->MyTriangles[triStartIndex] = 0;
	pMeshData->MyTriangles[triStartIndex + 1] = sectorSideVertCount * sectorSideVertCount;
	pMeshData->MyTriangles[triStartIndex + 2] = sectorSideVertCount;

	pMeshData->MyTriangles[triStartIndex + 3] = sectorSideVertCount;
	pMeshData->MyTriangles[triStartIndex + 4] = sectorSideVertCount * sectorSideVertCount;
	pMeshData->MyTriangles[triStartIndex + 5] = sectorSideVertCount * (sectorSideVertCount + 2);

	// Top right corner
	triStartIndex += 6;

	pMeshData->MyTriangles[triStartIndex] = sectorSideVertCount * (sectorSideVertCount - 1);
	pMeshData->MyTriangles[triStartIndex + 1] = (sectorSideVertCount * (sectorSideVertCount + 2)) + sectorSideVertCount - 3;
	pMeshData->MyTriangles[triStartIndex + 2] = sectorSideVertCount * (sectorSideVertCount + 1);

	pMeshData->MyTriangles[triStartIndex + 3] = sectorSideVertCount * (sectorSideVertCount - 1);
	pMeshData->MyTriangles[triStartIndex + 4] = sectorSideVertCount * (sectorSideVertCount - 2);
	pMeshData->MyTriangles[triStartIndex + 5] = (sectorSideVertCount * (sectorSideVertCount + 2)) + sectorSideVertCount - 3;

	// Middle right part!
	startIndex = sectorSideVertCount * (sectorSideVertCount + 2);
	triStartIndex += 6;

	for (int i = 0; i < sectorSideVertCount - 3; ++i)
	{
		pMeshData->MyTriangles[triStartIndex + (i * 6)] = sectorSideVertCount * (i + 1);
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 1] = startIndex + i;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 2] = sectorSideVertCount * (i + 2);

		pMeshData->MyTriangles[triStartIndex + (i * 6) + 3] = sectorSideVertCount * (i + 2);
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 4] = startIndex + i;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 5] = startIndex + i + 1;
	}
	triStartIndex += ((sectorSideVertCount - 3) * 6);

	startIndex += (sectorSideVertCount - 2);
	// Left edge - bit different
	for (int i = 0; i < sectorSideVertCount - 2; ++i)
	{
		pMeshData->MyPositions[startIndex + i].X = pMeshData->MyPositions[((i + 1) * sectorSideVertCount) + sectorSideVertCount - 1].X;
		pMeshData->MyPositions[startIndex + i].Y = pMeshData->MyPositions[((i + 1) * sectorSideVertCount) + sectorSideVertCount - 1].Y;
		pMeshData->MyPositions[startIndex + i].Z = -30000.0f;

		pMeshData->MyNormals[startIndex + i] = pMeshData->MyNormals[((i + 1) * sectorSideVertCount) + sectorSideVertCount - 1];
	}
	// Bottom left corner

	pMeshData->MyTriangles[triStartIndex] = sectorSideVertCount - 1;
	pMeshData->MyTriangles[triStartIndex + 1] = (sectorSideVertCount * 2) - 1;
	pMeshData->MyTriangles[triStartIndex + 2] = startIndex;

	pMeshData->MyTriangles[triStartIndex + 3] = startIndex;
	pMeshData->MyTriangles[triStartIndex + 4] = (sectorSideVertCount * sectorSideVertCount) + sectorSideVertCount - 1;
	pMeshData->MyTriangles[triStartIndex + 5] = sectorSideVertCount - 1;

	// Top left corner
	triStartIndex += 6;

	pMeshData->MyTriangles[triStartIndex] = (sectorSideVertCount * sectorSideVertCount) - 1;
	pMeshData->MyTriangles[triStartIndex + 1] = (sectorSideVertCount * (sectorSideVertCount + 2)) - 1;
	pMeshData->MyTriangles[triStartIndex + 2] = (sectorSideVertCount * (sectorSideVertCount + 2)) + ((sectorSideVertCount - 2) * 2) - 1;

	pMeshData->MyTriangles[triStartIndex + 3] = (sectorSideVertCount * sectorSideVertCount) - 1;
	pMeshData->MyTriangles[triStartIndex + 4] = (sectorSideVertCount * (sectorSideVertCount + 2)) + ((sectorSideVertCount - 2) * 2) - 1;
	pMeshData->MyTriangles[triStartIndex + 5] = (sectorSideVertCount * (sectorSideVertCount - 2)) + sectorSideVertCount - 1;

	// Middle left part!

	triStartIndex += 6;

	for (int i = 0; i < sectorSideVertCount - 3; ++i)
	{
		pMeshData->MyTriangles[triStartIndex + (i * 6)] = (sectorSideVertCount * (i + 1)) + sectorSideVertCount - 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 1] = (sectorSideVertCount * (i + 2)) + sectorSideVertCount - 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 2] = startIndex + i + 1;

		pMeshData->MyTriangles[triStartIndex + (i * 6) + 3] = (sectorSideVertCount * (i + 1)) + sectorSideVertCount - 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 4] = startIndex + i + 1;
		pMeshData->MyTriangles[triStartIndex + (i * 6) + 5] = startIndex + i;
	}
}

void FCGTerrainGeneratorWorker::GetNormalFromHeightMapForVertex(const int32& vertexX, const int32& vertexY, FVector& aOutNormal) //, FVector& aOutTangent)
{
	FVector result;

	FVector tangentVec, bitangentVec;

	const int32 rowLength = workLOD == 0 ? pTerrainConfig.BlocksPerSector + 1 : (pTerrainConfig.BlocksPerSector / (pTerrainConfig.LODs[workLOD].ResolutionDivisor) + 1);
	const int32 heightMapRowLength = rowLength + 2;

	// the heightmapIndex for this vertex index
	const int32 heightMapIndex = vertexX + 1 + ((vertexY + 1) * heightMapRowLength);
	const float worldSectorBlockPositionX = workJob.mySector.X * pTerrainConfig.BlocksPerSector;
	const float worldSectorBlockPositionY = workJob.mySector.Y * pTerrainConfig.BlocksPerSector;
	const float& unitSize = workLOD == 0 ? pTerrainConfig.BlockSize : pTerrainConfig.BlockSize * pTerrainConfig.LODs[workLOD].ResolutionDivisor;
	const float& ampl = pTerrainConfig.HeightMultiplier;

	FVector origin = FVector((worldSectorBlockPositionX + vertexX) * unitSize, (worldSectorBlockPositionY + vertexY) * unitSize, pMeshData->HeightMap[heightMapIndex] * ampl);

	// Get the 4 neighbouring points
	FVector up, down, left, right;

	up = FVector((worldSectorBlockPositionX + vertexX) * unitSize, (worldSectorBlockPositionY + vertexY + 1) * unitSize, pMeshData->HeightMap[heightMapIndex + heightMapRowLength] * ampl) - origin;
	down = FVector((worldSectorBlockPositionX + vertexX) * unitSize, (worldSectorBlockPositionY + vertexY - 1) * unitSize, pMeshData->HeightMap[heightMapIndex - heightMapRowLength] * ampl) - origin;
	left = FVector((worldSectorBlockPositionX + vertexX + 1) * unitSize, (worldSectorBlockPositionY + vertexY) * unitSize, pMeshData->HeightMap[heightMapIndex + 1] * ampl) - origin;
	right = FVector((worldSectorBlockPositionX + vertexX - 1) * unitSize, (worldSectorBlockPositionY + vertexY) * unitSize, pMeshData->HeightMap[heightMapIndex - 1] * ampl) - origin;

	FVector n1, n2, n3, n4;

	n1 = FVector::CrossProduct(left, up);
	n2 = FVector::CrossProduct(up, right);
	n3 = FVector::CrossProduct(right, down);
	n4 = FVector::CrossProduct(down, left);

	result = n1 + n2 + n3 + n4;

	aOutNormal = result.GetSafeNormal();

	// We can mega cheap out here as we're dealing with a simple flat grid
	//aOutTangent = FRuntimeMeshTangent(left.GetSafeNormal(), false);
}

void FCGTerrainGeneratorWorker::UpdateOneBlockGeometry(const int32& aX, const int32& aY, int32& aVertCounter, int32& triCounter)
{
	int32 thisX = aX;
	int32 thisY = aY;
	int32 heightMapX = thisX + 1;
	int32 heightMapY = thisY + 1;
	// LOD adjusted dimensions
	int32 rowLength = workLOD == 0 ? pTerrainConfig.BlocksPerSector + 1 : (pTerrainConfig.BlocksPerSector / (pTerrainConfig.LODs[workLOD].ResolutionDivisor) + 1);
	int32 heightMapRowLength = rowLength + 2;
	// LOD adjusted unit size
	int32 exUnitSize = workLOD == 0 ? pTerrainConfig.BlockSize : pTerrainConfig.BlockSize * (pTerrainConfig.LODs[workLOD].ResolutionDivisor);

	const int blockX = 0;
	const int blockY = 0;
	const float& unitSize = pTerrainConfig.BlockSize;
	const float& ampl = pTerrainConfig.HeightMultiplier;

	FVector heightMapToWorldOffset = FVector(0.0f, 0.0f, 0.0f);

	// TL
	pMeshData->MyPositions[thisX + (thisY * rowLength)] = FVector((blockX + thisX) * exUnitSize, (blockY + thisY) * exUnitSize, pMeshData->HeightMap[heightMapX + (heightMapY * heightMapRowLength)] * ampl) - heightMapToWorldOffset;
	// TR
	pMeshData->MyPositions[thisX + ((thisY + 1) * rowLength)] = FVector((blockX + thisX) * exUnitSize, (blockY + thisY + 1) * exUnitSize, pMeshData->HeightMap[heightMapX + ((heightMapY + 1) * heightMapRowLength)] * ampl) - heightMapToWorldOffset;
	// BL
	pMeshData->MyPositions[(thisX + 1) + (thisY * rowLength)] = FVector((blockX + thisX + 1) * exUnitSize, (blockY + thisY) * exUnitSize, pMeshData->HeightMap[(heightMapX + 1) + (heightMapY * heightMapRowLength)] * ampl) - heightMapToWorldOffset;
	// BR
	pMeshData->MyPositions[(thisX + 1) + ((thisY + 1) * rowLength)] = FVector((blockX + thisX + 1) * exUnitSize, (blockY + thisY + 1) * exUnitSize, pMeshData->HeightMap[(heightMapX + 1) + ((heightMapY + 1) * heightMapRowLength)] * ampl) - heightMapToWorldOffset;
}

int32 FCGTerrainGeneratorWorker::GetNumberOfNoiseSamplePoints()
{
	return workLOD == 0 ? pTerrainConfig.BlocksPerSector + 3 : (pTerrainConfig.BlocksPerSector / (pTerrainConfig.LODs[workLOD].ResolutionDivisor)) + 3;
}
