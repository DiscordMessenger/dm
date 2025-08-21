// Homebrew implementation of the protobuf messaging protocol.
#pragma once

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <stdexcept>

#ifndef _countof
#define _countof(x) (sizeof(x) / sizeof(*(x)))
#endif

//#define DISABLE_PROTOBUF

#define returnIfNonNull(expr) \
    do { auto __ = (expr); if (!__.has_value()) return __; } while(0)

namespace Protobuf
{
	/// <summary>
	/// Represents a value or an error. Similar to C++23's `std::expected`.
	/// </summary>
	/// <typeparam name="T">The type of the value.</typeparam>
	/// <typeparam name="E">The type of the error code (default: <see cref="ErrorCode" />)</typeparam>
	template<typename T, typename E = ErrorCode>
	class ErrorOr {
	public:
		/// <summary>The type of the stored value.</summary>
		using value_type = T;

		/// <summary>The type of the error code.</summary>
		using error_type = E;

		/// <summary>
		/// Default constructor: creates a default value and default error (success).
		/// </summary>
		ErrorOr() : m_value(), m_error() {}

		/// <summary>
		/// Constructs with a value and optional error.
		/// </summary>
		/// <param name="val">The value to store.</param>
		/// <param name="err">The error code (default: success).</param>
		ErrorOr(const T& val, const E& err = E{}) : m_value(val), m_error(err) {}

		/// <summary>Move constructor with optional error.</summary>
		/// <param name="val">The value to store (rvalue).</param>
		/// <param name="err">The error code (default: success).</param>
		ErrorOr(T&& val, E&& err = E{}) : m_value(std::move(val)), m_error(std::move(err)) {}

		/// <summary>Defaulted copy constructor.</summary>
		ErrorOr(const ErrorOr&) = default;

		/// <summary>Defaulted move constructor.</summary>
		ErrorOr(ErrorOr&&) = default;

		/// <summary>Defaulted copy assignment operator.</summary>
		ErrorOr& operator=(const ErrorOr&) = default;

		/// <summary>Defaulted move assignment operator.</summary>
		ErrorOr& operator=(ErrorOr&&) = default;

		/// <summary>Returns true if the stored value represents success.</summary>
		bool has_value() const { return m_error == E{}; }

		/// <summary>Explicit conversion to bool: true if success.</summary>
		explicit operator bool() const { return has_value(); }

		/// <summary>Returns the stored value. Throws if there is an error.</summary>
		T& value() {
			if (!has_value()) throw std::logic_error("ErrorOr: no value");
			return m_value;
		}

		/// <summary>Returns the stored value (const). Throws if there is an error.</summary>
		const T& value() const {
			if (!has_value()) throw std::logic_error("ErrorOr: no value");
			return m_value;
		}

		/// <summary>Returns the error code.</summary>
		E error() const { return m_error; }

		/// <summary>Returns the stored value or a fallback if there is an error.</summary>
		/// <typeparam name="U">Type convertible to T.</typeparam>
		/// <param name="fallback">The fallback value.</param>
		template<typename U>
		T value_or(U&& fallback) const {
			return has_value() ? m_value : static_cast<T>(std::forward<U>(fallback));
		}

		// Helpers for creating success/failure

		/// <summary>Creates a successful ErrorOr with the given value.</summary>
		template<typename U = T>
		static ErrorOr success(U&& val) {
			return ErrorOr(std::forward<U>(val), E{});
		}

		/// <summary>Creates a failed ErrorOr with the given error code.</summary>
		static ErrorOr failure(E err) {
			return ErrorOr(T{}, std::move(err));
		}

		/// <summary>Pointer-like access to the value.</summary>
		const T& operator*() const { return m_value; }
		T& operator*() { return m_value; }

		/// <summary>Equality comparison operator.</summary>
		bool operator==(const ErrorOr& other) const {
			return m_value == other.m_value && m_error == other.m_error;
		}
		/// <summary>Inequality comparison operator.</summary>
		bool operator!=(const ErrorOr& other) const {
			return !(*this == other);
		}

