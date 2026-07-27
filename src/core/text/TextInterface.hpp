#pragma once

#include <string>
#include <cstring>
#include "../models/RectAndPoint.hpp"

#ifdef _WIN32
#include <tchar.h>

class String
{
private:
	TCHAR* m_content = NULL;
	size_t m_size = 0;

public:
	String() {}
	~String() { Clear(); }

	String(const String& copy) {
		if (!copy.m_content)
			return;

		m_content = new TCHAR[copy.m_size + 1];
		m_size = copy.m_size;
		_tcscpy(m_content, copy.m_content);
	}

	String& operator=(const String& eq) {
		Clear();
		if (!eq.m_content)
			return (*this);

		m_content = new TCHAR[eq.m_size + 1];
		m_size = eq.m_size;
		_tcscpy(m_content, eq.m_content);
		return (*this);
	}

	String(String&& move) noexcept {
		m_content = move.m_content;
		m_size = move.m_size;
		move.m_content = NULL;
		move.m_size = m_size;
	}

	void Clear() {
		if (m_content)
			delete[] m_content;

		m_content = NULL;
	}

	bool Empty() const {
		if (!m_content || !m_size) return true;
		return m_content[0] == (TCHAR)'\0';
	}

	void Set(const std::string& text); // TextInterface_Win32

	String(const std::string& text) {
		Set(text);
	}

	const TCHAR* GetWrapped() const noexcept {
		return m_content;
	}

	size_t GetSize() const {
		size_t sz = m_size;
		if (sz == 0) return 0;
		while (sz > 1 && m_content[sz - 1] == (TCHAR)'\0') sz--;
		return sz;
	}
};

#else

class String
{
private:
	std::string m_content;

public:
	void Clear() {
		m_content = "";
	}
	void Set(const std::string& text) {
		m_content = text;
	}
	const std::string& GetWrapped() const {
		return m_content;
	}
};

#endif

struct DrawingContext;

Point MdMeasureString(DrawingContext* context, const String& word, int styleFlags, bool& outWasWordWrapped, int maxWidth = 0);
int MdLineHeight(DrawingContext* context, int styleFlags);
int MdSpaceWidth(DrawingContext* context, int styleFlags);
void MdDrawString(DrawingContext* context, const Rect& rect, const String& str, int styleFlags);
void MdDrawCodeBackground(DrawingContext* context, const Rect& rect);
void MdDrawForwardBackground(DrawingContext* context, const Rect& rect);
int MdGetQuoteIndentSize();
void MdSetClippingRect(DrawingContext* context, const Rect& rect);
void MdClearClippingRect(DrawingContext* context);

