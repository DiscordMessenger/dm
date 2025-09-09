//
//  iProgramInCpp's JSON Parser
// 
//  Copyright (C) 2025 iProgramInCpp.  Licensed under the MIT License.
//

#pragma once
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <climits>

#include <string>
#include <map>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include <type_traits>

// This is iProgramInCpp's JSON parser, which hopes to be faster than nlohmann::json while being
// semi-compatible with it.
// 
// This was designed for Purplecord which is an iOS 3 compatible Discord client, based on Discord
// Messenger.
//
// NOTE: This is not going to be very standards-compliant, it'll only support a small subset of JSON.

// Disable exceptions.  Use if using exceptions is not allowed - the program will terminate on error instead.
//#define IPROG_JSON_DISABLE_EXCEPTIONS

// Enable keeping the original string that determines a JSON object.
// This substantially increases the memory usage, but allows for easier debugging.
//#define IPROG_JSON_ENABLE_ORIGINAL_STRING

namespace iprog
{
	enum class eType {
		Null,
		Number,
		Decimal,
		Boolean,
		StringShort,
		String,
		Struct,
		Array
	};

	class JsonUtil
	{
	public:
		static uint32_t decode_utf8(const char* byteSeq, int& sizeOut)
		{
			sizeOut = 1;

			const uint8_t * chars = (const uint8_t*)byteSeq;
			uint8_t char0 = chars[0];

			if (char0 < 0x80)
			{
				// 1 byte ASCII character. Fine
				sizeOut = 1;
				return char0;
			}

			uint8_t char1 = chars[1];
			if (char1 == 0) return 0xFFFD; // this character is broken.
			if (char0 < 0xE0)
			{
				// 2 byte character.
				uint32_t codepoint = ((char0 & 0x1F) << 6) | (char1 & 0x3F);
				sizeOut = 2;
				return codepoint;
			}

			uint8_t char2 = chars[2];
			if (char2 == 0) return 0xFFFD; // this character is broken.
			if (char0 < 0xF0)
			{
				// 3 byte character.
				uint32_t codepoint = ((char0 & 0xF) << 12) | ((char1 & 0x3F) << 6) | (char2 & 0x3F);
				sizeOut = 3;
				return codepoint;
			}

			uint8_t char3 = chars[3];
			// 4 byte character.
			if (char3 == 0) return 0xFFFD;

			uint32_t codepoint = ((char0 & 0x07) << 18) | ((char1 & 0x3F) << 12) | ((char2 & 0x3F) << 6) | (char3 & 0x3F);
			sizeOut = 4;
			return codepoint;
		}
	};

	class JsonObject;

	// This struct defines a name.  Usually it's shorter than 28 characters, so we will be able to
	// avoid an expensive heap operation by storing the data directly inside.
	class JsonName
	{
	public:
		JsonName()
		{
			length = 0;
		}

		JsonName(const char* name, size_t len = 0)
		{
			if (len == 0)
				len = strlen(name);

			// cut off
			if (len > INT_MAX)
				len = INT_MAX;

			length = (int) len;
			if (len > sizeof(data.nameShort)) {
				data.nameLong = new char[len];
				memcpy(data.nameLong, name, len);
			}
			else {
				memcpy(data.nameShort, name, len);
			}
		}

		JsonName(const std::string& name) : JsonName(name.c_str(), name.size()) {}

		// move constructor can remain simple
		JsonName(JsonName&& other)
		{
			memcpy((void*) this, (void*) &other, sizeof(JsonName));
			memset((void*) &other, 0, sizeof(JsonName));
		}

		// but not the copy constructor
		JsonName(const JsonName& other) : JsonName(other.RawData(), other.Length()) {}

		~JsonName()
		{
			if (IsLong())
				delete[] data.nameLong;
		}

		JsonName& operator=(const JsonName& other)
		{
			if (this != &other) {
				JsonName temp(other);
				Swap(temp);
			}
			return *this;
		}

		JsonName& operator=(JsonName&& other)
		{
			if (this != &other) {
				JsonName temp(std::move(other));
				Swap(temp);
			}
			return *this;
		}

		// NOTE: Does NOT return a null terminated string! You must call Length() separately.
		const char* RawData() const
		{
			return IsLong() ? data.nameLong : data.nameShort;
		}

		std::string Data() const
		{
			if (IsLong())
				return std::string(data.nameLong, length);
			else
				return std::string(data.nameShort, length);
		}

		int Length() const { return length; }

		bool Equals(const char* data, size_t len = 0) const noexcept
		{
			if (len == 0)
				len = strlen(data);

			if (Length() != len)
				return false;

			return memcmp(RawData(), data, Length()) == 0;
		}

	private:
		void Swap(JsonName& other) noexcept
		{
			std::swap(length, other.length);
			std::swap(data, other.data);
		}

		bool IsLong() const { return length > sizeof(data.nameShort); }

		int length;
		union {
			char nameShort[28];
			char* nameLong;
		}
		data;
	};

	class JsonObject
	{
	public:
		// Constructors

		// Default constructor
		JsonObject() : type(eType::Null)
		{
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			fullParsedString = std::make_shared<std::string>("Null created from constructor");
#endif
		}

		// Move constructor - can be kept trivial
		JsonObject(JsonObject&& other)
		{
			type = other.type;
			data = other.data;
			memset(&other.data, 0, sizeof(other.data));
			other.type = eType::Null;

#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			originalString = std::move(other.originalString);
			fullParsedString = other.fullParsedString;
#endif
		}