	private:
		/// <summary>The stored value.</summary>
		T m_value; 

		/// <summary>The stored error code.</summary>
		E m_error;
	};

	/// <summary>
	/// Specialization of ErrorOr for void type (no value stored).
	/// Useful for functions that only return success/failure.
	/// </summary>
	/// <typeparam name="E">The type of the error code.</typeparam>
	template<typename E>
	class ErrorOr<void, E> {
	public:
		using value_type = void; /// No value stored.
		using error_type = E;    /// The type of the error code.

		/// <summary>Default constructor: success (no error).</summary>
		ErrorOr() : m_error() {}

		/// <summary>Constructor with error code.</summary>
		/// <param name="err">The error code to store.</param>
		explicit ErrorOr(E err) : m_error(err) {}

		ErrorOr(const ErrorOr&) = default;
		ErrorOr(ErrorOr&&) = default;
		ErrorOr& operator=(const ErrorOr&) = default;
		ErrorOr& operator=(ErrorOr&&) = default;

		/// <summary>Returns true if success.</summary>
		bool has_value() const { return m_error == E{}; }

		/// <summary>Explicit conversion to bool: true if success.</summary>
		explicit operator bool() const { return has_value(); }

		/// <summary>Returns the stored error code.</summary>
		E error() const { return m_error; }

		/// <summary>Creates a successful ErrorOr (no value).</summary>
		static ErrorOr success() { return ErrorOr(); }

		/// <summary>Creates a failed ErrorOr with the given error code.</summary>
		static ErrorOr failure(E err) { return ErrorOr(err); }

		/// <summary>Equality comparison operator.</summary>
		bool operator==(const ErrorOr& other) const { return m_error == other.m_error; }

		/// <summary>Inequality comparison operator.</summary>
		bool operator!=(const ErrorOr& other) const {
			return !(*this == other);
		}

	private:
		E m_error;  /// Stored error code
	};

	template<typename T>
	using Result = ErrorOr<void, T>;


	typedef uint64_t FieldNumber;

	enum
	{
		TAG_VARINT,
		TAG_I64,
		TAG_LEN,
		TAG_I32 = 5,
	};

	/// <summary>
	/// Error codes used throughout the decoding/encoding library.
	/// </summary>
	enum class ErrorCode : int
	{
		/// <summary>Accessed data goes out of bounds.</summary>
		OutOfBounds = 1,

		/// <summary>Variable-length integer is too large.</summary>
		VarIntTooBig = 2,

		/// <summary>Unknown tag encountered during decoding.</summary>
		UnknownTag = 3,

		/// <summary>Expected a unique field but found duplicate.</summary>
		SupposedToBeUnique = 4,

		/// <summary>Cannot add an embedded message to a root message.</summary>
		CantAddRootMsg = 5,

		/// <summary>The requested operation is not supported.</summary>
		NotSupported = 6
	};

	// ===== DECODE HINT =====
	// Note! The decode hint should only be used on the root (the top level call to DecodeBlock)
	class DecodeHint
	{
	public:
		enum {
			O_UNKNOWN,
			O_BYTES,
			O_STRING,
			O_MESSAGE,
		};

		DecodeHint() {
			m_type = O_UNKNOWN;
		}
		DecodeHint(int type) {
			m_type = type;
		}
		~DecodeHint() {
			for (auto& ch : m_children) {
				delete ch.second;
			}
		}

		DecodeHint* Lookup(FieldNumber fieldNum) {
			auto iter = m_children.find(fieldNum);
			if (iter == m_children.end())
				return nullptr;
			return iter->second;
		}

		DecodeHint* AddChild(FieldNumber fieldNum, int type = O_UNKNOWN) {
			DecodeHint*& ptr = m_children[fieldNum];
			if (!ptr) {
				ptr = new DecodeHint(type);
			}
			return ptr;
		}

		int GetType() const {
			return m_type;
		}

