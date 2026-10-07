#pragma once

#include <functional>
#include <string>

#include "Xm.hpp"
#include "Canvas.hpp"

// Logging in by QR code, as discord.com's login page offers: Discord's
// remote-auth gateway gives a code that the phone app scans; once the
// login is confirmed on the phone, the account's token arrives encrypted
// to a key made for this login only.
namespace QrLogin
{
	// Shows the dialog.  done(token) when the phone confirmed; done("")
	// when the user quits.  useToken() when they would rather paste a token.
	void Show(Widget parent, const PixelFormat& fmt, const std::string& message,
		std::function<void(const std::string&)> done, std::function<void()> useToken);

	// The remote-auth gateway's connection, -1 when none: the frontend hands
	// its traffic here (any thread).
	int GatewayId();
	void OnGatewayMessage(const std::string& payload); // UI thread
	void OnGatewayClosed(int code, const std::string& reason); // UI thread
}
