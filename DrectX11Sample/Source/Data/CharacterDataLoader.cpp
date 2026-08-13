#include "Data/CharacterDataLoader.h"

#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <sstream>

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";

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
	/// AttackList.json の attackDataId から実際に読む JSON パスを解決する。
	/// </summary>
	/// <param name="attackDataId">AttackData ID、または assets/AttackData からの相対パス。</param>
	/// <param name="outPath">見つかった JSON パスの書き込み先。</param>
	/// <returns>読み込み対象ファイルが一意に見つかった場合は true。</returns>
	bool ResolveAttackDataPath(const std::string& attackDataId, std::filesystem::path& outPath)
	{
		if (attackDataId.empty())
		{
			return false;
		}

		const std::filesystem::path requestedPath = WithJsonExtension(std::filesystem::path(attackDataId));
		const std::filesystem::path rootPath(AttackDataRootPath);
		std::vector<std::filesystem::path> candidatePaths;

		if (requestedPath.is_absolute())
		{
			candidatePaths.push_back(requestedPath);
		}
		else
		{
			// "assets/AttackData/Ground/slot_00.json" のように書かれている場合をそのまま試す。
			candidatePaths.push_back(requestedPath);
			// "debug_punch" や "Ground/slot_00" のような AttackData ルート基準 ID を試す。
			candidatePaths.push_back(rootPath / requestedPath);
		}

		for (const std::filesystem::path& candidatePath : candidatePaths)
		{
			std::error_code errorCode;
			if (std::filesystem::is_regular_file(candidatePath, errorCode))
			{
				outPath = candidatePath;
				return true;
			}
		}

		if (requestedPath.has_parent_path())
		{
			return false;
		}

		std::vector<std::filesystem::path> matchedPaths;
		std::error_code errorCode;
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::recursive_directory_iterator(rootPath, errorCode))
		{
			if (errorCode)
			{
				break;
			}

			if (!entry.is_regular_file(errorCode))
			{
				continue;
			}

			if (entry.path().filename() == requestedPath.filename())
			{
				matchedPaths.push_back(entry.path());
			}
		}

		if (matchedPaths.size() == 1)
		{
			outPath = matchedPaths.front();
			return true;
		}

		if (matchedPaths.size() > 1)
		{
			DebugLog("[CharacterData] AttackData ID is ambiguous. Id=", attackDataId);
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
	/// Object から bool 値を取得し、存在しない場合は既定値を返す。
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
	/// x / y / z を持つ JSON Object から Vector3 を作る。
	/// </summary>
	/// <param name="object">x / y / z を含む JSON Object。</param>
	/// <param name="defaultValue">不足時に使う既定値。</param>
	/// <returns>読み込んだ Vector3。</returns>
	DirectX::SimpleMath::Vector3 GetVector3(const JsonValue& object, const DirectX::SimpleMath::Vector3& defaultValue)
	{
		return DirectX::SimpleMath::Vector3(
			GetFloat(object, "x", defaultValue.x),
			GetFloat(object, "y", defaultValue.y),
			GetFloat(object, "z", defaultValue.z));
	}

	/// <summary>
	/// offset / size を持つ JSON Object から、キャラクターの判定初期値を読み込む。
	/// </summary>
	/// <param name="object">box 設定を持つ JSON Object。</param>
	/// <param name="box">読み込み結果を書き込む box パラメータ。</param>
	void LoadCharacterBoxFromJson(const JsonValue& object, CharacterBoxParameterData& box)
	{
		const JsonValue* offset = object.Find("offset");
		if (offset && offset->IsObject())
		{
			box.offset = GetVector2(*offset, box.offset);
		}

		const JsonValue* size = object.Find("size");
		if (size && size->IsObject())
		{
			box.size = GetVector2(*size, box.size);
		}
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
	/// JSON の attackKind 文字列を AttackKind に変換する。
	/// </summary>
	/// <param name="text">JSON に保存されている攻撃種別。</param>
	/// <returns>対応する AttackKind。不明な場合は Unknown。</returns>
	AttackKind ParseAttackKind(const std::string& text)
	{
		if (text == "Normal")
		{
			return AttackKind::Normal;
		}
		if (text == "Special")
		{
			return AttackKind::Special;
		}
		return AttackKind::Unknown;
	}

	/// <summary>
	/// JSON の commandId 文字列を AttackCommandId に変換する。
	/// </summary>
	/// <param name="text">JSON に保存されているコマンド名。</param>
	/// <returns>対応する AttackCommandId。不明な場合は Unknown。</returns>
	AttackCommandId ParseAttackCommandId(const std::string& text)
	{
		if (text == "None" || text.empty())
		{
			return AttackCommandId::None;
		}
		if (text == "Hadouken" || text == "Hadoken")
		{
			return AttackCommandId::Hadouken;
		}
		if (text == "Shoryuu" || text == "Shoryu")
		{
			return AttackCommandId::Shoryuu;
		}
		if (text == "Yoga")
		{
			return AttackCommandId::Yoga;
		}
		if (text == "ReverseYoga")
		{
			return AttackCommandId::ReverseYoga;
		}
		if (text == "FullRotate")
		{
			return AttackCommandId::FullRotate;
		}
		return AttackCommandId::Unknown;
	}

	/// <summary>
	/// JSON の usableState 文字列を AttackUsableState に変換する。
	/// </summary>
	/// <param name="text">JSON に保存されている発動可能状態。</param>
	/// <returns>対応する AttackUsableState。不明な場合は Unknown。</returns>
	AttackUsableState ParseAttackUsableState(const std::string& text)
	{
		if (text == "Ground")
		{
			return AttackUsableState::Ground;
		}
		if (text == "Air")
		{
			return AttackUsableState::Air;
		}
		if (text == "Both")
		{
			return AttackUsableState::Both;
		}
		return AttackUsableState::Unknown;
	}

	/// <summary>
	/// JSON の hitReactionType 文字列を HitReactionType に変換する。
	/// </summary>
	/// <param name="text">JSON に保存されている被弾反応種別。</param>
	/// <returns>対応する HitReactionType。不明な場合は Unknown。</returns>
	HitReactionType ParseHitReactionType(const std::string& text)
	{
		if (text == "Normal")
		{
			return HitReactionType::Normal;
		}
		if (text == "Down")
		{
			return HitReactionType::Down;
		}
		if (text == "Burst")
		{
			return HitReactionType::Burst;
		}
		if (text == "HardBurst")
		{
			return HitReactionType::HardBurst;
		}
		return HitReactionType::Unknown;
	}

	/// <summary>
	/// JSON の button 文字列を AttackButtonId に変換する。
	/// </summary>
	/// <param name="text">AttackA / AttackB / AttackX / AttackY などの文字列。</param>
	/// <returns>対応する AttackButtonId。不明な場合は Unknown。</returns>
	AttackButtonId ParseAttackButtonId(const std::string& text)
	{
		if (text.empty() || text == "None")
		{
			return AttackButtonId::None;
		}
		if (text == "AttackA" || text == "A" || text == "NormalA" || text == "SpecialA")
		{
			return AttackButtonId::AttackA;
		}
		if (text == "AttackB" || text == "B" || text == "NormalB" || text == "SpecialB")
		{
			return AttackButtonId::AttackB;
		}
		if (text == "AttackX" || text == "X" || text == "NormalX" || text == "SpecialX")
		{
			return AttackButtonId::AttackX;
		}
		if (text == "AttackY" || text == "Y" || text == "NormalY" || text == "SpecialY")
		{
			return AttackButtonId::AttackY;
		}
		return AttackButtonId::Unknown;
	}

	/// <summary>
	/// AttackUsableState をデバッグログ用の文字列へ変換する。
	/// </summary>
	/// <param name="state">文字列化する発動可能状態。</param>
	/// <returns>発動可能状態を表す文字列。</returns>
	const char* ToAttackUsableStateText(AttackUsableState state)
	{
		switch (state)
		{
		case AttackUsableState::Ground:
			return "Ground";
		case AttackUsableState::Air:
			return "Air";
		case AttackUsableState::Both:
			return "Both";
		default:
			return "Unknown";
		}
	}

	/// <summary>
	/// 技スロットが要求する地上/空中種別と、AttackData 側の発動可能状態が一致するか確認する。
	/// </summary>
	/// <param name="slot">検証するキャラクター側の技スロット。</param>
	/// <param name="attackData">スロットに割り当てる技データ。</param>
	/// <returns>割り当ててよい組み合わせなら true。</returns>
	bool IsAttackSlotCompatible(const CharacterAttackSlotData& slot, const AttackData& attackData)
	{
		if (slot.slotType == AttackSlotType::Normal && attackData.attackKind != AttackKind::Normal)
		{
			return false;
		}
		if (slot.slotType == AttackSlotType::Special && attackData.attackKind != AttackKind::Special)
		{
			return false;
		}

		if (slot.slotUsableState == AttackUsableState::Both
			|| slot.slotUsableState == AttackUsableState::Unknown)
		{
			return true;
		}

		return attackData.usableState == slot.slotUsableState;
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
		parameter.maxHp = GetInt(source, "maxHp", parameter.maxHp);

		const JsonValue* modelScale = source.Find("modelScale");
		if (modelScale && modelScale->IsObject())
		{
			parameter.modelScale = GetVector3(*modelScale, parameter.modelScale);
		}

		const JsonValue* pushBox = source.Find("pushBox");
		if (pushBox && pushBox->IsObject())
		{
			LoadCharacterBoxFromJson(*pushBox, parameter.pushBox);
		}

		const JsonValue* hurtBox = source.Find("hurtBox");
		if (hurtBox && hurtBox->IsObject())
		{
			LoadCharacterBoxFromJson(*hurtBox, parameter.hurtBox);
		}
	}

	/// <summary>
	/// AttackList.json の指定配列から技スロットと AttackData ID の対応を読み込む。
	/// </summary>
	/// <param name="root">AttackList.json の root Object。</param>
	/// <param name="arrayKey">読み込む配列キー。</param>
	/// <param name="slotType">配列内のスロット種別。</param>
	/// <param name="outSlots">読み込んだスロット情報の追加先。</param>
	void AppendAttackSlotsFromJsonArray(
		const JsonValue& root,
		const std::string& arrayKey,
		AttackSlotType slotType,
		AttackUsableState slotUsableState,
		std::vector<CharacterAttackSlotData>& outSlots)
	{
		const JsonValue* attackSlots = root.Find(arrayKey);
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
			slot.slotType = slotType;
			slot.slotUsableState = slotUsableState;
			slot.button = ParseAttackButtonId(GetString(slotValue, "button", slot.slotId));
			if (!slot.slotId.empty()
				&& !slot.attackDataId.empty()
				&& slot.button != AttackButtonId::None
				&& slot.button != AttackButtonId::Unknown)
			{
				outSlots.push_back(slot);
			}
		}
	}

	/// <summary>
	/// AttackList.json から通常攻撃スロットと必殺技スロットを読み込む。
	/// </summary>
	/// <param name="root">AttackList.json の root Object。</param>
	/// <param name="outSlots">読み込んだスロット情報の書き込み先。</param>
	void LoadAttackSlotsFromJson(const JsonValue& root, std::vector<CharacterAttackSlotData>& outSlots)
	{
		outSlots.clear();
		AppendAttackSlotsFromJsonArray(root, "groundNormalAttackSlots", AttackSlotType::Normal, AttackUsableState::Ground, outSlots);
		AppendAttackSlotsFromJsonArray(root, "airNormalAttackSlots", AttackSlotType::Normal, AttackUsableState::Air, outSlots);
		AppendAttackSlotsFromJsonArray(root, "normalAttackSlots", AttackSlotType::Normal, AttackUsableState::Both, outSlots);
		AppendAttackSlotsFromJsonArray(root, "specialAttackSlots", AttackSlotType::Special, AttackUsableState::Both, outSlots);

		// 旧形式互換。attackSlots しかない場合は通常攻撃スロットとして扱う。
		if (outSlots.empty())
		{
			AppendAttackSlotsFromJsonArray(root, "attackSlots", AttackSlotType::Normal, AttackUsableState::Both, outSlots);
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
	/// JSON Object から単数のキャンセル設定を読み込む。
	/// </summary>
	/// <param name="source">startFrame / endFrame / cancelTypes を含む JSON Object。</param>
	/// <param name="outCancelSetting">読み込んだキャンセル設定の書き込み先。</param>
	void LoadCancelSettingObject(const JsonValue& source, AttackCancelSettingData& outCancelSetting)
	{
		outCancelSetting.startFrame = GetInt(source, "startFrame", outCancelSetting.startFrame);
		outCancelSetting.endFrame = GetInt(source, "endFrame", outCancelSetting.endFrame);
		outCancelSetting.cancelTypes.clear();

		const JsonValue* cancelTypes = source.Find("cancelTypes");
		if (cancelTypes && cancelTypes->IsArray())
		{
			for (const JsonValue& cancelTypeValue : cancelTypes->AsArray())
			{
				if (cancelTypeValue.IsString())
				{
					outCancelSetting.cancelTypes.push_back(ParseCancelType(cancelTypeValue.AsString()));
				}
			}
		}
	}

	/// <summary>
	/// AttackData JSON からキャンセル設定を読み込む。
	/// </summary>
	/// <param name="root">AttackData JSON の root Object。</param>
	/// <param name="outAttackData">読み込んだキャンセル設定を書き込む AttackData。</param>
	void LoadCancelSettingFromJson(const JsonValue& root, AttackData& outAttackData)
	{
		outAttackData.canAttackCancel = GetBool(root, "canAttackCancel", outAttackData.canAttackCancel);

		const JsonValue* cancelSetting = root.Find("cancelSetting");
		if (cancelSetting && cancelSetting->IsObject())
		{
			LoadCancelSettingObject(*cancelSetting, outAttackData.cancelSetting);
			return;
		}

		// 旧 JSON 互換。配列形式が残っている場合は先頭 1 件だけ単数設定へ移す。
		const JsonValue* cancelSettings = root.Find("cancelSettings");
		if (!cancelSettings || !cancelSettings->IsArray())
		{
			cancelSettings = root.Find("cancelWindows");
		}

		if (!cancelSettings || !cancelSettings->IsArray())
		{
			return;
		}

		for (const JsonValue& settingValue : cancelSettings->AsArray())
		{
			if (settingValue.IsObject())
			{
				LoadCancelSettingObject(settingValue, outAttackData.cancelSetting);
				outAttackData.canAttackCancel = true;
				return;
			}
		}
	}

	/// <summary>
	/// AttackData JSON から当たり判定の形状を読み込む。
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
		assignedAttack.slotType = slot.slotType;
		assignedAttack.button = slot.button;
		assignedAttack.slotUsableState = slot.slotUsableState;
		assignedAttack.attack = attackData;

		if (!IsAttackSlotCompatible(slot, attackData))
		{
			DebugLog(
				"[CharacterData] Attack slot mismatch. Slot=",
				slot.slotId,
				" AttackData=",
				slot.attackDataId,
				" SlotUsableState=",
				ToAttackUsableStateText(slot.slotUsableState),
				" AttackUsableState=",
				ToAttackUsableStateText(attackData.usableState));
			loaded = false;
			continue;
		}

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
	std::filesystem::path attackPath;
	if (!ResolveAttackDataPath(attackDataId, attackPath))
	{
		DebugLog("[CharacterData] AttackData file not found. Id=", attackDataId);
		return false;
	}

	JsonValue root;
	if (!ReadJsonFile(attackPath, root))
	{
		return false;
	}

	outAttackData.attackDataId = GetString(root, "attackDataId", attackDataId);
	outAttackData.displayName = GetString(root, "displayName", outAttackData.attackDataId);
	outAttackData.damage = GetInt(root, "damage", outAttackData.damage);
	outAttackData.hitstunFrames = GetInt(root, "hitstunFrames", GetInt(root, "hitstanFrames", outAttackData.hitstunFrames));
	outAttackData.guardstunFrames = GetInt(root, "guardstunFrames", outAttackData.hitstunFrames);
	outAttackData.attackKind = ParseAttackKind(GetString(root, "attackKind", "Normal"));
	outAttackData.commandId = ParseAttackCommandId(GetString(root, "commandId", "None"));
	outAttackData.usableState = ParseAttackUsableState(GetString(root, "usableState", "Both"));
	outAttackData.hitReactionType = ParseHitReactionType(GetString(root, "hitReactionType", "Normal"));
	LoadAttackFrameFromJson(root, outAttackData.frame);
	LoadCancelSettingFromJson(root, outAttackData);
	LoadHitboxesFromJson(root, outAttackData.hitboxes);
	return true;
}
