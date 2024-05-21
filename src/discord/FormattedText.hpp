#pragma once

#include <vector>
#include <string>
#include "TextInterface.hpp"

struct Token
{
	enum Type
	{
		TEXT,
		FINISH,
		QUOTE,
		SPACE,
		NEWLINE,
		LINK,
		MENTION,
		TIMESTAMP,
		CODE,
		SMALLCODE,
		MLCODE_BEGIN,
		MLCODE_END,
		STRONG_BEGIN,
		STRONG_END,
		ITALIC_BEGIN,
		ITALIC_END,
		UNDERL_BEGIN,
		UNDERL_END,
		ITALIE_BEGIN,
		ITALIE_END,
		LIST_ITEM,
		EVERYONE,
		HERE,
		CUSTOM_EMOJI,
	};

	int m_type = TEXT;
	std::string m_text;

	Token(int type, const std::string& text) :
		m_type(type),
		m_text(text)
	{}
};

#define WORD_SPACE     (1 << 0) // if the word is actually a space
#define WORD_NEWLINE   (1 << 1) // if the word is actually a new-line
#define WORD_STRONG    (1 << 2)
#define WORD_ITALIC    (1 << 3)
#define WORD_ITALIE    (1 << 4)
#define WORD_UNDERL    (1 << 5)
#define WORD_QUOTE     (1 << 6)
#define WORD_LISTITEM  (1 << 7)
#define WORD_CODE      (1 << 8)
#define WORD_MLCODE    (1 << 9)
#define WORD_DONTDRAW  (1 << 10) // can be cleared or set by Layout, tells the renderer to ignore
#define WORD_LINK      (1 << 11)
#define WORD_MENTION   (1 << 12)
#define WORD_TIMESTAMP (1 << 13)
#define WORD_AFNEWLINE (1 << 14) // word had a new line or word wrap cutoff preceding it.
#define WORD_EVERYONE  (1 << 15) // @everyone and @here
#define WORD_WRAPPED   (1 << 16) // word was wrapped.  Ignored for anything that doesn't have the WORD_MLCODE on
#define WORD_EMOJI     (1 << 17) // Unicode emoji.  Unsupported as of yet.
#define WORD_CEMOJI    (1 << 18) // Custom emoji.
#define WORD_HEADER1   (1 << 19) // Header 1 style. Currently only used by emoji.

struct Word
{
	int m_flags = 0;

	// The content of the word.
	std::string m_content;
	std::string m_contentOverride;

	// Interface content. On Windows, a 16-bit wide character version of m_content
	String m_ifContent;

	// The rectangle this word will occupy.
	Rect m_rect;

	// The size of the word.  If (0, 0), or certain other conditions are met,
	// then the size will be recalculated.
	Point m_size;

	bool ShouldRelayout(const Rect& layoutRect) const
	{
		if (m_size.x == 0 && m_size.y == 0)
			return true;

		// If this is a multiline code block, return true.  
		if ((m_flags & WORD_MLCODE) &&
			(m_size.x > layoutRect.Width() || ((m_flags & WORD_WRAPPED) && m_size.x < layoutRect.Width())))
			return true;

		return false;
	}

	Word() {}
	Word(int flags, const std::string& content) : m_flags(flags), m_content(content) {}
	Word(int flags, const char* content, size_t size) : m_flags(flags), m_content(content, size) {}

	void SetContentOverride(const std::string & oride)
	{
		if (oride.empty())
		{
			m_contentOverride = "";
			m_ifContent.Set(m_content);
		}
		else
		{
			m_contentOverride = oride;
			m_ifContent.Set(m_contentOverride);
		}
	}

	const std::string& GetContentOverride() const
	{
		if (m_contentOverride.empty())
			return m_content;
		else
			return m_contentOverride;
	}

	void UpdateIfContent()
	{
		m_ifContent.Set(GetContentOverride());
	}
};

class FormattedText
{
public:
	void SetMessage(const std::string& msg);
	void Layout(DrawingContext* context, const Rect& rect, int offsetX = 0);
	void Draw(DrawingContext* context, int offsetY = 0);
	Rect GetExtent();

	std::vector<Word>& GetWords() {
		return m_words;
	}

	void Clear() {
		m_rawMessage.clear();
		m_blocks.clear();
		m_tokens.clear();
		m_words.clear();
		m_bFormatted = false;
	}

	void ClearFormatting() {
		m_bFormatted = false;

		for (auto& w : m_words) {
			w.m_rect = {};
		}
	}

	bool Empty() const {
		if (m_rawMessage.empty())
			return true;
		if (m_words.empty())
			return true;
		return false;
	}

private:
	std::string m_rawMessage;
	std::vector<std::vector<std::pair<std::string, std::string>>> m_blocks; // see note 1. and 2.
	std::vector<Token> m_tokens;
	std::vector<Word> m_words;
	bool m_bFormatted = false;
	Rect m_layoutRect;

	// Notes!
	// 1. This is bullshit, to be honest. But it should work.
	//    First, we split by "```" separators. After, we split
	//    each *even* member by "`" separators.
	// 2. The "pair" represents the pair of strings containing the
	//    regex-formatted string and the raw string, respectively.
	//    They both have the same length, because of the way we
	//    use regex to perform the replacements.
	// 3. Words that are too long to fit aren't being split.
	//    I decided not to bother with that, although it is possible.
	// 4. The decision to use a pair was done retroactively because
	//    links were breaking without this trick.  Formatting was
	//    applied in the links, and I was too lazy to make sure
	//    it does not happen (by adding another step in the parse
	//    process), so I ended up resorting to this approach.
	//    Should still be plenty fast I hope :)

private:
	void AddWord(const Word& w) {
		m_words.push_back(w);
	}
	std::vector<std::pair<std::string, std::string> > SplitBackticks(const std::string& str); // see note 2. and 4.
	void UseRegex(std::string& str);
	void Tokenize(const std::string& str, const std::string& oldmsg);
	std::string EscapeChars(const std::string& str);

	void SplitBlocks();
	void RegexNecessary();
	void TokenizeAll();
	void ParseText();
};