		// Copy constructor - a bit more complicated
		JsonObject(const JsonObject& other)
		{
			type = other.type;

#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			originalString = other.originalString;
			fullParsedString = other.fullParsedString;
#endif

			switch (other.type)
			{
				case eType::Null:
					// Do Nothing
					break;

				case eType::Number:
					data.number = other.data.number;
					break;

				case eType::Decimal:
					data.decimal = other.data.decimal;
					break;

				case eType::Boolean:
					data.boolean = other.data.boolean;
					break;

				case eType::StringShort:
					memcpy(&data.stringShort, &other.data.stringShort, sizeof data.stringShort);
					break;

				case eType::String:
					data.string.data = new char[other.data.string.size];
					data.string.size = other.data.string.size;
					memcpy(data.string.data, other.data.string.data, other.data.string.size);
					break;

				case eType::Struct:
					// copy the names
					data.structured.names = (JsonName*) ::operator new[](other.data.structured.itemCount * sizeof(JsonName));
					for (size_t i = 0; i < other.data.structured.itemCount; i++) {
						new (&data.structured.names[i]) JsonName(other.data.structured.names[i]);
					}

					// fall through
				case eType::Array:
					// copy the objects
					data.structured.array = (JsonObject*) ::operator new[](other.data.structured.itemCount * sizeof(JsonObject));
					for (size_t i = 0; i < other.data.structured.itemCount; i++)
						new (&data.structured.array[i]) JsonObject(other.data.structured.array[i]);

					data.structured.itemCount = other.data.structured.itemCount;
					break;
			}
		}

		// Destructor
		~JsonObject()
		{
			switch (type)
			{
				case eType::String:
					delete[] data.string.data;
					break;

				case eType::Struct:
				case eType::Array:
					internal_clear();
					break;

				default:
					// Do Nothing
					break;
			}
		}
		
		// Copy equals
		JsonObject& operator=(const JsonObject& other)
		{
			if (this != &other) {
				JsonObject temp(other);
				swap(temp);
			}

			return *this;
		}

		// Move equals
		JsonObject& operator=(JsonObject&& other) noexcept
		{
			if (this != &other) {
				this->~JsonObject();
				new (this) JsonObject(std::move(other));
			}

			return *this;
		}
		
		// Template assignment equals
		template<typename T, typename = std::enable_if_t<(std::is_integral_v<T> && !std::is_same_v<T, bool>) || std::is_enum_v<T>>>
		JsonObject& operator=(T a)
		{
			*this = JsonObject(static_cast<int64_t>(a));
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			fullParsedString = std::make_shared<std::string>("Number created from equals operator");
#endif
			return *this;
		}
		
		template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_same_v<T, JsonObject>>>
		JsonObject& operator=(T (&arr)[N])
		{
			*this = JsonObject::array();
			this->internal_resize(N);

			for (size_t i = 0; i < N; ++i)
				(*this)[i] = JsonObject(arr[i]);

#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			fullParsedString = std::make_shared<std::string>("Array created from equals operator");
#endif
			return *this;
		}
		
		// Assignment equals
		JsonObject& operator=(std::nullptr_t a)     { *this = JsonObject(); return *this; }
		JsonObject& operator=(bool a)               { *this = JsonObject(a); return *this; }
		JsonObject& operator=(double a)             { *this = JsonObject(a); return *this; }
		JsonObject& operator=(const char* a)        { *this = JsonObject(a); return *this; }
		JsonObject& operator=(const std::string& a) { *this = JsonObject(a); return *this; }
		
		// Swap
		void swap(JsonObject& other) noexcept
		{
			std::swap(type, other.type);
			std::swap(data, other.data);
		}

		// Initialize Number
		template<typename T, typename = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
		JsonObject(T number)
		{
			type = eType::Number;
			data.number = static_cast<int64_t>(number);
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			fullParsedString = std::make_shared<std::string>("Number created from constructor");
#endif
		}

		// Initialize Decimal
		JsonObject(double decimal)
		{
			type = eType::Decimal;
			data.decimal = decimal;
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			fullParsedString = std::make_shared<std::string>("Decimal created from constructor");
#endif
		}

		// Initialize Boolean
		JsonObject(bool boolean)
		{
			type = eType::Boolean;
			data.boolean = boolean;
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			fullParsedString = std::make_shared<std::string>("Boolean created from constructor");
#endif
		}

		// Initialize String
		JsonObject(const char* str, size_t len = 0)
		{
			if (len == 0)
				len = strlen(str);

			if (len > sizeof(data.stringShort.data))
			{
				type = eType::String;
				data.string.size = len;
				data.string.data = new char[len];
				memcpy(data.string.data, str, len);
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				fullParsedString = std::make_shared<std::string>("String created from constructor");
#endif
			}
			else
			{
				type = eType::StringShort;
				data.stringShort.size = (char)len;
				memcpy(data.stringShort.data, str, len);
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				fullParsedString = std::make_shared<std::string>("StringShort created from constructor");
#endif
			}
		}

		JsonObject(const std::string& str) : JsonObject(str.c_str(), str.size()) {}

		// Initialize Array
		static JsonObject array()
		{
			JsonObject obj;
			obj.type = eType::Array;
			obj.data.structured.itemCount = 0;
			obj.data.structured.array = nullptr;
			obj.data.structured.names = nullptr;
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			obj.fullParsedString = std::make_shared<std::string>("Array created from array()");
#endif
			return obj;
		}

		// Initialize Struct
		static JsonObject object()
		{
			JsonObject obj;
			obj.type = eType::Struct;
			obj.data.structured.itemCount = 0;
			obj.data.structured.array = nullptr;
			obj.data.structured.names = nullptr;
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			obj.fullParsedString = std::make_shared<std::string>("Struct created from object()");
#endif
			return obj;
		}

		// Type Checks
		constexpr bool is_structured() const { return type == eType::Struct || type == eType::Array; }

		constexpr bool is_null() const { return type == eType::Null; }

		constexpr bool is_primitive() const { return type == eType::Boolean || type == eType::Decimal || type == eType::Number || type == eType::Null; }