	private:
		int m_type;
		std::map<FieldNumber, DecodeHint*> m_children;
	};

	// ===== ENCODE =====
	constexpr uint64_t CombineFieldNumberAndTag(FieldNumber fieldNumber, int tag)
	{
		return (fieldNumber << 3) | tag;
	}

	static void EncodeVarInt(std::vector<uint8_t>& stream, uint64_t num)
	{
		uint8_t digits[16];
		int numDigits = 0;
		do
		{
			uint8_t thisByte = num % 128;
			num /= 128;
			digits[numDigits++] = thisByte | 0x80;
		} while (num);

		digits[numDigits - 1] &= ~0x80;
		for (int i = 0; i < numDigits; i++)
			stream.push_back(digits[i]);
	}

	static size_t GetVarIntSize(uint64_t num)
	{
		int numDigits = 0;
		do
		{
			num /= 128;
			numDigits++;
		} while (num);
		return size_t(numDigits);
	}

	class ObjectBase
	{
	public:
		ObjectBase(FieldNumber fieldNumber) {
			m_fieldNumber = fieldNumber;
		}

		virtual ~ObjectBase() { }

		// Gets the size of the payload.
		virtual size_t GetPayloadSize() const = 0;

		// Gets the size of the dirty payload.
		virtual size_t GetDirtyPayloadSize() const {
			return GetPayloadSize();
		}

		// Encodes the entire payload.
		virtual void Encode(std::vector<uint8_t>& outputStream) const = 0;

		// Encodes the parts of the payload that have changed.
		virtual void EncodeDirty(std::vector<uint8_t>& outputStream) const {
			Encode(outputStream);
		}

		// Checks if this object is a message object.
		virtual bool IsMessageObject() const { return false; }

		// Checks if this object is an embedded message object.
		virtual bool IsEmbeddedMessageObject() const { return false; }

		// Checks if this object is a byte storage object.  Namely, a message, string or byte array object.
		virtual bool IsByteStorageObject() const { return false; }

		// Checks if this object is a string object
		virtual bool IsStringObject() const { return false; }

		// Checks if this object is a byte array object
		virtual bool IsByteArrayObject() const { return false; }

		// Gets the last child element with the specified field number.
		virtual ObjectBase* GetFieldObject(FieldNumber fieldNumber) { return nullptr; }

		// Gets the last child element with the specified field number. If not found, place an object there.
		// If the object _is_ there, then the pointer given is deleted.
		virtual ObjectBase* GetFieldObjectWithDefault(FieldNumber fieldNumber, ObjectBase* pObj) { delete pObj; return nullptr; }

		// Gets a pointer to the vector of field objects with the specified field number.
		virtual std::vector<ObjectBase*>* GetFieldObjects(FieldNumber fieldNumber) { return nullptr; }

		// Checks if the object is empty.
		virtual bool IsEmpty() { return true; }

		// Erases a child field if possible.
		virtual bool EraseField(FieldNumber fieldNumber) { return false; }

		// Gets a pointer to a field object, and creates a default if it wasn't found.
		template<typename T>
		T* GetFieldObjectDefault(FieldNumber fieldNumber) {
			T* pT = new T(fieldNumber);

			bool wantsMessageObject = pT->IsMessageObject();

			ObjectBase* pObj = GetFieldObjectWithDefault(fieldNumber, pT);

			// If the object didn't already exist, now it exists.
			if (pObj == pT)
				return pT;

			// If the object returned nullptr, that means that it really can't do that.
			// N.B. The default handler has deleted pT.
			if (!pObj)
				return nullptr;

			// If expecting a message object, yet we have an empty byte storage that isn't a message object
			// --- OR ---
			// If expecting a non message object yet we have an empty message object
			// --- THEN ---
			// Delete the old object and shove our new one in.
			if ((wantsMessageObject && pObj->IsByteStorageObject() && !pObj->IsMessageObject() && pObj->IsEmpty()) ||
				(!wantsMessageObject && pObj->IsMessageObject() && pObj->IsEmpty()))
			{
				// Erase the old object
				if (!EraseField(fieldNumber))
					assert(!"Huh");

				// Add a new one in.
				pT = new T(fieldNumber);
				pObj = GetFieldObjectWithDefault(fieldNumber, pT);
				assert(pObj == pT);
			}

			if (wantsMessageObject)
				assert(pObj->IsMessageObject());
			else
				assert(!pObj->IsMessageObject());

			return reinterpret_cast<T*>(pObj);
		}

