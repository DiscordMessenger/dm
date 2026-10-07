#pragma once

#include <functional>
#include <string>
#include "Xm.hpp"

// Asks for the account token.  done(token) runs when the user logs in;
// done("") when they cancel.  message, when not empty, says why it asks
// (a token that was refused, for example).
void ShowLogonDialog(Widget parent, const std::string& message, std::function<void(const std::string&)> done);
