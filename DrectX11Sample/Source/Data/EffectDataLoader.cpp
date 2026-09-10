#include "Data/EffectDataLoader.h"

#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* EffectDataRootPath = "assets/EffectData";

	/// <summary>
	/// パス末尾が .json でない場合だけ .json を補う。
	/// </summary>
	/// <param name="path">確認するパス。</param>
	/// <returns>.json 拡張子を持つパス。</returns>
	std::filesystem::path WithJsonExtension(const std::filesystem::path& path)
	{
		std::filesystem::path result = path;
		if (result.extension() != ".json")
		{
			result += ".json";
		}

		return result;
	}

	/// <summary>
	/// EffectData ID から読み込み対象 JSON の実パスを解決する。
	/// </summary>
	/// <param name="effectDataId">EffectData ID、または assets/EffectData からの相対パス。</param>
	/// <param name="outPath">見つかった JSON パスの書き込み先。</param>
	/// <returns>読み込み対象ファイルが見つかった場合は true。</returns>
	bool ResolveEffectDataPath(const std::string& effectDataId, std::filesystem::path& outPath)
	{
		if (effectDataId.empty())
		{
			return false;
		}

		const std::filesystem::path requestedPath = WithJsonExtension(std::filesystem::path(effectDataId));
		const std::filesystem::path rootPath(EffectDataRootPath);
		const std::vector<std::filesystem::path> candidatePaths =
		{
			requestedPath,
			rootPath / requestedPath,
			std::filesystem::path("DrectX11Sample") / rootPath / requestedPath,
			std::filesystem::path("../../DrectX11Sample") / rootPath / requestedPath,
		};

		for (const std::filesystem::path& candidatePath : candidatePaths)
		{
			std::error_code errorCode;
			if (std::filesystem::is_regular_file(candidatePath, errorCode))
			{
				outPath = candidatePath;
				return true;
			}
		}

		return false;
	}

	/// <summary>
	/// テキストファイルを読み込み、文字列として返す。
	/// </summary>
	/// <param name="path">読み込むファイルパス。</param>
	/// <param name="outText">読み込んだ文字列の書き込み先。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool ReadTextFile(const std::filesystem::path& path, std::string& outText)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return false;
		}

		std::ostringstream stream;
		stream << file.rdbuf();
		outText = stream.str();
		return true;
	}

	/// <summary>
	/// UTF-8 BOM 付き JSON を JsonParser が扱えるよう、先頭の BOM だけを取り除く。
	/// </summary>
	/// <param name="text">読み込んだテキスト。BOM がある場合はこの関数内で削除される。</param>
	void RemoveUtf8Bom(std::string& text)
	{
		constexpr unsigned char Bom0 = 0xEF;
		constexpr unsigned char Bom1 = 0xBB;
		constexpr unsigned char Bom2 = 0xBF;
		if (text.size() >= 3
			&& static_cast<unsigned char>(text[0]) == Bom0
			&& static_cast<unsigned char>(text[1]) == Bom1
			&& static_cast<unsigned char>(text[2]) == Bom2)
		{
			text.erase(0, 3);
		}
	}

	/// <summary>
	/// JSON ファイルを読み込み、JsonValue として解析する。
	/// </summary>
	/// <param name="path">読み込む JSON ファイルパス。</param>
	/// <param name="outValue">解析結果の書き込み先。</param>
	/// <returns>読み込みと解析に成功した場合は true。</returns>
	bool ReadJsonFile(const std::filesystem::path& path, JsonValue& outValue)
	{
		std::string text;
		if (!ReadTextFile(path, text))
		{
			DebugLog("[EffectData] JSON file not found: ", path.string());
			return false;
		}
		RemoveUtf8Bom(text);

		std::string error;
		if (!JsonParser::Parse(text, outValue, error))
		{
			DebugLog("[EffectData] JSON parse failed: ", path.string(), " Error=", error);
			return false;
		}

		return true;
	}

	/// <summary>
	/// Object から文字列を取得し、存在しない場合は既定値を返す。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="key">取得するキー。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した文字列または既定値。</returns>
	std::string GetString(const JsonValue& object, const std::string& key, const std::string& defaultValue)
	{
		const JsonValue* value = object.Find(key);
		return value && value->IsString() ? value->AsString() : defaultValue;
	}

	/// <summary>
	/// Object から int を取得し、存在しない場合は既定値を返す。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="key">取得するキー。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した int 値または既定値。</returns>
	int GetInt(const JsonValue& object, const std::string& key, int defaultValue)
	{
		const JsonValue* value = object.Find(key);
		return value && value->IsNumber() ? static_cast<int>(value->AsNumber(defaultValue)) : defaultValue;
	}

	/// <summary>
	/// Object から float を取得し、存在しない場合は既定値を返す。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="key">取得するキー。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した float 値または既定値。</returns>
	float GetFloat(const JsonValue& object, const std::string& key, float defaultValue)
	{
		const JsonValue* value = object.Find(key);
		return value && value->IsNumber() ? static_cast<float>(value->AsNumber(defaultValue)) : defaultValue;
	}

	/// <summary>
	/// Object から bool を取得し、存在しない場合は既定値を返す。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="key">取得するキー。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した bool 値または既定値。</returns>
	bool GetBool(const JsonValue& object, const std::string& key, bool defaultValue)
	{
		const JsonValue* value = object.Find(key);
		return value && value->IsBool() ? value->AsBool(defaultValue) : defaultValue;
	}

	/// <summary>
	/// x / y を持つ JSON Object から Vector2 を取得する。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した Vector2 または既定値。</returns>
	Vector2 GetVector2(const JsonValue& object, const Vector2& defaultValue)
	{
		return Vector2(
			GetFloat(object, "x", defaultValue.x),
			GetFloat(object, "y", defaultValue.y));
	}

	/// <summary>
	/// x / y / z を持つ JSON Object から Vector3 を取得する。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した Vector3 または既定値。</returns>
	Vector3 GetVector3(const JsonValue& object, const Vector3& defaultValue)
	{
		return Vector3(
			GetFloat(object, "x", defaultValue.x),
			GetFloat(object, "y", defaultValue.y),
			GetFloat(object, "z", defaultValue.z));
	}

	/// <summary>
	/// r / g / b / a を持つ JSON Object から Color を取得する。
	/// </summary>
	/// <param name="object">参照する JSON Object。</param>
	/// <param name="defaultValue">キーがない場合の既定値。</param>
	/// <returns>取得した Color または既定値。</returns>
	Color GetColor(const JsonValue& object, const Color& defaultValue)
	{
		return Color(
			GetFloat(object, "r", defaultValue.x),
			GetFloat(object, "g", defaultValue.y),
			GetFloat(object, "b", defaultValue.z),
			GetFloat(object, "a", defaultValue.w));
	}

	/// <summary>
	/// JSON の blendMode 文字列を EffectBlendMode に変換する。
	/// </summary>
	/// <param name="text">Alpha / Additive などの文字列。</param>
	/// <returns>対応する EffectBlendMode。不明な場合は Unknown。</returns>
	EffectBlendMode ParseEffectBlendMode(const std::string& text)
	{
		if (text == "Alpha")
		{
			return EffectBlendMode::Alpha;
		}
		if (text == "Additive")
		{
			return EffectBlendMode::Additive;
		}
		return EffectBlendMode::Unknown;
	}

	/// <summary>
	/// JSON Object から 1 エミッター分の設定を読み込む。
	/// </summary>
	/// <param name="value">読み込む JSON Object。</param>
	/// <param name="outEmitter">読み込み結果の書き込み先。</param>
	/// <returns>読み込み可能な Object なら true。</returns>
	bool LoadEmitterFromJson(const JsonValue& value, EffectEmitterData& outEmitter)
	{
		if (!value.IsObject())
		{
			return false;
		}

		outEmitter.startFrame = std::max(0, GetInt(value, "startFrame", outEmitter.startFrame));
		outEmitter.emitCount = std::max(0, GetInt(value, "emitCount", outEmitter.emitCount));
		outEmitter.particleLifeFrames = std::max(1, GetInt(value, "particleLifeFrames", outEmitter.particleLifeFrames));
		outEmitter.texturePath = GetString(value, "texturePath", outEmitter.texturePath);
		outEmitter.blendMode = ParseEffectBlendMode(GetString(value, "blendMode", "Additive"));
		if (outEmitter.blendMode == EffectBlendMode::Unknown)
		{
			outEmitter.blendMode = EffectBlendMode::Additive;
		}
		outEmitter.depthEnabled = GetBool(value, "depthEnabled", outEmitter.depthEnabled);
		outEmitter.startScale = GetFloat(value, "startScale", outEmitter.startScale);
		outEmitter.endScale = GetFloat(value, "endScale", outEmitter.endScale);
		outEmitter.angularVelocityMin = GetFloat(value, "angularVelocityMin", outEmitter.angularVelocityMin);
		outEmitter.angularVelocityMax = GetFloat(value, "angularVelocityMax", outEmitter.angularVelocityMax);

		const JsonValue* baseOffset = value.Find("baseOffset");
		if (baseOffset && baseOffset->IsObject())
		{
			outEmitter.baseOffset = GetVector3(*baseOffset, outEmitter.baseOffset);
		}

		const JsonValue* spawnRange = value.Find("spawnRange");
		if (spawnRange && spawnRange->IsObject())
		{
			outEmitter.spawnRange = GetVector2(*spawnRange, outEmitter.spawnRange);
		}

		const JsonValue* velocityMin = value.Find("velocityMin");
		if (velocityMin && velocityMin->IsObject())
		{
			outEmitter.velocityMin = GetVector2(*velocityMin, outEmitter.velocityMin);
		}

		const JsonValue* velocityMax = value.Find("velocityMax");
		if (velocityMax && velocityMax->IsObject())
		{
			outEmitter.velocityMax = GetVector2(*velocityMax, outEmitter.velocityMax);
		}

		const JsonValue* acceleration = value.Find("acceleration");
		if (acceleration && acceleration->IsObject())
		{
			outEmitter.acceleration = GetVector2(*acceleration, outEmitter.acceleration);
		}

		const JsonValue* startColor = value.Find("startColor");
		if (startColor && startColor->IsObject())
		{
			outEmitter.startColor = GetColor(*startColor, outEmitter.startColor);
		}

		const JsonValue* endColor = value.Find("endColor");
		if (endColor && endColor->IsObject())
		{
			outEmitter.endColor = GetColor(*endColor, outEmitter.endColor);
		}

		return true;
	}
}