		constexpr bool is_array() const { return type == eType::Array; }
		
		constexpr bool is_object() const { return type == eType::Struct; }

		constexpr bool is_boolean() const { return type == eType::Boolean; }
		
		constexpr bool is_number() const { return type == eType::Decimal || type == eType::Number; }
		
		constexpr bool is_number_float() const { return type == eType::Decimal; }
		
		constexpr bool is_string() const { return type == eType::String || type == eType::StringShort; }
		
		// INCOMPATIBILITY: is_number_integer() and is_number_unsigned() can be true at the same time!
		// Nlohmann::json does not support this by default, but for my use case it's fine.
		constexpr bool is_number_integer() const { return type == eType::Number; }

		constexpr bool is_number_unsigned() const { return type == eType::Number; }

		// Equality
		bool operator==(const JsonObject& other) const noexcept
		{
			if (type != other.type) return false;

			switch (type)
			{
				case eType::Number: return data.number == other.data.number;
				case eType::Decimal: return data.decimal == other.data.decimal;
				case eType::Boolean: return data.boolean == other.data.boolean;

				case eType::StringShort:
					if (data.stringShort.size != other.data.stringShort.size)
						return false;

					return memcmp(data.stringShort.data, other.data.stringShort.data, data.stringShort.size) == 0;

				case eType::String:
					if (data.string.size != other.data.string.size)
						return false;

					return memcmp(data.string.data, other.data.string.data, data.string.size) == 0;

				case eType::Array:
					if (data.structured.itemCount != other.data.structured.itemCount)
						return false;

					for (size_t i = 0; i < data.structured.itemCount; i++)
						if (data.structured.array[i] != other.data.structured.array[i])
							return false;

					break;

				case eType::Struct:
				{
					if (data.structured.itemCount != other.data.structured.itemCount)
						return false;

					// Big Expensive Check
					for (size_t i = 0; i < data.structured.itemCount; i++)
					{
						int nameLength = data.structured.names[i].Length();

						for (size_t j = i + 1; j < other.data.structured.itemCount; j++)
						{
							int otherNameLength = other.data.structured.names[j].Length();
							if (otherNameLength != nameLength)
								continue;

							// name length same, compare data
							if (memcmp(data.structured.names[i].RawData(), other.data.structured.names[j].RawData(), nameLength) != 0)
								continue;

							// names same, check equality
							if (data.structured.array[i] != other.data.structured.array[j])
								return false;
						}
					}

					return true;
				}
			}

			return false;
		}

		bool operator!=(const JsonObject& other) const noexcept { return !(*this == other); }
		
		// Contains
		bool contains(const char* key, size_t keyLen = 0) const
		{
			if (is_null())
				// null objects CAN be placeholders for missing structures, so handle this case.
				// however, they are null, so obviously they can't have references to anything
				return false;
			
			if (!is_object())
				throw_error("json object is not a structure");

			if (keyLen == 0)
				keyLen = strlen(key);

			// If there are no names but there are items in the list
			if (!data.structured.names && data.structured.itemCount)
			{
				fprintf(stderr, "ERROR: No data.structured.names and yet the object has %zu items!\n", data.structured.itemCount);
				fflush(stderr);
				
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				if (fullParsedString) {
					fprintf(stderr, "Printing full string that this was parsed from: <string>%s</string>\n", fullParsedString->c_str());
					fflush(stderr);
				}
				else {
					fprintf(stderr, "NO full parsed string was assigned :(\n");
				}
				
				fprintf(stderr, "Printing original string: <string>%s</string>\n", originalString.c_str());
				fflush(stderr);
#endif
				
				fprintf(stderr, "Dumping object:\n");
				fflush(stderr);
				
				const std::string& obj = dump();
				fprintf(stderr, "%s\n", obj.c_str());
				fflush(stderr);
			}
			
			assert(data.structured.names || !data.structured.itemCount);
			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.names[i].Equals(key, keyLen))
					return true;
			}

