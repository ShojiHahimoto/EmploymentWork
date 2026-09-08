#include "Data/MotionDataSaver.h"

#include "System/Debugger.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* MotionDataRootPath = "assets/MotionData";

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
	/// <param name="indent">行頭インデント。</param>
	/// <param name="value">保存する Vector3。</param>
	void WriteVector3(std::ostringstream& stream, const char* indent, const Vector3& value)
	{
		stream << "{ \"x\": " << value.x << ", \"y\": " << value.y << ", \"z\": " << value.z << " }";
	}
}

bool MotionDataSaver::SaveMotionData(const std::string& motionDataId, const MotionData& motionData)
{
	if (motionDataId.empty())
	{
		DebugLog("[MotionDataSaver] Save failed. Empty motionDataId.");
		return false;
	}

	const std::filesystem::path savePath = std::filesystem::path(MotionDataRootPath) / (motionDataId + ".json");
	std::error_code errorCode;
	std::filesystem::create_directories(savePath.parent_path(), errorCode);
	if (errorCode)
	{
		DebugLog("[MotionDataSaver] Directory creation failed. Path=", savePath.parent_path().string());
		return false;
	}

	std::ostringstream json;
	json << std::fixed << std::setprecision(3);
	json << "{\n";
	json << "  \"motionDataId\": \"" << EscapeJsonString(motionDataId) << "\",\n";
	json << "  \"displayName\": \"" << EscapeJsonString(motionData.displayName) << "\",\n";
	json << "  \"totalFrames\": " << motionData.totalFrames << ",\n";
	json << "  \"looping\": " << (motionData.looping ? "true" : "false") << ",\n";
	json << "  \"boneTracks\": [";
	for (size_t trackIndex = 0; trackIndex < motionData.boneTracks.size(); ++trackIndex)
	{
		const MotionBoneTrackData& track = motionData.boneTracks[trackIndex];
		json << (trackIndex == 0 ? "\n" : ",\n");
		json << "    {\n";
		json << "      \"boneName\": \"" << EscapeJsonString(track.boneName) << "\",\n";
		json << "      \"keyframes\": [";
		for (size_t keyIndex = 0; keyIndex < track.keyframes.size(); ++keyIndex)
		{
			const MotionBoneKeyframeData& keyframe = track.keyframes[keyIndex];
			json << (keyIndex == 0 ? "\n" : ",\n");
			json << "        {\n";
			json << "          \"frame\": " << keyframe.frame;
			if (keyframe.hasPosition)
			{
				json << ",\n          \"position\": ";
				WriteVector3(json, "          ", keyframe.localPosition);
			}
			if (keyframe.hasRotation)
			{
				json << ",\n          \"rotationEulerDegrees\": ";
				WriteVector3(json, "          ", keyframe.localRotationEulerDegrees);
			}
			if (keyframe.hasScale)
			{
				json << ",\n          \"scale\": ";
				WriteVector3(json, "          ", keyframe.localScale);
			}
			json << "\n        }";
		}
		json << (track.keyframes.empty() ? "]\n" : "\n      ]\n");
		json << "    }";
	}
	json << (motionData.boneTracks.empty() ? "]\n" : "\n  ]\n");
	json << "}\n";

	std::ofstream file(savePath, std::ios::binary);
	if (!file)
	{
		DebugLog("[MotionDataSaver] File open failed. Path=", savePath.string());
		return false;
	}

	const unsigned char utf8Bom[] = { 0xEF, 0xBB, 0xBF };
	file.write(reinterpret_cast<const char*>(utf8Bom), sizeof(utf8Bom));
	const std::string text = json.str();
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return true;
}
