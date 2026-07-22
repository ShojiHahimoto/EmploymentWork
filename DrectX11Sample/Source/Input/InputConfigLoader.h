#pragma once

#include "Input/InputBindings.h"

#include <string>
#include <vector>

namespace Input
{
	class InputConfigLoader
	{
	public:
		// JSON から Binding を読み込むための入口。現段階では未実装。
		static bool LoadBindingsFromJson(const std::string& path, std::vector<InputBinding>& outBindings);

		// 現在の Binding を JSON に保存するための入口。現段階では未実装。
		static bool SaveBindingsToJson(const std::string& path, const std::vector<InputBinding>& bindings);
	};
}
