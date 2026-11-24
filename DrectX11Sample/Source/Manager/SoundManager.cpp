#include "Manager/SoundManager.h"

#include "Data/JsonValue.h"
#include "Core/Debugger.h"

#include <Audio.h>
#include <Windows.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{
	constexpr const char* AudioSettingsPath = "assets/Settings/AudioSettings.json";

	/// <summary>
	/// UTF-8 文字列を Windows API / DirectXTK が受け取れるワイド文字列へ変換する。
	/// </summary>
	/// <param name="text">変換する UTF-8 文字列。</param>
	/// <returns>変換後のワイド文字列。</returns>
	std::wstring ToWideString(const std::string& text)
	{
		if (text.empty())
		{
			return std::wstring();
		}

		const int requiredLength = MultiByteToWideChar(
			CP_UTF8,
			0,
			text.c_str(),
			static_cast<int>(text.size()),
			nullptr,
			0);
		if (requiredLength <= 0)
		{
			return std::wstring(text.begin(), text.end());
		}

		std::wstring wideText(static_cast<size_t>(requiredLength), L'\0');
		MultiByteToWideChar(
			CP_UTF8,
			0,
			text.c_str(),
			static_cast<int>(text.size()),
			wideText.data(),
			requiredLength);
		return wideText;
	}

	/// <summary>
	/// 拡張子が省略された音源パスに .wav を付ける。
	/// </summary>
	/// <param name="path">確認するパス。</param>
	/// <returns>拡張子付きのパス。</returns>
	std::filesystem::path WithWaveExtension(const std::filesystem::path& path)
	{
		if (path.has_extension())
		{
			return path;
		}

		std::filesystem::path withExtension = path;
		withExtension += ".wav";
		return withExtension;
	}

	/// <summary>
	/// 実行ディレクトリの違いを吸収しながら、実在する音源パスを探す。
	/// </summary>
	/// <param name="requestedPath">呼び出し側が指定した音源パス。</param>
	/// <param name="outPath">見つかった実在パスの書き込み先。</param>
	/// <returns>実在するファイルが見つかれば true。</returns>
	bool ResolveSoundPath(const std::filesystem::path& requestedPath, std::filesystem::path& outPath)
	{
		const std::filesystem::path wavePath = WithWaveExtension(requestedPath);
		std::vector<std::filesystem::path> candidates;
		candidates.push_back(wavePath);

		if (wavePath.is_relative())
		{
			candidates.push_back(std::filesystem::path("assets/Sound") / wavePath);
			candidates.push_back(std::filesystem::path("DrectX11Sample/assets/Sound") / wavePath);
			candidates.push_back(std::filesystem::path("../../DrectX11Sample/assets/Sound") / wavePath);
		}

		for (const std::filesystem::path& candidate : candidates)
		{
			std::error_code errorCode;
			if (std::filesystem::exists(candidate, errorCode) && !errorCode)
			{
				outPath = candidate;
				return true;
			}
		}

		return false;
	}

	/// <summary>
	/// JsonValue Object から float 値を取得する。
	/// </summary>
	/// <param name="root">検索対象の JSON Object。</param>
	/// <param name="key">取得するキー。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した float 値。</returns>
	float GetFloat(const JsonValue& root, const std::string& key, float defaultValue)
	{
		const JsonValue* value = root.Find(key);
		if (!value || !value->IsNumber())
		{
			return defaultValue;
		}

		return static_cast<float>(value->AsNumber(defaultValue));
	}
}

/// <summary>アプリ全体で共有する SoundManager インスタンスを取得する。</summary>
/// <returns>SoundManager の唯一のインスタンス。</returns>
SoundManager& SoundManager::GetInstance()
{
	static SoundManager instance;
	return instance;
}

/// <summary>DirectXTK AudioEngine を初期化する。既に初期化済みなら何もしない。</summary>
/// <returns>初期化済み、または初期化に成功した場合は true。</returns>
bool SoundManager::Initialize()
{
	if (audioEngine)
	{
		return true;
	}

	try
	{
		audioEngine = std::make_unique<DirectX::AudioEngine>();
		if (!audioSettingsLoaded)
		{
			LoadAudioSettings();
		}
		return true;
	}
	catch (const std::exception& exception)
	{
		DebugLog("[Sound] AudioEngine initialize failed. ", exception.what());
		audioEngine.reset();
		return false;
	}
	catch (...)
	{
		DebugLog("[Sound] AudioEngine initialize failed. Unknown exception.");
		audioEngine.reset();
		return false;
	}
}

