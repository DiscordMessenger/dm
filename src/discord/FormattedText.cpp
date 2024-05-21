#include "FormattedText.hpp"
#include <cassert>
#include <stack>
#include <algorithm>
#include "RectAndPoint.hpp"

#define USE_STL_REGEX //-- way slower than Boost Regex

#ifdef USE_STL_REGEX
#include <regex>
#define REN std // regex namespace
#else
#include "boost/regex.hpp"
#define REN boost // regex namespace
#endif

// ======== KNOWN ISSUES ========
//
// 1. Chinese characters don't get split up like regular DrawText does
// 2. Arabic words render in reverse.  That should be fixed later.
//

#define CHAR_BEG_STRONG (char(0x01))
#define CHAR_END_STRONG (char(0x02))
#define CHAR_BEG_ITALIC (char(0x03))
#define CHAR_END_ITALIC (char(0x04))
#define CHAR_BEG_UNDERL (char(0x05))
#define CHAR_END_UNDERL (char(0x06))
#define CHAR_BEG_ITALIE (char(0x07))
#define CHAR_END_ITALIE (char(0x08))
#define CHAR_BEG_CODE   (char(0x0B))
#define CHAR_END_CODE   (char(0x0C))
#define CHAR_BEG_MLCODE (char(0x0E))
#define CHAR_END_MLCODE (char(0x0F))
#define CHAR_BEG_QUOTE  (char(0x10))
#define CHAR_BEG_LITEM  (char(0x11))
#define CHAR_NOOP       (char(0x12)) // will be ignored in the final output
#define CHAR_EVERYONE   (char(0x13))
#define CHAR_HERE       (char(0x14))
#define CHAR_ESCAPE_UNDERSCORE (char(0x15)) // _
#define CHAR_ESCAPE_ASTERISK   (char(0x16)) // *
#define CHAR_ESCAPE_BACKSLASH  (char(0x17)) // \.
#define CHAR_ESCAPE_BACKTICK   (char(0x18)) // `
#define CHAR_ESCAPE_POUND      (char(0x19)) // #
#define CHAR_ESCAPE_MINUS      (char(0x1A)) // -
#define CHAR_ESCAPE_PLUS       (char(0x1B)) // +

int g_tokenTypeTable[] = {
	0,
	Token::STRONG_BEGIN,
	Token::STRONG_END,
	Token::ITALIC_BEGIN,
	Token::ITALIC_END,
	Token::UNDERL_BEGIN,
	Token::UNDERL_END,
	Token::ITALIE_BEGIN,
	Token::ITALIE_END,
	0, // tab
	0, // line feed
	0, // mlcode-chars don't use this system
	0, // mlcode-chars don't use this system
	0, // carriage return
	Token::MLCODE_BEGIN,
	Token::MLCODE_END,
	Token::QUOTE,
	Token::LIST_ITEM,
	0, // character means no op
};

static REN::regex g_StrongMatch("(\\*){2}[^\\*\\r\\n].*?(\\*){2}");
static REN::regex g_ItalicMatch("\\*[^\\*\\r\\n].*?\\*");
static REN::regex g_UnderlMatch("(_){2}[^_\\r\\n].*?(_){2}");
static REN::regex g_ItalieMatch("(?=[ \\_\\r\\n])_.*?_(?=[ \\_\\r\\n])");
static REN::regex g_QuoteMatch("^> .*?(\n|$)");

// Basic Markdown syntax:
//
// something - Normal formatting
// *something* - Emphasis (italic)
// **something** - Big emphasis (bold)
// _something_ - Under emphasis (italic)
// __something__ - Underline
// ~~something~~ - Strikethrough
// `something` - Code display
// ```<LF>something<LF>``` - Multi-line code display
//
// Discord specific features:
// http://something or https://something - Link
// <@something> - Mention User
// <#something> - Mention Channel
// <@&something> - Mention Role
// <t:SOMETHING:SOMETHING> - Time stamp
//
// Added in newer Discord:
// > something - Quote
// # something - Header 1
// ## something - Header 2 ... so on until header 6
// * something - List item
// - something - List item
// 1. something - Numbered list item
//
// Discord Markdown does not support these features, so we won't either:
// - Tables
// - Warning / Note stuff

