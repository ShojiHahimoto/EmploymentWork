#include "Input/InputConfigLoader.h"

namespace Input
{
	// キーコンフィグ JSON 読み込み用の予約実装。
	// JSON ライブラリや保存形式を決めた段階で中身を追加する。
	bool InputConfigLoader::LoadBindingsFromJson(const std::string& path, std::vector<InputBinding>& outBindings)
	{
		(void)path;
		(void)outBindings;
		return false;
	}

	// キーコンフィグ JSON 保存用の予約実装。
	// false は「まだ外部保存には対応していない」ことを表す。
	bool InputConfigLoader::SaveBindingsToJson(const std::string& path, const std::vector<InputBinding>& bindings)
	{
		(void)path;
		(void)bindings;
		return false;
	}
}
