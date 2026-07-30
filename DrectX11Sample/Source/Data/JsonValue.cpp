#include "Data/JsonValue.h"

#include <cctype>
#include <cstdlib>

namespace
{
	const std::string EmptyString;
	const JsonValue::Array EmptyArray;
	const JsonValue::Object EmptyObject;

	class Parser
	{
	public:
		Parser(const std::string& sourceText)
			: source(sourceText)
		{
		}

		bool Parse(JsonValue& outValue, std::string& outError)
		{
			SkipWhitespace();
			if (!ParseValue(outValue, outError))
			{
				return false;
			}

			SkipWhitespace();
			if (!IsEnd())
			{
				outError = "JSON has trailing characters.";
				return false;
			}

			return true;
		}

	private:
		const std::string& source;
		size_t position = 0;

		bool IsEnd() const
		{
			return position >= source.size();
		}

		char Peek() const
		{
			return IsEnd() ? '\0' : source[position];
		}

		char Advance()
		{
			return IsEnd() ? '\0' : source[position++];
		}

		void SkipWhitespace()
		{
			while (!IsEnd() && std::isspace(static_cast<unsigned char>(Peek())) != 0)
			{
				++position;
			}
		}

		bool MatchLiteral(const char* literal)
		{
			size_t index = 0;
			while (literal[index] != '\0')
			{
				if (position + index >= source.size() || source[position + index] != literal[index])
				{
					return false;
				}
				++index;
			}

			position += index;
			return true;
		}

		bool ParseValue(JsonValue& outValue, std::string& outError)
		{
			SkipWhitespace();
			if (IsEnd())
			{
				outError = "Unexpected end of JSON.";
				return false;
			}

			const char current = Peek();
			if (current == '{')
			{
				return ParseObject(outValue, outError);
			}
			if (current == '[')
			{
				return ParseArray(outValue, outError);
			}
			if (current == '"')
			{
				std::string value;
				if (!ParseString(value, outError))
				{
					return false;
				}
				outValue = JsonValue::MakeString(value);
				return true;
			}
			if (current == '-' || std::isdigit(static_cast<unsigned char>(current)) != 0)
			{
				return ParseNumber(outValue, outError);
			}
			if (MatchLiteral("true"))
			{
				outValue = JsonValue::MakeBool(true);
				return true;
			}
			if (MatchLiteral("false"))
			{
				outValue = JsonValue::MakeBool(false);
				return true;
			}
			if (MatchLiteral("null"))
			{
				outValue = JsonValue::MakeNull();
				return true;
			}

			outError = "Unexpected token in JSON.";
			return false;
		}

		bool ParseObject(JsonValue& outValue, std::string& outError)
		{
			JsonValue::Object object;
			Advance();
			SkipWhitespace();

			if (Peek() == '}')
			{
				Advance();
				outValue = JsonValue::MakeObject(object);
				return true;
			}

			while (!IsEnd())
			{
				SkipWhitespace();
				std::string key;
				if (!ParseString(key, outError))
				{
					return false;
				}

				SkipWhitespace();
				if (Advance() != ':')
				{
					outError = "Expected ':' after object key.";
					return false;
				}

				JsonValue value;
				if (!ParseValue(value, outError))
				{
					return false;
				}
				object[key] = value;

				SkipWhitespace();
				const char delimiter = Advance();
				if (delimiter == '}')
				{
					outValue = JsonValue::MakeObject(object);
					return true;
				}
				if (delimiter != ',')
				{
					outError = "Expected ',' or '}' in object.";
					return false;
				}
			}

			outError = "Unterminated object.";
			return false;
		}

		bool ParseArray(JsonValue& outValue, std::string& outError)
		{
			JsonValue::Array array;
			Advance();
			SkipWhitespace();

			if (Peek() == ']')
			{
				Advance();
				outValue = JsonValue::MakeArray(array);
				return true;
			}

			while (!IsEnd())
			{
				JsonValue value;
				if (!ParseValue(value, outError))
				{
					return false;
				}
				array.push_back(value);

				SkipWhitespace();
				const char delimiter = Advance();
				if (delimiter == ']')
				{
					outValue = JsonValue::MakeArray(array);
					return true;
				}
				if (delimiter != ',')
				{
					outError = "Expected ',' or ']' in array.";
					return false;
				}
			}

			outError = "Unterminated array.";
			return false;
		}

