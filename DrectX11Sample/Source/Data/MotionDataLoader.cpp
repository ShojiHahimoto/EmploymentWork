#include "Data/MotionDataLoader.h"

#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <DirectXMath.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* MotionDataRootPath = "assets/MotionData";

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
	/// MotionData ID から読み込み対象 JSON の実パスを解決する。
	/// </summary>
	/// <param name="motionDataId">MotionData ID、または assets/MotionData からの相対パス。</param>
	/// <param name="outPath">見つかった JSON パスの書き込み先。</param>
	/// <returns>読み込み対象ファイルが見つかった場合は true。</returns>
	bool ResolveMotionDataPath(const std::string& motionDataId, std::filesystem::path& outPath)
	{
		if (motionDataId.empty())
		{
			return false;
		}

		const std::filesystem::path requestedPath = WithJsonExtension(std::filesystem::path(motionDataId));
		const std::filesystem::path rootPath(MotionDataRootPath);
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
			DebugLog("[MotionData] JSON file not found: ", path.string());
			return false;
		}
		RemoveUtf8Bom(text);

		std::string error;
		if (!JsonParser::Parse(text, outValue, error))
		{
			DebugLog("[MotionData] JSON parse failed: ", path.string(), " Error=", error);
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
	/// x/y/z を持つ JSON Object から Vector3 を取得する。
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
	/// Euler degree の JSON Object を Quaternion へ変換する。
	/// </summary>
	/// <param name="object">x/y/z degree を持つ JSON Object。</param>
	/// <returns>変換した Quaternion。</returns>
	Quaternion GetEulerDegreesAsQuaternion(const JsonValue& object)
	{
		const Vector3 eulerDegrees = GetVector3(object, Vector3::Zero);
		Quaternion rotation = Quaternion::CreateFromYawPitchRoll(
			XMConvertToRadians(eulerDegrees.y),
			XMConvertToRadians(eulerDegrees.x),
			XMConvertToRadians(eulerDegrees.z));
		rotation.Normalize();
		return rotation;
	}

	/// <summary>
	/// 1 キーフレーム分の姿勢データを JSON から読み込む。
	/// </summary>
	/// <param name="keyframeValue">読み込む JSON Object。</param>
	/// <param name="outKeyframe">読み込んだ値の書き込み先。</param>
	/// <returns>最低限 frame を読めた場合は true。</returns>
	bool LoadKeyframeFromJson(const JsonValue& keyframeValue, MotionBoneKeyframeData& outKeyframe)
	{
		if (!keyframeValue.IsObject())
		{
			return false;
		}

		outKeyframe.frame = std::max(0, GetInt(keyframeValue, "frame", outKeyframe.frame));

		const JsonValue* position = keyframeValue.Find("position");
		if (position && position->IsObject())
		{
			outKeyframe.hasPosition = true;
			outKeyframe.localPosition = GetVector3(*position, outKeyframe.localPosition);
		}

		const JsonValue* rotation = keyframeValue.Find("rotationEulerDegrees");
		if (rotation && rotation->IsObject())
		{
			outKeyframe.hasRotation = true;
			outKeyframe.localRotationEulerDegrees = GetVector3(*rotation, outKeyframe.localRotationEulerDegrees);
			outKeyframe.localRotation = GetEulerDegreesAsQuaternion(*rotation);
		}

		const JsonValue* scale = keyframeValue.Find("scale");
		if (scale && scale->IsObject())
		{
			outKeyframe.hasScale = true;
			outKeyframe.localScale = GetVector3(*scale, outKeyframe.localScale);
		}

		return true;
	}

	/// <summary>
	/// 1 ボーントラック分のキーフレーム配列を JSON から読み込む。
	/// </summary>
	/// <param name="trackValue">読み込む JSON Object。</param>
	/// <param name="outTrack">読み込んだ値の書き込み先。</param>
	/// <returns>ボーン名とキーフレームを読めた場合は true。</returns>
	bool LoadBoneTrackFromJson(const JsonValue& trackValue, MotionBoneTrackData& outTrack)
	{
		if (!trackValue.IsObject())
		{
			return false;
		}

		outTrack.boneName = GetString(trackValue, "boneName", "");
		if (outTrack.boneName.empty())
		{
			return false;
		}

		const JsonValue* keyframes = trackValue.Find("keyframes");
		if (!keyframes || !keyframes->IsArray())
		{
			return false;
		}

		outTrack.keyframes.clear();
		for (const JsonValue& keyframeValue : keyframes->AsArray())
		{
			MotionBoneKeyframeData keyframe;
			if (LoadKeyframeFromJson(keyframeValue, keyframe))
			{
				outTrack.keyframes.push_back(keyframe);
			}
		}

		std::sort(
			outTrack.keyframes.begin(),
			outTrack.keyframes.end(),
			[](const MotionBoneKeyframeData& left, const MotionBoneKeyframeData& right)
			{
				return left.frame < right.frame;
			});

		return !outTrack.keyframes.empty();
	}
}

bool MotionDataLoader::LoadMotionData(const std::string& motionDataId, MotionData& outMotionData)
{
	std::filesystem::path motionPath;
	if (!ResolveMotionDataPath(motionDataId, motionPath))
	{
		DebugLog("[MotionData] Resolve failed. Id=", motionDataId);
		return false;
	}

	JsonValue root;
	if (!ReadJsonFile(motionPath, root) || !root.IsObject())
	{
		return false;
	}

	outMotionData = MotionData();
	outMotionData.motionDataId = GetString(root, "motionDataId", motionDataId);
	outMotionData.displayName = GetString(root, "displayName", outMotionData.motionDataId);
	outMotionData.totalFrames = std::max(1, GetInt(root, "totalFrames", outMotionData.totalFrames));
	outMotionData.looping = GetBool(root, "looping", outMotionData.looping);

	const JsonValue* boneTracks = root.Find("boneTracks");
	if (boneTracks && boneTracks->IsArray())
	{
		for (const JsonValue& trackValue : boneTracks->AsArray())
		{
			MotionBoneTrackData track;
			if (LoadBoneTrackFromJson(trackValue, track))
			{
				outMotionData.boneTracks.push_back(track);
			}
		}
	}

	DebugLog(
		"[MotionData] Load result: ",
		outMotionData.boneTracks.empty() ? "empty" : "success",
		" Id=",
		outMotionData.motionDataId,
		" Tracks=",
		outMotionData.boneTracks.size());

	return true;
}

std::unordered_map<std::string, std::unique_ptr<MotionData>> MotionDataManager::resources;

bool MotionDataManager::LoadMotionData(const std::string& motionDataId)
{
	if (motionDataId.empty())
	{
		return false;
	}

	if (resources.find(motionDataId) != resources.end())
	{
		return true;
	}

	std::unique_ptr<MotionData> motionData = std::make_unique<MotionData>();
	if (!MotionDataLoader::LoadMotionData(motionDataId, *motionData))
	{
		return false;
	}

	resources[motionDataId] = std::move(motionData);
	return true;
}

const MotionData* MotionDataManager::GetMotionData(const std::string& motionDataId)
{
	const auto found = resources.find(motionDataId);
	return found != resources.end() ? found->second.get() : nullptr;
}

void MotionDataManager::UnloadAll()
{
	resources.clear();
}
