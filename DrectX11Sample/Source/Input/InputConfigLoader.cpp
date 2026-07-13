#include "Input/InputConfigLoader.h"

namespace Input
{
	bool InputConfigLoader::LoadBindingsFromJson(const std::string& path, std::vector<InputBinding>& outBindings)
	{
		(void)path;
		(void)outBindings;
		return false;
	}

	bool InputConfigLoader::SaveBindingsToJson(const std::string& path, const std::vector<InputBinding>& bindings)
	{
		(void)path;
		(void)bindings;
		return false;
	}
}
