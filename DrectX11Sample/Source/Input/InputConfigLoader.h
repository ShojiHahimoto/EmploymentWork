#pragma once

#include "Data/BattleSetupData.h"
#include "Input/InputBindings.h"

#include <string>
#include <vector>

namespace Input
{
	struct NamedInputConfig
	{
		// ファイル名や内部参照に使う ID。ユーザー表示には configName を使う。
		std::string configId;
		std::string configName;
		BattleSetup::InputDeviceKind deviceKind = BattleSetup::InputDeviceKind::Keyboard;
		BattleSetup::KeyboardKeyConfig keyboardConfig;
		BattleSetup::GamepadKeyConfig gamepadConfig;
		std::string filePath;
	};

	class InputConfigLoader
	{
	public:
		// JSON から Binding を読み込むための入口。現段階では未実装。
		static bool LoadBindingsFromJson(const std::string& path, std::vector<InputBinding>& outBindings);

		// 現在の Binding を JSON に保存するための入口。現段階では未実装。
		static bool SaveBindingsToJson(const std::string& path, const std::vector<InputBinding>& bindings);

		/// <summary>
		/// 指定デバイス種別の保存済みキーコンフィグをすべて読み込む。
		/// </summary>
		/// <param name="deviceKind">Keyboard / Gamepad のどちらを読むか。</param>
		/// <returns>読み込めたキーコンフィグ配列。</returns>
		static std::vector<NamedInputConfig> LoadNamedInputConfigs(BattleSetup::InputDeviceKind deviceKind);

		/// <summary>
		/// 名前付きキーコンフィグ JSON を保存する。
		/// </summary>
		/// <param name="config">保存するキーコンフィグ。</param>
		/// <returns>保存に成功した場合は true。</returns>
		static bool SaveNamedInputConfig(const NamedInputConfig& config);

		/// <summary>
		/// 指定デバイス種別で未使用の configId を作る。
		/// </summary>
		/// <param name="deviceKind">Keyboard / Gamepad のどちら用に作るか。</param>
		/// <returns>保存ファイル名に使える configId。</returns>
		static std::string GenerateNextConfigId(BattleSetup::InputDeviceKind deviceKind);

		/// <summary>
		/// configId から保存先 JSON パスを作る。
		/// </summary>
		/// <param name="deviceKind">Keyboard / Gamepad のどちら用か。</param>
		/// <param name="configId">保存ファイル名に使う ID。</param>
		/// <returns>assets/InputConfig 配下の保存先パス。</returns>
		static std::string BuildConfigFilePath(BattleSetup::InputDeviceKind deviceKind, const std::string& configId);
	};
}
