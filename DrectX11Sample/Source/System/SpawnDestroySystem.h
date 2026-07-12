#pragma once

class World;

class SpawnDestroySystem
{
public:
	static void Update(World& world);

private:
	static void ApplySpawnRequests(World& world);
	static void ApplyDestroyRequests(World& world);
};
