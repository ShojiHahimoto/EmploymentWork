#pragma once

#include <map>
#include <string>
#include <vector>

class JsonValue
{
public:
	enum class Type
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object
	};

	using Array = std::vector<JsonValue>;
	using Object = std::map<std::string, JsonValue>;

	static JsonValue MakeNull();
	static JsonValue MakeBool(bool value);
	static JsonValue MakeNumber(double value);
	static JsonValue MakeString(const std::string& value);
	static JsonValue MakeArray(const Array& value);
	static JsonValue MakeObject(const Object& value);

	Type GetType() const;
	bool IsNull() const;
	bool IsBool() const;
	bool IsNumber() const;
	bool IsString() const;
	bool IsArray() const;
	bool IsObject() const;

	bool AsBool(bool defaultValue = false) const;
	double AsNumber(double defaultValue = 0.0) const;
	const std::string& AsString() const;
	const Array& AsArray() const;
	const Object& AsObject() const;

	const JsonValue* Find(const std::string& key) const;

private:
	Type type = Type::Null;
	bool boolValue = false;
	double numberValue = 0.0;
	std::string stringValue;
	Array arrayValue;
	Object objectValue;
};

class JsonParser
{
public:
	/// <summary>
	/// JSON 文字列を JsonValue に変換する。
	/// </summary>
	/// <param name="text">解析する JSON 文字列。</param>
	/// <param name="outValue">解析結果を書き込む JsonValue。</param>
	/// <param name="outError">失敗時のエラー内容を書き込む文字列。</param>
	/// <returns>解析に成功した場合は true。</returns>
	static bool Parse(const std::string& text, JsonValue& outValue, std::string& outError);
};
