#include "Data/PosePreset.h"

#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* PosePresetRootPath = "assets/PosePreset";

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
	/// プリセット ID から読み書き対象 JSON の標準パスを作る。
	/// </summary>
	/// <param name="presetId">プリセット ID。</param>
	/// <returns>assets/PosePreset 配下の JSON パス。</returns>
	std::filesystem::path CreatePresetPath(const std::string& presetId)
	{
		return std::filesystem::path(PosePresetRootPath) / WithJsonExtension(std::filesystem::path(presetId));
	}

	/// <summary>
	/// プリセット ID から読み込み対象 JSON の実パスを解決する。
	/// </summary>
	/// <param name="presetId">プリセット ID。</param>
	/// <param name="outPath">見つかった JSON パスの書き込み先。</param>
	/// <returns>読み込み対象ファイルが見つかった場合は true。</returns>
	bool ResolvePresetPath(const std::string& presetId, std::filesystem::path& outPath)
	{
		if (presetId.empty())
		{
			return false;
		}

		const std::filesystem::path requestedPath = WithJsonExtension(std::filesystem::path(presetId));
		const std::filesystem::path rootPath(PosePresetRootPath);
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
	/// 読み込み候補ディレクトリのうち最初に存在するパスを取得する。
	/// </summary>
	/// <param name="outPath">見つかったディレクトリの書き込み先。</param>
	/// <returns>見つかった場合は true。</returns>
	bool FindPresetDirectory(std::filesystem::path& outPath)
	{
		const std::vector<std::filesystem::path> rootCandidates =
		{
			std::filesystem::path(PosePresetRootPath),
			std::filesystem::path("DrectX11Sample") / PosePresetRootPath,
			std::filesystem::path("../../DrectX11Sample") / PosePresetRootPath,
		};

		for (const std::filesystem::path& rootPath : rootCandidates)
		{
			std::error_code errorCode;
			if (std::filesystem::is_directory(rootPath, errorCode))
			{
				outPath = rootPath;
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
			DebugLog("[PosePreset] JSON file not found: ", path.string());
			return false;
		}
		RemoveUtf8Bom(text);

		std::string error;
		if (!JsonParser::Parse(text, outValue, error))
		{
			DebugLog("[PosePreset] JSON parse failed: ", path.string(), " Error=", error);
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
	/// JSON 文字列として安全に保存できるよう、最低限必要な文字をエスケープする。
	/// </summary>
	/// <param name="text">保存する元文字列。</param>
	/// <returns>JSON 文字列内に書けるエスケープ済み文字列。</returns>
	std::string EscapeJsonString(const std::string& text)
	{
		std::ostringstream escaped;
		for (char character : text)
		{
			switch (character)
			{
			case '\\':
				escaped << "\\\\";
				break;
			case '"':
				escaped << "\\\"";
				break;
			case '\n':
				escaped << "\\n";
				break;
			case '\r':
				escaped << "\\r";
				break;
			case '\t':
				escaped << "\\t";
				break;
			default:
				escaped << character;
				break;
			}
		}

		return escaped.str();
	}

	/// <summary>
	/// Vector3 を JSON の { x, y, z } 形式で書き込む。
	/// </summary>
	/// <param name="stream">書き込み先ストリーム。</param>
	/// <param name="value">保存する Vector3。</param>
	void WriteVector3(std::ostringstream& stream, const Vector3& value)
	{
		stream << "{ \"x\": " << value.x << ", \"y\": " << value.y << ", \"z\": " << value.z << " }";
	}
}

std::vector<std::string> PosePresetStore::ListPresetIds()
{
	std::vector<std::string> presetIds;

	std::filesystem::path rootPath;
	if (!FindPresetDirectory(rootPath))
	{
		return presetIds;
	}

	std::error_code errorCode;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(rootPath, errorCode))
	{
		if (errorCode)
		{
			break;
		}
		if (!entry.is_regular_file(errorCode) || entry.path().extension() != ".json")
		{
			continue;
		}

		// 旧実装で日本語ファイル名が文字化けしている場合も、既存データ読み込み互換のため stem を ID として保持する。
		presetIds.push_back(entry.path().stem().string());
	}

	std::sort(presetIds.begin(), presetIds.end());
	presetIds.erase(std::unique(presetIds.begin(), presetIds.end()), presetIds.end());
	return presetIds;
}

bool PosePresetStore::LoadPreset(const std::string& presetId, PosePresetData& outPreset)
{
	std::filesystem::path presetPath;
	if (!ResolvePresetPath(presetId, presetPath))
	{
		DebugLog("[PosePreset] Resolve failed. Id=", presetId);
		return false;
	}

	JsonValue root;
	if (!ReadJsonFile(presetPath, root) || !root.IsObject())
	{
		return false;
	}

	outPreset = PosePresetData();
	outPreset.presetId = GetString(root, "presetId", presetId);
	outPreset.displayName = GetString(root, "displayName", outPreset.presetId);

	const JsonValue* bones = root.Find("bones");
	if (bones && bones->IsArray())
	{
		for (const JsonValue& boneValue : bones->AsArray())
		{
			if (!boneValue.IsObject())
			{
				continue;
			}

			PosePresetBoneData bone;
			bone.boneName = GetString(boneValue, "boneName", "");
			const JsonValue* rotation = boneValue.Find("rotationEulerDegrees");
			if (!bone.boneName.empty() && rotation && rotation->IsObject())
			{
				bone.localRotationEulerDegrees = GetVector3(*rotation, Vector3::Zero);
				outPreset.bones.push_back(bone);
			}
		}
	}

	return !outPreset.bones.empty();
}

bool PosePresetStore::SavePreset(const std::string& presetId, const PosePresetData& preset)
{
	if (!IsValidPresetId(presetId))
	{
		DebugLog("[PosePreset] Save failed. Invalid presetId=", presetId);
		return false;
	}

	const std::filesystem::path savePath = CreatePresetPath(presetId);
	std::error_code errorCode;
	std::filesystem::create_directories(savePath.parent_path(), errorCode);
	if (errorCode)
	{
		DebugLog("[PosePreset] Directory creation failed. Path=", savePath.parent_path().string());
		return false;
	}

	std::ostringstream json;
	json << std::fixed << std::setprecision(3);
	json << "{\n";
	json << "  \"presetId\": \"" << EscapeJsonString(presetId) << "\",\n";
	json << "  \"displayName\": \"" << EscapeJsonString(preset.displayName.empty() ? presetId : preset.displayName) << "\",\n";
	json << "  \"bones\": [";
	for (size_t boneIndex = 0; boneIndex < preset.bones.size(); ++boneIndex)
	{
		const PosePresetBoneData& bone = preset.bones[boneIndex];
		json << (boneIndex == 0 ? "\n" : ",\n");
		json << "    {\n";
		json << "      \"boneName\": \"" << EscapeJsonString(bone.boneName) << "\",\n";
		json << "      \"rotationEulerDegrees\": ";
		WriteVector3(json, bone.localRotationEulerDegrees);
		json << "\n";
		json << "    }";
	}
	json << (preset.bones.empty() ? "]\n" : "\n  ]\n");
	json << "}\n";

	std::ofstream file(savePath, std::ios::binary);
	if (!file)
	{
		DebugLog("[PosePreset] File open failed. Path=", savePath.string());
		return false;
	}

	const unsigned char utf8Bom[] = { 0xEF, 0xBB, 0xBF };
	file.write(reinterpret_cast<const char*>(utf8Bom), sizeof(utf8Bom));
	const std::string text = json.str();
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return true;
}

std::string PosePresetStore::CreateUniquePresetId()
{
	for (int index = 1; index <= 9999; ++index)
	{
		std::ostringstream id;
		id << "pose_" << std::setw(4) << std::setfill('0') << index;
		if (!Exists(id.str()))
		{
			return id.str();
		}
	}

	return "pose_overflow";
}

bool PosePresetStore::DisplayNameExists(const std::string& displayName)
{
	if (displayName.empty())
	{
		return false;
	}

	for (const std::string& presetId : ListPresetIds())
	{
		PosePresetData preset;
		if (LoadPreset(presetId, preset) && preset.displayName == displayName)
		{
			return true;
		}
	}

	return false;
}

bool PosePresetStore::Exists(const std::string& presetId)
{
	std::filesystem::path presetPath;
	return ResolvePresetPath(presetId, presetPath);
}

bool PosePresetStore::IsValidPresetId(const std::string& presetId)
{
	if (presetId.empty() || presetId == "." || presetId == "..")
	{
		return false;
	}

	const std::string invalidCharacters = "\\/:*?\"<>|";
	return presetId.find_first_of(invalidCharacters) == std::string::npos;
}
