#include "video/tcp.hpp"

#ifdef NO_TCP_SOCKETS

namespace
{
	void deleter_fn(void* f)
	{
	}
}

socket_address::socket_address(const std::string& spec)
{
	throw std::runtime_error("TCP/IP support not compiled in");
}

socket_address socket_address::next()
{
}

std::ostream& socket_address::connect()
{
	throw std::runtime_error("TCP/IP support not compiled in");
}

bool socket_address::supported()
{
	return false;
}

#else
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64)
//Why the fuck does windows have nonstandard socket API???
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
struct sockaddr_un { int sun_family; char sun_path[108]; };
#else
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#endif
#include <sys/types.h>

namespace
{
	class socket_output
	{
	public:
		typedef char char_type;
		typedef struct : public boost::iostreams::sink_tag, boost::iostreams::closable_tag {} category;
		socket_output(int _fd)
			: fd(_fd)
		{
			broken = false;
		}

		void close()
		{
			::close(fd);
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			if(broken)
				return n;
			size_t w = n;
			while(n > 0) {
				ssize_t r = ::send(fd, s, n, 0);
				if(r >= 0) {
					s += r;
					n -= r;
				} else if(errno == EPIPE) {
					std::cerr << "The other end of socket went away" << std::endl;
					broken = true;
					n = 0;
					break;
				} else {	//Error.
					int err = errno;
					std::cerr << "Socket write error: " << strerror(err) << std::endl;
					n = 0;
					break;
				}
			}
			return w;
		}
		protected:
			int fd;
			bool broken;
	};

	void deleter_fn(void* f)
	{
		delete reinterpret_cast<boost::iostreams::stream<socket_output>*>(f);
	}
}

socket_address::socket_address(const std::string& name)
{
	struct addrinfo hints;
	struct addrinfo* ainfo;
	int r;
	std::string node, service, tmp = name;
	size_t s;
	struct sockaddr_un uaddr;
	if(name[0] == '/' || name[0] == '@') {
		//Fake a unix-domain.
		if(name.length() >= sizeof(sockaddr_un) - offsetof(sockaddr_un, sun_path) - 1)
			throw std::runtime_error("Path too long for filesystem socket");
		size_t namelen = offsetof(struct sockaddr_un, sun_path) + name.length();
		uaddr.sun_family = AF_UNIX;
		strcpy(uaddr.sun_path, name.c_str());
		if(name[0] == '@')
			uaddr.sun_path[0] = 0;	//Mark as abstract namespace socket.
		family = AF_UNIX;
		socktype = SOCK_STREAM;
		protocol = 0;
		memory.resize((name[0] == '@') ? namelen : sizeof(sockaddr_un));
		memcpy(&memory[0], &uaddr, memory.size());
		return;
	}
	//Split into address and port.
	s = tmp.find_last_of(":");
	if(s >= tmp.length())
		throw std::runtime_error("Port number has to be specified");
	node = tmp.substr(0, s);
	service = tmp.substr(s + 1);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
#ifdef AI_V4MAPPED
	hints.ai_flags = AI_V4MAPPED;
#endif
#ifdef AI_ADDRCONFIG
	hints.ai_flags = AI_ADDRCONFIG;
#endif
	r = getaddrinfo(node.c_str(), service.c_str(), &hints, &ainfo);
	if(r < 0)
		throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(r));
	family = ainfo->ai_family;
	socktype = ainfo->ai_socktype;
	protocol = ainfo->ai_protocol;
	try {
		memory.resize(ainfo->ai_addrlen);
		memcpy(&memory[0], ainfo->ai_addr, ainfo->ai_addrlen);
	} catch(...) {
		freeaddrinfo(ainfo);
		throw;
	}
	freeaddrinfo(ainfo);
}

socket_address socket_address::next()
{
	std::vector<char> newaddr = memory;
	struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&newaddr[0]);
	socklen_t addrlen = memory.size();
	switch(addr->sa_family) {
	case AF_INET: {		//IPv4
		struct sockaddr_in* _addr = (struct sockaddr_in*)addr;
		_addr->sin_port = htons(htons(_addr->sin_port) + 1);
		break;
	}
	case AF_INET6: {	//IPv6
		struct sockaddr_in6* _addr = (struct sockaddr_in6*)addr;
		_addr->sin6_port = htons(htons(_addr->sin6_port) + 1);
		break;
	}
	case AF_UNIX: {		//Unix domain sockets.
		struct sockaddr_un* _addr = (struct sockaddr_un*)addr;
		const char* b1 = (char*)_addr;
		const char* b2 = (char*)&_addr->sun_path;
		size_t maxpath = addrlen - (b2 - b1);
		for(size_t i = 0; i < maxpath; i++)
			if(i && !_addr->sun_path[i]) {
				maxpath = i;
				break;
			}
		if(!maxpath)
			throw std::runtime_error("Eh, empty unix domain socket path?");
		_addr->sun_path[maxpath - 1]++;
		break;
	}
	default:
		throw std::runtime_error("This address family is not supported, sorry.");
	}
	socket_address n(family, socktype, protocol);
	n.memory = newaddr;
	return n;
}

std::ostream& socket_address::connect()
{
	int a = socket(family, socktype, protocol);
	if(a < 0) {
		int err = errno;
		throw std::runtime_error(std::string("socket: ") + strerror(err));
	}
	int r;
	struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&memory[0]);
	socklen_t addrlen = memory.size();
#if defined(_WIN32) || defined(_WIN64)
	r = ::connect(a, addr, addrlen) ? -1 : 0;
#else
	r = ::connect(a, addr, addrlen);
#endif
	if(r < 0) {
		int err = errno;
		::close(a);
		throw std::runtime_error(std::string("connect: ") + strerror(err));
	}
	try {
		return *new boost::iostreams::stream<socket_output>(a);
	} catch(...) {
		::close(a);
		throw;
	}
}

bool socket_address::supported()
{
	return true;
}

#endif

deleter_fn_t socket_address::deleter()
{
	return deleter_fn;
}


socket_address::socket_address(int f, int st, int p)
{
	family = f;
	socktype = st;
	protocol = p;
}