		// Marks the object as clean.
		virtual void MarkClean() {
			m_bIsDirty = false;
		}

		int64_t GetFieldNumber() const {
			return m_fieldNumber;
		}
		ObjectBase* SetFieldNumber(int64_t fn) {
			m_fieldNumber = fn;
			return this;
		}
		void MarkDirty() {
			m_bIsDirty = true;
			if (m_pParent)
				m_pParent->MarkDirty();
		}
		bool IsDirty() const {
			return m_bIsDirty;
		}
		ObjectBase* GetParent() {
			return m_pParent;
		}

	protected:
		friend class ObjectBaseMessage;
		void SetParent(ObjectBase* pMsg) {
			m_pParent = pMsg;
		}
	private:
		int64_t m_fieldNumber = 0;
		bool m_bIsDirty = false;
		ObjectBase* m_pParent = nullptr;
	};

	class ObjectBaseMessage : public ObjectBase
	{
	public:
		ObjectBaseMessage(FieldNumber fieldNumber = 0) : ObjectBase(fieldNumber) { }

		~ObjectBaseMessage()
		{
			for (auto& ob : m_objects)
				for (auto& ob2 : ob.second)
					delete ob2;

			if (m_pDecodeHint)
				delete m_pDecodeHint;
		}

		Result<ErrorCode> AddObject(ObjectBase* ob)
		{
			if (ob->IsMessageObject() && !ob->IsEmbeddedMessageObject())
				return Result<ErrorCode>::failure(ErrorCode::CantAddRootMsg);

			FieldNumber fieldNum = ob->GetFieldNumber();
			if (!m_fieldsUnique[fieldNum] || m_objects[fieldNum].empty()) {
				m_objects[fieldNum].push_back(ob);
				MarkDirty();
			}
			else {
				for (auto& ob : m_objects[fieldNum])
					delete ob;
				m_objects[fieldNum].resize(1);
				m_objects[fieldNum][0] = ob;
				MarkDirty();
			}

			ob->SetParent(this);
			return Result<ErrorCode>::success();
		}

		ObjectBaseMessage* Clear()
		{
			m_objects.clear();
			return this;
		}

		// Sets a field as unique.  By default, a field is repeated.
		ObjectBaseMessage* SetFieldUnique(FieldNumber fieldNum) {
			m_fieldsUnique[fieldNum] = true;
			MarkDirty();
			return this;
		}

		ObjectBase* GetFieldObject(FieldNumber fieldNum) override
		{
			auto iter = m_objects.find(fieldNum);
			if (iter == m_objects.end())
				return nullptr;
			if (iter->second.empty())
				return nullptr;

			return iter->second[iter->second.size() - 1];
		}

		ObjectBase* GetFieldObjectWithDefault(FieldNumber fieldNum, ObjectBase* pDefaultObj) override
		{
			auto iter = m_objects.find(fieldNum);
			if (iter == m_objects.end() || iter->second.empty())
			{
				m_objects[fieldNum].push_back(pDefaultObj);
				pDefaultObj->SetParent(this);
				MarkDirty();
				return pDefaultObj;
			}

			delete pDefaultObj;
			return iter->second[iter->second.size() - 1];
		}

		bool EraseField(FieldNumber fieldNumber) override
		{
			auto iter = m_objects.find(fieldNumber);
			if (iter == m_objects.end())
				return false;

			for (auto obj : iter->second)
				delete obj;

			iter->second.clear();
			m_objects.erase(iter);
			MarkDirty();
			return true;
		}

