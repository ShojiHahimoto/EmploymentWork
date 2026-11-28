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

		static std::vector<NamedInputConfig> LoadNamedInputConfigs(BattleSetup::InputDeviceKind deviceKind);

		static bool SaveNamedInputConfig(const NamedInputConfig& config);

		static std::string GenerateNextConfigId(BattleSetup::InputDeviceKind deviceKind);

		static std::string BuildConfigFilePath(BattleSetup::InputDeviceKind deviceKind, const std::string& configId);
	};
}