			return false;
		}
		
		bool contains(const std::string& str) const { return contains(str.c_str(), str.size()); }

		// Array Access
		template<typename SizeType, typename = std::enable_if_t<std::is_integral_v<SizeType> && !std::is_same_v<SizeType, bool> && !std::is_pointer_v<SizeType>>>
		JsonObject& operator[](SizeType index)
		{
			// note: can't be adapted into an array here since any access
			// performed would be out of bounds
			if (!is_array())
				throw_error("json object is not an array");

			if (index >= data.structured.itemCount)
				throw_error("index out of bounds");

			return data.structured.array[index];
		}
		
		// Struct Access
		JsonObject& get_key(const char* key, size_t keyLen = 0)
		{
			if (is_null())
				// can be re-adapted into an array
				*this = JsonObject::object();
			else if (!is_object())
				throw_error("json object is not a structure");

			if (keyLen == 0)
				keyLen = strlen(key);

			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.names[i].Equals(key, keyLen))
					return data.structured.array[i];
			}

			internal_resize(data.structured.itemCount + 1);

			data.structured.names[data.structured.itemCount - 1] = JsonName(key);
			return data.structured.array[data.structured.itemCount - 1];
		}

		const JsonObject& get_key_cst(const char* key, size_t keyLen = 0) const
		{
			if (!is_object())
				throw_error("json object is not a structure");

			if (keyLen == 0)
				keyLen = strlen(key);

			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.names[i].Equals(key, keyLen))
					return data.structured.array[i];
			}

			throw_error("key does not exist");
		}

		JsonObject& operator[](const char* key) { return get_key(key, 0); }
		JsonObject& operator[](const std::string& key) { return get_key(key.c_str(), key.size()); }
		const JsonObject& operator[](const char* key) const { return get_key_cst(key, 0); }
		const JsonObject& operator[](const std::string& key) const { return get_key_cst(key.c_str(), key.size()); }
		
		// Push Back
		void push_back(const JsonObject& object)
		{
			if (is_null())
				// can be re-adapted into an array
				*this = JsonObject::array();
			else if (!is_array())
				throw_error("push_back() json object must be array");

			size_t sz = data.structured.itemCount;
			internal_resize(sz + 1);
			data.structured.array[sz] = object;
		}

		// Empty
		bool empty() const
		{
			if (is_null())
				return true;

			if (!is_structured())
				throw_error("empty() json object must be structured");

			return data.structured.itemCount == 0;
		}

		// Getters
		template <typename T, std::enable_if_t<
			(std::is_integral_v<T> || std::is_enum_v<T>)
			&& !std::is_same_v<T, bool>
			&& !std::is_same_v<T, char>, int> = 0>
		operator T() const
		{
			if (!is_number_integer())
				throw_error("json object not a number");

			return static_cast<T>(data.number);
		}
		
		template <typename T, std::enable_if_t<std::is_same_v<T, bool>, int> = 0>
		operator T() const
		{
			if (!is_boolean())
				throw_error("json object not a boolean");

			return static_cast<T>(data.boolean);
		}
		
		explicit operator double() const
		{
			if (!is_number_float())
				throw_error("json object not a decimal");

			return data.decimal;
		}

		operator std::string() const
		{
			if (!is_string())
				throw_error("json object not a string");

			if (type == eType::StringShort)
				return std::string(data.stringShort.data, data.stringShort.size);
			else
				return std::string(data.string.data, data.string.size);
		}

		// Size
		size_t size() const noexcept
		{
			switch (type)
			{
				case eType::Null:
					return 0;
				case eType::Struct:
				case eType::Array:
					return data.structured.itemCount;
				default:
					return 1;
			}
		}

		// Iterators
		class iterator
		{
			/// iterator_category, value_type, difference_type, pointer, and reference.
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = JsonObject;
			using pointer = JsonObject*;
			using reference = JsonObject&;
			using difference_type = size_t;

		public:
			iterator(JsonName* namePtr, JsonObject* objectPtr) : name(namePtr), object(objectPtr) {}

			reference operator*() const { return *object; }
			pointer operator->() const { return object; }

			// increments and decrements
			iterator& operator++()
			{
				if (name)
					name++;
				object++;
				return *this;
			}

			iterator operator++(int)
			{
				iterator tmp = *this;
				++(*this);
				return tmp;
			}

			iterator& operator--()
			{
				if (name)
					name--;
				object--;
				return *this;
			}

			iterator operator--(int)
			{
				iterator tmp = *this;
				--(*this);
				return tmp;
			}

			friend bool operator==(const iterator& a, const iterator& b) { return a.object == b.object; }
			friend bool operator!=(const iterator& a, const iterator& b) { return a.object != b.object; }

			std::string key() const
			{
				if (!name)
					throw_error("cannot get key from array iterator");

				return name->Data();
			}

			reference value() const { return *object; }

		private:
			JsonName* name;
			JsonObject* object;
		};

		class const_iterator
		{
			/// iterator_category, value_type, difference_type, pointer, and reference.
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = JsonObject;
			using pointer = const JsonObject*;
			using reference = const JsonObject&;
			using difference_type = size_t;

		public:
			const_iterator(JsonName* namePtr, JsonObject* objectPtr) : name(namePtr), object(objectPtr) {}

			reference operator*() const { return *object; }
			pointer operator->() const { return object; }

			// increments and decrements
			const_iterator& operator++()
			{
				if (name)
					name++;
				object++;
				return *this;
			}

			const_iterator operator++(int)
			{
				const_iterator tmp = *this;
				++(*this);
				return tmp;
			}

			const_iterator& operator--()
			{
				if (name)
					name--;
				object--;
				return *this;
			}

			const_iterator operator--(int)
			{
				const_iterator tmp = *this;
				--(*this);
				return tmp;
			}

			friend bool operator==(const const_iterator& a, const const_iterator& b) { return a.object == b.object; }
			friend bool operator!=(const const_iterator& a, const const_iterator& b) { return a.object != b.object; }

			std::string key() const
			{
				if (!name)
					throw_error("cannot get key from array iterator");

				return name->Data();
			}

			reference value() const { return *object; }

		private:
			const JsonName* name;
			const JsonObject* object;
		};

		iterator begin()
		{
			if (is_null())
				return iterator(nullptr, nullptr);

			if (!is_structured())
				throw_error("json object cannot be iterated");

			return iterator(
				is_object() ? data.structured.names : nullptr,
				data.structured.array
			);
		}

		iterator end()
		{
			if (is_null())
				return iterator(nullptr, nullptr);

			// I will cheat by omitting the structured checks here, because we use iterator::end()
			// significantly more times than iterator::begin().  Do not use ::end() if this is not
			// an array or object, otherwise this is UB.
			return iterator(
				is_object() ? data.structured.names + data.structured.itemCount : nullptr,
				data.structured.array + data.structured.itemCount
			);
		}

		const_iterator begin() const
		{
			if (!is_structured())
				throw_error("json object cannot be iterated");

			return const_iterator(
				is_object() ? data.structured.names : nullptr,
				data.structured.array
			);
		}

		const_iterator end() const
		{
			// I will cheat by omitting the structured checks here, because we use iterator::end()
			// significantly more times than iterator::begin().  Do not use ::end() if this is not
			// an array or object, otherwise this is UB.
			return const_iterator(
				is_object() ? data.structured.names + data.structured.itemCount : nullptr,
				data.structured.array + data.structured.itemCount
			);
		}
		
		iterator find(const char* key, size_t keyLen = 0)
		{
			if (!is_object())
				throw_error("find() json object must be structured object");

			if (keyLen == 0)
				keyLen = strlen(key);

			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.names[i].Equals(key, keyLen))
				{
					return iterator(
						&data.structured.names[i],
						&data.structured.array[i]
					);
				}
			}

			return end();
		}
		
		const_iterator find(const char* key, size_t keyLen = 0) const
		{
			if (!is_object())
				throw_error("find() json object must be structured object");

			if (keyLen == 0)
				keyLen = strlen(key);

			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.names[i].Equals(key, keyLen))
				{
					return const_iterator(
						&data.structured.names[i],
						&data.structured.array[i]
					);
				}
			}

			return end();
		}
		
		iterator find(const std::string& key)
		{
			return find(key.c_str(), key.size());
		}
		
		const_iterator find(const std::string& key) const
		{
			return find(key.c_str(), key.size());
		}
		
		iterator find(const JsonObject& value)
		{
			if (!is_structured())
				throw_error("find() json object must be structured object");

			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.array[i] == value)
				{
					return iterator(
						is_object() ? &data.structured.names[i] : nullptr,
						&data.structured.array[i]
					);
				}
			}

			return end();
		}
		
		const_iterator find(const JsonObject& value) const
		{
			if (!is_structured())
				throw_error("find() json object must be structured object");

			for (size_t i = 0; i < data.structured.itemCount; i++)
			{
				if (data.structured.array[i] == value)
				{
					return const_iterator(
						is_object() ? &data.structured.names[i] : nullptr,
						&data.structured.array[i]
					);
				}
			}

			return end();
		}

		// JSON Generation
		std::string dump() const
		{
			switch (type)
			{
				case eType::Null: return "null";
				case eType::Boolean: return data.boolean ? "true" : "false";
				case eType::Number: return std::to_string(data.number);
				case eType::Decimal: return std::to_string(data.decimal);
				case eType::String: return "\"" + escape_string(std::string(data.string.data, data.string.size)) + "\"";
				case eType::StringShort: return "\"" + escape_string(std::string(data.stringShort.data, data.stringShort.size)) + "\"";
			}
			
			std::string dump = "[";
			switch (type)
			{
				case eType::Array:
				{
				dumpAsArray:
					bool first = true;

					for (size_t i = 0; i < data.structured.itemCount; i++)
					{
						if (!first)
							dump += ",";
						
						first = false;

						dump += data.structured.array[i].dump();
					}

					dump += "]";
					return dump;
				}
				
				case eType::Struct:
				{
					if (!data.structured.names)
					{
						dump = "??[ERROR ERROR CORRUPTED - Struct missing names, dumping as array]??[";
						goto dumpAsArray;
					}
					
					dump = "{";
					bool first = true;

					for (size_t i = 0; i < data.structured.itemCount; i++)
					{
						if (!first)
							dump += ",";

						first = false;

						dump += "\"" + data.structured.names[i].Data() + "\":";
						dump += data.structured.array[i].dump();
					}

					dump += "}";
					return dump;
				}

				default: return "??";
			}
		}

		// Debug
		const char* get_type() const
		{
			switch (type)
			{
				case eType::Null: return "Null";
				case eType::String: return "String";
				case eType::StringShort: return "StringShort";
				case eType::Number: return "Number";
				case eType::Decimal: return "Decimal";
				case eType::Boolean: return "Boolean";
				case eType::Array: return "Array";
				case eType::Struct: return "Struct";
				default: return "???";
			}
		}

		static std::string escape_string(const std::string& str)
		{
			std::string new_string;
			new_string.reserve(str.size() * 2);

			for (size_t i = 0; i < str.size(); i++)
			{
				char chr = str[i];
				
				// common escapes
				if (chr == 0) new_string += "\0";
				else if (chr == '\a') new_string += "\\a";
				else if (chr == '\b') new_string += "\\b";
				else if (chr == '\x1F') new_string += "\\e";
				else if (chr == '\n') new_string += "\\n";
				else if (chr == '\t') new_string += "\\t";
				else if (chr == '\v') new_string += "\\v";
				else if (chr == '\f') new_string += "\\f";
				else if (chr == '"') new_string += "\\\"";
				else if (chr == '\\') new_string += "\\\\";
				// UTF-8
				else if (chr < 0) {
					new_string += utf8_to_unicode_escape(str.c_str() + i, i);
					i--;
				}
				// default
				else new_string += chr;
			}

			return new_string;
		}
		
		static std::string utf8_to_unicode_escape(const char* ptr, size_t& i)
		{
			int szOut = 0;
			uint32_t code = JsonUtil::decode_utf8(ptr, szOut);

			if (szOut == 0) szOut = 1;
			i += szOut;

			char buff[16];
			if (code < 0x10000)
			{
				snprintf(buff, sizeof buff, "\\u%04X", code);
			}
			else
			{
				uint32_t n = code - 0x10000;
				uint32_t high = 0xD800 + (n >> 10);
				uint32_t low  = 0xDC00 + (n & 0x3FF);
				snprintf(buff, sizeof buff, "\\u%04X\\u%04X", high, low);
			}

			return std::string(buff);
		}

	protected:
		friend class JsonParser;

		// Exception
		[[noreturn]]
		static void throw_error(const char* message, ...)
		{
			va_list vl;
			va_start(vl, message);

#ifdef IPROG_JSON_DISABLE_EXCEPTIONS
			fprintf(stderr, "JSON error: ");
			vfprintf(stderr, message, vl);
			fflush(stderr);
			std::terminate();
#else
			char buffer[512];
			vsnprintf(buffer, sizeof buffer, message, vl);
			buffer[sizeof buffer - 1] = 0;

			throw std::runtime_error(buffer);
#endif
		}

		// Array Manipulation
		void internal_clear()
		{
			assert(is_structured());

			bool isObject = is_object();

			if (isObject && data.structured.names)
			{
				// since we created the names array using placement new, we must use placement delete
				for (size_t i = 0; i < data.structured.itemCount; i++)
					data.structured.names[i].~JsonName();

				::operator delete[](data.structured.names);
			}

			if (data.structured.array)
			{
				for (size_t i = 0; i < data.structured.itemCount; i++)
					data.structured.array[i].~JsonObject();

				::operator delete[](data.structured.array);
			}
			
			data.structured.array = nullptr;
			data.structured.names = nullptr;
			data.structured.itemCount = 0;
		}

		void internal_resize(size_t newSize)
		{
			// TODO: Exception safety.  It's neglected for speed right now.
			// The only exception that can be thrown here is std::bad_alloc, which is bad news anyway.
			assert(is_structured());

			if (newSize == 0)
			{
				internal_clear();
				return;
			}

			JsonObject* newObjects = nullptr;
			JsonName* newNames = nullptr;
			
			if (newSize > 50000 || newSize == 0) {
				fprintf(stderr, "ATOMIC NUCLEAR ALERT: Trying to allocate %zu items", newSize);
				fflush(stderr);
			}

			newObjects = (JsonObject*) ::operator new[](newSize * sizeof(JsonObject));
			if (is_object())
				newNames = (JsonName*) ::operator new[](newSize * sizeof(JsonName));

			size_t objectsToCopy = data.structured.itemCount;
			if (objectsToCopy > newSize)
				objectsToCopy = newSize;

			// copy the objects we have to copy
			for (size_t i = 0; i < objectsToCopy; i++)
			{
				new (&newObjects[i]) JsonObject(std::move(data.structured.array[i]));

				if (newNames)
					new (&newNames[i]) JsonName(std::move(data.structured.names[i]));
			}

			// and default initialize the rest
			for (size_t i = objectsToCopy; i < newSize; i++)
			{
				new (&newObjects[i]) JsonObject();

				if (newNames)
					new (&newNames[i]) JsonName();
			}

			// destroy the old data
			internal_clear();

			// replace it with the new data
			data.structured.array = newObjects;
			data.structured.names = newNames;
			data.structured.itemCount = newSize;
		}

		void internal_set_at(size_t index, JsonObject&& object)
		{
			data.structured.array[index] = std::move(object);
		}

		void internal_set_at(size_t index, const std::string& name, JsonObject&& object)
		{
			data.structured.array[index] = std::move(object);
			data.structured.names[index] = JsonName(name);
		}

	private:
		eType type = eType::Null;

		union
		{
			// valid for eType::Number
			int64_t number;

			// valid for eType::Decimal
			double decimal;

			// valid for eType::Boolean
			bool boolean;

			// valid for eType::StringShort
			struct {
				char data[31];
				char size;
			}
			stringShort;

			// valid for eType::String
			struct {
				char* data;
				size_t size;
			}
			string;

			// structured.names is only valid for eType::Struct,
			// structured.array and structured.itemCount are valid for eType::Array too
			struct {
				size_t itemCount;
				JsonObject* array;
				JsonName* names;
			}
			structured;
		}
		data;
		
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
		std::string originalString;
		std::shared_ptr<std::string> fullParsedString;
		
	public:
		void set_original_string(const char* str, size_t len = 0)
		{
			if (len == 0)
				len = strlen(str);
			
			originalString = std::string(str, len);
		}
		
		void set_original_string(const std::string& str) { set_original_string(str.c_str(), str.size()); }
		
		void set_full_parsed_string(std::shared_ptr<std::string> str)
		{
			fullParsedString = str;
		}