static void AddAndClearToken(std::vector<Token>& tokens, std::string& tok, int type)
{
	if (!tok.empty())
	{
		tokens.push_back(Token(type, tok));
		tok.clear();
	}
}

static void RegexReplace(std::string& msg, const REN::regex& regex, int length1, int length2, char chr1, char chr2)
{
	REN::smatch match;
	while (REN::regex_search(msg, match, regex))
	{
		auto match_begin = match.position();
		auto match_end = match_begin + match.length();

		for (int i = 1; i < length1; i++)
			msg[match_begin + i] = CHAR_NOOP;
		for (int i = 1; i < length2; i++)
			msg[match_end - 1 - i] = CHAR_NOOP;

		if (length1)
			msg[match_begin] = chr1;
		if (length2)
			msg[match_end - 1] = chr2;
	}
}

void FormattedText::Tokenize(const std::string& msg, const std::string& oldmsg)
{
	assert(msg.size() == oldmsg.size());
	auto& tokens = m_tokens;
	std::string current = "";
	size_t msgSize = msg.size();
	for (size_t i = 0; i < msgSize; )
	{
		char chr = msg[i];

		switch (chr)
		{
			case CHAR_ESCAPE_UNDERSCORE: chr = '_'; goto _def;
			case CHAR_ESCAPE_ASTERISK:   chr = '*'; goto _def;
			case CHAR_ESCAPE_BACKSLASH:  chr ='\\'; goto _def;
			case CHAR_ESCAPE_BACKTICK:   chr = '`'; goto _def;
			case CHAR_ESCAPE_POUND:      chr = '#'; goto _def;
			case CHAR_ESCAPE_MINUS:      chr = '-'; goto _def;
			case CHAR_ESCAPE_PLUS:       chr = '+'; goto _def;
			case CHAR_NOOP: i++; break;
			case CHAR_BEG_STRONG:
			case CHAR_END_STRONG:
			case CHAR_BEG_ITALIC:
			case CHAR_END_ITALIC:
			case CHAR_BEG_UNDERL:
			case CHAR_END_UNDERL:
			case CHAR_BEG_ITALIE:
			case CHAR_END_ITALIE:
			case CHAR_BEG_CODE:
			case CHAR_END_CODE:
			case CHAR_BEG_QUOTE:
			case CHAR_BEG_LITEM:
			{
				// The line feed is a separator. It has nothing to do with formatting, however.
				AddAndClearToken(tokens, current, Token::TEXT);
				i++;
#ifdef _DEBUG
				current = "[This is a formatting token]";
#else
				current = "x";
#endif
				AddAndClearToken(tokens, current, g_tokenTypeTable[chr]);
				break;
			}
			case CHAR_BEG_MLCODE:
			{
				AddAndClearToken(tokens, current, Token::TEXT);
				// this ought to be followed by two NO_OP chars
				if (i + 2 >= msgSize)
					break;
				i += 3;

				size_t backup = i; // to allow retreat
				size_t foundEnd = std::string::npos;
				for (; i <= msgSize - 3; i++) {
					if (msg[i] == CHAR_NOOP && msg[i + 1] == CHAR_NOOP && msg[i + 2] == CHAR_END_MLCODE) {
						foundEnd = i;
						break;
					}
				}

				if (foundEnd == std::string::npos) {
					assert(!"Screw that!");
					i = backup;
					break;
				}

				current = msg.substr(backup, foundEnd - backup);
				AddAndClearToken(tokens, current, Token::CODE);
				i = foundEnd + 3;
				break;
			}
			case '\r':
			{
				// seems like Discord puts \r for some reason?  Well that's OK, just skip the char and break the token
				AddAndClearToken(tokens, current, Token::TEXT);
				i++;
				break;
			}
			case '\n':
			{
				// The line feed is a separator. It has nothing to do with formatting, however.
				AddAndClearToken(tokens, current, Token::TEXT);
				current = "\n";
				AddAndClearToken(tokens, current, Token::NEWLINE);
				i++;
				break;
			}
			case ' ':
			{
				// The space is a separator. It has nothing to do with actual formatting, it only
				// has to do with separating words when word-wrapping.
				AddAndClearToken(tokens, current, Token::TEXT);
				current = " ";
				AddAndClearToken(tokens, current, Token::SPACE);
				i++;
				break;
			}
			case CHAR_EVERYONE:
			case CHAR_HERE:
			{
				AddAndClearToken(tokens, current, Token::TEXT);
				current = ".";
				AddAndClearToken(tokens, current, chr == CHAR_HERE ? Token::HERE : Token::EVERYONE);
				i++;
				break;
			}
			case '<':
			{
				AddAndClearToken(tokens, current, Token::TEXT);

				size_t beginningIdx = i;
				i++;

				if (i == msgSize || (msg[i] != '@' && msg[i] != '#' && msg[i] != 't' && msg[i] != ':')) {
					i = beginningIdx;
					goto _def;
				}

				char type = msg[i];
				size_t foundEnd = std::string::npos;
				i++;
				for (; i < msgSize; i++) {
					if (msg[i] == '>') {
						foundEnd = i;
						break;
					}
					if (msg[i] == ' ' || msg[i] == '\n' || msg[i] == '\r')
						break;
				}

				if (foundEnd == std::string::npos) {
					i = beginningIdx;
					goto _def;
				}

				current = msg.substr(beginningIdx + 1, foundEnd - 1 - beginningIdx);

				int tokType = Token::MENTION;

				switch (type) {
					case 't':
						tokType = Token::TIMESTAMP;
						break;
					case ':':
						tokType = Token::CUSTOM_EMOJI;
						break;
				}

				AddAndClearToken(tokens, current, tokType);
				i = foundEnd + 1;
				break;
			}
			case 'h': // http:// and https://
			{
				std::string http = "http://";
				std::string https = "https://";

				if (i + http.size() > msgSize)
					goto _def;

				bool handleLink = false;
				size_t beginningLink = i;
				if (strncmp(msg.c_str() + i, http.c_str(), http.size()) == 0)
				{
					// handle HTTP here
					handleLink = true;
					i += http.size();
				}
				else if (i + https.size() <= msgSize)
				{
					if (strncmp(msg.c_str() + i, https.c_str(), https.size()) == 0)
					{
						// handle HTTPS
						handleLink = true;
						i += https.size();
					}
				}

				if (!handleLink)
					goto _def; // treat it as a regular word

				AddAndClearToken(tokens, current, Token::TEXT);

				int parenDepth = 0;
				int brackDepth = 0;
				int curlyDepth = 0;

				for (; i < msgSize; i++) {
					/**/ if (msg[i] == '(') parenDepth++;
					else if (msg[i] == '[') brackDepth++;
					else if (msg[i] == '{') curlyDepth++;
					else if (msg[i] == ')') parenDepth--;
					else if (msg[i] == ']') brackDepth--;
					else if (msg[i] == '}') curlyDepth--;

					if (parenDepth < 0 || brackDepth < 0 || curlyDepth < 0 ||
						msg[i] == ' ' || msg[i] == '\n' || msg[i] == '\r' || msg[i] == '>')
						break;
				}

				current = oldmsg.substr(beginningLink, i - beginningLink);
				AddAndClearToken(tokens, current, Token::LINK);

				break;
			}
			default:
			{
			_def:
				current += chr;
				i++;
			}
		}
	}

	AddAndClearToken(tokens, current, Token::TEXT);
	AddAndClearToken(tokens, current, Token::FINISH);
}

