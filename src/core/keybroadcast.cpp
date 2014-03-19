#include "core/window.hpp"
#include "core/keybroadcast.hpp"
#include "core/keymapper.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include <sstream>
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

namespace
{
#if defined(_WIN32) || defined(_WIN64)
	typedef SOCKET sock_handle_t;
	sock_handle_t invalid_socket = INVALID_SOCKET;
#else
	typedef int sock_handle_t;
	sock_handle_t invalid_socket = -1;
#endif

	class k_link
	{
	public:
		k_link(sock_handle_t handle)
		{
			link_handle = handle;
			dead_flag = true;
		}
		~k_link()
		{
			low_close();
		}
		//Returns true if link has something to transmit, else false.
		bool can_tx()
		{
			umutex_class h(lock);
			return !msg_tx_queue.empty();
		}
		//Write some data to link. If can_tx() returned true, this will try to transmit something.
		void handle_tx()
		{
			umutex_class h(lock);
			if(dead_flag)
				return;
			if(msg_tx_queue.empty())
				return;
			const std::string& txmsg = msg_tx_queue.front();
			char buf[512];
			size_t totransmit = txmsg.length() - first_msg_tx_count;
			if(totransmit > sizeof(buf))
				totransmit = sizeof(buf);
			size_t tocopy = totransmit;
			if(totransmit < sizeof(buf))
				buf[totransmit++] = '\0';
			if(tocopy > 0)
				std::copy(txmsg.begin() + first_msg_tx_count, txmsg.begin() + first_msg_tx_count +
					tocopy, buf);
			//Actually try to transmit. If entiere message is transmitted, remove it.
			first_msg_tx_count += low_write(buf, totransmit);
			if(first_msg_tx_count >= txmsg.length() + 1)
				msg_tx_queue.pop_front();
		}
		//Read some data from link.
		void handle_rx()
		{
			umutex_class h(lock);
			if(dead_flag)
				return;
			char buf[512];
			size_t recvd = low_read(buf, sizeof(buf));
			size_t off = 0;
			for(unsigned i = 0; i < recvd; i++) {
				if(buf[i] == '\0') {
					//End of message.
					size_t off2 = partial_rx_msg.size();
					partial_rx_msg.resize(off2 + i - off);
					std::copy(buf + off, buf + i, partial_rx_msg.begin() + off2);
					msg_rx_queue.push_back(std::string(partial_rx_msg.begin(),
						partial_rx_msg.end()));
					off = i + 1;
				}
			}
			//Copy incomplete parts.
			size_t off2 = partial_rx_msg.size();
			partial_rx_msg.resize(off2 + recvd - off);
			std::copy(buf + off, buf + recvd, partial_rx_msg.begin() + off2);
		}
		//Returns true if there is a pending message.
		bool recv_ready()
		{
			umutex_class h(lock);
			return !msg_rx_queue.empty();
		}
		//Read a pending message.
		std::string recv()
		{
			umutex_class h(lock);
			if(msg_rx_queue.empty())
				return "";
			std::string msg = msg_rx_queue.front();
			msg_rx_queue.pop_front();
			return msg;
		}
		//Send a message to link.
		void send(const std::string& str)
		{
			umutex_class h(lock);
			msg_tx_queue.push_back(str);
		}
		//Is dead?
		bool dead()
		{
			return dead_flag;
		}
		//Get link handle.
		sock_handle_t handle()
		{
			return link_handle;
		}
	private:
		size_t low_read(char* buf, size_t maxread);
		size_t low_write(const char* buf, size_t maxwrite);
		void low_close();
		k_link(const k_link&);
		k_link& operator=(const k_link&);
		sock_handle_t link_handle;
		std::list<std::string> msg_rx_queue;
		std::list<std::string> msg_tx_queue;
		size_t first_msg_tx_count;
		std::vector<char> partial_rx_msg;
		bool dead_flag;
		mutex_class lock;
	};

	k_link* client_link;
	mutex_class client_link_lock;
	cv_class client_link_change;
	bool client_link_exit;
	bool client_link_exited;

	void slave_thread()
	{
		client_link_exited = false;
		while(!client_link_exit) {
			{
				umutex_class h(client_link_lock);
				if(!client_link);
			}
		}
		client_link_exited = true;
	}