/// <summary>AudioEngine と再生中 SE のフレーム更新を行う。</summary>
void SoundManager::Update()
{
	if (!audioEngine)
	{
		return;
	}

	try
	{
		if (!audioEngine->Update() && audioEngine->IsCriticalError())
		{
			DebugLog("[Sound] AudioEngine critical error.");
		}

	}
	catch (const std::exception& exception)
	{
		DebugLog("[Sound] Update failed. ", exception.what());
	}
	catch (...)
	{
		DebugLog("[Sound] Update failed. Unknown exception.");
	}
}

/// <summary>アプリ終了時に音声リソースを解放する。シーン遷移時には呼ばない。</summary>
void SoundManager::Shutdown()
{
	try
	{
		StopBGM();
		StopAllSE();
		bgmInstances.clear();
		soundResources.clear();
		audioEngine.reset();
		currentBgmId.clear();
	}
	catch (const std::exception& exception)
	{
		DebugLog("[Sound] Shutdown failed. ", exception.what());
	}
	catch (...)
	{
		DebugLog("[Sound] Shutdown failed. Unknown exception.");
	}
}

/// <summary>非ループ SE として wav を読み込む。</summary>
/// <param name="soundId">再生時に指定する音源 ID。</param>
/// <param name="path">wav ファイルパス。相対パスは assets/Sound 基準も試す。</param>
/// <returns>読み込みに成功した場合は true。</returns>
bool SoundManager::LoadSE(const std::string& soundId, const std::string& path)
{
	return LoadSound(soundId, path, false, false);
}

/// <summary>BGM として wav を読み込む。</summary>
/// <param name="soundId">再生時に指定する音源 ID。</param>
/// <param name="path">wav ファイルパス。相対パスは assets/Sound 基準も試す。</param>
/// <param name="loop">BGM をループ再生する場合は true。</param>
/// <returns>読み込みに成功した場合は true。</returns>
bool SoundManager::LoadBGM(const std::string& soundId, const std::string& path, bool loop)
{
	return LoadSound(soundId, path, loop, true);
}

/// <summary>指定 ID の SE を再生する。未読み込みなら assets/Sound 配下から読み込みを試す。</summary>
/// <param name="soundId">再生する SE の音源 ID。</param>
void SoundManager::PlaySE(const std::string& soundId)
{
	if (soundId.empty())
	{
		return;
	}

	auto found = soundResources.find(soundId);
	if (found == soundResources.end())
	{
		if (!LoadSoundFromId(soundId, false, false))
		{
			return;
		}

		found = soundResources.find(soundId);
		if (found == soundResources.end())
		{
			return;
		}
	}

	try
	{
		// 短いSEは DirectXTK の one-shot 再生に任せる。
		// 自前で SoundEffectInstance を即時生成・削除すると、再生途中で切れる素材がある。
		found->second.effect->Play(masterVolume * seVolume, 0.0f, 0.0f);
	}
	catch (const std::exception& exception)
	{
		DebugLog("[Sound] PlaySE failed. Id=", soundId, " Error=", exception.what());
	}
	catch (...)
	{
		DebugLog("[Sound] PlaySE failed. Id=", soundId, " Unknown exception.");
	}
}

/// <summary>指定 ID の BGM を再生する。別 BGM が再生中なら停止してから切り替える。</summary>
/// <param name="soundId">再生する BGM の音源 ID。</param>
void SoundManager::PlayBGM(const std::string& soundId)
{
	if (soundId.empty())
	{
		return;
	}

	auto found = soundResources.find(soundId);
	if (found == soundResources.end())
	{
		if (!LoadSoundFromId(soundId, true, true))
		{
			return;
		}

		found = soundResources.find(soundId);
		if (found == soundResources.end())
		{
			return;
		}
	}

	try
	{
		if (!currentBgmId.empty() && currentBgmId != soundId)
		{
			StopBGM();
		}

		std::unique_ptr<DirectX::SoundEffectInstance>& instance = bgmInstances[soundId];
		if (!instance)
		{
			instance = found->second.effect->CreateInstance();
		}

		if (!instance)
		{
			return;
		}

		instance->Stop(true);
		instance->SetVolume(masterVolume * bgmVolume);
		instance->Play(found->second.loop);
		currentBgmId = soundId;
	}
	catch (const std::exception& exception)
	{
		DebugLog("[Sound] PlayBGM failed. Id=", soundId, " Error=", exception.what());
	}
	catch (...)
	{
		DebugLog("[Sound] PlayBGM failed. Id=", soundId, " Unknown exception.");
	}
}