void FormattedText::ParseText()
{
	int style = 0;

	int lastTokenType = Token::NEWLINE;
	for (size_t i = 0; i < m_tokens.size(); i++)
	{
		Token& tk = m_tokens[i];
		switch (tk.m_type) {
			case Token::STRONG_BEGIN: style |= WORD_STRONG;   break;
			case Token::ITALIC_BEGIN: style |= WORD_ITALIC;   break;
			case Token::UNDERL_BEGIN: style |= WORD_UNDERL;   break;
			case Token::ITALIE_BEGIN: style |= WORD_ITALIE;   break;
			case Token::STRONG_END:   style &=~WORD_STRONG;   break;
			case Token::ITALIC_END:   style &=~WORD_ITALIC;   break;
			case Token::UNDERL_END:   style &=~WORD_UNDERL;   break;
			case Token::ITALIE_END:   style &=~WORD_ITALIE;   break;
			case Token::LIST_ITEM:    style |= WORD_LISTITEM; break;
			case Token::QUOTE:        style |= WORD_QUOTE;    break;
			case Token::CODE: {
				if (tk.m_text.empty()) {
					AddWord(Word(style, "``````"));
					break;
				}

				if (lastTokenType != Token::NEWLINE)
					AddWord(Word(style | WORD_NEWLINE, ""));

				const char* strv = tk.m_text.c_str();
				size_t len = tk.m_text.size();
				if (*strv) {
					if (strv[0] == '\n') {
						strv++;
						len--;
					}
				}
				if (*strv) {
					if (strv[len - 1] == '\n')
						len--;
				}
				AddWord(Word(style | WORD_MLCODE, strv, len));

				if (i + 1 < m_tokens.size() && m_tokens[i + 1].m_type != Token::NEWLINE)
					AddWord(Word(style | WORD_NEWLINE, ""));
				break;
			}
			case Token::SMALLCODE:
				if (tk.m_text.empty())
					AddWord(Word(style, "``"));
				else
					AddWord(Word(style | WORD_CODE, tk.m_text));
				break;
			case Token::SPACE: {
				AddWord(Word(style | WORD_SPACE, ""));
				break;
			}
			case Token::NEWLINE: {
				style &= ~(WORD_QUOTE | WORD_LISTITEM);
				AddWord(Word(style | WORD_NEWLINE, ""));
				break;
			}
			case Token::EVERYONE: {
				AddWord(Word(style | WORD_EVERYONE, "@everyone"));
				break;
			}
			case Token::HERE: {
				AddWord(Word(style | WORD_EVERYONE, "@here"));
				break;
			}
			default: {
				int specificStyle = 0;
				switch (tk.m_type) {
					case Token::LINK:         specificStyle |= WORD_LINK;      break;
					case Token::MENTION:      specificStyle |= WORD_MENTION;   break;
					case Token::TIMESTAMP:    specificStyle |= WORD_TIMESTAMP; break;
					case Token::CODE:         specificStyle |= WORD_MLCODE;    break;
					case Token::CUSTOM_EMOJI: specificStyle |= WORD_CEMOJI;    break;
				}
				AddWord(Word(style | specificStyle, tk.m_text));
				break;
			}
		}
		lastTokenType = tk.m_type;
	}

	m_tokens.clear();
}