	class k_server
	{
	public:
		//Fill the sets with available socket handles.
		void poll_readyness(std::set<sock_handle_t>& rx, std::set<sock_handle_t>& tx)
		{
			for(auto i : links) {
				//Always ready for RX.
				rx.insert(i->handle());
				if(i->can_tx()) tx.insert(i->handle());
			}
		}
		//Given sockets with activity, do RX/TX cycle.
		void do_rx_tx(std::set<sock_handle_t>& rx, std::set<sock_handle_t>& tx)
		{
			//Read/write all available sockets.
			for(auto i : links) {
				if(rx.count(i->handle()))
					i->handle_rx();
				if(tx.count(i->handle()))
					i->handle_tx();
			}
			//Check for dead connections and close those.
			for(auto i = links.begin(); i != links.end();) {
				if((*i)->dead()) {
					delete *i;
					i = links.erase(i);
				} else
					i++;
			}
			redistribute_messages();
		}
		//Receive from all connections, broadcast the message.
		void redistribute_messages()
		{
			//Receive on all connections, send on all other connections.
			for(auto i : links) {
				//Loop over all received messages.
				while(i->recv_ready()) {
					std::string msg = i->recv();
					if(!msg.length()) continue;	//Skip empty messages.
					//Send to all others.
					for(auto j : links) {
						if(i == j) continue;	//Don't send back.
						j->send(msg);
					}
				}
			}
		}
		//Accept a new connection.
		void accept()
		{
			sock_handle_t h = low_accept();
			if(h == invalid_socket)
				return;
			links.push_back(new k_link(h));
		}
		//Main poll loop.
		void loop()
		{
			while(true) {
				fd_set rset;
				fd_set wset;
				sock_handle_t bound = 0;
				FD_ZERO(&rset);
				FD_ZERO(&wset);

				std::set<sock_handle_t> rx;
				std::set<sock_handle_t> tx;
				rx.insert(server_handle);
				poll_readyness(rx, tx);
				for(auto i : rx) FD_SET(i, &rset);
				for(auto i : tx) FD_SET(i, &wset);
				if(!rx.empty()) bound = max(bound, *rx.rbegin() + 1);
				if(!tx.empty()) bound = max(bound, *tx.rbegin() + 1);
				int r = select(bound, &rset, &wset, NULL, NULL);
				for(auto i = rx.begin(); i != rx.end(); i++) {
					if(FD_ISSET(*i, &rset))
						i = rx.erase(i);
					else
						i++;
				}
				for(auto i = tx.begin(); i != tx.end(); i++) {
					if(FD_ISSET(*i, &wset))
						i = tx.erase(i);
					else
						i++;
				}
				do_rx_tx(rx, tx);
				if(FD_ISSET(server_handle, &rset))
					accept();
			}
		}
		//Thread function for the server.
		static int thread_main(void* x)
		{
			k_server* _x = reinterpret_cast<k_server*>(x);
			_x->loop();
			return 0;
		}
	private:
		sock_handle_t low_accept();
		std::list<k_link*> links;
		sock_handle_t server_handle;
	};

	struct s_triple
	{
		s_triple();
		s_triple(const std::string& r);
		std::set<std::string> mods;
		std::string key;
		int32_t value;
		bool ok;
		operator std::string();
	};

	s_triple::s_triple() {}
	s_triple::s_triple(const std::string& r)
	{
		ok = false;
		size_t count;
		size_t length;
		char tmp;
		std::vector<char> tmp3;
		//size:{length:string}...length:string value
		std::istringstream x(r);
		x >> count;
		if(!x) return;	//Bad message.
		x >> tmp;
		for(size_t i = 0; i < count; i++) {
			x >> length;
			x >> tmp;
			if(length > r.length())
				return;	//Bad message.
			tmp3.resize(length);
			x.read(&tmp3[0], length);
			if(!x) return;	//Bad message
			std::string tmp2(tmp3.begin(), tmp3.end());
			mods.insert(tmp2);
		}
		x >> length;
		x >> tmp;
		if(length > r.length())
			return;  //Bad message.
		tmp3.resize(length);
		x.read(&tmp3[0], length);
		key = std::string(tmp3.begin(), tmp3.end());
		if(!x) return;	//Bad message
		x >> value;
		if(x) ok = true;
	}

	s_triple::operator std::string()
	{
		std::ostringstream x;
		x << mods.size() << ":";
		for(auto& i : mods)
			x << i.length() << ":" << i;
		x << key.length() << ":" << key;
		x << value;
		return x.str();
	}

	void event_handler(keyboard::modifier_set& mods, keyboard::key& key, keyboard::event& event)
	{
		if(!client_link)
			return;

		std::set<keyboard::modifier*> tmp = mods.get_set();
		std::string _key = key.get_name();
		int32_t value = event.get_raw();

		std::set<std::string> _mods;
		for(auto i : tmp)
			_mods.insert(i->get_name());

		s_triple s;
		s.mods = _mods;
		s.key = _key;
		s.value = value;
		std::string rep = s;
		client_link->send(rep);
	}

	//Inject specified rep.
	void inject_event(const std::string& rep)
	{
		s_triple s(rep);
		if(!s.ok) {
			messages << "Warning: inject_event: Skipping malformed message." << std::endl;
			return;
		}
		keyboard::modifier_set _mods;
		for(auto& i : s.mods) {
			keyboard::modifier* m = lsnes_kbd.try_lookup_modifier(i);
			if(m)
				_mods.add(*m);
			else
				messages << "Warning: inject_event: Ignoring unknown modifier '" << i << "'"
					<< std::endl;
		}
		keyboard::key* _key = lsnes_kbd.try_lookup_key(s.key);
		if(!_key) {
			messages << "Warning: inject_event: Skipping unknown key '" << s.key << "'"
				<< std::endl;
			return;
		}
		platform::queue(keypress(_mods, *_key, s.value));
	}

	int slave_thread(void* arg)
	{
		//TODO.
	}
}

void keybroadcast_notify_foreground(bool fg)
{
	if(client_link)
		lsnes_gamepads.master_enable(!fg);
}


void keybroadcast_set_master(uint16_t port)
{
	//TODO.
}

void keybroadcast_set_slave(uint16_t port)
{
	//TODO.
}
