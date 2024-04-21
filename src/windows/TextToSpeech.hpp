#pragma once

#include "Main.hpp"

namespace TextToSpeech
{
	void Initialize();
	void Deinitialize();
	void Speak(const std::string& str);
	void StopAll();
};

