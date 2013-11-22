#include "httpauth.hpp"
#include "string.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>

namespace
{
	std::string encode_hex(const uint8_t* data, size_t datasize)
	{
		std::ostringstream x;
		for(size_t i = 0; i < datasize; i++)
			x << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
		return x.str();
	}

	//Can only handle 9, 32-126, 128-255.
	std::string quote_field(const std::string& field)
	{
		std::ostringstream x;
		for(size_t i = 0; i < field.length(); i++) {
			if(field[i] == '\\')
				x << "\\\\";
			else if(field[i] == '\"')
				x << "\\\"";
			else
				x << field[i];
		}
		return x.str();
	}

	//Identity transform.
	std::string identity(const std::string& field)
	{
		return field;
	}

	//Character class.
	//0 => Controls, except HT
	//1 => Whitespace (HT and SP).
	//2 => Token chars (!#$%&'*+.^_`|~0-9A-Za-z-)
	//3 => Double quote (")
	//4 => Other quoted characters.
	//5 => Comma
	//6 => Equals sign.
	//7 => Backslash
	//8 => Slash
	//9 => EOS.
	uint8_t charclass[] = {
	//	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, //0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //1
		1, 2, 3, 2, 2, 2, 2, 2, 4, 4, 2, 2, 5, 2, 2, 8, //2
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 6, 4, 4, //3
		4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, //4
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 7, 4, 2, 2, //5
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, //6
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 4, 2, 0, //7
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //8
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //9
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //A
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //B
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //C
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //D
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //E
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4  //F
	};

	unsigned get_charclass(const std::string& str, size_t pos) {
		if(pos < str.length())
			return charclass[(uint8_t)str[pos]];
		else
			return 9;
	}

	std::string substr(std::string x, std::pair<size_t, size_t> m, size_t base)
	{
		return x.substr(m.first + base, m.second);
	}

	bool parse_authenticate(const std::string& _input, std::list<std::map<std::string, std::string>>& params)
	{
		std::string classstr = _input;
		for(size_t i = 0; i < classstr.length(); i++)
			classstr[i] = 48 + get_charclass(classstr, i);
		size_t start = 0;
		std::map<std::string, std::string> tmp;
		while(start < _input.length()) {
			regex_results r;
			if(classstr[0] == '1' || classstr[0] == '5') {
				//Skip Whitespace or comma.
				start++;
				classstr = classstr.substr(1);
				continue;
			}
			if(r = regex("1*(2+)1+([28]+)6*1*(5.*|$)", classstr)) {
				//This is authscheme (1) followed by token68 (2). Tail is (3)
				if(!tmp.empty()) params.push_back(tmp);
				tmp.clear();
				tmp[":method"] = substr(_input, r.match(1), start);
				tmp[":token"] = substr(_input, r.match(2), start);
				start += r.match(3).first;
				classstr = classstr.substr(r.match(3).first);
				//std::cerr << "Parsed authscheme=" << tmp[":method"] << " with token68: "
				//	<< tmp[":token"] << std::endl; 
			} else if(r = regex("1*(2+)1+((5|2+1*61*[23]).*|$)", classstr)) {
				//This is authscheme (1) followed by parameter. Tail is (2)
				if(!tmp.empty()) params.push_back(tmp);
				tmp.clear();
				tmp[":method"] = substr(_input, r.match(1), start);
				start += r.match(2).first;
				classstr = classstr.substr(r.match(2).first);
				//std::cerr << "Parsed authscheme=" << tmp[":method"] << std::endl; 
			} else if(r = regex("1*(2+)1*61*(2+|3([124568]|7[12345678])+3)1*(5.*|$)", classstr)) {
				//This is auth-param (name (1) = value (2)). Tail is (4).
				std::string name = substr(_input, r.match(1), start);
				std::string value = substr(_input, r.match(2), start);
				if(value[0] == '"') {
					//Process quoted-string.
					std::ostringstream x;
					for(size_t i = 1; i < value.length(); i++) {
						if(value[i] == '\"') break;
						else if(value[i] == '\\') { x << value[i + 1]; i++; }
						else x << value[i];
					}
					value = x.str();
				}
				tmp[name] = value;
				start += r.match(4).first;
				classstr = classstr.substr(r.match(4).first);
				//std::cerr << "Parsed param: " << name << " -> '" << value << "'" << std::endl; 
			} else
				return false;	//Syntax error.
		}
		if(!tmp.empty()) params.push_back(tmp);
		return true;
	}

	//Parse a hex char.
	inline uint8_t hparse(char _ch)
	{
		uint8_t ch = _ch;
		uint8_t itbl[] = {9,1,16,2,10,3,11,4,12,5,13,6,14,7,15,8,0};
		return itbl[(uint8_t)(2*ch + 22*(ch>>5)) % 17];
	}

