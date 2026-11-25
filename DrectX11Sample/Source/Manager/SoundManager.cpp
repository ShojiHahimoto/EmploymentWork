#include "Manager/SoundManager.h"

#include "Data/JsonValue.h"
#include "Core/Debugger.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"

#include <Audio.h>
#include <Windows.h>

#include <exception>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{
	constexpr const char* AudioSettingsPath = "assets/Settings/AudioSettings.json";

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
/// <param name="loader">音量設定の読み込みを担当するワーカー。</param>
/// <param name="writer">音量設定の保存を担当するワーカー。</param>
/// <returns>初期化済み、または初期化に成功した場合は true。</returns>
bool SoundManager::Initialize(LoadWorker &loader, SaveWorker &writer)
{
	if (audioEngine)
	{
		return true;
	}

	try
	{
		audioEngine = std::make_unique<DirectX::AudioEngine>();
		loadWorker = &loader;
		saveWorker = &writer;
		LoadAudioSettings();
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
	ApplySettingsResults();

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
	loadWorker = nullptr;
	saveWorker = nullptr;
	pendingSettingsLoad = {};
	pendingSettingsSave = {};

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

/// <summary>準備済みの指定IDのSEを再生する。</summary>
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
		return;
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
		return;
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

/// <summary>音量設定の読み込みを依頼する。</summary>
void SoundManager::LoadAudioSettings()
{
	if (loadWorker && !pendingSettingsLoad.valid() && !pendingSettingsSave.valid())
	{
		pendingSettingsLoad = loadWorker->Enqueue([settings = AudioSettings{masterVolume, bgmVolume, seVolume}]
		{
			return ReadAudioSettings(settings);
		});
	}
}

/// <summary>音量設定の保存を依頼する。</summary>
void SoundManager::SaveAudioSettings()
{
	if (saveWorker && !pendingSettingsLoad.valid() && !pendingSettingsSave.valid())
	{
		pendingSettingsSave = saveWorker->Enqueue([settings = AudioSettings{masterVolume, bgmVolume, seVolume}]
		{
			return WriteAudioSettings(settings);
		});
	}
}

/// <summary>音量設定を JSON から読み込む。</summary>
/// <param name="settings">ファイルに記載がない項目の既定値。</param>
/// <returns>読み込み結果。失敗時は空。</returns>
std::optional<SoundManager::AudioSettings> SoundManager::ReadAudioSettings(AudioSettings settings)
{
	std::ifstream file(AudioSettingsPath, std::ios::binary);
	if (!file)
	{
		return settings;
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
		return std::nullopt;
	}

	settings.master = ClampVolume(GetFloat(root, "masterVolume", settings.master));
	settings.bgm = ClampVolume(GetFloat(root, "bgmVolume", settings.bgm));
	settings.se = ClampVolume(GetFloat(root, "seVolume", settings.se));

	return settings;
}

/// <summary>音量設定を JSON へ保存する。</summary>
/// <param name="settings">保存ボタンを押した時の設定。</param>
/// <returns>保存に成功した場合は true。</returns>
bool SoundManager::WriteAudioSettings(const AudioSettings &settings)
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
	json << "  \"masterVolume\": " << settings.master << ",\n";
	json << "  \"bgmVolume\": " << settings.bgm << ",\n";
	json << "  \"seVolume\": " << settings.se << "\n";
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
	file.close();

	return !file.fail();
}

/// <summary>ワーカーの結果をメインスレッドで受け取る。</summary>
void SoundManager::ApplySettingsResults()
{
	const auto ready = [](const auto &future)
	{
		return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	};

	try
	{
		if (ready(pendingSettingsLoad))
		{
			if (const auto settings = pendingSettingsLoad.get())
			{
				masterVolume = settings->master;
				bgmVolume = settings->bgm;
				seVolume = settings->se;
				ApplyBGMVolume();
			}
		}

		if (ready(pendingSettingsSave) && !pendingSettingsSave.get())
		{
			DebugLog("[Sound] Audio settings save failed.");
		}
	}
	catch (const std::exception &error)
	{
		DebugLog("[Sound] Audio settings job failed: ", error.what());
	}
}

/// <summary>RIFF/WAVEのPCMデータをCPUメモリへ読み込む。</summary>
/// <param name="path">音源へのパス。既存のassets/Sound基準のパス解決も行う。</param>
/// <returns>読み込み済み音源。欠落または未対応形式の場合は空。</returns>
SoundManager::PreparedSound SoundManager::PrepareSound(const std::string &path)
{
	std::filesystem::path resolved;
	if (!ResolveSoundPath(path, resolved))
	{
		DebugLog("[Sound] WAV not found: ", path);

		return {};
	}

	std::ifstream input(resolved, std::ios::binary);
	std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
	if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
		std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
	{
		return {};
	}

	WAVEFORMATEX format{};
	std::size_t sampleOffset = 0;
	std::size_t sampleSize = 0;
	bool hasFormat = false;

	for (std::size_t offset = 12; bytes.size() - offset >= 8;)
	{
		std::uint32_t size = 0;
		std::memcpy(&size, bytes.data() + offset + 4, sizeof(size));
		const std::size_t start = offset + 8;
		if (size > bytes.size() - start)
		{
			return {};
		}

		if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && size >= 16)
		{
			std::memcpy(&format, bytes.data() + start, 16);
			hasFormat = true;
		}
		else if (std::memcmp(bytes.data() + offset, "data", 4) == 0)
		{
			sampleOffset = start;
			sampleSize = size;
		}

		offset = start + size;
		if ((size & 1) && offset < bytes.size())
		{
			++offset;
		}
	}

	if (!hasFormat || format.wFormatTag != WAVE_FORMAT_PCM || !format.nBlockAlign ||
		!sampleSize || sampleSize % format.nBlockAlign != 0)
	{
		DebugLog("[Sound] Invalid or unsupported PCM WAV: ", path);

		return {};
	}

	PreparedSound sound;
	sound.audioOffset = sizeof(format);
	sound.audioBytes = sampleSize;
	sound.data = std::make_unique<std::uint8_t[]>(sound.audioOffset + sampleSize);
	std::memcpy(sound.data.get(), &format, sizeof(format));
	std::memcpy(sound.data.get() + sound.audioOffset, bytes.data() + sampleOffset, sampleSize);

	return sound;
}

/// <summary>メインスレッドでデコード済み音源を再生エンジンへ登録する。</summary>
/// <param name="soundId">再生時に参照するID。</param>
/// <param name="sound">準備済みPCMデータ。登録時に所有権を移す。</param>
/// <param name="loop">ループ再生する場合はtrue。</param>
/// <param name="bgm">BGMとして登録する場合はtrue。</param>
/// <returns>登録済みまたは登録に成功した場合はtrue。</returns>
bool SoundManager::RegisterSound(const std::string &soundId, PreparedSound sound, bool loop, bool bgm)
{
	if (soundResources.contains(soundId))
	{
		return true;
	}

	if (!audioEngine || !sound.data)
	{
		return false;
	}

	try
	{
		const auto *format = reinterpret_cast<const WAVEFORMATEX *>(sound.data.get());
		const auto *samples = sound.data.get() + sound.audioOffset;
		SoundResource resource;
		resource.effect = std::make_unique<DirectX::SoundEffect>(audioEngine.get(), sound.data, format, samples,
															 sound.audioBytes);
		resource.loop = loop;
		resource.bgm = bgm;
		soundResources.emplace(soundId, std::move(resource));

		return true;
	}
	catch (const std::exception &error)
	{
		DebugLog("[Sound] Registration failed: ", soundId, " ", error.what());

		return false;
	}
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
