#pragma once

#include "Input/InputBindings.h"

#include <string>
#include <vector>

namespace Input
{
	class InputConfigLoader
	{
	public:
		static bool LoadBindingsFromJson(const std::string& path, std::vector<InputBinding>& outBindings);
		static bool SaveBindingsToJson(const std::string& path, const std::vector<InputBinding>& bindings);
	};
}