	//Undo hex encoding.
	void unhex(uint8_t* buf, const std::string& str)
	{
		bool polarity = false;
		size_t ptr = 0;
		uint8_t val = 0;
		while(ptr < str.length()) {
			val = val * 16 + hparse(str[ptr++]);
			if(!(polarity = !polarity)) *(buf++) = val;
		}
	}
}

dh25519_http_auth::dh25519_http_auth(const uint8_t* _privkey)
{
	memcpy(privkey, _privkey, 32);
	curve25519_clamp(privkey);
	curve25519(pubkey, privkey, curve25519_base);
	ready = false;
}

std::string dh25519_http_auth::format_get_session_request()
{
	return "dh25519 key="+encode_hex(pubkey,32);
}

dh25519_http_auth::request_hash dh25519_http_auth::start_request(const std::string& url, const std::string& verb)
{
	unsigned _nonce;
	regex_results r = regex("[^:]+(://.*)", url);
	std::string url2 = r ? r[1] : url;
	std::string personalization = verb + " " + url2;
	char buf[32];
	uint8_t prereq[8];
	if(!ready)
		throw std::runtime_error("Authenticator is not ready for request auth");
	_nonce = nonce++;
	sprintf(buf, "%u", _nonce);

	skein_hash hp(skein_hash::PIPE_512, 64);
	hp.write(ssecret, 32, skein_hash::T_KEY);
	hp.write((const uint8_t*)personalization.c_str(), personalization.length(), skein_hash::T_PERSONALIZATION);
	hp.write(pubkey, 32, skein_hash::T_PUBKEY);
	hp.write((uint8_t*)buf, strlen(buf), skein_hash::T_NONCE);
	hp.read(prereq);

	skein_hash h(skein_hash::PIPE_512, 256);
	h.write(ssecret, 32, skein_hash::T_KEY);
	h.write((const uint8_t*)personalization.c_str(), personalization.length(), skein_hash::T_PERSONALIZATION);
	h.write(pubkey, 32, skein_hash::T_PUBKEY);
	h.write((uint8_t*)buf, strlen(buf), skein_hash::T_NONCE);
	return request_hash(id, pubkey, _nonce, h, prereq);
}

std::string dh25519_http_auth::request_hash::get_authorization()
{
	char buf[32];
	uint8_t response[32];
	sprintf(buf, "%u", nonce);
	h.read(response);
	return "dh25519 id="+quote_field(id)+",key="+encode_hex(pubkey,32)+",nonce="+identity(buf)+
		",response="+encode_hex(response,32)+",response2="+encode_hex(prereq,8)+",noprotocol=1";
}

void dh25519_http_auth::parse_auth_response(const std::string& response)
{
	std::list<std::map<std::string, std::string>> pparse;
	if(!parse_authenticate(response, pparse)) {
		throw std::runtime_error("Response parse error: <"+response+">");
	}
	for(auto i : pparse)
		parse_auth_response(i);
}

void dh25519_http_auth::parse_auth_response(std::map<std::string, std::string> pparse)
{
	if(pparse[":method"] != "dh25519") return;

	//If there are id and challenge fields, use those to reseed.
	bool stale = (pparse.count("error") && pparse["error"] == "stale");
	bool reseeded = false;
	if(pparse.count("id") && pparse.count("challenge") && (!ready || pparse["id"] != id || stale)) {
		id = pparse["id"];
		std::string challenge = pparse["challenge"];
		if(challenge.length() != 64) goto no_reseed;
		uint8_t _challenge[32];
		unhex(_challenge, challenge);
		curve25519(ssecret, privkey, _challenge);
		nonce = 0;
		reseeded = true;
		ready = true;
	}
no_reseed:
	if(pparse.count("error")) {
		if(pparse["error"] == "ok")
			;  //Do nothing.
		else if(pparse["error"] == "stale") {
			//This is only an error if not reseeded this round.
			if(!reseeded) {
				ready = false;
				throw std::runtime_error("Authentication is stale");
			}
		} else if(pparse["error"] == "badkey") {
			ready = false;
			throw std::runtime_error("Client key not registed with target site");
		} else if(pparse["error"] == "badmac")
			throw std::runtime_error("Request failed MAC check");
		else if(pparse["error"] == "replay")
			throw std::runtime_error("Request was replayed");
		else if(pparse["error"] == "syntaxerr")
			throw std::runtime_error("Request syntax error");
		else
			throw std::runtime_error("Unknown error '" + pparse["error"] + "'");
	}
}

void dh25519_http_auth::get_pubkey(uint8_t* _pubkey)
{
	memcpy(_pubkey, pubkey, 32);
}

#ifdef TEST_HTTP_AUTH_PARSE
int main(int argc, char** argv)
{
	std::list<std::map<std::string, std::string>> params;
	bool r = parse_authenticate(argv[1], params);
	if(!r) { std::cerr << "Parse error" << std::endl; return 1; }
	for(auto i : params) {
		for(auto j : i)
			std::cerr << j.first << "=" << j.second << std::endl;
		std::cerr << "---------------------------------------" << std::endl;
	}
	return 0;
}
#endif
