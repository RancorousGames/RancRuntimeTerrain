# RancRuntimeTerrain
Procedural Terrain Generator for UnrealEngine 5.1

This plugin generates heightmap-based terrain tiles in realtime, and move the tiles around to track a player pawn. 

This plugin was originally developed by Chris Ashworth http://cashworth.net/ with the name CashGen. He made a Demo as well for his early version:
[![CashGen Demo](http://img.youtube.com/vi/r68mpFKRAbA/0.jpg)](http://www.youtube.com/watch?v=r68mpFKRAbA "Video Title")

Chris also made samples of his version that can be found here:
https://github.com/midgen/CashDemo

I forked this project since CashGen seemed mostly dead and I wanted to change something for my procedural top down island game. 
My intention is to make changes that support a top down multiplayer game but without breaking any of the original features.


Features:

* Multithreaded heightmap, erosion and geometry generation
* A simple hydraulic erosion algorithm
* Multiple tile LODs with per-LOD collision, tesselation and subdivision
* Dithered LOD transitions (when using a suitable material instance)
* Slope scalar in vertex colour channel 
* Depth map texture generation for water material

It has dependencies on :

* UnrealFastNoise by Chris Ashworth, a modular noise generation plugin 

1. Create a new C++ project
2. Checkout UnrealFastNoisePlugin into your engine or project plugins folder ( https://github.com/midgen/UnrealFastNoise )
3. Checkout this repository into your engine or project plugins folder
4. Add "RancRuntimeTerrain", "UnrealFastNoisePlugin" to your project Build.cs (required to package project)
```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "RancRuntimeTerrain", "UnrealFastNoisePlugin" });
```
5. Enable the plugin in the editor Edit -> Plugins -> UnrealFastNoisePlugin and RancRuntimeTerrain
6. Create a new Blueprint based on CGTerrainManager
7. OnBeginPlay in the blueprint, call either SetupTerrainGeneratorFastNoise or SetupTerrainGeneratorHeightMap
	1. SetupTerrainGeneratorFastNoise using UnrealFastNoisePlugin, see image https://i.imgur.com/qXyZ38E.png or check out the CashGen demo
	2. Construct GCTextureHeightmap object, Set its height map to a heightmap texture and TerrainSamplingScalar and call intialize then call SetupTerrainGeneratorHeightMap with it
	3. Create a new UObject blueprint, implement the WorldHeightInterface interface in the blueprints class settings, implement GetHeightAtPoint function, call SetupTerrainGeneratorHeightMap with it
8. On the  blueprint root detail, set up Ranc Runtime Terrain -> My Terrain Config with appropriate materials. Set up LODs e.g. {{Radius 3, Resolution Div. 1, col yes},{Radius 6, resolution div. 2, col no}}
	* See explanation of terrain scaling values: https://i.imgur.com/kGVZ83M.png
9. Add a CGTerrainTrackerComponent to any actors you wish to have terrain formed around
10. Place blueprint in level and press Play.

11. You can optionally tell the tracker component to hide/disable gravity on the spawned actors until terrain generation is complete
12. Vertex Colours - Red = slope. Green = the biome mask specified in terrain config


Troubleshooting
 * No terrain is drawn - Check your Terrain Config on your terrain manager, especially terrain material and LOD maps. Also check that your character has the CGTerrainTrackerComponent 
 * Whole terrain disappears and my character falls through the world - This can happen if your character moves too fast/teleports. Not yet sure why this happens.
 * Terrain is flat with fastnoise - You need much higher values for heightmultiplier and blocksize. e.g. 30000 and 5000. 
