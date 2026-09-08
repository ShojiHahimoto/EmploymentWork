#include "Input/InputConfigLoader.h"

#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Input
{
	namespace
	{
		constexpr const char* InputConfigRootPath = "assets/InputConfig";

		/// <summary>
		/// JSON 文字列として保存できるよう、必要な文字をエスケープする。
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
		/// テキストファイルを読み込み、先頭に UTF-8 BOM があれば取り除く。
		/// </summary>
		/// <param name="path">読み込むファイルパス。</param>
		/// <param name="outText">読み込んだ文字列。</param>
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
			if (outText.size() >= 3
				&& static_cast<unsigned char>(outText[0]) == 0xEF
				&& static_cast<unsigned char>(outText[1]) == 0xBB
				&& static_cast<unsigned char>(outText[2]) == 0xBF)
			{
				outText.erase(0, 3);
			}

			return true;
		}

		/// <summary>
		/// テキストを UTF-8 BOM 付きで保存する。
		/// </summary>
		/// <param name="path">保存先ファイルパス。</param>
		/// <param name="text">保存する文字列。</param>
		/// <returns>保存に成功した場合は true。</returns>
		bool WriteUtf8BomTextFile(const std::filesystem::path& path, const std::string& text)
		{
			std::ofstream file(path, std::ios::binary);
			if (!file)
			{
				DebugLog("[InputConfig] File open failed. Path=", path.string());
				return false;
			}

			const unsigned char utf8Bom[] = { 0xEF, 0xBB, 0xBF };
			file.write(reinterpret_cast<const char*>(utf8Bom), sizeof(utf8Bom));
			file.write(text.data(), static_cast<std::streamsize>(text.size()));
			return true;
		}

		/// <summary>
		/// 指定 JSON Object から文字列を取得する。
		/// </summary>
		/// <param name="object">検索対象の JSON Object。</param>
		/// <param name="key">取得するキー。</param>
		/// <param name="defaultValue">存在しない場合の既定値。</param>
		/// <returns>取得した文字列。</returns>
		std::string GetString(const JsonValue& object, const std::string& key, const std::string& defaultValue)
		{
			const JsonValue* value = object.Find(key);
			if (!value || !value->IsString())
			{
				return defaultValue;
			}

			return value->AsString();
		}

		/// <summary>
		/// デバイス種別を保存用文字列へ変換する。
		/// </summary>
		/// <param name="deviceKind">変換するデバイス種別。</param>
		/// <returns>JSON に保存する deviceType 文字列。</returns>
		const char* ToDeviceTypeText(BattleSetup::InputDeviceKind deviceKind)
		{
			return deviceKind == BattleSetup::InputDeviceKind::Keyboard ? "Keyboard" : "Gamepad";
		}

		/// <summary>
		/// 保存用文字列からデバイス種別を復元する。
		/// </summary>
		/// <param name="text">JSON から読んだ deviceType 文字列。</param>
		/// <param name="defaultKind">変換できない場合の既定値。</param>
		/// <returns>復元したデバイス種別。</returns>
		BattleSetup::InputDeviceKind ParseDeviceKind(const std::string& text, BattleSetup::InputDeviceKind defaultKind)
		{
			if (text == "Keyboard")
			{
				return BattleSetup::InputDeviceKind::Keyboard;
			}

			if (text == "Gamepad")
			{
				return BattleSetup::InputDeviceKind::Gamepad;
			}

			return defaultKind;
		}

		/// <summary>
		/// KeyboardKey を保存用文字列へ変換する。
		/// </summary>
		/// <param name="key">変換する KeyboardKey。</param>
		/// <returns>JSON に保存するキー名。</returns>
		const char* ToKeyboardKeyText(KeyboardKey key)
		{
			switch (key)
			{
			case KeyboardKey::A: return "A";
			case KeyboardKey::B: return "B";
			case KeyboardKey::C: return "C";
			case KeyboardKey::D: return "D";
			case KeyboardKey::E: return "E";
			case KeyboardKey::F: return "F";
			case KeyboardKey::G: return "G";
			case KeyboardKey::H: return "H";
			case KeyboardKey::I: return "I";
			case KeyboardKey::J: return "J";
			case KeyboardKey::K: return "K";
			case KeyboardKey::L: return "L";
			case KeyboardKey::M: return "M";
			case KeyboardKey::N: return "N";
			case KeyboardKey::O: return "O";
			case KeyboardKey::P: return "P";
			case KeyboardKey::Q: return "Q";
			case KeyboardKey::R: return "R";
			case KeyboardKey::S: return "S";
			case KeyboardKey::T: return "T";
			case KeyboardKey::U: return "U";
			case KeyboardKey::V: return "V";
			case KeyboardKey::W: return "W";
			case KeyboardKey::X: return "X";
			case KeyboardKey::Y: return "Y";
			case KeyboardKey::Z: return "Z";
			case KeyboardKey::Space: return "Space";
			case KeyboardKey::Up: return "Up";
			case KeyboardKey::Down: return "Down";
			case KeyboardKey::Left: return "Left";
			case KeyboardKey::Right: return "Right";
			case KeyboardKey::Enter: return "Enter";
			case KeyboardKey::Escape: return "Escape";
			case KeyboardKey::None:
			default:
				return "None";
			}
		}

		/// <summary>
		/// 保存用文字列から KeyboardKey を復元する。
		/// </summary>
		/// <param name="text">JSON から読んだキー名。</param>
		/// <param name="defaultKey">変換できない場合の既定値。</param>
		/// <returns>復元した KeyboardKey。</returns>
		KeyboardKey ParseKeyboardKey(const std::string& text, KeyboardKey defaultKey)
		{
			if (text.size() == 1 && text[0] >= 'A' && text[0] <= 'Z')
			{
				return static_cast<KeyboardKey>(text[0]);
			}

			if (text == "Space") return KeyboardKey::Space;
			if (text == "Up") return KeyboardKey::Up;
			if (text == "Down") return KeyboardKey::Down;
			if (text == "Left") return KeyboardKey::Left;
			if (text == "Right") return KeyboardKey::Right;
			if (text == "Enter") return KeyboardKey::Enter;
			if (text == "Escape") return KeyboardKey::Escape;
			if (text == "None") return KeyboardKey::None;
			return defaultKey;
		}

		/// <summary>
		/// GamepadButton を保存用文字列へ変換する。
		/// </summary>
		/// <param name="button">変換する GamepadButton。</param>
		/// <returns>JSON に保存するボタン名。</returns>
		const char* ToGamepadButtonText(GamepadButton button)
		{
			switch (button)
			{
			case GamepadButton::DPadUp: return "DPadUp";
			case GamepadButton::DPadDown: return "DPadDown";
			case GamepadButton::DPadLeft: return "DPadLeft";
			case GamepadButton::DPadRight: return "DPadRight";
			case GamepadButton::A: return "A";
			case GamepadButton::B: return "B";
			case GamepadButton::X: return "X";
			case GamepadButton::Y: return "Y";
			case GamepadButton::LeftShoulder: return "LB";
			case GamepadButton::RightShoulder: return "RB";
			case GamepadButton::LeftTrigger: return "LT";
			case GamepadButton::RightTrigger: return "RT";
			case GamepadButton::Start: return "Start";
			case GamepadButton::Back: return "Back";
			case GamepadButton::LeftThumb: return "LeftThumb";
			case GamepadButton::RightThumb: return "RightThumb";
			case GamepadButton::None:
			default:
				return "None";
			}
		}

		/// <summary>
		/// 保存用文字列から GamepadButton を復元する。
		/// </summary>
		/// <param name="text">JSON から読んだボタン名。</param>
		/// <param name="defaultButton">変換できない場合の既定値。</param>
		/// <returns>復元した GamepadButton。</returns>
		GamepadButton ParseGamepadButton(const std::string& text, GamepadButton defaultButton)
		{
			if (text == "DPadUp") return GamepadButton::DPadUp;
			if (text == "DPadDown") return GamepadButton::DPadDown;
			if (text == "DPadLeft") return GamepadButton::DPadLeft;
			if (text == "DPadRight") return GamepadButton::DPadRight;
			if (text == "A") return GamepadButton::A;
			if (text == "B") return GamepadButton::B;
			if (text == "X") return GamepadButton::X;
			if (text == "Y") return GamepadButton::Y;
			if (text == "LB") return GamepadButton::LeftShoulder;
			if (text == "RB") return GamepadButton::RightShoulder;
			if (text == "LT") return GamepadButton::LeftTrigger;
			if (text == "RT") return GamepadButton::RightTrigger;
			if (text == "Start") return GamepadButton::Start;
			if (text == "Back") return GamepadButton::Back;
			if (text == "LeftThumb") return GamepadButton::LeftThumb;
			if (text == "RightThumb") return GamepadButton::RightThumb;
			if (text == "None") return GamepadButton::None;
			return defaultButton;
		}

		/// <summary>
		/// デバイス種別ごとの保存フォルダを取得する。
		/// </summary>
		/// <param name="deviceKind">Keyboard / Gamepad のどちら用か。</param>
		/// <returns>assets/InputConfig 配下のデバイス別フォルダ。</returns>
		std::filesystem::path GetDeviceConfigDirectory(BattleSetup::InputDeviceKind deviceKind)
		{
			return std::filesystem::path(InputConfigRootPath) / ToDeviceTypeText(deviceKind);
		}

		/// <summary>
		/// Keyboard 用 bindings Object からキー設定を読み込む。
		/// </summary>
		/// <param name="bindings">JSON の bindings Object。</param>
		/// <param name="config">読み込み先キー設定。</param>
		void LoadKeyboardConfig(const JsonValue& bindings, BattleSetup::KeyboardKeyConfig& config)
		{
			config.moveUp = ParseKeyboardKey(GetString(bindings, "moveUp", ToKeyboardKeyText(config.moveUp)), config.moveUp);
			config.moveDown = ParseKeyboardKey(GetString(bindings, "moveDown", ToKeyboardKeyText(config.moveDown)), config.moveDown);
			config.moveLeft = ParseKeyboardKey(GetString(bindings, "moveLeft", ToKeyboardKeyText(config.moveLeft)), config.moveLeft);
			config.moveRight = ParseKeyboardKey(GetString(bindings, "moveRight", ToKeyboardKeyText(config.moveRight)), config.moveRight);
			config.attackA = ParseKeyboardKey(GetString(bindings, "attackA", ToKeyboardKeyText(config.attackA)), config.attackA);
			config.attackB = ParseKeyboardKey(GetString(bindings, "attackB", ToKeyboardKeyText(config.attackB)), config.attackB);
			config.attackX = ParseKeyboardKey(GetString(bindings, "attackX", ToKeyboardKeyText(config.attackX)), config.attackX);
			config.attackY = ParseKeyboardKey(GetString(bindings, "attackY", ToKeyboardKeyText(config.attackY)), config.attackY);
		}

		/// <summary>
		/// Gamepad 用 bindings Object からボタン設定を読み込む。
		/// </summary>
		/// <param name="bindings">JSON の bindings Object。</param>
		/// <param name="config">読み込み先ボタン設定。</param>
		void LoadGamepadConfig(const JsonValue& bindings, BattleSetup::GamepadKeyConfig& config)
		{
			config.moveUp = ParseGamepadButton(GetString(bindings, "moveUp", ToGamepadButtonText(config.moveUp)), config.moveUp);
			config.moveDown = ParseGamepadButton(GetString(bindings, "moveDown", ToGamepadButtonText(config.moveDown)), config.moveDown);
			config.moveLeft = ParseGamepadButton(GetString(bindings, "moveLeft", ToGamepadButtonText(config.moveLeft)), config.moveLeft);
			config.moveRight = ParseGamepadButton(GetString(bindings, "moveRight", ToGamepadButtonText(config.moveRight)), config.moveRight);
			config.attackA = ParseGamepadButton(GetString(bindings, "attackA", ToGamepadButtonText(config.attackA)), config.attackA);
			config.attackB = ParseGamepadButton(GetString(bindings, "attackB", ToGamepadButtonText(config.attackB)), config.attackB);
			config.attackX = ParseGamepadButton(GetString(bindings, "attackX", ToGamepadButtonText(config.attackX)), config.attackX);
			config.attackY = ParseGamepadButton(GetString(bindings, "attackY", ToGamepadButtonText(config.attackY)), config.attackY);
		}

		/// <summary>
		/// キーコンフィグ JSON を 1 ファイル読み込む。
		/// </summary>
		/// <param name="path">読み込む JSON ファイルパス。</param>
		/// <param name="outConfig">読み込み先キーコンフィグ。</param>
		/// <returns>読み込みに成功した場合は true。</returns>
		bool LoadNamedInputConfigFromJson(const std::filesystem::path& path, NamedInputConfig& outConfig)
		{
			std::string text;
			if (!ReadTextFile(path, text))
			{
				return false;
			}

			JsonValue root;
			std::string error;
			if (!JsonParser::Parse(text, root, error) || !root.IsObject())
			{
				DebugLog("[InputConfig] JSON parse failed. Path=", path.string(), " Error=", error);
				return false;
			}

			const std::string fallbackConfigId = path.stem().generic_string();
			outConfig.configId = GetString(root, "configId", fallbackConfigId);
			outConfig.configName = GetString(root, "configName", outConfig.configId);
			outConfig.deviceKind = ParseDeviceKind(GetString(root, "deviceType", "Keyboard"), BattleSetup::InputDeviceKind::Keyboard);
			outConfig.filePath = path.generic_string();

			const JsonValue* bindings = root.Find("bindings");
			if (bindings && bindings->IsObject())
			{
				if (outConfig.deviceKind == BattleSetup::InputDeviceKind::Keyboard)
				{
					LoadKeyboardConfig(*bindings, outConfig.keyboardConfig);
				}
				else
				{
					LoadGamepadConfig(*bindings, outConfig.gamepadConfig);
				}
			}

			return true;
		}

		/// <summary>
		/// 名前付きキーコンフィグの保存用 JSON テキストを作る。
		/// </summary>
		/// <param name="config">保存するキーコンフィグ。</param>
		/// <returns>保存用 JSON テキスト。</returns>
		std::string BuildNamedInputConfigJson(const NamedInputConfig& config)
		{
			std::ostringstream json;
			json << "{\n";
			json << "  \"version\": 1,\n";
			json << "  \"configId\": \"" << EscapeJsonString(config.configId) << "\",\n";
			json << "  \"configName\": \"" << EscapeJsonString(config.configName) << "\",\n";
			json << "  \"deviceType\": \"" << ToDeviceTypeText(config.deviceKind) << "\",\n";
			json << "  \"bindings\": {\n";

			if (config.deviceKind == BattleSetup::InputDeviceKind::Keyboard)
			{
				const BattleSetup::KeyboardKeyConfig& keyConfig = config.keyboardConfig;
				json << "    \"moveUp\": \"" << ToKeyboardKeyText(keyConfig.moveUp) << "\",\n";
				json << "    \"moveDown\": \"" << ToKeyboardKeyText(keyConfig.moveDown) << "\",\n";
				json << "    \"moveLeft\": \"" << ToKeyboardKeyText(keyConfig.moveLeft) << "\",\n";
				json << "    \"moveRight\": \"" << ToKeyboardKeyText(keyConfig.moveRight) << "\",\n";
				json << "    \"attackA\": \"" << ToKeyboardKeyText(keyConfig.attackA) << "\",\n";
				json << "    \"attackB\": \"" << ToKeyboardKeyText(keyConfig.attackB) << "\",\n";
				json << "    \"attackX\": \"" << ToKeyboardKeyText(keyConfig.attackX) << "\",\n";
				json << "    \"attackY\": \"" << ToKeyboardKeyText(keyConfig.attackY) << "\"\n";
			}
			else
			{
				const BattleSetup::GamepadKeyConfig& keyConfig = config.gamepadConfig;
				json << "    \"moveUp\": \"" << ToGamepadButtonText(keyConfig.moveUp) << "\",\n";
				json << "    \"moveDown\": \"" << ToGamepadButtonText(keyConfig.moveDown) << "\",\n";
				json << "    \"moveLeft\": \"" << ToGamepadButtonText(keyConfig.moveLeft) << "\",\n";
				json << "    \"moveRight\": \"" << ToGamepadButtonText(keyConfig.moveRight) << "\",\n";
				json << "    \"attackA\": \"" << ToGamepadButtonText(keyConfig.attackA) << "\",\n";
				json << "    \"attackB\": \"" << ToGamepadButtonText(keyConfig.attackB) << "\",\n";
				json << "    \"attackX\": \"" << ToGamepadButtonText(keyConfig.attackX) << "\",\n";
				json << "    \"attackY\": \"" << ToGamepadButtonText(keyConfig.attackY) << "\"\n";
			}

			json << "  }\n";
			json << "}\n";
			return json.str();
		}
	}

	/// <summary>
	/// キーコンフィグ JSON から Binding 一覧を読み込むための予約関数。
	/// </summary>
	/// <param name="path">読み込み対象 JSON ファイルのパス。</param>
	/// <param name="outBindings">読み込み成功時に Binding を書き込む配列。</param>
	/// <returns>読み込みに成功した場合は true。現段階では未実装のため false。</returns>
	bool InputConfigLoader::LoadBindingsFromJson(const std::string& path, std::vector<InputBinding>& outBindings)
	{
		(void)path;
		(void)outBindings;
		return false;
	}

	/// <summary>
	/// Binding 一覧をキーコンフィグ JSON へ保存するための予約関数。
	/// </summary>
	/// <param name="path">保存先 JSON ファイルのパス。</param>
	/// <param name="bindings">保存する Binding 配列。</param>
	/// <returns>保存に成功した場合は true。現段階では未実装のため false。</returns>
	bool InputConfigLoader::SaveBindingsToJson(const std::string& path, const std::vector<InputBinding>& bindings)
	{
		(void)path;
		(void)bindings;
		return false;
	}

	std::vector<NamedInputConfig> InputConfigLoader::LoadNamedInputConfigs(BattleSetup::InputDeviceKind deviceKind)
	{
		std::vector<NamedInputConfig> configs;
		const std::filesystem::path directory = GetDeviceConfigDirectory(deviceKind);
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
		{
			return configs;
		}

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".json")
			{
				continue;
			}

			NamedInputConfig config;
			if (LoadNamedInputConfigFromJson(entry.path(), config) && config.deviceKind == deviceKind)
			{
				configs.push_back(config);
			}
		}

		std::sort(
			configs.begin(),
			configs.end(),
			[](const NamedInputConfig& left, const NamedInputConfig& right)
			{
				return left.configName < right.configName;
			});
		return configs;
	}

	bool InputConfigLoader::SaveNamedInputConfig(const NamedInputConfig& config)
	{
		if (config.configId.empty() || config.configName.empty())
		{
			DebugLog("[InputConfig] Save failed. Empty configId or configName.");
			return false;
		}

		const std::filesystem::path path = config.filePath.empty()
			? std::filesystem::path(BuildConfigFilePath(config.deviceKind, config.configId))
			: std::filesystem::path(config.filePath);
		const std::filesystem::path directory = path.parent_path();
		if (!directory.empty() && !std::filesystem::exists(directory))
		{
			std::error_code error;
			std::filesystem::create_directories(directory, error);
			if (error)
			{
				DebugLog("[InputConfig] Directory creation failed. Path=", directory.string());
				return false;
			}
		}

		return WriteUtf8BomTextFile(path, BuildNamedInputConfigJson(config));
	}

	std::string InputConfigLoader::GenerateNextConfigId(BattleSetup::InputDeviceKind deviceKind)
	{
		const char* prefix = deviceKind == BattleSetup::InputDeviceKind::Keyboard
			? "keyboard_config_"
			: "gamepad_config_";

		for (int index = 0; index < 10000; ++index)
		{
			std::ostringstream id;
			id << prefix << std::setw(2) << std::setfill('0') << index;
			if (!std::filesystem::exists(BuildConfigFilePath(deviceKind, id.str())))
			{
				return id.str();
			}
		}

		return std::string(prefix) + "overflow";
	}

	std::string InputConfigLoader::BuildConfigFilePath(BattleSetup::InputDeviceKind deviceKind, const std::string& configId)
	{
		return (GetDeviceConfigDirectory(deviceKind) / (configId + ".json")).generic_string();
	}
}
