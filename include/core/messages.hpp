#ifndef _messages__hpp__included__
#define _messages__hpp__included__

#include <iostream>

/**
 * messages -> window::out().
 */
class messages_relay_class
{
public:
	operator std::ostream&() { return getstream(); }
	static std::ostream& getstream();
};
template<typename T> inline std::ostream& operator<<(messages_relay_class& x, T value)
{
	return messages_relay_class::getstream() << value;
};
inline std::ostream& operator<<(messages_relay_class& x, std::ostream& (*fn)(std::ostream& o))
{
	return fn(messages_relay_class::getstream());
};
extern messages_relay_class messages;

#endif
