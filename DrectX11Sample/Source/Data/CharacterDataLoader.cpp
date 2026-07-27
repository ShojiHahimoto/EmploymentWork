#include "Data/CharacterDataLoader.h"

#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";

	/// <summary>
	/// テキストファイルを読み込み、文字列として返す。
	/// </summary>
	/// <param name="path">読み込むファイルパス。</param>
	/// <param name="outText">読み込んだ文字列の書き込み先。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	bool ReadTextFile(const std::filesystem::path& path, std::string& outText)
	{
		std::ifstream file(path);
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
			DebugLog("[CharacterData] JSON file not found: ", path.string());
			return false;
		}
		RemoveUtf8Bom(text);

		std::string error;
		if (!JsonParser::Parse(text, outValue, error))
		{
			DebugLog("[CharacterData] JSON parse failed: ", path.string(), " Error=", error);
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
	/// Object から int 値を取得し、存在しない場合は既定値を返す。
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
	/// Object から float 値を取得し、存在しない場合は既定値を返す。
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
	/// x / y を持つ JSON Object から Vector2 を作る。
	/// </summary>
	/// <param name="object">x / y を含む JSON Object。</param>
	/// <param name="defaultValue">不足時に使う既定値。</param>
	/// <returns>読み込んだ Vector2。</returns>
	DirectX::SimpleMath::Vector2 GetVector2(const JsonValue& object, const DirectX::SimpleMath::Vector2& defaultValue)
	{
		return DirectX::SimpleMath::Vector2(
			GetFloat(object, "x", defaultValue.x),
			GetFloat(object, "y", defaultValue.y));
	}

	/// <summary>
	/// 文字列で保存されたキャンセル種別を enum に変換する。
	/// </summary>
	/// <param name="text">JSON に保存されているキャンセル種別名。</param>
	/// <returns>対応する AttackCancelType。不明な場合は Unknown。</returns>
	AttackCancelType ParseCancelType(const std::string& text)
	{
		if (text == "Normal")
		{
			return AttackCancelType::Normal;
		}
		if (text == "Special")
		{
			return AttackCancelType::Special;
		}
		if (text == "Jump")
		{
			return AttackCancelType::Jump;
		}
		return AttackCancelType::Unknown;
	}

	/// <summary>
	/// Parameter.json の JSON Object から CharacterParameterData を読み込む。
	/// </summary>
	/// <param name="root">Parameter.json の root Object。</param>
	/// <param name="parameter">読み込んだ値を書き込む CharacterParameterData。</param>
	void LoadParameterFromJson(const JsonValue& root, CharacterParameterData& parameter)
	{
		parameter.characterId = GetString(root, "characterId", parameter.characterId);
		parameter.displayName = GetString(root, "displayName", parameter.displayName);

		const JsonValue* parameters = root.Find("parameters");
		const JsonValue& source = parameters && parameters->IsObject() ? *parameters : root;
		parameter.forwardWalkSpeed = GetFloat(source, "forwardWalkSpeed", parameter.forwardWalkSpeed);
		parameter.backwardWalkSpeed = GetFloat(source, "backwardWalkSpeed", parameter.backwardWalkSpeed);
		parameter.jumpInitialVelocity = GetFloat(source, "jumpInitialVelocity", parameter.jumpInitialVelocity);
		parameter.frontJumpHorizontalVelocity = GetFloat(source, "frontJumpHorizontalVelocity", parameter.frontJumpHorizontalVelocity);
		parameter.backJumpHorizontalVelocity = GetFloat(source, "backJumpHorizontalVelocity", parameter.backJumpHorizontalVelocity);
		parameter.riseGravityPerFrame = GetFloat(source, "riseGravityPerFrame", parameter.riseGravityPerFrame);
		parameter.fallGravityPerFrame = GetFloat(source, "fallGravityPerFrame", parameter.fallGravityPerFrame);
	}

	/// <summary>
	/// AttackList.json から技スロットと AttackData ID の対応を読み込む。
	/// </summary>
	/// <param name="root">AttackList.json の root Object。</param>
	/// <param name="outSlots">読み込んだスロット情報の書き込み先。</param>
	void LoadAttackSlotsFromJson(const JsonValue& root, std::vector<CharacterAttackSlotData>& outSlots)
	{
		outSlots.clear();
		const JsonValue* attackSlots = root.Find("attackSlots");
		if (!attackSlots || !attackSlots->IsArray())
		{
			return;
		}

		for (const JsonValue& slotValue : attackSlots->AsArray())
		{
			if (!slotValue.IsObject())
			{
				continue;
			}

			CharacterAttackSlotData slot;
			slot.slotId = GetString(slotValue, "slotId", "");
			slot.attackDataId = GetString(slotValue, "attackDataId", "");
			if (!slot.slotId.empty() && !slot.attackDataId.empty())
			{
				outSlots.push_back(slot);
			}
		}
	}

	/// <summary>
	/// AttackData JSON の frame Object からフレーム情報を読み込む。
	/// </summary>
	/// <param name="root">AttackData JSON の root Object。</param>
	/// <param name="frame">読み込んだ値を書き込む AttackFrameData。</param>
	void LoadAttackFrameFromJson(const JsonValue& root, AttackFrameData& frame)
	{
		const JsonValue* frameValue = root.Find("frame");
		const JsonValue& source = frameValue && frameValue->IsObject() ? *frameValue : root;
		frame.startup = GetInt(source, "startup", frame.startup);
		frame.active = GetInt(source, "active", frame.active);
		frame.recovery = GetInt(source, "recovery", frame.recovery);
	}

	/// <summary>
	/// AttackData JSON からキャンセル可能フレーム情報を読み込む。
	/// </summary>
	/// <param name="root">AttackData JSON の root Object。</param>
	/// <param name="outCancelWindows">読み込んだキャンセル情報の書き込み先。</param>
	void LoadCancelWindowsFromJson(const JsonValue& root, std::vector<AttackCancelWindowData>& outCancelWindows)
	{
		outCancelWindows.clear();
		const JsonValue* cancelWindows = root.Find("cancelWindows");
		if (!cancelWindows || !cancelWindows->IsArray())
		{
			return;
		}

		for (const JsonValue& windowValue : cancelWindows->AsArray())
		{
			if (!windowValue.IsObject())
			{
				continue;
			}

			AttackCancelWindowData cancelWindow;
			cancelWindow.startFrame = GetInt(windowValue, "startFrame", cancelWindow.startFrame);
			cancelWindow.endFrame = GetInt(windowValue, "endFrame", cancelWindow.endFrame);

			const JsonValue* cancelTypes = windowValue.Find("cancelTypes");
			if (cancelTypes && cancelTypes->IsArray())
			{
				for (const JsonValue& cancelTypeValue : cancelTypes->AsArray())
				{
					if (cancelTypeValue.IsString())
					{
						cancelWindow.cancelTypes.push_back(ParseCancelType(cancelTypeValue.AsString()));
					}
				}
			}

			outCancelWindows.push_back(cancelWindow);
		}
	}

	/// <summary>
	/// AttackData JSON から当たり判定の発生タイミングと形状を読み込む。
	/// </summary>
	/// <param name="root">AttackData JSON の root Object。</param>
	/// <param name="outHitboxes">読み込んだ当たり判定情報の書き込み先。</param>
	void LoadHitboxesFromJson(const JsonValue& root, std::vector<AttackHitboxData>& outHitboxes)
	{
		outHitboxes.clear();
		const JsonValue* hitboxes = root.Find("hitboxes");
		if (!hitboxes || !hitboxes->IsArray())
		{
			return;
		}

		for (const JsonValue& hitboxValue : hitboxes->AsArray())
		{
			if (!hitboxValue.IsObject())
			{
				continue;
			}

			AttackHitboxData hitbox;
			hitbox.startFrame = GetInt(hitboxValue, "startFrame", hitbox.startFrame);
			hitbox.endFrame = GetInt(hitboxValue, "endFrame", hitbox.endFrame);

			const JsonValue* offset = hitboxValue.Find("offset");
			if (offset && offset->IsObject())
			{
				hitbox.offset = GetVector2(*offset, hitbox.offset);
			}

			const JsonValue* size = hitboxValue.Find("size");
			if (size && size->IsObject())
			{
				hitbox.size = GetVector2(*size, hitbox.size);
			}

			outHitboxes.push_back(hitbox);
		}
	}
}