		bool ParseString(std::string& outValue, std::string& outError)
		{
			if (Advance() != '"')
			{
				outError = "Expected string.";
				return false;
			}

			outValue.clear();
			while (!IsEnd())
			{
				const char current = Advance();
				if (current == '"')
				{
					return true;
				}

				if (current == '\\')
				{
					if (IsEnd())
					{
						outError = "Unterminated escape sequence.";
						return false;
					}

					const char escaped = Advance();
					switch (escaped)
					{
					case '"': outValue.push_back('"'); break;
					case '\\': outValue.push_back('\\'); break;
					case '/': outValue.push_back('/'); break;
					case 'b': outValue.push_back('\b'); break;
					case 'f': outValue.push_back('\f'); break;
					case 'n': outValue.push_back('\n'); break;
					case 'r': outValue.push_back('\r'); break;
					case 't': outValue.push_back('\t'); break;
					default:
						outError = "Unsupported escape sequence.";
						return false;
					}
					continue;
				}

				outValue.push_back(current);
			}

			outError = "Unterminated string.";
			return false;
		}

		bool ParseNumber(JsonValue& outValue, std::string& outError)
		{
			const size_t start = position;
			if (Peek() == '-')
			{
				Advance();
			}

			while (!IsEnd() && std::isdigit(static_cast<unsigned char>(Peek())) != 0)
			{
				Advance();
			}

			if (Peek() == '.')
			{
				Advance();
				while (!IsEnd() && std::isdigit(static_cast<unsigned char>(Peek())) != 0)
				{
					Advance();
				}
			}

			if (Peek() == 'e' || Peek() == 'E')
			{
				Advance();
				if (Peek() == '+' || Peek() == '-')
				{
					Advance();
				}
				while (!IsEnd() && std::isdigit(static_cast<unsigned char>(Peek())) != 0)
				{
					Advance();
				}
			}

			char* endPointer = nullptr;
			const std::string numberText = source.substr(start, position - start);
			const double value = std::strtod(numberText.c_str(), &endPointer);
			if (endPointer == numberText.c_str())
			{
				outError = "Invalid number.";
				return false;
			}

			outValue = JsonValue::MakeNumber(value);
			return true;
		}
	};
}

JsonValue JsonValue::MakeNull()
{
	return JsonValue();
}

JsonValue JsonValue::MakeBool(bool value)
{
	JsonValue jsonValue;
	jsonValue.type = Type::Bool;
	jsonValue.boolValue = value;
	return jsonValue;
}

JsonValue JsonValue::MakeNumber(double value)
{
	JsonValue jsonValue;
	jsonValue.type = Type::Number;
	jsonValue.numberValue = value;
	return jsonValue;
}

JsonValue JsonValue::MakeString(const std::string& value)
{
	JsonValue jsonValue;
	jsonValue.type = Type::String;
	jsonValue.stringValue = value;
	return jsonValue;
}

JsonValue JsonValue::MakeArray(const Array& value)
{
	JsonValue jsonValue;
	jsonValue.type = Type::Array;
	jsonValue.arrayValue = value;
	return jsonValue;
}

JsonValue JsonValue::MakeObject(const Object& value)
{
	JsonValue jsonValue;
	jsonValue.type = Type::Object;
	jsonValue.objectValue = value;
	return jsonValue;
}

JsonValue::Type JsonValue::GetType() const
{
	return type;
}

bool JsonValue::IsNull() const
{
	return type == Type::Null;
}

bool JsonValue::IsBool() const
{
	return type == Type::Bool;
}

bool JsonValue::IsNumber() const
{
	return type == Type::Number;
}

bool JsonValue::IsString() const
{
	return type == Type::String;
}

bool JsonValue::IsArray() const
{
	return type == Type::Array;
}

bool JsonValue::IsObject() const
{
	return type == Type::Object;
}

bool JsonValue::AsBool(bool defaultValue) const
{
	return IsBool() ? boolValue : defaultValue;
}

double JsonValue::AsNumber(double defaultValue) const
{
	return IsNumber() ? numberValue : defaultValue;
}

const std::string& JsonValue::AsString() const
{
	return IsString() ? stringValue : EmptyString;
}

const JsonValue::Array& JsonValue::AsArray() const
{
	return IsArray() ? arrayValue : EmptyArray;
}

const JsonValue::Object& JsonValue::AsObject() const
{
	return IsObject() ? objectValue : EmptyObject;
}

const JsonValue* JsonValue::Find(const std::string& key) const
{
	if (!IsObject())
	{
		return nullptr;
	}

	const auto found = objectValue.find(key);
	return found == objectValue.end() ? nullptr : &found->second;
}

bool JsonParser::Parse(const std::string& text, JsonValue& outValue, std::string& outError)
{
	Parser parser(text);
	return parser.Parse(outValue, outError);
}