#endif
	};

	class JsonParser
	{
	public:
		JsonParser() {}
		~JsonParser() {}

	public:
		// Public Interfaces
		static JsonObject parse(const char* data, size_t len = 0)
		{
			if (len == 0)
				len = strlen(data);

			JsonParser parser;
			parser.set_data_input(data, len);
			return parser.parse();
		}
		static JsonObject parse(const std::string& data)
		{
			return parse(data.c_str(), data.size());
		}

	public:
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
		JsonObject _parse()
#else
		JsonObject parse()
#endif
		{
			if (reached_end())
				JsonObject::throw_error("no object");

			// what kind of object is this?
			char firstCharacter = get_skip();

			if (firstCharacter == '{')
				return parse_object();

			if (firstCharacter == '[')
				return parse_array();

			if (firstCharacter == '"')
				return parse_string();

			if (firstCharacter == 'f' || firstCharacter == 't' || firstCharacter == 'n')
			{
				unget();
				return parse_boolean_or_null();
			}

			if (firstCharacter == '-' || firstCharacter == '.' || (firstCharacter >= '0' && firstCharacter <= '9'))
			{
				unget();
				return parse_number();
			}

			JsonObject::throw_error("unexpected character '%c'", firstCharacter);
		}

#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
		JsonObject parse()
		{
			size_t cursorBeg = cursor;
			JsonObject object = _parse();
			size_t cursorEnd = cursor;
			
			if (cursorBeg != 0)
				cursorBeg--; // seems like original string cuts off 1 character?? not sure why.
			
			object.set_original_string(data + cursorBeg, cursorEnd - cursorBeg);
			object.set_full_parsed_string(fullString);
			return object;
		}
		
		void set_full_parsed_string(std::shared_ptr<std::string> ptr)
		{
			fullString = ptr;
		}
#endif

		void set_data_input(const char* in_data, size_t in_size)
		{
			data = in_data;
			size = in_size;
			
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
			if (!fullString)
				fullString = std::make_shared<std::string>(data, size);
#endif
		}

	private:
		JsonObject parse_object()
		{
			// estimate the amount of items in the array by pre-scanning it for commas.
			size_t itemCount = 1;
			bool isEmpty = false;

			int braceLevel = 0, curlyLevel = 0;
			for (size_t i = cursor; i != size; i++)
			{
				switch (data[i])
				{
					case '}':
						// if we aren't in any braces other than the main ones,
						// then we've reached the end of this array object.
						if (curlyLevel == 0)
						{
							if (i == cursor)
								isEmpty = true;
							i = size - 1; // stop scanning
						}
						else
						{
							curlyLevel--;
						}
						break;

					case ',':
						if (braceLevel == 0 && curlyLevel == 0)
							itemCount++;
						break;

					case '{': curlyLevel++; break;
					case '[': braceLevel++; break;
					case ']': braceLevel--; break;
				}
			}

			JsonObject mainObject = JsonObject::object();
			mainObject.internal_resize(isEmpty ? 0 : itemCount);

			char pk = peek_skip();

			size_t itemPos = 0;
			while (pk != '}' && !reached_end())
			{
				// parse the name
				std::string name = parse_string_2(true);

				char colon = get_skip();
				if (colon != ':')
					JsonObject::throw_error("expected ':'");

				// parse the object
				JsonParser parser;
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				parser.set_full_parsed_string(fullString);
#endif
				parser.set_data_input(data + cursor, size - cursor);
				JsonObject object = JsonParser::parse();
				
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				object.set_full_parsed_string(fullString);
#endif

				// skip its size
				cursor += parser.cursor;

				// insert it into our array
				if (itemPos >= itemCount)
				{
					// THIS SHOULD NOT HAPPEN! Severe performance penalty otherwise!
					fprintf(stderr, "THIS SHOULD NOT HAPPEN!  parse_object() expanding from %zu to %zu", itemCount, itemPos + 1);
					fflush(stderr);
					itemCount = itemPos + 1;
					mainObject.internal_resize(itemCount);
				}

				mainObject.internal_set_at(itemPos, name, std::move(object));
				itemPos++;

				// comma incoming, skip it too
				pk = peek_skip();
				if (pk == ',') {
					get();
					pk = peek_skip();
				}
			}

			if (!isEmpty && itemPos != itemCount)
			{
				// TODO: performance penalty here! Should just update the capacity!
				// This edge case is hit for trailing commas
				mainObject.internal_resize(itemPos);
			}

			if (reached_end())
				JsonObject::throw_error("unterminated struct");

			pk = get();
			assert(pk == '}');

			return mainObject;
		}

		JsonObject parse_array()
		{
			// estimate the amount of items in the array by pre-scanning it for commas.
			size_t itemCount = 1;
			bool isEmpty = false;

			int curlyLevel = 0, braceLevel = 0;
			for (size_t i = cursor; i != size; i++)
			{
				switch (data[i])
				{
					case ']':
						// if we aren't in any braces other than the main ones,
						// then we've reached the end of this array object.
						if (braceLevel == 0)
						{
							if (i == cursor)
								isEmpty = true;
							i = size - 1; // stop scanning
						}
						else
						{
							braceLevel--;
						}
						break;

					case ',':
						if (curlyLevel == 0 && braceLevel == 0)
							itemCount++;
						break;

					case '[': braceLevel++; break;
					case '{': curlyLevel++; break;
					case '}': curlyLevel--; break;
				}
			}

			JsonObject array = JsonObject::array();
			array.internal_resize(isEmpty ? 0 : itemCount);

			char pk = peek_skip();

			size_t itemPos = 0;
			while (pk != ']' && !reached_end())
			{
				// parse the object
				JsonParser parser;
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				parser.set_full_parsed_string(fullString);
#endif
				parser.set_data_input(data + cursor, size - cursor);
				JsonObject object = JsonParser::parse();
				
#ifdef IPROG_JSON_ENABLE_ORIGINAL_STRING
				object.set_full_parsed_string(fullString);
#endif

				// skip its size
				cursor += parser.cursor;

				// insert it into our array
				if (itemPos >= itemCount)
				{
					// THIS SHOULD NOT HAPPEN! Severe performance penalty otherwise!
					fprintf(stderr, "THIS SHOULD NOT HAPPEN!  parse_array() expanding from %zu to %zu", itemCount, itemPos + 1);
					fflush(stderr);
					itemCount = itemPos + 1;
					array.internal_resize(itemCount);
				}

				array.internal_set_at(itemPos, std::move(object));
				itemPos++;

				// comma incoming, skip it too
				pk = peek_skip();
				if (pk == ',') {
					get();
					pk = peek_skip();
				}
			}

			if (!isEmpty && itemPos != itemCount)
			{
				// TODO: performance penalty here! Should just update the capacity!
				// This edge case is hit for trailing commas
				array.internal_resize(itemPos);
			}

			if (reached_end())
				JsonObject::throw_error("unterminated array");

			pk = get();
			assert(pk == ']');

			return array;
		}

		JsonObject parse_number()
		{
			size_t current = cursor;
			size_t end;
			bool hasDot = false;

			// TODO: Decimal support with exponents
			for (end = current + 1; end != size; end++)
			{
				char chr = data[end];
				if (chr >= '0' && chr <= '9') continue;
				
				if (chr == '.')
				{
					if (hasDot)
						JsonObject::throw_error("two or more dots in number");
					hasDot = true;
					continue;
				}

				// unrecognized character, so break
				break;
			}

			cursor = end;

			if (hasDot)
			{
				// OPTIMIZE: todo, optimize this
				double dbl;
				std::stringstream ss = std::stringstream(std::string(&data[current], end - current));
				ss >> dbl;

				return JsonObject(dbl);
			}
			else
			{
				// parse manually
				int64_t t = 0;
				bool neg = false;
				size_t i = current;
				if (i != end && data[i] == '-') {
					neg = true;
					i++;
				}

				for (; i != end; i++)
					t = t * 10 + (data[i] - '0');

				if (neg)
					t = -t;

				return JsonObject(t);
			}
		}

		JsonObject parse_boolean_or_null()
		{
			char chr = get();

			switch (chr)
			{
				case 'f': // false
					cursor += 4;
					return JsonObject(false);

				case 't': // true
					cursor += 3;
					return JsonObject(true);

				case 'n': // null
					cursor += 3;
					return JsonObject();
			}

			JsonObject::throw_error("unexpected character '%c'", chr);
		}

		JsonObject parse_string()
		{
			return JsonObject(parse_string_2());
		}

		std::string parse_string_2(bool parseWithBeginningQuote = false)
		{
			if (parseWithBeginningQuote)
			{
				char chr = get_skip();
				if (chr != '"')
					JsonObject::throw_error("expected '\"'");
			}

			// NOTE: we have to parse the string because we need to solve escape characters
			std::string str;
			str.reserve(256);

			do
			{
				if (reached_end())
					JsonObject::throw_error("malformed string");

				char chr = get();

				if (chr == '"') break;

				if (chr == '\\')
				{
					// escape character
					if (reached_end())
						JsonObject::throw_error("malformed string");

					char nextChr = get();
					switch (nextChr)
					{
						default:
							// escaped character
							str += nextChr;
							break;
							
						// known escapes
						case '0': str += '\0'; break;
						case 'a': str += '\a'; break;
						case 'b': str += '\b'; break;
						case 'e': str += '\x1F'; break;
						case 'f': str += '\f'; break;
						case 'n': str += '\n'; break;
						case 't': str += '\t'; break;
						case 'v': str += '\v'; break;

						case 'x':
							JsonObject::throw_error("NYI: hex escapes");
							break;
							
						case 'u':
						{
							// parse this hex code
							uint32_t codePoint1 = parse_hex_char_escape();
							str += become_utf8(codePoint1);
							break;
						}
					}
					continue;
				}

				str += chr;
			}
			while (true);

			// string was parsed
			return str;
		}

		size_t get_cursor() const
		{
			return cursor;
		}

		char get()
		{
			if (cursor >= size)
				return -1;

			return data[cursor++];
		}

		void unget()
		{
			if (cursor == 0)
				return;

			cursor--;
		}

		char peek() const
		{
			if (cursor >= size)
				return -1;

			return data[cursor];
		}

		bool reached_end() const
		{
			return cursor >= size;
		}

		// gets a character that isn't whitespace
		char get_skip()
		{
			while (isspace(peek()) && !reached_end()) get();
			return get();
		}

		char peek_skip()
		{
			while (isspace(peek()) && !reached_end()) get();
			return peek();
		}
		
		uint32_t parse_hex_char_escape()
		{
			uint32_t codePoint1 = 0;

			for (int i = 0; i < 4; i++)
			{
				if (reached_end())
					JsonObject::throw_error("malformed \\u escape code");

				char chr = get();
				codePoint1 <<= 4;
				codePoint1 += from_hex(chr);
			}

			// N.B.  A surrogate pair must start with a high surrogate codepoint
			// and must end with a low surrogate codepoint.  This means that we
			// can get away with only checking for high surrogates, any low surrogates
			// should've been preceded by high surrogates
			if (codePoint1 >= 0xD800 && codePoint1 <= 0xDBFF)
			{
				// Surrogate pair
				size_t cursorBackup = cursor;
				char backslashExpected = get();
				char uExpected = get();

				if (backslashExpected == '\\' && uExpected == 'u')
				{
					// parse the second code
					uint32_t codePoint2 = 0;
					for (int i = 0; i < 4; i++)
					{
						if (reached_end())
							JsonObject::throw_error("malformed \\u escape code");

						char chr = get();
						codePoint2 <<= 4;
						codePoint2 += from_hex(chr);
					}

					// combine it with code point 1 to get a full 32-bit unicode character
					codePoint1 = 0x10000 + ((codePoint1 - 0xD800) << 10) + (codePoint2 - 0xDC00);
				}
				else
				{
					// undo the consumption of two characters
					cursor = cursorBackup;
				}
			}
			
			return codePoint1;
		}
		
		int from_hex(char chr)
		{
			if (chr >= '0' && chr <= '9')
				return chr - '0';
			else if (chr >= 'A' && chr <= 'F')
				return chr - 'A' + 0xA;
			else if (chr >= 'a' && chr <= 'f')
				return chr - 'a' + 0xA;
			else
				JsonObject::throw_error("invalid hex digit");
		}
		
		static std::string become_utf8(uint32_t codePoint)
		{
			std::string str;
			if (codePoint < 0x80)
			{
				str += char(codePoint);
				return str;
			}

			if (codePoint < 0x800)
			{
				str += char(0b11000000 | (codePoint >> 6));
				str += char(0b10000000 | (codePoint & 0x3F));
				return str;
			}

			if (codePoint < 0x10000)
			{
				str += char(0b11100000 | (codePoint >> 12));
				str += char(0b10000000 | ((codePoint >> 6) & 0x3F));
				str += char(0b10000000 | (codePoint & 0x3F));
				return str;
			}

			if (codePoint < 0x110000)
			{
				str += char(0b11110000 | (codePoint >> 18));
				str += char(0b10000000 | ((codePoint >> 12) & 0x3F));
				str += char(0b10000000 | ((codePoint >> 6) & 0x3F));
				str += char(0b10000000 | (codePoint & 0x3F));
				return str;
			}

			return become_utf8(0xFFFD);
		}
		
	private:
		std::shared_ptr<std::string> fullString;
		const char* data = nullptr;
		size_t cursor = 0;
		size_t size = 0;
	};
};