bool CharacterDataLoader::LoadCharacterData(const std::string& characterFolderPath, CharacterData& outCharacterData)
{
	bool loaded = true;
	const std::filesystem::path characterFolder(characterFolderPath);

	JsonValue parameterRoot;
	if (ReadJsonFile(characterFolder / "Parameter.json", parameterRoot))
	{
		LoadParameterFromJson(parameterRoot, outCharacterData.parameter);
	}
	else
	{
		loaded = false;
	}

	JsonValue attackListRoot;
	std::vector<CharacterAttackSlotData> attackSlots;
	if (ReadJsonFile(characterFolder / "AttackList.json", attackListRoot))
	{
		LoadAttackSlotsFromJson(attackListRoot, attackSlots);
	}
	else
	{
		loaded = false;
	}

	outCharacterData.attacks.clear();
	for (const CharacterAttackSlotData& slot : attackSlots)
	{
		AttackData attackData;
		if (!LoadAttackData(slot.attackDataId, attackData))
		{
			loaded = false;
			continue;
		}

		CharacterAssignedAttackData assignedAttack;
		assignedAttack.slotId = slot.slotId;
		assignedAttack.attack = attackData;
		outCharacterData.attacks.push_back(assignedAttack);
	}

	DebugLog(
		"[CharacterData] Load result. Character=",
		outCharacterData.parameter.displayName,
		" Attacks=",
		outCharacterData.attacks.size());
	return loaded;
}

bool CharacterDataLoader::LoadAttackData(const std::string& attackDataId, AttackData& outAttackData)
{
	const std::filesystem::path attackPath = std::filesystem::path(AttackDataRootPath) / (attackDataId + ".json");

	JsonValue root;
	if (!ReadJsonFile(attackPath, root))
	{
		return false;
	}

	outAttackData.attackDataId = GetString(root, "attackDataId", attackDataId);
	outAttackData.displayName = GetString(root, "displayName", outAttackData.attackDataId);
	LoadAttackFrameFromJson(root, outAttackData.frame);
	LoadCancelWindowsFromJson(root, outAttackData.cancelWindows);
	LoadHitboxesFromJson(root, outAttackData.hitboxes);
	return true;
}
