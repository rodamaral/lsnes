#include "core/messages.hpp"
#include "core/window.hpp"

std::ostream& messages_relay_class::getstream() { return platform::out(); }
messages_relay_class messages;