		std::vector<ObjectBase*>* GetFieldObjects(FieldNumber fieldNum) override
		{
			auto iter = m_objects.find(fieldNum);
			if (iter == m_objects.end())
				return nullptr;
			if (iter->second.empty())
				return nullptr;

			return &iter->second;
		}

		bool IsEmpty() override
		{
			if (m_objects.empty())
				return true;

			for (auto& obj : m_objects) {
				if (!obj.second.empty())
					return false;
			}

			return true;
		}

		ObjectBaseMessage(const ObjectBaseMessage&) = delete; // delete the copy constructor. You can't do that because I'm too lazy to make it possible

		ObjectBaseMessage(ObjectBaseMessage&& oth) noexcept : ObjectBase(std::move(oth)), m_objects(std::move(oth.m_objects)) {}

		ObjectBaseMessage& operator=(ObjectBaseMessage&& other) noexcept
		{
			m_objects = std::move(other.m_objects);
			SetFieldNumber(other.GetFieldNumber());
			return *this;
		}

		void MarkDirtyAndChildren()
		{
			MarkDirty();

			for (auto& ob : m_objects)
			{
				for (auto& ob2 : ob.second)
				{
					if (ob2->IsMessageObject())
						reinterpret_cast<ObjectBaseMessage*>(ob2)->MarkDirtyAndChildren();
					else
						ob2->MarkDirty();
				}
			}
		}

		// This completely guts the other message, because I can't be bothered to make a virtual clone function.
		Result<ErrorCode> MergeWith(ObjectBaseMessage* pOtherMsg)
		{
			for (auto& obj : pOtherMsg->m_objects)
			{
				auto& srcVec = obj.second;
				auto& dstVec = m_objects[obj.first];

				// If the field is repeated, clear.  I don't know how you'd encode an "add" or "remove" with protobuf.
				if (!m_fieldsUnique[obj.first])
				{
					// delete all existing objects in the destination
					for (auto obj : dstVec)
						delete obj;
					dstVec.clear();

					// set all existing objects in the source
					for (auto obj : srcVec) {
						dstVec.push_back(obj);
						obj->SetParent(this);
					}
					srcVec.clear();
					continue;
				}

				// The field is unique.  If the destination vector is empty, set it.
				if (dstVec.empty())
				{
					for (auto obj : srcVec) {
						dstVec.push_back(obj);
						obj->SetParent(this);
					}
					srcVec.clear();
					continue;
				}

				if (dstVec.size() != 1 && srcVec.size() != 1)
					return Result<ErrorCode>::failure(ErrorCode::SupposedToBeUnique);

				// If this is a message object, recursively merge it.
				if (dstVec[0]->IsMessageObject())
				{
					ObjectBaseMessage* pMsg = (ObjectBaseMessage*)dstVec[0];
					ObjectBaseMessage* pMsg2 = (ObjectBaseMessage*)srcVec[0];

					returnIfNonNull(pMsg->MergeWith(pMsg2));

					// N.B. Gonna leave it alone. I hope there are no memory leaks!!
					continue;
				}

				// Overwrite the value.
				delete dstVec[0];
				dstVec[0] = srcVec[0];
				dstVec[0]->SetParent(this);
				srcVec.clear();
			}

			MarkDirty();

			return Result<ErrorCode>::success();
		}

	public:
		bool IsMessageObject() const override { return true; }

		bool IsByteStorageObject() const override { return true; }

		size_t GetPayloadSize() const override
		{
			size_t totalSize = 0;
			for (auto& ob : m_objects)
				for (auto& ob2 : ob.second)
					totalSize += ob2->GetPayloadSize();

			return totalSize;
		}

		size_t GetDirtyPayloadSize() const override
		{
			size_t totalSize = 0;
			for (auto& ob : m_objects)
				for (auto& ob2 : ob.second)
					if (ob2->IsDirty())
						totalSize += ob2->GetDirtyPayloadSize();

			return totalSize;
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			for (auto& ob : m_objects)
				for (auto& ob2 : ob.second)
					ob2->Encode(outputStream);
		}

