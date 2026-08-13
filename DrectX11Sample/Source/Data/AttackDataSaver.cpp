#include "Data/AttackDataSaver.h"

#include "System/Debugger.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";

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
	/// AttackKind を JSON 保存用の文字列へ変換する。
	/// </summary>
	/// <param name="value">保存する AttackKind。</param>
	/// <returns>JSON に書く文字列。</returns>
	const char* ToAttackKindText(AttackKind value)
	{
		switch (value)
		{
		case AttackKind::Special:
			return "Special";
		case AttackKind::Normal:
		default:
			return "Normal";
		}
	}

	/// <summary>
	/// AttackCommandId を JSON 保存用の文字列へ変換する。
	/// </summary>
	/// <param name="value">保存する AttackCommandId。</param>
	/// <returns>JSON に書く文字列。</returns>
	const char* ToAttackCommandIdText(AttackCommandId value)
	{
		switch (value)
		{
		case AttackCommandId::Hadouken:
			return "Hadouken";
		case AttackCommandId::Shoryuu:
			return "Shoryuu";
		case AttackCommandId::Yoga:
			return "Yoga";
		case AttackCommandId::ReverseYoga:
			return "ReverseYoga";
		case AttackCommandId::FullRotate:
			return "FullRotate";
		case AttackCommandId::None:
		default:
			return "None";
		}
	}

	/// <summary>
	/// AttackUsableState を JSON 保存用の文字列へ変換する。
	/// </summary>
	/// <param name="value">保存する AttackUsableState。</param>
	/// <returns>JSON に書く文字列。</returns>
	const char* ToAttackUsableStateText(AttackUsableState value)
	{
		switch (value)
	{
	case AttackUsableState::Ground:
		return "Ground";
	case AttackUsableState::Air:
		return "Air";
	default:
		return "Ground";
	}
}

	/// <summary>
	/// HitReactionType を JSON 保存用の文字列へ変換する。
	/// </summary>
	/// <param name="value">保存する HitReactionType。</param>
	/// <returns>JSON に書く文字列。</returns>
	const char* ToHitReactionTypeText(HitReactionType value)
	{
		switch (value)
		{
		case HitReactionType::Down:
			return "Down";
		case HitReactionType::Burst:
			return "Burst";
		case HitReactionType::HardBurst:
			return "HardBurst";
		case HitReactionType::Normal:
		default:
			return "Normal";
		}
	}

	/// <summary>
	/// Vector2 を JSON の { x, y } 形式で書き込む。
	/// </summary>
	/// <param name="stream">書き込み先ストリーム。</param>
	/// <param name="indent">行頭インデント。</param>
	/// <param name="value">保存する Vector2。</param>
	void WriteVector2(std::ostringstream& stream, const char* indent, const Vector2& value)
	{
		stream << "{\n";
		stream << indent << "  \"x\": " << value.x << ",\n";
		stream << indent << "  \"y\": " << value.y << "\n";
		stream << indent << "}";
	}
}

/// <summary>
/// AttackData を Loader 互換の JSON として assets/AttackData 配下へ保存する。
/// </summary>
/// <param name="attackDataId">保存先 ID。例: Ground/slot_00。</param>
/// <param name="attackData">保存する技データ。</param>
/// <returns>保存に成功した場合は true。</returns>
bool AttackDataSaver::SaveAttackData(const std::string& attackDataId, const AttackData& attackData)
{
	if (attackDataId.empty())
	{
		DebugLog("[AttackDataSaver] Save failed. Empty attackDataId.");
		return false;
	}

	const std::filesystem::path savePath = std::filesystem::path(AttackDataRootPath) / (attackDataId + ".json");
	std::error_code errorCode;
	std::filesystem::create_directories(savePath.parent_path(), errorCode);
	if (errorCode)
	{
		DebugLog("[AttackDataSaver] Directory creation failed. Path=", savePath.parent_path().string());
		return false;
	}

	std::ostringstream json;
	json << std::fixed << std::setprecision(3);
	json << "{\n";
	json << "  \"attackDataId\": \"" << EscapeJsonString(attackDataId) << "\",\n";
	json << "  \"displayName\": \"" << EscapeJsonString(attackData.displayName) << "\",\n";
	json << "  \"attackKind\": \"" << ToAttackKindText(attackData.attackKind) << "\",\n";
	json << "  \"commandId\": \"" << ToAttackCommandIdText(attackData.commandId) << "\",\n";
	json << "  \"usableState\": \"" << ToAttackUsableStateText(attackData.usableState) << "\",\n";
	json << "  \"damage\": " << attackData.damage << ",\n";
	json << "  \"hitstunFrames\": " << attackData.hitstunFrames << ",\n";
	json << "  \"guardstunFrames\": " << attackData.guardstunFrames << ",\n";
	json << "  \"hitReactionType\": \"" << ToHitReactionTypeText(attackData.hitReactionType) << "\",\n";
	json << "  \"frame\": {\n";
	json << "    \"startup\": " << attackData.frame.startup << ",\n";
	json << "    \"active\": " << attackData.frame.active << ",\n";
	json << "    \"recovery\": " << attackData.frame.recovery << "\n";
	json << "  },\n";
	json << "  \"canAttackCancel\": " << (attackData.canAttackCancel ? "true" : "false") << ",\n";
	json << "  \"cancelSetting\": {\n";
	json << "    \"startFrame\": " << attackData.cancelSetting.startFrame << ",\n";
	json << "    \"endFrame\": " << attackData.cancelSetting.endFrame << ",\n";
	json << "    \"cancelTypes\": [";
	for (size_t cancelIndex = 0; cancelIndex < attackData.cancelSetting.cancelTypes.size(); ++cancelIndex)
	{
		if (cancelIndex > 0)
		{
			json << ", ";
		}

		const AttackCancelType cancelType = attackData.cancelSetting.cancelTypes[cancelIndex];
		const char* cancelText = cancelType == AttackCancelType::Special
			? "Special"
			: cancelType == AttackCancelType::Jump
				? "Jump"
				: "Normal";
		json << "\"" << cancelText << "\"";
	}
	json << "]\n";
	json << "  },\n";
	json << "  \"hitboxes\": [";
	for (size_t index = 0; index < attackData.hitboxes.size(); ++index)
	{
		const AttackHitboxData& hitbox = attackData.hitboxes[index];
		json << (index == 0 ? "\n" : ",\n");
		json << "    {\n";
		json << "      \"offset\": ";
		WriteVector2(json, "      ", hitbox.offset);
		json << ",\n";
		json << "      \"size\": ";
		WriteVector2(json, "      ", hitbox.size);
		json << "\n";
		json << "    }";
	}
	json << (attackData.hitboxes.empty() ? "]\n" : "\n  ]\n");
	json << "}\n";

	std::ofstream file(savePath, std::ios::binary);
	if (!file)
	{
		DebugLog("[AttackDataSaver] File open failed. Path=", savePath.string());
		return false;
	}

	const unsigned char utf8Bom[] = { 0xEF, 0xBB, 0xBF };
	file.write(reinterpret_cast<const char*>(utf8Bom), sizeof(utf8Bom));
	const std::string text = json.str();
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return true;
}
