#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "library/serialization.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <fstream>
#include <zlib.h>
#ifndef NO_TCP_SOCKETS
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
#endif

#define IS_RGB(m) (((m) + ((m) >> 3)) & 2)
#define IS_64(m) (m % 5 < 2)
#define IS_TCP(m) (((m % 5) * (m % 5)) % 5 == 1)



namespace
{
#ifndef NO_TCP_SOCKETS
	//Increment port number by 1.
	void mung_sockaddr(struct sockaddr* addr, socklen_t addrlen)
	{
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
	}

	int compat_connect(int fd, struct sockaddr* addr, socklen_t addrlen)
	{
#if defined(_WIN32) || defined(_WIN64)
		return connect(fd, addr, addrlen) ? -1 : 0;
#else
		return connect(fd, addr, addrlen);
#endif
	}

	std::pair<int, int> establish_connections(struct addrinfo* i)
	{
		struct sockaddr* addr = i->ai_addr;
		socklen_t addrlen = i->ai_addrlen;
		int a = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if(a < 0) {
			int err = errno;
			throw std::runtime_error(std::string("socket: ") + strerror(err));
		}
		int b = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if(b < 0) {
			int err = errno;
			close(a);
			throw std::runtime_error(std::string("socket: ") + strerror(err));
		}
		if(compat_connect(a, addr, addrlen) < 0) {
			int err = errno;
			close(a);
			close(b);
			throw std::runtime_error(std::string("connect (video): ") + strerror(err));
		}
		mung_sockaddr(addr, addrlen);
		if(compat_connect(b, addr, addrlen) < 0) {
			int err = errno;
			close(a);
			close(b);
			throw std::runtime_error(std::string("connect (audio): ") + strerror(err));
		}
		std::cerr << "Routing video to socket " << a << std::endl;
		std::cerr << "Routing audio to socket " << b << std::endl;
		
		return std::make_pair(a, b);
	}

	std::pair<int, int> get_sockets(const std::string& name)
	{
		struct addrinfo hints;
		struct addrinfo* ainfo;
		bool real = false;
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
			ainfo = &hints;
			ainfo->ai_flags = 0;
			ainfo->ai_family = AF_UNIX;
			ainfo->ai_socktype = SOCK_STREAM;
			ainfo->ai_protocol = 0;
			ainfo->ai_addrlen = (name[0] == '@') ? namelen : sizeof(sockaddr_un),
			ainfo->ai_addr = reinterpret_cast<sockaddr*>(&uaddr);
			ainfo->ai_canonname = NULL;
			ainfo->ai_next = NULL;
			goto establish;
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
		real = true;
		r = getaddrinfo(node.c_str(), service.c_str(), &hints, &ainfo);
		if(r < 0)
			throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(r));
establish:
		auto x = establish_connections(ainfo);
		if(real)
			freeaddrinfo(ainfo);
		return x;
	}

	class socket_output
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;
		socket_output(int _fd)
			: fd(_fd)
		{
		}

		void close()
		{
			::close(fd);
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			size_t w = n;
			while(n > 0) {
				ssize_t r = ::send(fd, s, n, 0);
				if(r >= 0) {
					s += r;
					n -= r;
				} else {	//Error.
					int err = errno;
					messages << "Socket write error: " << strerror(err) << std::endl;
					break;
				}
			}
			return w;
		}
	protected:
		int fd;
	};
	bool tcp_dump_supported = true;
#else
	std::pair<int, int> get_sockets(const std::string& name)
	{
		throw std::runtime_error("Dumping over TCP/IP not supported");
	}

	class socket_output
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;
		socket_output(int _fd) {}
		void close() {}
		std::streamsize write(const char* s, std::streamsize n) { return n; }
	};
	bool tcp_dump_supported = false;