		void EncodeDirty(std::vector<uint8_t>& outputStream) const override
		{
			for (auto& ob : m_objects)
				for (auto& ob2 : ob.second)
					if (ob2->IsDirty())
						ob2->EncodeDirty(outputStream);
		}

		void MarkClean() override
		{
			for (auto& ob : m_objects)
				for (auto& ob2 : ob.second)
					ob2->MarkClean();
			ObjectBase::MarkClean();
		}

		void SetDecodingHint(DecodeHint* pHint) {
			if (m_pDecodeHint)
				delete m_pDecodeHint;
			m_pDecodeHint = pHint;
		}

		DecodeHint* GetDecodeHint() {
			return m_pDecodeHint;
		}

	private:
		std::map<uint64_t, std::vector<ObjectBase*> > m_objects;
		std::map<uint64_t, bool> m_fieldsUnique;
		DecodeHint* m_pDecodeHint = nullptr;
	};

	class ObjectMessage : public ObjectBaseMessage
	{
	public:
		ObjectMessage(FieldNumber fieldNum) : ObjectBaseMessage(fieldNum) {}

	public:
		bool IsEmbeddedMessageObject() const override { return true; }

		size_t GetPayloadSize() const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetPayloadSize();
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(embPayloadSize) + embPayloadSize;
		}

