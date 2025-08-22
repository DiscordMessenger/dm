// Homebrew implementation of the protobuf messaging protocol.
#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <memory>

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

	enum Tag
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

	/// <summary>
	/// Helps guide decoding of protobuf-like messages. Only used at root level.
	/// </summary>
	class DecodeHint
	{
	public:
		/// <summary>
		/// Type of the hinted object.
		/// </summary>
		enum Type {
			O_UNKNOWN,
			O_BYTES,
			O_STRING,
			O_MESSAGE
		};

		DecodeHint(Type type = O_UNKNOWN) : m_type(type) {}

		~DecodeHint() = default;

		/// <summary>
		/// Lookup a child hint for a given field number.
		/// </summary>
		DecodeHint* Lookup(FieldNumber fieldNum) const {
			auto it = m_children.find(fieldNum);
			return (it != m_children.end()) ? it->second.get() : nullptr;
		}

		/// <summary>
		/// Add or get a child hint for a given field number.
		/// </summary>
		DecodeHint* AddChild(FieldNumber fieldNum, Type type = O_UNKNOWN) {
			auto& ptr = m_children[fieldNum];
			if (!ptr)
				ptr = std::make_unique<DecodeHint>(type);
			return ptr.get();
		}

		/// <summary>Return the type of this hint.</summary>
		Type GetType() const { return m_type; }

	private:
		Type m_type;
		std::unordered_map<FieldNumber, std::unique_ptr<DecodeHint>> m_children;
	};

	/// <summary>
	/// Combines a field number and wire tag into a single protobuf key.
	/// </summary>
	constexpr uint64_t CombineFieldNumberAndTag(FieldNumber fieldNumber, Tag tag) noexcept
	{
		return (fieldNumber << 3) | static_cast<uint64_t>(tag);
	}

	static constexpr uint8_t MSB = 0x80;
	static constexpr uint8_t MSBALL = ~0x7F;

	static constexpr uint64_t N1 = 1ULL << 7;
	static constexpr uint64_t N2 = 1ULL << 14;
	static constexpr uint64_t N3 = 1ULL << 21;
	static constexpr uint64_t N4 = 1ULL << 28;
	static constexpr uint64_t N5 = 1ULL << 35;
	static constexpr uint64_t N6 = 1ULL << 42;
	static constexpr uint64_t N7 = 1ULL << 49;
	static constexpr uint64_t N8 = 1ULL << 56;
	static constexpr uint64_t N9 = 1ULL << 63;

	/// <summary>
	/// Computes the number of bytes needed to encode a 64-bit unsigned integer as a protobuf varint.
	/// </summary>
	static size_t GetVarIntSize(uint64_t n) noexcept
	{
		return (
			n < N1 ? 1
			: n < N2 ? 2
			: n < N3 ? 3
			: n < N4 ? 4
			: n < N5 ? 5
			: n < N6 ? 6
			: n < N7 ? 7
			: n < N8 ? 8
			: n < N9 ? 9
			: 10
			);
	}

	/// <summary>
	/// Encodes a 64-bit unsigned integer as a protobuf varint and appends it to the stream.
	/// Uses the standard little-endian base-128 encoding (LEB128).
	/// </summary>
	/// <param name="stream">Vector to which encoded bytes will be appended.</param>
	/// <param name="num">Unsigned 64-bit integer to encode.</param>
	static void EncodeVarInt(std::vector<uint8_t>& stream, uint64_t num)
	{
		const size_t needed = GetVarIntSize(num);
		size_t oldSize = stream.size();
		stream.resize(oldSize + needed);
		uint8_t* writePtr = stream.data() + oldSize;

		// write bytes from the end backwards to fill 'needed' slots
		size_t n = 0;
		while (num >= 0x80)
		{
			writePtr[n++] = static_cast<uint8_t>(num & 0x7F) | 0x80;
			num >>= 7;
		}
		writePtr[n++] = static_cast<uint8_t>(num);
		assert(n == needed);
	}

	/// <summary>
	/// Base class for all protobuf field objects.
	/// </summary>
	/// <remarks>
	/// Represents a single field object (primitive, bytes/string, or embedded message).
	/// Provides a uniform interface for sizing, encoding and tree operations.
	/// Implementations typically override payload size / encode semantics and type queries.
	/// </remarks>
	class ObjectBase
	{
	public:
		/// <summary>
		/// Construct an ObjectBase for the given field number.
		/// </summary>
		/// <param name="fieldNumber">The protobuf field number this object represents.</param>
		ObjectBase(FieldNumber fieldNumber) : m_fieldNumber{ fieldNumber } {}

		/// <summary>
		/// Virtual destructor to allow safe deletion via base pointer.
		/// </summary>
		virtual ~ObjectBase() { }

		/// <summary>
		/// Get the size, in bytes, of this object's encoded payload.
		/// </summary>
		/// <returns>The payload size in bytes when encoded into the stream.</returns>
		virtual size_t GetPayloadSize() const = 0;

		/// <summary>
		/// Get the size, in bytes, of this object's encoded payload when only
		/// considering dirty/changed data.
		/// </summary>
		/// <returns>
		/// The number of bytes that would be produced by EncodeDirty().
		/// Default implementation defers to GetPayloadSize().
		/// </returns>
		virtual size_t GetDirtyPayloadSize() const {
			return GetPayloadSize();
		}

		/// <summary>
		/// Encode the entire payload for this object into the provided output stream.
		/// </summary>
		/// <param name="outputStream">Vector to which encoded bytes are appended.</param>
		virtual void Encode(std::vector<uint8_t>& outputStream) const = 0;

		/// <summary>
		/// Encode only the parts of the payload that are dirty/changed into the provided stream.
		/// </summary>
		/// <param name="outputStream">Vector to which encoded bytes are appended.</param>
		/// <remarks>
		/// Default implementation encodes the entire payload by calling Encode().
		/// Subclasses that support fine-grained dirty encoding should override this.
		/// </remarks>
		virtual void EncodeDirty(std::vector<uint8_t>& outputStream) const {
			Encode(outputStream);
		}

		/// <summary>
		/// Returns true if this object is a message container (i.e., contains fields).
		/// </summary>
		virtual bool IsMessageObject() const { return false; }

		/// <summary>
		/// Returns true if this message is an embedded message (i.e., not a root message).
		/// </summary>
		virtual bool IsEmbeddedMessageObject() const { return false; }

		/// <summary>
		/// Returns true if this object stores bytes (message/string/byte array).
		/// </summary>
		virtual bool IsByteStorageObject() const { return false; }

		/// <summary>
		/// Returns true if this object is a string object.
		/// </summary>
		virtual bool IsStringObject() const { return false; }

		/// <summary>
		/// Returns true if this object is a byte-array object.
		/// </summary>
		virtual bool IsByteArrayObject() const { return false; }

		/// <summary>
		/// Returns the last child element with the specified field number, or nullptr.
		/// </summary>
		/// <param name="fieldNumber">Field number to look up.</param>
		/// <returns>Pointer to the last matching ObjectBase or nullptr if not found.</returns>
		virtual ObjectBase* GetFieldObject(FieldNumber fieldNumber) { return nullptr; }

		/// <summary>
		/// Get the last child element with the specified field number, or place the given default object there.
		/// If the default is not needed it will be deleted by this function.
		/// </summary>
		/// <param name="fieldNumber">Field number to look up.</param>
		/// <param name="pObj">Default object to insert if none exists (ownership transferred on success or deleted on failure).</param>
		/// <returns>Pointer to the found or inserted object, or nullptr if the operation is not supported.</returns>
		virtual ObjectBase* GetFieldObjectWithDefault(FieldNumber fieldNumber, ObjectBase* pObj) { delete pObj; return nullptr; }

		/// <summary>
		/// Get a pointer to the internal vector of objects for the specified field number.
		/// </summary>
		/// <param name="fieldNumber">Field number to look up.</param>
		/// <returns>Pointer to the internal vector or nullptr if not present/empty.</returns>
		virtual std::vector<ObjectBase*>* GetFieldObjects(FieldNumber fieldNumber) { return nullptr; }

		/// <summary>
		/// Determines whether this object is considered empty (no meaningful content).
		/// </summary>
		/// <returns>True if empty, false otherwise.</returns>
		virtual bool IsEmpty() { return true; }

		/// <summary>
		/// Erase the child field with the given field number if possible.
		/// </summary>
		/// <param name="fieldNumber">Field number to erase.</param>
		/// <returns>True if erased, false if not found or not removable.</returns>
		virtual bool EraseField(FieldNumber fieldNumber) { return false; }

		/// <summary>
		/// Convenience: get a pointer to a field object of type T, creating a default T if necessary.
		/// </summary>
		/// <typeparam name="T">Type of object to get/create. Must derive from ObjectBase and be constructible from FieldNumber.</typeparam>
		/// <param name="fieldNumber">Field number to look up.</param>
		/// <returns>Pointer to the existing or newly created object, or nullptr on failure.</returns>
		/// <remarks>
		/// The default object is created with new and ownership is transferred into the container.
		/// This helper also attempts to replace empty mismatched-type placeholders with the requested type.
		/// </remarks>
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
			// Delete the old object and replace it with our new one.
			if ((wantsMessageObject && pObj->IsByteStorageObject() && !pObj->IsMessageObject() && pObj->IsEmpty()) ||
				(!wantsMessageObject && pObj->IsMessageObject() && pObj->IsEmpty()))
			{
				// Erase the old object
				if (!EraseField(fieldNumber))
					assert(!"Failed to erase field");

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

		/// <summary>
		/// Mark this object (and its dirty flag) as clean.
		/// </summary>
		virtual void MarkClean() {
			m_bIsDirty = false;
		}

		/// <summary>
		/// Get the protobuf field number associated with this object.
		/// </summary>
		/// <returns>The FieldNumber value.</returns>
		FieldNumber GetFieldNumber() const {
			return m_fieldNumber;
		}

		/// <summary>
		/// Set the protobuf field number for this object.
		/// </summary>
		/// <param name="fn">New field number.</param>
		/// <returns>Returns this pointer for chaining.</returns>
		ObjectBase* SetFieldNumber(FieldNumber fn) {
			m_fieldNumber = fn;
			return this;
		}

		/// <summary>
		/// Mark this object as dirty; also marks parent objects as dirty recursively.
		/// </summary>
		void MarkDirty() {
			m_bIsDirty = true;
			if (m_pParent)
				m_pParent->MarkDirty();
		}

		/// <summary>
		/// Returns true when the object is flagged as dirty/modified.
		/// </summary>
		bool IsDirty() const {
			return m_bIsDirty;
		}

		/// <summary>
		/// Returns the parent object in the message tree, or nullptr if none.
		/// </summary>
		ObjectBase* GetParent() {
			return m_pParent;
		}

	protected:
		friend class ObjectBaseMessage;

		/// <summary>
		/// Set the parent pointer for this object (used by container classes).
		/// </summary>
		/// <param name="pMsg">Parent object to set.</param>
		void SetParent(ObjectBase* pMsg) {
			m_pParent = pMsg;
		}
	private:
		/// <summary>
		/// The field number this object represents.
		/// </summary>
		FieldNumber m_fieldNumber = 0;

		/// <summary>
		/// Dirty/modified flag; when true the object is considered changed.
		/// </summary>
		bool m_bIsDirty = false;

		/// <summary>
		/// Pointer to the parent container object (if any).
		/// </summary>
		ObjectBase* m_pParent = nullptr;
	};

	/// <summary>
	/// Container object that represents a protobuf message (collection of fields).
	/// </summary>
	/// <remarks>
	/// Stores child field objects indexed by field number. Fields can be repeated or marked unique.
	/// This class manages ownership of child pointers (deletes them in destructor/clear/erase).
	/// </remarks>
	class ObjectBaseMessage : public ObjectBase
	{
	public:
		/// <summary>
		/// Construct an ObjectBaseMessage for the given field number (0 for root messages).
		/// </summary>
		/// <param name="fieldNumber">Field number for this message object.</param>
		ObjectBaseMessage(FieldNumber fieldNumber = 0) : ObjectBase(fieldNumber) { }

		/// <summary>
		/// Destructor: deletes all owned child objects.
		/// </summary>
		~ObjectBaseMessage()
		{
			for (auto& kv : m_objects)
				for (auto& obj : kv.second)
					delete obj;
		}

		/// <summary>
		/// Add a child object to this message.
		/// </summary>
		/// <param name="ob">Object pointer to add. Ownership is transferred on success.</param>
		/// <returns>
		/// Result::success() on success or Result::failure(ErrorCode::CantAddRootMsg) if attempting to
		/// add a non-embedded root message. Other error codes may be returned for uniqueness violations.
		/// </returns>
		/// <remarks>
		/// If the field is marked unique and a value already exists, the existing value(s) are deleted
		/// and replaced by the provided object.
		/// </remarks>
		Result<ErrorCode> AddObject(ObjectBase* ob)
		{
			if (ob->IsMessageObject() && !ob->IsEmbeddedMessageObject())
				return Result<ErrorCode>::failure(ErrorCode::CantAddRootMsg);

			FieldNumber fieldNum = ob->GetFieldNumber();

			auto& vec = m_objects[fieldNum];
			if (!m_fieldsUnique[fieldNum] || vec.empty()) {
				vec.push_back(ob);
				MarkDirty();
			}
			else {
				for (auto& existing : vec)
					delete existing;
				vec.clear();
				vec.push_back(ob);
				MarkDirty();
			}

			ob->SetParent(this);
			return Result<ErrorCode>::success();
		}

		/// <summary>
		/// Clear all child fields, deleting owned objects.
		/// </summary>
		/// <returns>Returns this pointer for chaining.</returns>
		ObjectBaseMessage* Clear()
		{
			for (auto& kv : m_objects) {
				for (auto p : kv.second) delete p;
			}
			m_objects.clear();
			return this;
		}

		/// <summary>
		/// Mark a field as unique (non-repeated). By default fields are repeated.
		/// </summary>
		/// <param name="fieldNum">Field number to mark unique.</param>
		/// <returns>Returns this pointer for chaining.</returns>
		ObjectBaseMessage* SetFieldUnique(FieldNumber fieldNum) {
			m_fieldsUnique[fieldNum] = true;
			MarkDirty();
			return this;
		}

		/// <summary>
		/// Get the last object for the specified field number or nullptr if none exists.
		/// </summary>
		/// <param name="fieldNum">Field number to query.</param>
		/// <returns>Pointer to the last object for that field or nullptr.</returns>
		ObjectBase* GetFieldObject(FieldNumber fieldNum) override
		{
			auto iter = m_objects.find(fieldNum);
			return (iter != m_objects.end() && !iter->second.empty()) ? iter->second.back() : nullptr;
		}

		/// <summary>
		/// Get the last object for the specified field number or insert the provided default object.
		/// </summary>
		/// <param name="fieldNum">Field number to query.</param>
		/// <param name="pDefaultObj">Default object to insert if none exists (ownership transferred on insert, deleted if not used).</param>
		/// <returns>Pointer to the existing or newly inserted object.</returns>
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
			return iter->second.back();
		}

		/// <summary>
		/// Erase and delete all objects associated with the given field number.
		/// </summary>
		/// <param name="fieldNumber">Field number to erase.</param>
		/// <returns>True if erased, false if not present.</returns>
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

		/// <summary>
		/// Get a pointer to the internal vector of objects for the specified field number.
		/// </summary>
		/// <param name="fieldNum">Field number to query.</param>
		/// <returns>Pointer to the internal vector or nullptr if not present/empty.</returns>
		std::vector<ObjectBase*>* GetFieldObjects(FieldNumber fieldNum) override
		{
			auto iter = m_objects.find(fieldNum);
			return (iter != m_objects.end() && !iter->second.empty()) ? &iter->second : nullptr;
		}

		/// <summary>
		/// Returns true when the message contains no child objects.
		/// </summary>
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

		/// <summary>
		/// Deleted copy constructor to avoid accidental shallow copies of owned pointers.
		/// </summary>
		ObjectBaseMessage(const ObjectBaseMessage&) = delete;

		/// <summary>
		/// Move constructor transfers ownership of child pointers and decode hint.
		/// </summary>
		/// <param name="other">Source object to move from.</param>
		ObjectBaseMessage(ObjectBaseMessage&& other) noexcept
			: ObjectBase(other.GetFieldNumber()),
			m_objects(std::move(other.m_objects)),
			m_fieldsUnique(std::move(other.m_fieldsUnique)),
			m_pDecodeHint(std::move(other.m_pDecodeHint))
		{
			// clear other's hint pointer to avoid double-delete (unique_ptr moved)
			other.m_pDecodeHint = nullptr;
		}

		/// <summary>
		/// Move assignment transfers ownership and clears current owned children.
		/// </summary>
		/// <param name="other">Source object to move-assign from.</param>
		/// <returns>Reference to this object.</returns>
		ObjectBaseMessage& operator=(ObjectBaseMessage&& other) noexcept
		{
			if (this != &other)
			{
				// release current
				for (auto& kv : m_objects)
					for (auto p : kv.second) delete p;
				m_objects.clear();

				m_objects = std::move(other.m_objects);
				m_fieldsUnique = std::move(other.m_fieldsUnique);
				m_pDecodeHint = std::move(other.m_pDecodeHint);
				SetFieldNumber(other.GetFieldNumber());
				other.m_pDecodeHint = nullptr;
			}
			return *this;
		}

		/// <summary>
		/// Mark this message and all child objects (recursively) as dirty.
		/// </summary>
		void MarkDirtyAndChildren()
		{
			MarkDirty();

			for (auto& kv : m_objects)
			{
				for (auto& obj : kv.second)
				{
					if (obj->IsMessageObject())
						reinterpret_cast<ObjectBaseMessage*>(obj)->MarkDirtyAndChildren();
					else
						obj->MarkDirty();
				}
			}
		}

		/// <summary>
		/// Merge another message into this one. Ownership of source child pointers is transferred when copied.
		/// </summary>
		/// <param name="pOtherMsg">Message to merge from (ownership of contained pointers may be moved).</param>
		/// <returns>Result::success() on success, or a failure code on conflict (e.g. SupposedToBeUnique).</returns>
		/// <remarks>
		/// The merge semantics are:
		/// - For repeated fields: destination is cleared and source elements are moved in.
		/// - For unique fields: destination is replaced or merged (for embedded messages).
		/// </remarks>
		Result<ErrorCode> MergeWith(ObjectBaseMessage* pOtherMsg)
		{
			for (auto& obj : pOtherMsg->m_objects)
			{
				auto& srcVec = obj.second;
				auto& dstVec = m_objects[obj.first];

				// If the field is repeated, clear destination and copy src pointers.
				if (!m_fieldsUnique[obj.first])
				{
					for (auto d : dstVec) delete d;
					dstVec.clear();

					for (auto s : srcVec) {
						dstVec.push_back(s);
						s->SetParent(this);
					}
					srcVec.clear();
					continue;
				}

				// Unique field
				if (dstVec.empty())
				{
					for (auto s : srcVec) {
						dstVec.push_back(s);
						s->SetParent(this);
					}
					srcVec.clear();
					continue;
				}

				if (dstVec.size() != 1 && srcVec.size() != 1)
					return Result<ErrorCode>::failure(ErrorCode::SupposedToBeUnique);

				// If this is a message object, recursively merge it.
				if (dstVec[0]->IsMessageObject())
				{
					ObjectBaseMessage* pMsg = static_cast<ObjectBaseMessage*>(dstVec[0]);
					ObjectBaseMessage* pMsg2 = static_cast<ObjectBaseMessage*>(srcVec[0]);

					returnIfNonNull(pMsg->MergeWith(pMsg2));
					// leave pointers in place (ownership unchanged)
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
		/// <summary>
		/// Returns true for message objects.
		/// </summary>
		bool IsMessageObject() const override { return true; }

		/// <summary>
		/// Returns true for byte-storage message objects.
		/// </summary>
		bool IsByteStorageObject() const override { return true; }

		/// <summary>
		/// Compute the total payload size of this message by summing child payload sizes.
		/// </summary>
		/// <returns>Total encoded payload size in bytes.</returns>
		size_t GetPayloadSize() const override
		{
			size_t totalSize = 0;
			for (auto& kv : m_objects)
				for (auto& obj : kv.second)
					if (obj->IsDirty())
						totalSize += obj->GetPayloadSize();

			return totalSize;
		}

		/// <summary>
		/// Compute the total dirty payload size by summing child dirty sizes.
		/// </summary>
		/// <returns>Total dirty encoded payload size in bytes.</returns>
		size_t GetDirtyPayloadSize() const override
		{
			size_t totalSize = 0;
			for (auto& kv : m_objects)
				for (auto& obj : kv.second)
					if (obj->IsDirty())
						totalSize += obj->GetDirtyPayloadSize();

			return totalSize;
		}

		/// <summary>
		/// Encode all child objects into the provided output stream.
		/// </summary>
		/// <param name="outputStream">Vector to which encoded bytes are appended.</param>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			for (auto& kv : m_objects)
				for (auto& obj : kv.second)
					obj->Encode(outputStream);
		}

		/// <summary>
		/// Encode only dirty child objects into the provided output stream.
		/// </summary>
		/// <param name="outputStream">Vector to which encoded bytes are appended.</param>
		void EncodeDirty(std::vector<uint8_t>& outputStream) const override
		{
			for (auto& kv : m_objects)
				for (auto& obj : kv.second)
					if (obj->IsDirty())
						obj->EncodeDirty(outputStream);
		}

		/// <summary>
		/// Mark this message and all children as clean.
		/// </summary>
		void MarkClean() override
		{
			for (auto& kv : m_objects)
				for (auto& ob2 : kv.second)
					ob2->MarkClean();

			ObjectBase::MarkClean();
		}

		/// <summary>
		/// Set a decoding hint for this message. The message takes ownership of the pointer.
		/// </summary>
		/// <param name="pHint">Pointer to a DecodeHint; ownership is transferred.</param>
		void SetDecodingHint(DecodeHint* pHint) {
			m_pDecodeHint.reset(pHint);
		}

		/// <summary>
		/// Get the decoding hint associated with this message, or nullptr.
		/// </summary>
		/// <returns>Pointer to the current DecodeHint (not owned by the caller) or nullptr.</returns>
		DecodeHint* GetDecodeHint() {
			return m_pDecodeHint.get();
		}

	private:
		/// <summary>
		/// Map of field number to vector of child pointers (ownership held by this container).
		/// </summary>
		std::unordered_map<uint64_t, std::vector<ObjectBase*> > m_objects;

		/// <summary>
		/// Tracks whether a given field number is unique (non-repeated).
		/// </summary>
		std::unordered_map<uint64_t, bool> m_fieldsUnique;

		/// <summary>
		/// Optional decode-time hint tree owned by this message.
		/// </summary>
		std::unique_ptr<DecodeHint> m_pDecodeHint;
	};

	/// <summary>
	/// An embedded message object (a length-delimited field that itself contains fields).
	/// </summary>
	class ObjectMessage : public ObjectBaseMessage
	{
	public:
		/// <summary>Construct an embedded message object for the given field number.</summary>
		explicit ObjectMessage(FieldNumber fieldNum) : ObjectBaseMessage(fieldNum) {}

	public:
		/// <summary>Returns true for embedded message objects.</summary>
		bool IsEmbeddedMessageObject() const override { return true; }

		/// <summary>Return encoded payload size including tag and length prefix.</summary>
		size_t GetPayloadSize() const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetPayloadSize();
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(embPayloadSize) + embPayloadSize;
		}

		/// <summary>Return encoded dirty payload size including tag and length prefix.</summary>
		size_t GetDirtyPayloadSize() const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetDirtyPayloadSize();
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(embPayloadSize) + embPayloadSize;
		}

		/// <summary>Encode the embedded message (tag, length, then child payloads).</summary>
		/// <param name="outputStream">Stream to append encoded bytes to.</param>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetPayloadSize();

			// Reserve capacity: tag + length + payload
			auto tag = CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN);
			outputStream.reserve(outputStream.size() + GetVarIntSize(tag) + GetVarIntSize(embPayloadSize) + embPayloadSize);

			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, embPayloadSize);
			ObjectBaseMessage::Encode(outputStream);
		}

		/// <summary>Encode only dirty parts (tag, length, then dirty child payloads).</summary>
		void EncodeDirty(std::vector<uint8_t>& outputStream) const override
		{
			size_t embPayloadSize = ObjectBaseMessage::GetDirtyPayloadSize();
			auto tag = CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN);
			outputStream.reserve(outputStream.size() + GetVarIntSize(tag) + GetVarIntSize(embPayloadSize) + embPayloadSize);

			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, embPayloadSize);
			ObjectBaseMessage::EncodeDirty(outputStream);
		}
	};

	/// <summary>
	/// Represents a string (length-delimited) field.
	/// </summary>
	class ObjectString : public ObjectBase
	{
	public:
		/// <summary>Construct a string object for a field (optional initial content).</summary>
		ObjectString(FieldNumber fn, const std::string& str = "") : ObjectBase(fn), m_content(str) {}

		/// <summary>Set the string content and mark the object dirty.</summary>
		/// <returns>this pointer for chaining.</returns>
		ObjectString* SetContent(const std::string& str) noexcept
		{
			m_content = str;
			MarkDirty();
			return this;
		}

		/// <summary>Get a const-reference to the string content (avoids copying).</summary>
		const std::string& GetContent() const noexcept
		{
			return m_content;
		}

	public:
		/// <summary>Return encoded payload size (tag + length varint + string bytes).</summary>
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(m_content.size()) + m_content.size();
		}

		/// <summary>Encode this string field into the output stream.</summary>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			size_t tagSize = GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			size_t lenSize = GetVarIntSize(m_content.size());
			size_t need = tagSize + lenSize + m_content.size();
			outputStream.reserve(outputStream.size() + need);

			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, m_content.size());
			size_t oldLength = outputStream.size();
			outputStream.resize(oldLength + m_content.size());
			memcpy(outputStream.data() + oldLength, m_content.data(), m_content.size());
		}

		/// <summary>Returns true if the contained string is empty.</summary>
		bool IsEmpty() override
		{
			return m_content.empty();
		}

		bool IsByteStorageObject() const override { return true; }
		bool IsStringObject() const override { return true; }

	private:
		std::string m_content;
	};

	/// <summary>
	/// Represents raw bytes (length-delimited) field.
	/// </summary>
	class ObjectBytes : public ObjectBase
	{
	public:
		/// <summary>Construct from a vector of bytes.</summary>
		explicit ObjectBytes(FieldNumber fn, const std::vector<uint8_t>& val = {}) : ObjectBase(fn), m_content(val) {}

		/// <summary>Construct from raw char pointer and size.</summary>
		ObjectBytes(FieldNumber fn, const char* data, size_t sz) : ObjectBase(fn)
		{
			m_content.resize(sz);
			if (data && sz) memcpy(m_content.data(), data, sz);
		}

		/// <summary>Construct from raw uint8_t pointer and size.</summary>
		ObjectBytes(FieldNumber fn, const uint8_t* data, size_t sz) : ObjectBase(fn)
		{
			m_content.resize(sz);
			if (data && sz) memcpy(m_content.data(), data, sz);
		}

		/// <summary>Set the byte content (copy) and mark dirty.</summary>
		ObjectBytes* SetContent(const std::vector<uint8_t>& val) noexcept
		{
			m_content = val;
			MarkDirty();
			return this;
		}

		/// <summary>Return a const-reference to the underlying byte vector.</summary>
		const std::vector<uint8_t>& GetContent() const noexcept
		{
			return m_content;
		}

	public:
		/// <summary>Return encoded payload size (tag + length + bytes).</summary>
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN)) + GetVarIntSize(m_content.size()) + m_content.size();
		}

		/// <summary>Encode this bytes field into the provided output stream.</summary>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			size_t tagSize = GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			size_t lenSize = GetVarIntSize(m_content.size());
			size_t need = tagSize + lenSize + m_content.size();
			outputStream.reserve(outputStream.size() + need);

			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_LEN));
			EncodeVarInt(outputStream, m_content.size());
			size_t oldLength = outputStream.size();
			outputStream.resize(oldLength + m_content.size());
			memcpy(outputStream.data() + oldLength, m_content.data(), m_content.size());
		}

		/// <summary>Return true if there are no bytes.</summary>
		bool IsEmpty() override
		{
			return m_content.empty();
		}

		bool IsByteStorageObject() const override { return true; }
		bool IsByteArrayObject() const override { return true; }

	private:
		std::vector<uint8_t> m_content;
	};

	/// <summary>
	/// Represents a varint (integer) field.
	/// </summary>
	class ObjectVarInt : public ObjectBase
	{
	public:
		/// <summary>Construct a varint object for the given field and initial value.</summary>
		ObjectVarInt(FieldNumber fn, uint64_t val = 0) : ObjectBase(fn), m_value(val) {}

		/// <summary>Set the integer value and mark dirty.</summary>
		ObjectVarInt* SetValue(uint64_t c) noexcept
		{
			m_value = c;
			MarkDirty();
			return this;
		}

		/// <summary>Get the stored integer value.</summary>
		uint64_t GetValue() const noexcept
		{
			return m_value;
		}

	public:
		/// <summary>Return payload size (tag varint + value varint).</summary>
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_VARINT)) + GetVarIntSize(m_value);
		}

		/// <summary>Encode tag and varint value into the output stream.</summary>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			outputStream.reserve(outputStream.size() + GetPayloadSize());
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_VARINT));
			EncodeVarInt(outputStream, m_value);
		}

		bool IsEmpty() override
		{
			return false;
		}

	private:
		uint64_t m_value;
	};

	/// <summary>
	/// 64-bit fixed-width integer field (wire type 1).
	/// Encoded as little-endian 8 bytes per protobuf spec.
	/// </summary>
	class ObjectFixed64 : public ObjectBase
	{
	public:
		/// <summary>Construct with optional initial value.</summary>
		ObjectFixed64(FieldNumber fn, uint64_t val = 0) : ObjectBase(fn), m_value(val) {}

		/// <summary>Set the value and mark dirty.</summary>
		ObjectFixed64* SetValue(uint64_t c) noexcept
		{
			m_value = c;
			MarkDirty();
			return this;
		}

		/// <summary>Get the stored value.</summary>
		uint64_t GetValue() const noexcept
		{
			return m_value;
		}

	public:
		/// <summary>Return encoded payload size (tag + 8 bytes).</summary>
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_I64)) + sizeof(uint64_t);
		}

		/// <summary>Encode the fixed64 value in little-endian order.</summary>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			outputStream.reserve(outputStream.size() + GetPayloadSize());
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_I64));

			// append little-endian bytes explicitly (portable)
			uint64_t v = m_value;
			for (int i = 0; i < 8; ++i) {
				outputStream.push_back(static_cast<uint8_t>(v & 0xFF));
				v >>= 8;
			}
		}

		bool IsEmpty() override
		{
			return false;
		}

	private:
		uint64_t m_value;
	};

	/// <summary>
	/// 32-bit fixed-width integer field (wire type 5).
	/// Encoded as little-endian 4 bytes per protobuf spec.
	/// </summary>
	class ObjectFixed32 : public ObjectBase
	{
	public:
		/// <summary>Construct with optional initial value.</summary>
		ObjectFixed32(FieldNumber fn, uint32_t val = 0) : ObjectBase(fn), m_value(val) {}

		/// <summary>Set the value and mark dirty.</summary>
		ObjectFixed32* SetValue(uint32_t c) noexcept
		{
			m_value = c;
			MarkDirty();
			return this;
		}

		/// <summary>Get the stored value.</summary>
		uint32_t GetValue() const noexcept
		{
			return m_value;
		}

	public:
		/// <summary>Return encoded payload size (tag + 4 bytes).</summary>
		size_t GetPayloadSize() const override
		{
			return GetVarIntSize(CombineFieldNumberAndTag(GetFieldNumber(), TAG_I32)) + sizeof(uint32_t);
		}

		/// <summary>Encode the fixed32 value in little-endian order.</summary>
		void Encode(std::vector<uint8_t>& outputStream) const override
		{
			outputStream.reserve(outputStream.size() + GetPayloadSize());
			EncodeVarInt(outputStream, CombineFieldNumberAndTag(GetFieldNumber(), TAG_I32));

			uint32_t v = m_value;
			for (int i = 0; i < 4; ++i) {
				outputStream.push_back(static_cast<uint8_t>(v & 0xFF));
				v >>= 8;
			}
		}

	private:
		uint32_t m_value;
	};

	static ErrorOr<uint64_t> DecodeVarInt(const char* data, size_t& offset, size_t size)
	{
		const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
		uint64_t result = 0;
		int shift = 0;
		int byteCount = 0;

		while (true)
		{
			if (offset >= size)
				return ErrorOr<uint64_t>::failure(ErrorCode::OutOfBounds);

			uint8_t byte = ptr[offset++];
			++byteCount;

			// take 7 bits
			result |= static_cast<uint64_t>(byte & 0x7F) << shift;

			if ((byte & 0x80) == 0)
				break;

			shift += 7;
			if (shift >= 64 || byteCount >= 10)
				return ErrorOr<uint64_t>::failure(ErrorCode::VarIntTooBig);
		}

		return ErrorOr<uint64_t>::success(result);
	}

	/// Decode a buffer into an ObjectBaseMessage using optional hints.
	static Result<ErrorCode> DecodeBlock(const char* data, size_t sz, ObjectBaseMessage* block, DecodeHint* pHint = nullptr)
	{
#ifdef DISABLE_PROTOBUF
		return Result<ErrorCode>::failure(ErrorCode::NotSupported);
#endif

		if (!pHint)
			pHint = block->GetDecodeHint();

		size_t offset = 0;
		while (offset < sz)
		{
			auto fieldNumAndTag = DecodeVarInt(data, offset, sz);
			if (!fieldNumAndTag.has_value())
				return Result<ErrorCode>::failure(fieldNumAndTag.error());

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

				// Heuristic: printable ASCII (simple)
				bool isString = true;
				for (size_t i = 0; i < length; ++i) {
					uint8_t chr = static_cast<uint8_t>(dataSubset[i]);
					if (chr < ' ' || chr == 0x7F) { isString = false; break; }
				}

				// Decide by hint if available
				DecodeHint* pChildHint = nullptr;
				if (pHint)
					pChildHint = pHint->Lookup(fieldNum);

				if (pChildHint)
				{
					switch (pChildHint->GetType())
					{
					case DecodeHint::O_STRING:
					{
						auto addResult = block->AddObject(new ObjectString(fieldNum, std::string(dataSubset, length)));
						if (!addResult.has_value()) return addResult;
						break;
					}
					case DecodeHint::O_BYTES:
					{
						auto addResult = block->AddObject(new ObjectBytes(fieldNum, dataSubset, length));
						if (!addResult.has_value()) return addResult;
						break;
					}
					case DecodeHint::O_MESSAGE:
					{
						// Try parse as nested message; on failure treat as bytes
						ObjectMessage* pChld = new ObjectMessage(fieldNum);
						auto decodeResult = DecodeBlock(dataSubset, length, pChld, pChildHint);
						if (!decodeResult.has_value())
						{
							delete pChld;
							auto addResult = block->AddObject(new ObjectBytes(fieldNum, dataSubset, length));
							if (!addResult.has_value()) return addResult;
						}
						else
						{
							auto addResult = block->AddObject(pChld);
							if (!addResult.has_value()) return addResult;
						}
						break;
					}
					default:
					{
						// unknown hint type -> fallback to heuristic:
						if (isString) {
							auto addResult = block->AddObject(new ObjectString(fieldNum, std::string(dataSubset, length)));
							if (!addResult.has_value()) return addResult;
						}
						else {
							ObjectMessage* pChld = new ObjectMessage(fieldNum);
							auto decodeResult = DecodeBlock(dataSubset, length, pChld, pChildHint);
							if (!decodeResult.has_value()) {
								delete pChld;
								auto addResult = block->AddObject(new ObjectBytes(fieldNum, dataSubset, length));
								if (!addResult.has_value()) return addResult;
							}
							else {
								auto addResult = block->AddObject(pChld);
								if (!addResult.has_value()) return addResult;
							}
						}
						break;
					}
					} // switch pChildHint->GetType()
				}
				else
				{
					// No hint provided -> use heuristic
					if (isString)
					{
						auto addResult = block->AddObject(new ObjectString(fieldNum, std::string(dataSubset, length)));
						if (!addResult.has_value()) return addResult;
					}
					else
					{
						// Try parse as object; fall back to bytes
						ObjectMessage* pChld = new ObjectMessage(fieldNum);
						auto decodeResult = DecodeBlock(dataSubset, length, pChld, nullptr);
						if (!decodeResult.has_value())
						{
							delete pChld;
							auto addResult = block->AddObject(new ObjectBytes(fieldNum, dataSubset, length));
							if (!addResult.has_value()) return addResult;
						}
						else
						{
							auto addResult = block->AddObject(pChld);
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

				uint64_t val;
				memcpy(&val, reinterpret_cast<const uint8_t*>(data) + offset, sizeof(uint64_t));
				auto addResult = block->AddObject(new ObjectFixed64(fieldNum, val));
				if (!addResult.has_value()) return addResult;
				offset += sizeof(uint64_t);
				break;
			}

			case TAG_I32:
			{
				if (offset + sizeof(uint32_t) > sz)
					return Result<ErrorCode>::failure(ErrorCode::OutOfBounds);

				uint32_t val;
				memcpy(&val, reinterpret_cast<const uint8_t*>(data) + offset, sizeof(uint32_t));
				auto addResult = block->AddObject(new ObjectFixed32(fieldNum, val));
				if (!addResult.has_value()) return addResult;
				offset += sizeof(uint32_t);
				break;
			}

			default:
				return Result<ErrorCode>::failure(ErrorCode::UnknownTag);
			} // switch(tag)

			if (offset > sz)
				return Result<ErrorCode>::failure(ErrorCode::OutOfBounds);
		} // while

		return Result<ErrorCode>::success();
	}

	static Result<ErrorCode> DecodeBlock(const uint8_t* data, size_t sz, ObjectBaseMessage* block)
	{
		return DecodeBlock(reinterpret_cast<const char*>(data), sz, block);
	}
}
