#include "MainQueue.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace
{
	struct Item
	{
		std::function<void()> fn;
		bool* done; // set (under the lock) once fn ran, for Send
	};

	std::mutex g_lock;
	std::condition_variable g_doneCond;
	std::deque<Item> g_items;
	std::thread::id g_mainThread;
	int g_pipe[2] = { -1, -1 };
	bool g_shutdown = false;

	void Wake()
	{
		char c = 0;
		while (write(g_pipe[1], &c, 1) < 0 && errno == EINTR)
			;
	}
}

void MainQueue::Init()
{
	g_mainThread = std::this_thread::get_id();
	if (pipe(g_pipe) == 0) {
		fcntl(g_pipe[0], F_SETFL, fcntl(g_pipe[0], F_GETFL) | O_NONBLOCK);
		fcntl(g_pipe[1], F_SETFL, fcntl(g_pipe[1], F_GETFL) | O_NONBLOCK);
		fcntl(g_pipe[0], F_SETFD, FD_CLOEXEC);
		fcntl(g_pipe[1], F_SETFD, FD_CLOEXEC);
	}
}

int MainQueue::WakeFd()
{
	return g_pipe[0];
}

bool MainQueue::OnMainThread()
{
	return std::this_thread::get_id() == g_mainThread;
}

void MainQueue::Post(std::function<void()> fn)
{
	{
		std::lock_guard<std::mutex> lk(g_lock);
		if (g_shutdown)
			return;
		g_items.push_back(Item{ std::move(fn), nullptr });
	}
	Wake();
}

void MainQueue::Send(std::function<void()> fn)
{
	if (OnMainThread()) {
		fn();
		return;
	}

	bool done = false;
	std::unique_lock<std::mutex> lk(g_lock);
	if (g_shutdown)
		return;
	g_items.push_back(Item{ std::move(fn), &done });
	Wake();
	g_doneCond.wait(lk, [&] { return done || g_shutdown; });
}

void MainQueue::Drain()
{
	char buf[64];
	while (read(g_pipe[0], buf, sizeof buf) > 0)
		;

	for (;;)
	{
		Item item;
		{
			std::lock_guard<std::mutex> lk(g_lock);
			if (g_items.empty())
				return;
			item = std::move(g_items.front());
			g_items.pop_front();
		}

		item.fn();

		if (item.done) {
			std::lock_guard<std::mutex> lk(g_lock);
			*item.done = true;
			g_doneCond.notify_all();
		}
	}
}

void MainQueue::Shutdown()
{
	std::lock_guard<std::mutex> lk(g_lock);
	g_shutdown = true;
	g_items.clear();
	g_doneCond.notify_all();
}