/// <summary>指定 ID の再生中 SE を停止する。通常の one-shot SE は停止管理しないため、現状は将来のループSE用の入口。</summary>
/// <param name="soundId">停止する SE の音源 ID。</param>
void SoundManager::StopSE(const std::string& soundId)
{
	(void)soundId;
}

/// <summary>再生中の SE をすべて停止する。通常の one-shot SE は DirectXTK 側に寿命管理を任せる。</summary>
void SoundManager::StopAllSE()
{
}

/// <summary>現在再生中の BGM を停止する。</summary>
void SoundManager::StopBGM()
{
	try
	{
		if (currentBgmId.empty())
		{
			return;
		}

		auto found = bgmInstances.find(currentBgmId);
		if (found != bgmInstances.end() && found->second)
		{
			found->second->Stop(true);
		}

		currentBgmId.clear();
	}
	catch (...)
	{
		DebugLog("[Sound] StopBGM failed.");
		currentBgmId.clear();
	}
}

/// <summary>指定した音源 ID が読み込み済みか確認する。</summary>
/// <param name="soundId">確認する音源 ID。</param>
/// <returns>読み込み済みなら true。</returns>
bool SoundManager::IsSoundLoaded(const std::string& soundId) const
{
	return soundResources.find(soundId) != soundResources.end();
}

/// <summary>全音声に掛けるマスター音量を設定する。</summary>
/// <param name="volume">0.0 ～ 1.0 の音量値。</param>
void SoundManager::SetMasterVolume(float volume)
{
	masterVolume = ClampVolume(volume);
	ApplyBGMVolume();
}

/// <summary>BGM に掛ける音量を設定する。再生中 BGM には即時反映する。</summary>
/// <param name="volume">0.0 ～ 1.0 の音量値。</param>
void SoundManager::SetBGMVolume(float volume)
{
	bgmVolume = ClampVolume(volume);
	ApplyBGMVolume();
}

/// <summary>SE に掛ける音量を設定する。one-shot SE のため次回再生分から反映する。</summary>
/// <param name="volume">0.0 ～ 1.0 の音量値。</param>
void SoundManager::SetSEVolume(float volume)
{
	seVolume = ClampVolume(volume);
}

/// <summary>現在のマスター音量を取得する。</summary>
/// <returns>0.0 ～ 1.0 の音量値。</returns>
float SoundManager::GetMasterVolume() const
{
	return masterVolume;
}

/// <summary>現在の BGM 音量を取得する。</summary>
/// <returns>0.0 ～ 1.0 の音量値。</returns>
float SoundManager::GetBGMVolume() const
{
	return bgmVolume;
}

/// <summary>現在の SE 音量を取得する。</summary>
/// <returns>0.0 ～ 1.0 の音量値。</returns>
float SoundManager::GetSEVolume() const
{
	return seVolume;
}

