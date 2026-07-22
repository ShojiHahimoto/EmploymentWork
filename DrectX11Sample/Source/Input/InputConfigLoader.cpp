#include "Input/InputConfigLoader.h"

namespace Input
{
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
}
