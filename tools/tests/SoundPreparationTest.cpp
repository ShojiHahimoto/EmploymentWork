#include "Manager/SoundManager.h"

#include <cassert>

int main()
{
	for (const char *id : {SoundIds::BattleBgm01, SoundIds::HitNormal, SoundIds::HitHard})
	{
		auto sound = SoundManager::PrepareSound(id);
		assert(sound.data);
		assert(sound.audioOffset > 0);
		assert(sound.audioBytes > 0);
	}

	assert(!SoundManager::PrepareSound("__missing_sound__").data);
}