#endif

	
	unsigned strhash(const std::string& str)
	{
		unsigned h = 0;
		for(size_t i = 0; i < str.length(); i++)
			h = (2 * h + static_cast<unsigned char>(str[i])) % 11;
		return h;
	}

	class raw_avsnoop : public information_dispatch
	{
	public:
		raw_avsnoop(const std::string& prefix, bool _swap, bool _bits64, bool socket_mode)
			: information_dispatch("dump-raw")
		{
			enable_send_sound();
			if(socket_mode) {
				std::pair<int, int> socks = get_sockets(prefix);
				video = new boost::iostreams::stream<socket_output>(socks.first);
				audio = new boost::iostreams::stream<socket_output>(socks.second);
			} else {
				video = new std::ofstream(prefix + ".video", std::ios::out | std::ios::binary);
				audio = new std::ofstream(prefix + ".audio", std::ios::out | std::ios::binary);
			}
			if(!*video || !*audio)
				throw std::runtime_error("Can't open output files");
			have_dumped_frame = false;
			swap = _swap;
			bits64 = _bits64;
		}

		~raw_avsnoop() throw()
		{
			delete video;
			delete audio;
		}

		void on_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			if(!video)
				return;
			unsigned magic;
			if(bits64)
				magic = 0x30201000U;
			else
				magic = 0x18100800U;
			unsigned r = (reinterpret_cast<unsigned char*>(&magic))[swap ? 2 : 0];
			unsigned g = (reinterpret_cast<unsigned char*>(&magic))[1];
			unsigned b = (reinterpret_cast<unsigned char*>(&magic))[swap ? 0 : 2];
			uint32_t hscl = (_frame.width < 400) ? 2 : 1;
			uint32_t vscl = (_frame.height < 400) ? 2 : 1;
			if(bits64) {
				render_video_hud(dscr2, _frame, hscl, vscl, r, g, b, 0, 0, 0, 0, NULL);
				for(size_t i = 0; i < dscr2.height; i++)
					video->write(reinterpret_cast<char*>(dscr2.rowptr(i)), 8 * dscr2.width);
			} else {
				render_video_hud(dscr, _frame, hscl, vscl, r, g, b, 0, 0, 0, 0, NULL);
				for(size_t i = 0; i < dscr.height; i++)
					video->write(reinterpret_cast<char*>(dscr.rowptr(i)), 4 * dscr.width);
			}
			if(!*video)
				messages << "Video write error" << std::endl;
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(have_dumped_frame && audio) {
				char buffer[4];
				write16sbe(buffer + 0, l);
				write16sbe(buffer + 2, r);
				audio->write(buffer, 4);
			}
		}

		void on_dump_end()
		{
			delete video;
			delete audio;
			video = NULL;
			audio = NULL;
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		std::ostream* audio;
		std::ostream* video;
		bool have_dumped_frame;
		struct screen<false> dscr;
		struct screen<true> dscr2;
		bool swap;
		bool bits64;
	};

	raw_avsnoop* vid_dumper;

	class adv_raw_dumper : public adv_dumper
	{
	public:
		adv_raw_dumper() : adv_dumper("INTERNAL-RAW") {information_dispatch::do_dumper_update(); }
		~adv_raw_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			for(size_t i = 0; i < (tcp_dump_supported ? 2 : 1); i++)
				for(size_t j = 0; j < 2; j++)
					for(size_t k = 0; k < 2; k++)
						x.insert(std::string("") + (i ? "tcp" : "") + (j ? "bgr" : "rgb")
							+ (k ? "64" : "32"));
			return x;
		}

		bool wants_prefix(const std::string& mode) throw()
		{
			return true;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "RAW";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			unsigned _mode = strhash(mode);
			std::string x = std::string((IS_RGB(_mode) ? "RGB" : "BGR")) +
				(IS_64(_mode) ? " 64-bit" : " 32-bit") + (IS_TCP(_mode) ? " over TCP/IP" : "");
			return x;
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& prefix) throw(std::bad_alloc,
			std::runtime_error)
		{
			unsigned _mode = strhash(mode);
			bool bits64 = IS_64(_mode);
			bool swap = !IS_RGB(_mode);
			bool sock = IS_TCP(_mode);

			if(prefix == "")
				throw std::runtime_error("Expected prefix");
			if(vid_dumper)
				throw std::runtime_error("RAW dumping already in progress");
			try {
				vid_dumper = new raw_avsnoop(prefix, swap, bits64, sock);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting RAW dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << std::endl;
			information_dispatch::do_dumper_update();
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No RAW video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "RAW Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending RAW dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;
	
	adv_raw_dumper::~adv_raw_dumper() throw()
	{
	}
}
