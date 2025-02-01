// Copyright (C) 2025 iProgramInCpp

#ifndef terminateApplication

#ifdef USE_IPROGRAMS_TERMINATE

// Use my home grown implementation.
extern "C" void Terminate(const char*, ...);

#ifdef _MSC_VER
#define terminateApplication(blame) Terminate("Error in " blame " at %s:%d, function %s", __FILE__, __LINE__, __func__)
#else
#define terminateApplication(blame) Terminate("Error in " blame " at %s:%d, function %s", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

#else

#define terminateApplication(blame) std::terminate()

#endif

#define terminateAsio() terminateApplication("asio")
#define terminateWebsocketpp() terminateApplication("websocketpp")

#endif
