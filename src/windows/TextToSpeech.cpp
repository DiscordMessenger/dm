#include "TextToSpeech.hpp"

#ifndef USE_SPEECH

namespace TextToSpeech
{
	void Initialize() {}
	void Deinitialize() {}
	void Speak(const std::string& str) {}
	void StopAll() {}
};

#else

#include <sapi.h>

static ISpVoice* g_pVoice;

void TextToSpeech::Initialize()
{
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&g_pVoice);

	if (!SUCCEEDED(hr))
	{
		std::string xstr = "Failed to initialize SAPI:\n\n(" + std::to_string((long)hr) + ") " + GetStringFromHResult(hr);
		LPCTSTR tstr1 = ConvertCppStringToTString(xstr);
		MessageBox(g_Hwnd, tstr1, TEXT("Discord Messenger - Could Not Initialize SAPI"), MB_OK | MB_ICONERROR);
		free((void*)tstr1);
		// TODO: Let the user know that there is no text to speech.
		return;
	}
}

void TextToSpeech::Deinitialize()
{
	if (g_pVoice)
	{
		g_pVoice->Release();
		g_pVoice = NULL;
	}

	CoUninitialize();
}

void TextToSpeech::Speak(const std::string& str)
{
	if (!g_pVoice) return;

	LPCTSTR tstr = ConvertCppStringToTString(str);
	HRESULT hr = g_pVoice->Speak(tstr, SPF_ASYNC, NULL);
	if (FAILED(hr))
	{
		std::string xstr = "Failed to speak message \"" + str + "\":\n\n(" + std::to_string((long)hr) + ") " + GetStringFromHResult(hr);
		LPCTSTR tstr1 = ConvertCppStringToTString(xstr);
		MessageBox(g_Hwnd, tstr1, TEXT("Discord Messenger"), MB_OK | MB_ICONERROR);
		free((void*)tstr1);
	}
	free((void*)tstr);
}

void TextToSpeech::StopAll()
{
	// stop ALL speech
	ULONG skipped = 0;
	g_pVoice->Skip(L"SENTENCE", 999999999, &skipped);
}

#endif



