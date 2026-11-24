#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace DirectX
{
	class AudioEngine;
	class SoundEffect;
	class SoundEffectInstance;
}

namespace SoundIds
{
	inline constexpr const char* BattleBgm01 = "BGM/BGM_Battle01_maou_";
	inline constexpr const char* HitNormal = "SE/Battle/SE_Hit_Normal_maou";
	inline constexpr const char* HitHard = "SE/Battle/SE_Hit_Hard_maou";
}

class SoundManager
{
public:
	static SoundManager& GetInstance();

	bool Initialize();

	void Update();

	void Shutdown();

	bool LoadSE(const std::string& soundId, const std::string& path);

	bool LoadBGM(const std::string& soundId, const std::string& path, bool loop);

	void PlaySE(const std::string& soundId);

	void PlayBGM(const std::string& soundId);

	void StopSE(const std::string& soundId);

	void StopAllSE();

	void StopBGM();

	bool IsSoundLoaded(const std::string& soundId) const;

	void SetMasterVolume(float volume);

	void SetBGMVolume(float volume);

	void SetSEVolume(float volume);

	float GetMasterVolume() const;

	float GetBGMVolume() const;

	float GetSEVolume() const;

	bool LoadAudioSettings();

	bool SaveAudioSettings() const;

private:
	struct SoundResource
	{
		std::unique_ptr<DirectX::SoundEffect> effect;
		bool loop = false;
		bool bgm = false;
	};

	std::unique_ptr<DirectX::AudioEngine> audioEngine;
	std::unordered_map<std::string, SoundResource> soundResources;
	std::unordered_map<std::string, std::unique_ptr<DirectX::SoundEffectInstance>> bgmInstances;
	std::string currentBgmId;
	float masterVolume = 1.0f;
	float bgmVolume = 0.5f;
	float seVolume = 1.0f;
	bool audioSettingsLoaded = false;

	SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	bool LoadSound(const std::string& soundId, const std::string& path, bool loop, bool bgm);

	bool LoadSoundFromId(const std::string& soundId, bool loop, bool bgm);

	void ApplyBGMVolume();

	static float ClampVolume(float volume);
};