void FormattedText::Layout(DrawingContext* context, const Rect& rect, int offsetX)
{
	m_bFormatted = true;
	m_layoutRect = rect;
	
	Point wordPos = { int(rect.left) + offsetX, int(rect.top) };

	bool containsJustEmoji = true;
	for (auto& word : m_words) {
		// If a word isn't any one of these:
		if ((word.m_flags & ~(WORD_SPACE | WORD_NEWLINE | WORD_CEMOJI | WORD_EMOJI | WORD_HEADER1)) || word.m_flags == 0) {
			containsJustEmoji = false;
			break;
		}
	}

	if (containsJustEmoji) {
		for (auto& word : m_words) {
			if (word.m_flags & (WORD_CEMOJI | WORD_EMOJI))
				word.m_flags |= WORD_HEADER1;
		}
	}

	// first convert to interface string
	for (auto& word : m_words) {
		word.UpdateIfContent();
	}

	int nSpacesBefore = 0;
	bool bHadNewlineBefore = true;
	int maxLineY = MdLineHeight(context, 0);
	for (auto& word : m_words) {
		int& wflags = word.m_flags;

		// If this is a new line:
		if (wflags & WORD_NEWLINE) {
			wordPos.x = rect.left;
			wordPos.y += maxLineY;
			maxLineY = MdLineHeight(context, wflags);
			wflags |= WORD_DONTDRAW;
			bHadNewlineBefore = true;
			nSpacesBefore = 0;
			continue;
		}

		// If this is a space:
		if (wflags & WORD_SPACE) {
			word.m_flags |= WORD_DONTDRAW;
			nSpacesBefore++;
			continue;
		}

		if (bHadNewlineBefore) {
			wflags |= WORD_AFNEWLINE;
		}

		int maxWidth = 0;
		if (wflags & (WORD_MLCODE | WORD_CODE)) {
			maxWidth = rect.Width();
		}

		int spaceWidth = MdSpaceWidth(context, wflags) * nSpacesBefore;
		nSpacesBefore = 0;
		Point size = word.m_size;
		
		if (word.ShouldRelayout(rect))
		{
			bool wrapped = false;
			size = word.m_size = MdMeasureString(context, word.m_ifContent, wflags, wrapped, maxWidth);
			if (wrapped)
				word.m_flags |= WORD_WRAPPED;
			else
				word.m_flags &= ~WORD_WRAPPED;
		}

		if (maxLineY < size.y)
			maxLineY = size.y;

		if (wordPos.x + size.x + spaceWidth >= rect.right && !bHadNewlineBefore) {
			wordPos.x = rect.left;
			wordPos.y += maxLineY;
			maxLineY = MdLineHeight(context, wflags);
			wflags |= WORD_AFNEWLINE;
			nSpacesBefore = 0;
		}
		else {
			wordPos.x += spaceWidth;
		}

		if ((wflags & (WORD_AFNEWLINE | WORD_QUOTE)) == (WORD_AFNEWLINE | WORD_QUOTE))
			wordPos.x += MdGetQuoteIndentSize();

		int offs = 0;

		word.m_rect = { wordPos.x + offs, wordPos.y + offs, wordPos.x + offs + size.x, wordPos.y + offs + size.y };
		wordPos.x += size.x;
		wflags &= ~WORD_DONTDRAW;
		bHadNewlineBefore = false;
	}
}

