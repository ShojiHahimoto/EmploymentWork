#pragma once

#include <memory>
#include <cstdint>
#include <cstddef>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>

namespace DirectX
{
	class AudioEngine;
	class SoundEffect;
	class SoundEffectInstance;
}

class LoadWorker;
class SaveWorker;

namespace SoundIds
{
	inline constexpr const char* BattleBgm01 = "BGM/BGM_Battle01_maou_";
	inline constexpr const char* HitNormal = "SE/Battle/SE_Hit_Normal_maou";
	inline constexpr const char* HitHard = "SE/Battle/SE_Hit_Hard_maou";
}

class SoundManager
{
public:
	struct PreparedSound
	{
		std::unique_ptr<std::uint8_t[]> data;
		std::size_t audioOffset = 0;
		std::size_t audioBytes = 0;
	};

	static SoundManager& GetInstance();

	bool Initialize(LoadWorker &loader, SaveWorker &writer);

	void Update();

	void Shutdown();

	static PreparedSound PrepareSound(const std::string &path);
	bool RegisterSound(const std::string &soundId, PreparedSound sound, bool loop, bool bgm);

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

	void LoadAudioSettings();

	void SaveAudioSettings();

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
	struct AudioSettings
	{
		float master;
		float bgm;
		float se;
	};

	LoadWorker *loadWorker = nullptr;
	SaveWorker *saveWorker = nullptr;
	std::future<std::optional<AudioSettings>> pendingSettingsLoad;
	std::future<bool> pendingSettingsSave;

	static std::optional<AudioSettings> ReadAudioSettings(AudioSettings settings);
	static bool WriteAudioSettings(const AudioSettings &settings);
	void ApplySettingsResults();

	SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	void ApplyBGMVolume();

	static float ClampVolume(float volume);
};