bool EffectDataLoader::LoadEffectData(const std::string& effectDataId, EffectData& outEffectData)
{
	std::filesystem::path effectPath;
	if (!ResolveEffectDataPath(effectDataId, effectPath))
	{
		DebugLog("[EffectData] Resolve failed. Id=", effectDataId);
		return false;
	}

	JsonValue root;
	if (!ReadJsonFile(effectPath, root) || !root.IsObject())
	{
		return false;
	}

	outEffectData = EffectData();
	outEffectData.effectDataId = GetString(root, "effectDataId", effectDataId);
	outEffectData.displayName = GetString(root, "displayName", outEffectData.effectDataId);
	outEffectData.totalFrames = std::max(1, GetInt(root, "totalFrames", outEffectData.totalFrames));
	outEffectData.loop = GetBool(root, "loop", outEffectData.loop);

	const JsonValue* emitters = root.Find("emitters");
	if (emitters && emitters->IsArray())
	{
		for (const JsonValue& emitterValue : emitters->AsArray())
		{
			EffectEmitterData emitter;
			if (LoadEmitterFromJson(emitterValue, emitter))
			{
				outEffectData.emitters.push_back(emitter);
			}
		}
	}

	DebugLog(
		"[EffectData] Load result. Id=",
		outEffectData.effectDataId,
		" Emitters=",
		outEffectData.emitters.size());

	return true;
}

std::unordered_map<std::string, std::unique_ptr<EffectData>> EffectDataManager::resources;

bool EffectDataManager::LoadEffectData(const std::string& effectDataId)
{
	if (effectDataId.empty())
	{
		return false;
	}

	if (resources.find(effectDataId) != resources.end())
	{
		return true;
	}

	std::unique_ptr<EffectData> effectData = std::make_unique<EffectData>();
	if (!EffectDataLoader::LoadEffectData(effectDataId, *effectData))
	{
		return false;
	}

	resources[effectDataId] = std::move(effectData);
	return true;
}

const EffectData* EffectDataManager::GetEffectData(const std::string& effectDataId)
{
	const auto found = resources.find(effectDataId);
	return found != resources.end() ? found->second.get() : nullptr;
}

void EffectDataManager::UnloadAll()
{
	resources.clear();
}