Rect FormattedText::GetExtent()
{
	assert(m_bFormatted && "Have to call Layout(rc) first!");

	Rect ext;
	ext.left = ext.right = m_layoutRect.left;
	ext.top = ext.bottom = m_layoutRect.top;
	bool set = false;

	for (size_t i = 0; i < m_words.size(); i++) {
		Word& w = m_words[i];
		if (w.m_flags & WORD_DONTDRAW)
			continue;

		if (!set) {
			set = true;
			ext = w.m_rect;
			continue;
		}

		if (ext.left   > w.m_rect.left)
			ext.left   = w.m_rect.left;
		if (ext.top    > w.m_rect.top)
			ext.top    = w.m_rect.top;
		if (ext.right  < w.m_rect.right)
			ext.right  = w.m_rect.right;
		if (ext.bottom < w.m_rect.bottom)
			ext.bottom = w.m_rect.bottom;
	}

	return ext;
}

void FormattedText::Draw(DrawingContext* context, int offsetY)
{
	assert(m_bFormatted && "Have to call Layout(rc) first!");

	// draw backgrounds for code blocks
	for (auto& w : m_words)
	{
		if (w.m_flags & WORD_MLCODE) {
			Rect rc = w.m_rect;
			rc.top    += offsetY;
			rc.bottom += offsetY;
			MdDrawCodeBackground(context, rc);
		}
	}

	for (auto& w : m_words)
	{
		if (w.m_flags & WORD_DONTDRAW)
			continue;
		
		Rect rc = w.m_rect;
		rc.top    += offsetY;
		rc.bottom += offsetY;
		MdDrawString(context, rc, w.m_ifContent, w.m_flags);
	}
}

