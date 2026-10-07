#pragma once

#include <functional>

// Work handed to the UI thread: what SendMessage and PostMessage to the main
// window do in the Win32 frontend.  The UI thread watches WakeFd() (with
// XtAppAddInput, select, ...) and calls Drain() when it becomes readable.
namespace MainQueue
{
	// Call once, on the UI thread.
	void Init();

	// Readable while work is queued.
	int WakeFd();

	// Runs fn later on the UI thread.
	void Post(std::function<void()> fn);

	// Runs fn on the UI thread and waits for it.  On the UI thread itself
	// fn runs at once.
	void Send(std::function<void()> fn);

	// Runs everything queued so far.  UI thread only.
	void Drain();

	// After this, Send and Post drop their work, so threads still running
	// while the program exits never wait for a UI thread that is gone.
	void Shutdown();

	bool OnMainThread();
}
