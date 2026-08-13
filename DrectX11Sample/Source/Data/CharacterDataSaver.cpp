#include "Data/CharacterDataSaver.h"

#include "System/Debugger.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
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
	/// AttackButtonId を AttackList.json 保存用の文字列へ変換する。
	/// </summary>
	/// <param name="button">保存する攻撃ボタン。</param>
	/// <returns>JSON に書くボタン名。</returns>
	const char* ToAttackButtonIdText(AttackButtonId button)
	{
		switch (button)
		{
		case AttackButtonId::AttackA:
			return "AttackA";
		case AttackButtonId::AttackB:
			return "AttackB";
		case AttackButtonId::AttackX:
			return "AttackX";
		case AttackButtonId::AttackY:
			return "AttackY";
		case AttackButtonId::None:
		default:
			return "None";
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

	/// <summary>
	/// Vector3 を JSON の { x, y, z } 形式で書き込む。
	/// </summary>
	/// <param name="stream">書き込み先ストリーム。</param>
	/// <param name="indent">行頭インデント。</param>
	/// <param name="value">保存する Vector3。</param>
	void WriteVector3(std::ostringstream& stream, const char* indent, const Vector3& value)
	{
		stream << "{\n";
		stream << indent << "  \"x\": " << value.x << ",\n";
		stream << indent << "  \"y\": " << value.y << ",\n";
		stream << indent << "  \"z\": " << value.z << "\n";
		stream << indent << "}";
	}

	/// <summary>
	/// CharacterBoxParameterData を JSON Object として書き込む。
	/// </summary>
	/// <param name="stream">書き込み先ストリーム。</param>
	/// <param name="indent">行頭インデント。</param>
	/// <param name="box">保存する当たり判定初期値。</param>
	void WriteCharacterBox(std::ostringstream& stream, const char* indent, const CharacterBoxParameterData& box)
	{
		stream << "{\n";
		stream << indent << "  \"offset\": ";
		WriteVector2(stream, (std::string(indent) + "  ").c_str(), box.offset);
		stream << ",\n";
		stream << indent << "  \"size\": ";
		WriteVector2(stream, (std::string(indent) + "  ").c_str(), box.size);
		stream << "\n";
		stream << indent << "}";
	}

	/// <summary>
	/// テキストを UTF-8 BOM 付きで保存する。
	/// </summary>
	/// <param name="path">保存先ファイル。</param>
	/// <param name="text">保存する JSON テキスト。</param>
	/// <returns>保存に成功した場合は true。</returns>
	bool WriteUtf8BomTextFile(const std::filesystem::path& path, const std::string& text)
	{
		std::ofstream file(path, std::ios::binary);
		if (!file)
		{
			DebugLog("[CharacterDataSaver] File open failed. Path=", path.string());
			return false;
		}

		const unsigned char utf8Bom[] = { 0xEF, 0xBB, 0xBF };
		file.write(reinterpret_cast<const char*>(utf8Bom), sizeof(utf8Bom));
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		return true;
	}

	/// <summary>
	/// Parameter.json の内容を組み立てる。
	/// </summary>
	/// <param name="parameter">保存するキャラクター基本情報。</param>
	/// <returns>保存用 JSON テキスト。</returns>
	std::string BuildParameterJson(const CharacterParameterData& parameter)
	{
		std::ostringstream json;
		json << std::fixed << std::setprecision(3);
		json << "{\n";
		json << "  \"characterId\": \"" << EscapeJsonString(parameter.characterId) << "\",\n";
		json << "  \"characterName\": \"" << EscapeJsonString(parameter.characterName) << "\",\n";
		json << "  \"parameters\": {\n";
		json << "    \"forwardWalkSpeed\": " << parameter.forwardWalkSpeed << ",\n";
		json << "    \"backwardWalkSpeed\": " << parameter.backwardWalkSpeed << ",\n";
		json << "    \"jumpInitialVelocity\": " << parameter.jumpInitialVelocity << ",\n";
		json << "    \"frontJumpHorizontalVelocity\": " << parameter.frontJumpHorizontalVelocity << ",\n";
		json << "    \"backJumpHorizontalVelocity\": " << parameter.backJumpHorizontalVelocity << ",\n";
		json << "    \"riseGravityPerFrame\": " << parameter.riseGravityPerFrame << ",\n";
		json << "    \"fallGravityPerFrame\": " << parameter.fallGravityPerFrame << ",\n";
		json << "    \"maxHp\": " << parameter.maxHp << ",\n";
		json << "    \"modelScale\": ";
		WriteVector3(json, "    ", parameter.modelScale);
		json << ",\n";
		json << "    \"pushBox\": ";
		WriteCharacterBox(json, "    ", parameter.pushBox);
		json << ",\n";
		json << "    \"hurtBox\": ";
		WriteCharacterBox(json, "    ", parameter.hurtBox);
		json << "\n";
		json << "  }\n";
		json << "}\n";
		return json.str();
	}

	/// <summary>
	/// 指定条件に一致する技スロット配列を AttackList.json に書き込む。
	/// </summary>
	/// <param name="stream">書き込み先ストリーム。</param>
	/// <param name="arrayName">JSON 配列名。</param>
	/// <param name="attackSlots">保存対象スロット全体。</param>
	/// <param name="slotType">書き出すスロット種別。</param>
	/// <param name="slotUsableState">書き出す発動可能状態。Special は Unknown を指定する。</param>
	void WriteAttackSlotArray(
		std::ostringstream& stream,
		const char* arrayName,
		const std::vector<CharacterAttackSlotData>& attackSlots,
		AttackSlotType slotType,
		AttackUsableState slotUsableState)
	{
		stream << "  \"" << arrayName << "\": [";
		bool wroteAny = false;
		for (const CharacterAttackSlotData& slot : attackSlots)
		{
			if (slot.slotType != slotType || slot.slotUsableState != slotUsableState || slot.attackDataId.empty())
			{
				continue;
			}

			stream << (wroteAny ? ",\n" : "\n");
			stream << "    {\n";
			stream << "      \"slotId\": \"" << EscapeJsonString(slot.slotId) << "\",\n";
			stream << "      \"button\": \"" << ToAttackButtonIdText(slot.button) << "\",\n";
			stream << "      \"attackDataId\": \"" << EscapeJsonString(slot.attackDataId) << "\"\n";
			stream << "    }";
			wroteAny = true;
		}

		stream << (wroteAny ? "\n  ]" : "]");
	}

	/// <summary>
	/// AttackList.json の内容を組み立てる。
	/// </summary>
	/// <param name="parameter">キャラクター ID を保存するための基本情報。</param>
	/// <param name="attackSlots">保存する技スロット情報。</param>
	/// <returns>保存用 JSON テキスト。</returns>
	std::string BuildAttackListJson(
		const CharacterParameterData& parameter,
		const std::vector<CharacterAttackSlotData>& attackSlots)
	{
		std::ostringstream json;
		json << "{\n";
		json << "  \"characterId\": \"" << EscapeJsonString(parameter.characterId) << "\",\n";
		WriteAttackSlotArray(json, "groundAttackSlots", attackSlots, AttackSlotType::Normal, AttackUsableState::Ground);
		json << ",\n";
		WriteAttackSlotArray(json, "airAttackSlots", attackSlots, AttackSlotType::Normal, AttackUsableState::Air);
		json << ",\n";
		WriteAttackSlotArray(json, "specialAttackSlots", attackSlots, AttackSlotType::Special, AttackUsableState::Unknown);
		json << "\n";
		json << "}\n";
		return json.str();
	}
}

/// <summary>
/// CharacterData を Parameter.json と AttackList.json に保存する。
/// </summary>
/// <param name="characterFolderPath">保存先キャラクターフォルダ。</param>
/// <param name="parameter">保存するキャラクター基本情報。</param>
/// <param name="attackSlots">保存する技スロット情報。</param>
/// <returns>両方の JSON を保存できた場合は true。</returns>
bool CharacterDataSaver::SaveCharacterData(
	const std::string& characterFolderPath,
	const CharacterParameterData& parameter,
	const std::vector<CharacterAttackSlotData>& attackSlots)
{
	if (characterFolderPath.empty() || parameter.characterId.empty())
	{
		DebugLog("[CharacterDataSaver] Save failed. Empty character path or characterId.");
		return false;
	}

	const std::filesystem::path characterFolder(characterFolderPath);
	std::error_code errorCode;
	std::filesystem::create_directories(characterFolder, errorCode);
	if (errorCode)
	{
		DebugLog("[CharacterDataSaver] Directory creation failed. Path=", characterFolder.string());
		return false;
	}

	const bool savedParameter = WriteUtf8BomTextFile(
		characterFolder / "Parameter.json",
		BuildParameterJson(parameter));
	const bool savedAttackList = WriteUtf8BomTextFile(
		characterFolder / "AttackList.json",
		BuildAttackListJson(parameter, attackSlots));
	return savedParameter && savedAttackList;
}