std::vector<std::pair<std::string, std::string> >
FormattedText::SplitBackticks(const std::string& str)
{
	std::string emptystr = "";
	std::vector<std::pair<std::string, std::string> > out;
	size_t loc = 0, oldloc = 0;
	while ((loc = str.find('`', oldloc)) < str.size())
	{
		out.push_back(std::make_pair(str.substr(oldloc, loc - oldloc), emptystr));
		oldloc = loc + 1;
	}

	out.push_back(std::make_pair(str.substr(oldloc), emptystr));
	return out;
}

void FormattedText::SplitBlocks()
{
	size_t num = 0;
	size_t loc = 0, oldloc = 0;
	std::string emptystr = "";
	while ((loc = m_rawMessage.find("```", oldloc)) < m_rawMessage.size())
	{
		std::string str = m_rawMessage.substr(oldloc, loc - oldloc);

		if (num & 1)
			// just the string please
			m_blocks.push_back({ std::make_pair(str, emptystr) });
		else
			// additional processing
			m_blocks.push_back(SplitBackticks(str));

		num++;
		oldloc = loc + 3;
	}

	// the last one
	std::string str = m_rawMessage.substr(oldloc);

	if (num & 1)
		// just the string please
		m_blocks.push_back({ std::make_pair(str, emptystr) });
	else
		// additional processing
		m_blocks.push_back(SplitBackticks(str));
}

void FormattedText::UseRegex(std::string& str)
{
	// Use regex to replace markdown tags with a custom format that can be parsed easier
	// N.B. strong goes first because consumes more chars.

	const int HAS_STRONG = (1 << 0);
	const int HAS_EMPHAS = (1 << 1);
	const int HAS_QUOTE  = (1 << 2);
	const int HAS_AT     = (1 << 3);
	const int HAS_BSLASH = (1 << 4);
	int flags = 0;

	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] == '*') flags |= HAS_STRONG;
		if (str[i] == '_') flags |= HAS_EMPHAS;
		if (str[i] == '>') flags |= HAS_QUOTE;
		if (str[i] == '@') flags |= HAS_AT;
		if (str[i] == '\\') flags |= HAS_BSLASH;

		if (flags == (HAS_STRONG | HAS_EMPHAS | HAS_QUOTE | HAS_AT | HAS_BSLASH))
			break;
	}

	// Not too expensive I hope!
	if (flags & HAS_BSLASH)
	{
		for (size_t i = 0; i < str.size() - 1; i++) {
			if (str[i] != '\\')
				continue;
			
			char& thisChar = str[i];
			char& nextChar = str[i + 1];

			thisChar = CHAR_NOOP;
			switch (nextChar) {
				case '_': nextChar = CHAR_ESCAPE_UNDERSCORE; break;
				case '*': nextChar = CHAR_ESCAPE_ASTERISK;   break;
				case'\\': nextChar = CHAR_ESCAPE_BACKSLASH;  break;
				case '#': nextChar = CHAR_ESCAPE_POUND;      break;
				case '-': nextChar = CHAR_ESCAPE_MINUS;      break;
				case '+': nextChar = CHAR_ESCAPE_PLUS;       break;
				case '`': nextChar = CHAR_ESCAPE_BACKTICK;   break;  // NOTE: Processing backtick escapes earlier to disable escapes within code blocks
				case '!': case '|': case '.': break;
				default: thisChar = '\\'; break; // just replace it back with backslash
			}
		}
	}
	if (flags & HAS_QUOTE)
	{
		for (size_t i = 0; i < str.size() - 2; i++) {

			if ((i == 0 || str[i - 1] == '\n') && str[i] == '>' && str[i + 1] == ' ')
				str[i] = CHAR_BEG_QUOTE, str[i + 1] = CHAR_NOOP;
		}
	}
	if (flags & HAS_AT)
	{
		for (size_t i = 0; i < str.size() - 2; i++) {
			if (str[i] != '@')
				continue;

			int setEveryone = 0;
			if (strncmp(str.c_str() + i, "@everyone", 9) == 0)
				str[i] = CHAR_EVERYONE, setEveryone = 8;
			else if (strncmp(str.c_str() + i, "@here", 5) == 0)
				str[i] = CHAR_HERE, setEveryone = 4;

			for (size_t j = 1; j <= setEveryone; j++)
				str[i + j] = CHAR_NOOP;
		}
	}

	// Expensive!  But I couldn't really figure out another way.
	if (flags & HAS_STRONG)
	{
		RegexReplace(str, g_StrongMatch, 2, 2, CHAR_BEG_STRONG, CHAR_END_STRONG);
		RegexReplace(str, g_ItalicMatch, 1, 1, CHAR_BEG_ITALIC, CHAR_END_ITALIC);
	}
	if (flags & HAS_EMPHAS)
	{
		RegexReplace(str, g_UnderlMatch, 2, 2, CHAR_BEG_UNDERL, CHAR_END_UNDERL);
		RegexReplace(str, g_ItalieMatch, 1, 1, CHAR_BEG_ITALIE, CHAR_END_ITALIE);
	}
}