		size_t GetDirtyPayloadSize() const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetDirtyPayloadSize();
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(embPayloadSize) + embPayloadSize;
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetPayloadSize();
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, embPayloadSize);
			ObjectBaseMessage::Encode(outputStream);
		}

		void EncodeDirty(std::vector<uint8_t>& outputStream) const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetDirtyPayloadSize();
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, embPayloadSize);
			ObjectBaseMessage::EncodeDirty(outputStream);
		}
	};

	class ObjectString : public ObjectBase
	{
	public:
		ObjectString(FieldNumber fn, const std::string& str = "") : ObjectBase(fn), m_content(str) {}

		ObjectString* SetContent(const std::string& str)
		{
			m_content = str;
			MarkDirty();
			return this;
		}

		std::string GetContent() const
		{
			return m_content;
		}

	public:
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(m_content.size()) + m_content.size();
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, m_content.size());
			size_t oldLength = outputStream.size();
			outputStream.resize(oldLength + m_content.size());
			memcpy(outputStream.data() + oldLength, m_content.c_str(), m_content.size());
		}

		bool IsEmpty() override
		{
			return m_content.empty();
		}

		bool IsByteStorageObject() const override { return true; }

		bool IsStringObject() const override { return true; }

	private:
		std::string m_content;
	};

	class ObjectBytes : public ObjectBase
	{
	public:
		ObjectBytes(FieldNumber fn, const std::vector<uint8_t>& val = {}) : ObjectBase(fn), m_content(val) {}

		ObjectBytes(FieldNumber fn, const char* data, size_t sz) : ObjectBase(fn)
		{
			m_content.resize(sz);
			memcpy(m_content.data(), data, sz);
		}

		ObjectBytes(FieldNumber fn, const uint8_t* data, size_t sz) : ObjectBase(fn)
		{
			m_content.resize(sz);
			memcpy(m_content.data(), data, sz);
		}

		ObjectBytes* SetContent(const std::vector<uint8_t>& val)
		{
			m_content = val;
			MarkDirty();
			return this;
		}

		std::vector<uint8_t> GetContent() const
		{
			return m_content;
		}

	public:
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(m_content.size()) + m_content.size();
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, m_content.size());
			size_t oldLength = outputStream.size();
			outputStream.resize(oldLength + m_content.size());
			memcpy(outputStream.data() + oldLength, m_content.data(), m_content.size());
		}

		bool IsEmpty() override
		{
			return m_content.empty();
		}

		bool IsByteStorageObject() const override { return true; }

		bool IsByteArrayObject() const override { return true; }

	private:
		std::vector<uint8_t> m_content;
	};

	class ObjectVarInt : public ObjectBase
	{
	public:
		ObjectVarInt(FieldNumber fn, uint64_t val = 0) : ObjectBase(fn), m_value(val) {}

		ObjectVarInt* SetValue(uint64_t c)
		{
			m_value = c;
			MarkDirty();
			return this;
		}

		uint64_t GetValue() const
		{
			return m_value;
		}

	public:
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_VARINT)) + GetVarIntSize(m_value);
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_VARINT));
			EncodeVarInt(outputStream, m_value);
		}

		bool IsEmpty() override
		{
			return false;
		}

	private:
		int64_t m_value;
	};

	class ObjectFixed64 : public ObjectBase
	{
	public:
		ObjectFixed64(FieldNumber fn, uint64_t val = 0) : ObjectBase(fn), m_value(val) {}

		ObjectFixed64* SetValue(uint64_t c)
		{
			m_value = c;
			MarkDirty();
			return this;
		}

		uint64_t GetValue() const
		{
			return m_value;
		}

	public:
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_I64)) + sizeof(uint64_t);
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_I64));

			union {
				uint8_t bytes[8];
				uint64_t u64;
			} x;
			x.u64 = m_value;

			// TODO: Big endian support here
			//for (int i = 7; i >= 0; i--)
			for (int i = 0; i < 8; i++)
				outputStream.push_back(x.bytes[i]);
		}

		bool IsEmpty() override
		{
			return false;
		}

	private:
		int64_t m_value;
	};

	class ObjectFixed32 : public ObjectBase
	{
	public:
		ObjectFixed32(FieldNumber fn, uint32_t val = 0) : ObjectBase(fn), m_value(val) {}

		ObjectFixed32* SetValue(uint32_t c)
		{
			m_value = c;
			MarkDirty();
			return this;
		}
		uint32_t GetValue() const
		{
			return m_value;
		}

	public:
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_I32)) + sizeof(uint32_t);
		}

		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_I32));

			union {
				uint8_t bytes[4];
				uint32_t u32;
			} x;
			x.u32 = m_value;

			// TODO: Big endian support here
			//for (int i = 3; i >= 0; i--)
			for (int i = 0; i < 4; i++)
				outputStream.push_back(x.bytes[i]);
		}

	private:
		int32_t m_value;
	};

	// ===== DECODE =====
	static ErrorOr<uint64_t> DecodeVarInt(const char* data, size_t& offset, size_t sz)
	{
		uint8_t bytes[16]{};
		int byteCount = 0;

		while (true)
		{
			if (offset >= sz)
				return ErrorOr<uint64_t>::failure(ErrorCode::OutOfBounds);
			if (byteCount >= _countof(bytes))
				return ErrorOr<uint64_t>::failure(ErrorCode::VarIntTooBig);

			uint8_t byte = bytes[byteCount++] = data[offset++];
			if (~byte & 0x80)
				break;
		}

		uint64_t result = 0;
		for (int i = byteCount - 1; i >= 0; i--)
			result = result * 128 + (bytes[i] % 128);

		return ErrorOr<uint64_t>::success(result);
	}

	/// <summary>