/// <summary>音量設定を JSON から読み込む。</summary>
/// <returns>読み込みに成功した場合は true。</returns>
bool SoundManager::LoadAudioSettings()
{
	std::ifstream file(AudioSettingsPath, std::ios::binary);
	if (!file)
	{
		audioSettingsLoaded = true;
		return SaveAudioSettings();
	}

	std::string text(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (text.size() >= 3
		&& static_cast<unsigned char>(text[0]) == 0xEF
		&& static_cast<unsigned char>(text[1]) == 0xBB
		&& static_cast<unsigned char>(text[2]) == 0xBF)
	{
		text.erase(0, 3);
	}

	JsonValue root;
	std::string error;
	if (!JsonParser::Parse(text, root, error) || !root.IsObject())
	{
		DebugLog("[Sound] AudioSettings load failed. ", error);
		audioSettingsLoaded = true;
		return false;
	}

	masterVolume = ClampVolume(GetFloat(root, "masterVolume", masterVolume));
	bgmVolume = ClampVolume(GetFloat(root, "bgmVolume", bgmVolume));
	seVolume = ClampVolume(GetFloat(root, "seVolume", seVolume));
	audioSettingsLoaded = true;
	ApplyBGMVolume();
	return true;
}

/// <summary>現在の音量設定を JSON へ保存する。</summary>
/// <returns>保存に成功した場合は true。</returns>
bool SoundManager::SaveAudioSettings() const
{
	const std::filesystem::path savePath(AudioSettingsPath);
	std::error_code errorCode;
	std::filesystem::create_directories(savePath.parent_path(), errorCode);
	if (errorCode)
	{
		DebugLog("[Sound] AudioSettings directory creation failed. Path=", savePath.parent_path().string());
		return false;
	}

	std::ostringstream json;
	json << std::fixed << std::setprecision(3);
	json << "{\n";
	json << "  \"masterVolume\": " << masterVolume << ",\n";
	json << "  \"bgmVolume\": " << bgmVolume << ",\n";
	json << "  \"seVolume\": " << seVolume << "\n";
	json << "}\n";

	std::ofstream file(savePath, std::ios::binary);
	if (!file)
	{
		DebugLog("[Sound] AudioSettings save failed. Path=", savePath.string());
		return false;
	}

	const unsigned char utf8Bom[] = { 0xEF, 0xBB, 0xBF };
	file.write(reinterpret_cast<const char*>(utf8Bom), sizeof(utf8Bom));
	const std::string text = json.str();
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return true;
}

/// <summary>指定パスの wav を音源 ID に紐づけて読み込む。</summary>
/// <param name="soundId">再生時に指定する音源 ID。</param>
/// <param name="path">wav ファイルパス。</param>
/// <param name="loop">ループ再生する音源なら true。</param>
/// <param name="bgm">BGM として扱う音源なら true。</param>
/// <returns>読み込みに成功した場合は true。</returns>
bool SoundManager::LoadSound(const std::string& soundId, const std::string& path, bool loop, bool bgm)
{
	if (soundId.empty())
	{
		return false;
	}

	if (!Initialize())
	{
		return false;
	}

	auto found = soundResources.find(soundId);
	if (found != soundResources.end())
	{
		found->second.loop = loop;
		found->second.bgm = bgm;
		return true;
	}

	std::filesystem::path resolvedPath;
	if (!ResolveSoundPath(std::filesystem::path(path), resolvedPath))
	{
		DebugLog("[Sound] File not found. Id=", soundId, " Path=", path);
		return false;
	}

	try
	{
		SoundResource resource;
		resource.effect = std::make_unique<DirectX::SoundEffect>(audioEngine.get(), ToWideString(resolvedPath.string()).c_str());
		resource.loop = loop;
		resource.bgm = bgm;
		soundResources.emplace(soundId, std::move(resource));
		return true;
	}
	catch (const std::exception& exception)
	{
		DebugLog("[Sound] Load failed. Id=", soundId, " Path=", resolvedPath.string(), " Error=", exception.what());
		return false;
	}
	catch (...)
	{
		DebugLog("[Sound] Load failed. Id=", soundId, " Path=", resolvedPath.string(), " Unknown exception.");
		return false;
	}
}

/// <summary>音源 ID から assets/Sound 配下の wav パスを組み立てて読み込みを試す。</summary>
/// <param name="soundId">読み込みたい音源 ID。</param>
/// <param name="loop">読み込み後のループ設定。</param>
/// <param name="bgm">BGM として扱う音源なら true。</param>
/// <returns>読み込みに成功した場合は true。</returns>
bool SoundManager::LoadSoundFromId(const std::string& soundId, bool loop, bool bgm)
{
	if (soundId.empty())
	{
		return false;
	}

	return LoadSound(soundId, soundId, loop, bgm);
}

/// <summary>BGM インスタンスに現在のマスター音量と BGM 音量を反映する。</summary>
void SoundManager::ApplyBGMVolume()
{
	if (currentBgmId.empty())
	{
		return;
	}

	auto found = bgmInstances.find(currentBgmId);
	if (found == bgmInstances.end() || !found->second)
	{
		return;
	}

	try
	{
		found->second->SetVolume(masterVolume * bgmVolume);
	}
	catch (...)
	{
		DebugLog("[Sound] ApplyBGMVolume failed.");
	}
}

/// <summary>外部から来た音量値を 0.0 ～ 1.0 に丸める。</summary>
/// <param name="volume">丸める音量値。</param>
/// <returns>0.0 ～ 1.0 に丸めた値。</returns>
float SoundManager::ClampVolume(float volume)
{
	if (volume < 0.0f)
	{
		return 0.0f;
	}
	if (volume > 1.0f)
	{
		return 1.0f;
	}

	return volume;
}