void FormattedText::RegexNecessary()
{
	// regex only the even ones, because the odd ones are code blocks
	for (size_t i = 0; i < m_blocks.size(); i += 2)
	{
		auto& vec = m_blocks[i];
		for (size_t j = 0; j < vec.size(); j += 2) {
			// same here actually (they are single code blocks)
			vec[j].second = vec[j].first;
			UseRegex(vec[j].first);
		}
	}
}

std::string FormattedText::EscapeChars(const std::string& str)
{
	std::string outstr;
	outstr.reserve(str.size());

	for (size_t i = 0; i < str.size(); i++)
	{
		switch (str[i]) {
			case CHAR_NOOP: break;
			case CHAR_ESCAPE_UNDERSCORE: outstr += '_'; break;
			case CHAR_ESCAPE_ASTERISK:   outstr += '*'; break;
			case CHAR_ESCAPE_BACKSLASH:  outstr +='\\'; break;
			case CHAR_ESCAPE_BACKTICK:   outstr += '`'; break;
			case CHAR_ESCAPE_POUND:      outstr += '#'; break;
			case CHAR_ESCAPE_MINUS:      outstr += '-'; break;
			case CHAR_ESCAPE_PLUS:       outstr += '+'; break;
			default: outstr += str[i]; break;
		}
	}

	return outstr;
}

void FormattedText::TokenizeAll()
{
	for (size_t i = 0; i < m_blocks.size(); i++)
	{
		if (i & 1)
		{
			assert(m_blocks[i].size() == 1);
			m_tokens.push_back(Token(Token::CODE, EscapeChars(m_blocks[i][0].first)));
		}
		else
		{
			// even, so tokenize it all the way.
			auto& vec = m_blocks[i];
			for (size_t j = 0; j < vec.size(); j++) {
				if (j & 1)
					m_tokens.push_back(Token(Token::SMALLCODE, EscapeChars(m_blocks[i][j].first)));
				else
					Tokenize(m_blocks[i][j].first, m_blocks[i][j].second);
			}
		}
	}
}


void FormattedText::SetMessage(const std::string& msg)
{
	m_rawMessage = msg;
	m_bFormatted = false;
	m_blocks.clear();
	m_tokens.clear();
	m_words.clear();

	SplitBlocks();
	RegexNecessary();
	TokenizeAll();
	ParseText();

	m_blocks.clear();
	m_tokens.clear();
}