/// Decode a buffer into an ObjectBaseMessage using optional hints.
/// Returns a Result indicating success or failure.
/// </summary>
	static Result<ErrorCode> DecodeBlock(const char* data, size_t sz, ObjectBaseMessage* block, DecodeHint* pHint = nullptr)
	{
#ifdef DISABLE_PROTOBUF
		return Result<ErrorCode>::failure(ErrorCode::NotSupported);
#endif

		if (!pHint)
			pHint = block->GetDecodeHint();

		Result<ErrorCode> decodeResult;
		size_t offset = 0;
		while (offset < sz)
		{
			// read tag
			auto fieldNumAndTag = DecodeVarInt(data, offset, sz);
			if (!fieldNumAndTag.has_value())
				return Result<ErrorCode>::failure(fieldNumAndTag.error()); // propagate error

			FieldNumber fieldNum = fieldNumAndTag.value() >> 3;
			int tag = int(fieldNumAndTag.value() & 0x7);

			switch (tag)
			{
			case TAG_VARINT:
			{
				auto value = DecodeVarInt(data, offset, sz);
				if (!value.has_value())
					return Result<ErrorCode>::failure(value.error());

				auto addResult = block->AddObject(new ObjectVarInt(fieldNum, *value));
				if (!addResult.has_value())
					return addResult;

				break;
			}
			case TAG_LEN:
			{
				auto lengthE = DecodeVarInt(data, offset, sz);
				if (!lengthE.has_value())
					return Result<ErrorCode>::failure(lengthE.error());

				size_t length = static_cast<size_t>(*lengthE);

				if (offset + length > sz)
					return Result<ErrorCode>::failure(ErrorCode::OutOfBounds);

				const char* dataSubset = &data[offset];
				offset += length;

				// Detect if the data is a string
				bool isString = true;
				for (size_t i = 0; i < length && isString; i++) {
					uint8_t chr = static_cast<uint8_t>(dataSubset[i]);
					if (chr < ' ' || chr == 0x7F)
						isString = false;
				}

				DecodeHint* pChildHint = nullptr;
				bool isBytes = false;
				ObjectMessage* pChld = nullptr;

				if (pHint && (pChildHint = pHint->Lookup(fieldNum)))
				{
					switch (pChildHint->GetType()) {
					case DecodeHint::O_BYTES: goto parse_bytes;
					case DecodeHint::O_STRING: goto parse_string;
					case DecodeHint::O_MESSAGE: goto try_parse_object;
					}
				}
				else if (isString)
				{
				parse_string:
					{
						auto addResult = block->AddObject(new ObjectString(fieldNum, std::string(dataSubset, length)));
						if (!addResult.has_value()) return addResult;
					}
				}
				else
				{
				try_parse_object:
					pChld = new ObjectMessage(fieldNum);
					decodeResult = DecodeBlock(dataSubset, length, pChld, pChildHint);
					if (!decodeResult.has_value())
					{
						delete pChld;
						isBytes = true;
					}
					else
					{
						auto addResult = block->AddObject(pChld);
						if (!addResult.has_value()) return addResult;
					}

					if (isBytes)
					{
					parse_bytes:
						{
							auto addResult = block->AddObject(new ObjectBytes(fieldNum, dataSubset, length));
							if (!addResult.has_value()) return addResult;
						}
					}
				}

				break;
			}
			case TAG_I64:
			{
				if (offset + sizeof(uint64_t) > sz)
					return Result<ErrorCode>::failure(ErrorCode::OutOfBounds);

				auto addResult = block->AddObject(new ObjectFixed64(fieldNum, *reinterpret_cast<const uint64_t*>(&data[offset])));
				if (!addResult.has_value()) return addResult;
				offset += sizeof(uint64_t);
				break;
			}
			case TAG_I32:
			{
				if (offset + sizeof(uint32_t) > sz)
					return Result<ErrorCode>::failure(ErrorCode::OutOfBounds);

				auto addResult = block->AddObject(new ObjectFixed32(fieldNum, *reinterpret_cast<const uint32_t*>(&data[offset])));
				if (!addResult.has_value()) return addResult;
				offset += sizeof(uint32_t);
				break;
			}
			default:
				return Result<ErrorCode>::failure(ErrorCode::UnknownTag);
			}

			if (offset > sz)
				return Result<ErrorCode>::failure(ErrorCode::OutOfBounds);
		}

		return Result<ErrorCode>::success();
	}

	static Result<ErrorCode> DecodeBlock(const uint8_t* data, size_t sz, ObjectBaseMessage* block)
	{
		return DecodeBlock((const char*)data, sz, block);
	}
}
