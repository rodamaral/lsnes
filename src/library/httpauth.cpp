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
	//8 => EOS.
	uint8_t charclass[] = {
	//	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, //0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //1
		1, 2, 3, 2, 2, 2, 2, 2, 4, 4, 2, 2, 5, 2, 2, 4, //2
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

#define A_INVALID	0x0000	//Give up.
#define A_NOOP		0x1000	//Do nothing, not even eat the character.
#define A_EAT		0x2000	//Eat the character.
#define A_COPY_PNAME	0x3000	//Eat and copy to pname.
#define A_COPY_PVAL	0x4000	//Eat and copy to pvalue.
#define A_EMIT		0x5000	//Emit (pname,pvalue) and zeroize.
#define A_MASK		0xF000
#define A_STATE		0x0FFF

	unsigned auth_param_parser[] = {
		//CTRL    WS      TOKN    DBLQ    OTHQ    COMM    EQLS    BCKS    EOS
		// 0: Skip the initial whitespace.
		  0x0000, 0x2000, 0x1001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2000,
		// 1: Parse name.
		  0x0000, 0x1002, 0x3001, 0x0000, 0x0000, 0x0000, 0x1002, 0x0000, 0x0000,
		// 2: Parse whitespace after name (and =)
		  0x0000, 0x2002, 0x0000, 0x0000, 0x0000, 0x0000, 0x2003, 0x0000, 0x0000,
		// 3: Parse whitespace before value.
		  0x0000, 0x2003, 0x1004, 0x2005, 0x0000, 0x5007, 0x0000, 0x0000, 0x5000,
		// 4: Token value.
		  0x0000, 0x5007, 0x4004, 0x0000, 0x0000, 0x5007, 0x0000, 0x0000, 0x5000,
		// 5: Quoted-string value.
		  0x0000, 0x4005, 0x4005, 0x5009, 0x4005, 0x4005, 0x4005, 0x2006, 0x0000,
		// 6: Quoted-string escape.
		  0x0000, 0x4005, 0x4005, 0x4005, 0x4005, 0x4005, 0x4005, 0x4005, 0x0000,
		// 7: Whitespace after end of value.
		  0x0000, 0x2007, 0x0000, 0x0000, 0x0000, 0x2008, 0x0000, 0x0000, 0x2000,
		// 8: Whitespace after comma.
		  0x0000, 0x2008, 0x1001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		// 9: Eat double quote at end of quoted string.
		  0x0000, 0x0000, 0x0000, 0x2007, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	};

	unsigned get_charclass(const std::string& str, size_t pos) {
		if(pos < str.length())
			return charclass[(uint8_t)str[pos]];
		else
			return 8;
	}

	bool do_state_machine(const unsigned* machine, const std::string& input, size_t start,
		std::map<std::string, std::string>& params)
	{
		std::string pname, pvalue;
		unsigned state = 0;
		while(start <= input.length()) {
			unsigned act = machine[state * 9 + get_charclass(input, start)];
			switch(act & A_MASK) {
			case A_INVALID:
				return false;
			case A_NOOP:
				break;
			case A_EAT:
				start++;
				break;
			case A_COPY_PNAME:
				pname = pname + std::string(1, input[start++]);
				break;
			case A_COPY_PVAL:
				pvalue = pvalue + std::string(1, input[start++]);
				break;
			case A_EMIT:
				params[pname] = pvalue;
				pname = "";
				pvalue = "";
				break;
			};
			state = act & A_STATE;
		}
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
	std::string personalization = verb + " " + url;
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
		",response="+encode_hex(response,32)+",response2="+encode_hex(prereq,8);
}

void dh25519_http_auth::parse_auth_response(const std::string& response)
{
	std::map<std::string,std::string> pparse;
	if(response.substr(0, 7) != "dh25519") return;

	if(!do_state_machine(auth_param_parser, response, 7, pparse)) {
		throw std::runtime_error("Response parse error: <"+response+">");
	}

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
