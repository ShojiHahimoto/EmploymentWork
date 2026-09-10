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
	/// <summary>
	/// アプリ全体で共有する SoundManager インスタンスを取得する。
	/// </summary>
	/// <returns>SoundManager の唯一のインスタンス。</returns>
	static SoundManager& GetInstance();

	/// <summary>
	/// DirectXTK AudioEngine を初期化する。既に初期化済みなら何もしない。
	/// </summary>
	/// <returns>初期化済み、または初期化に成功した場合は true。</returns>
	bool Initialize();

	/// <summary>
	/// AudioEngine と再生中 SE のフレーム更新を行う。
	/// </summary>
	void Update();

	/// <summary>
	/// アプリ終了時に音声リソースを解放する。シーン遷移時には呼ばない。
	/// </summary>
	void Shutdown();

	/// <summary>
	/// 非ループ SE として wav を読み込む。
	/// </summary>
	/// <param name="soundId">再生時に指定する音源 ID。</param>
	/// <param name="path">wav ファイルパス。相対パスは assets/Sound 基準も試す。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool LoadSE(const std::string& soundId, const std::string& path);

	/// <summary>
	/// BGM として wav を読み込む。
	/// </summary>
	/// <param name="soundId">再生時に指定する音源 ID。</param>
	/// <param name="path">wav ファイルパス。相対パスは assets/Sound 基準も試す。</param>
	/// <param name="loop">BGM をループ再生する場合は true。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool LoadBGM(const std::string& soundId, const std::string& path, bool loop);

	/// <summary>
	/// 指定 ID の SE を再生する。未読み込みなら assets/Sound 配下から読み込みを試す。
	/// </summary>
	/// <param name="soundId">再生する SE の音源 ID。</param>
	void PlaySE(const std::string& soundId);

	/// <summary>
	/// 指定 ID の BGM を再生する。別 BGM が再生中なら停止してから切り替える。
	/// </summary>
	/// <param name="soundId">再生する BGM の音源 ID。</param>
	void PlayBGM(const std::string& soundId);

	/// <summary>
	/// 指定 ID の再生中 SE を停止する。通常の one-shot SE は停止管理しないため、現状は将来のループSE用の入口。
	/// </summary>
	/// <param name="soundId">停止する SE の音源 ID。</param>
	void StopSE(const std::string& soundId);

	/// <summary>
	/// 再生中の SE をすべて停止する。通常の one-shot SE は DirectXTK 側に寿命管理を任せる。
	/// </summary>
	void StopAllSE();

	/// <summary>
	/// 現在再生中の BGM を停止する。
	/// </summary>
	void StopBGM();

	/// <summary>
	/// 指定した音源 ID が読み込み済みか確認する。
	/// </summary>
	/// <param name="soundId">確認する音源 ID。</param>
	/// <returns>読み込み済みなら true。</returns>
	bool IsSoundLoaded(const std::string& soundId) const;

	/// <summary>
	/// 全音声に掛けるマスター音量を設定する。
	/// </summary>
	/// <param name="volume">0.0 ～ 1.0 の音量値。</param>
	void SetMasterVolume(float volume);

	/// <summary>
	/// BGM に掛ける音量を設定する。再生中 BGM には即時反映する。
	/// </summary>
	/// <param name="volume">0.0 ～ 1.0 の音量値。</param>
	void SetBGMVolume(float volume);

	/// <summary>
	/// SE に掛ける音量を設定する。one-shot SE のため次回再生分から反映する。
	/// </summary>
	/// <param name="volume">0.0 ～ 1.0 の音量値。</param>
	void SetSEVolume(float volume);

	/// <summary>
	/// 現在のマスター音量を取得する。
	/// </summary>
	/// <returns>0.0 ～ 1.0 の音量値。</returns>
	float GetMasterVolume() const;

	/// <summary>
	/// 現在の BGM 音量を取得する。
	/// </summary>
	/// <returns>0.0 ～ 1.0 の音量値。</returns>
	float GetBGMVolume() const;

	/// <summary>
	/// 現在の SE 音量を取得する。
	/// </summary>
	/// <returns>0.0 ～ 1.0 の音量値。</returns>
	float GetSEVolume() const;

	/// <summary>
	/// 音量設定を JSON から読み込む。
	/// </summary>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool LoadAudioSettings();

	/// <summary>
	/// 現在の音量設定を JSON へ保存する。
	/// </summary>
	/// <returns>保存に成功した場合は true。</returns>
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

	/// <summary>
	/// 指定パスの wav を音源 ID に紐づけて読み込む。
	/// </summary>
	/// <param name="soundId">再生時に指定する音源 ID。</param>
	/// <param name="path">wav ファイルパス。</param>
	/// <param name="loop">ループ再生する音源なら true。</param>
	/// <param name="bgm">BGM として扱う音源なら true。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool LoadSound(const std::string& soundId, const std::string& path, bool loop, bool bgm);

	/// <summary>
	/// 音源 ID から assets/Sound 配下の wav パスを組み立てて読み込みを試す。
	/// </summary>
	/// <param name="soundId">読み込みたい音源 ID。</param>
	/// <param name="loop">読み込み後のループ設定。</param>
	/// <param name="bgm">BGM として扱う音源なら true。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool LoadSoundFromId(const std::string& soundId, bool loop, bool bgm);

	/// <summary>
	/// BGM インスタンスに現在のマスター音量と BGM 音量を反映する。
	/// </summary>
	void ApplyBGMVolume();

	/// <summary>
	/// 外部から来た音量値を 0.0 ～ 1.0 に丸める。
	/// </summary>
	/// <param name="volume">丸める音量値。</param>
	/// <returns>0.0 ～ 1.0 に丸めた値。</returns>
	static float ClampVolume(float volume);
};
