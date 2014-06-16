#include "curve25519.hpp"
#include "hex.hpp"
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

	enum charclass
	{
		CHRCLASS_CONTROL,
		CHRCLASS_WHITESPACE,
		CHRCLASS_TOKEN,
		CHRCLASS_DBLQUOTE,
		CHRCLASS_OTHERQUOTED,
		CHRCLASS_COMMA,
		CHRCLASS_EQUALS,
		CHRCLASS_BACKSLASH,
		CHRCLASS_SLASH,
		CHRCLASS_EOS,
	};

	enum charclass_mask
	{
		CMASK_CONTROL = 1 << CHRCLASS_CONTROL,
		CMASK_WHITESPACE = 1 << CHRCLASS_WHITESPACE,
		CMASK_TOKEN = 1 << CHRCLASS_TOKEN,
		CMASK_DBLQUOTE = 1 << CHRCLASS_DBLQUOTE,
		CMASK_OTHERQUOTED = 1 << CHRCLASS_OTHERQUOTED,
		CMASK_COMMA = 1 << CHRCLASS_COMMA,
		CMASK_EQUALS = 1 << CHRCLASS_EQUALS,
		CMASK_BACKSLASH = 1 << CHRCLASS_BACKSLASH,
		CMASK_SLASH = 1 << CHRCLASS_SLASH,
		CMASK_EOS = 1 << CHRCLASS_EOS,
	};

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
			return CHRCLASS_EOS;
	}

	bool read_char(const std::string& input, size_t& itr, char ch)
	{
		if(input[itr] == ch) {
			itr++;
			return true;
		}
		return false;
	}

	bool is_of_class(const std::string& input, size_t itr, uint16_t mask)
	{
		return mask & (1 << get_charclass(input, itr));
	}

	bool read_whitespace(const std::string& input, size_t& itr)
	{
		size_t oitr = itr;
		while(get_charclass(input, itr) == CHRCLASS_WHITESPACE)
			itr++;
		return (itr != oitr);
	}

	std::string read_token(const std::string& input, size_t& itr)
	{
		size_t len = 0;
		while(get_charclass(input, itr + len) == CHRCLASS_TOKEN)
			len++;
		std::string tmp(input.begin() + itr, input.begin() + itr + len);
		itr += len;
		return tmp;
	}

	std::string read_quoted_string(const std::string& input, size_t& itr, bool& error)
	{
		itr++; //Skip the initial ".
		std::ostringstream tmp;
		while(true) {
			if(is_of_class(input, itr, CMASK_CONTROL | CMASK_EOS)) {
				error = true;
				return "";
			} else if(is_of_class(input, itr, CMASK_BACKSLASH)) {
				//Quoted pair.
				if(is_of_class(input, itr + 1, CMASK_CONTROL | CMASK_EOS)) {
					error = true;
					return "";
				} else {
					//Skip the backslash and take next char.
					itr++;
					tmp << input[itr++];
				}
			} else if(is_of_class(input, itr, CMASK_DBLQUOTE)) {
				//String ends.
				itr++;
				return tmp.str();
			} else {
				//Some char that is literial.
				tmp << input[itr++];
			}
		}
	}

	bool parse_authenticate(const std::string& input, std::list<std::map<std::string, std::string>>& params)
	{
		std::map<std::string, std::string> tmp;
		size_t itr = 0;
		size_t inlen = input.length();
		while(itr < inlen) {
			//Skip commas.
			if(read_char(input, itr, ','))
				continue;
			//Skip whitespace.
			if(read_whitespace(input, itr))
				continue;
			std::string pname = read_token(input, itr);
			if(!pname.length()) return false;  //Token is required here.
			//Now we have two choices:
			//1) Whitespace followed by CHRCLASS_TOKEN, CHRCLASS_SLASH, CHRCLASS_COMMA or CHRCLASS_EOS.
			//   -> This is method name.
			//2) Possible whitespace followed by CHRCLASS_EQUALS. -> This is a parameter.
			bool had_ws = read_whitespace(input, itr);
			bool is_param = false;
			if(had_ws & is_of_class(input, itr, CMASK_TOKEN | CMASK_COMMA | CMASK_SLASH | CMASK_EOS)) {
				//This is method name.
			} else if(is_of_class(input, itr, CMASK_EQUALS)) {
				//This is a parameter.
				is_param = true;
			} else return false;	//Bad syntax.

			//Okay, parse what follows.
			if(is_param) {
				//Now itr points to equals sign of the parameter.
				itr++;  //Advance to start of value.
				read_whitespace(input, itr);  //Skip BWS.
				std::string pvalue;
				bool err = false;
				if(is_of_class(input, itr, CMASK_TOKEN)) {
					//Token.
					pvalue = read_token(input, itr);
				} else if(is_of_class(input, itr, CMASK_DBLQUOTE)) {
					//Quoted string.
					pvalue = read_quoted_string(input, itr, err);
					if(err) return false; //Bad quoted string.
				} else return false; //Bad syntax.
				if(tmp.count(pname)) return false; //Each parameter must only occur once.
				//We don't check for realm being quoted string (httpbis p7 says "might" on accepting
				//token realm).
				tmp[pname] = pvalue;
			} else {
				//Now itr points to start of parameters.
				if(!tmp.empty()) params.push_back(tmp);
				tmp.clear();
				tmp[":method"] = pname;
				//We have to see if this has token68 or not. It is if what follows has signature:
				//(TOKEN|SLASH)+EQUALS*WHITESPACE*(COMMA|EOS).
				size_t itr2 = itr;
				while(is_of_class(input, itr2, CMASK_TOKEN | CMASK_SLASH)) itr2++;
				if(itr == itr2) continue; //Not token68.
				while(is_of_class(input, itr2, CMASK_EQUALS)) itr2++;
				while(is_of_class(input, itr2, CMASK_WHITESPACE)) itr2++;
				if(!is_of_class(input, itr2, CMASK_COMMA | CMASK_EOS)) continue; //Not token68.
				//OK, this is token68.
				size_t itr3 = itr;
				while(is_of_class(input, itr3, CMASK_TOKEN | CMASK_SLASH)) itr3++;
				tmp[":token"] = std::string(input.begin() + itr, input.begin() + itr3);
				//Advance point of read to the COMMA/EOS.
				itr = itr2;
			}
		}
		if(!tmp.empty()) params.push_back(tmp);
		return true;
	}
}

dh25519_http_auth::dh25519_http_auth(const uint8_t* _privkey)
{
	memcpy(privkey, _privkey, 32);
	curve25519_clamp(privkey);
	curve25519(pubkey, privkey, curve25519_base);
	ready = false;
}

dh25519_http_auth::~dh25519_http_auth()
{
	skein::zeroize(privkey, sizeof(privkey));
	skein::zeroize(pubkey, sizeof(pubkey));
	skein::zeroize(ssecret, sizeof(ssecret));
}

std::string dh25519_http_auth::format_get_session_request()
{
	return "dh25519 key="+hex::b_to(pubkey,32);
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

	skein::hash hp(skein::hash::PIPE_512, 64);
	hp.write(ssecret, 32, skein::hash::T_KEY);
	hp.write((const uint8_t*)personalization.c_str(), personalization.length(), skein::hash::T_PERSONALIZATION);
	hp.write(pubkey, 32, skein::hash::T_PUBKEY);
	hp.write((uint8_t*)buf, strlen(buf), skein::hash::T_NONCE);
	hp.read(prereq);

	skein::hash h(skein::hash::PIPE_512, 256);
	h.write(ssecret, 32, skein::hash::T_KEY);
	h.write((const uint8_t*)personalization.c_str(), personalization.length(), skein::hash::T_PERSONALIZATION);
	h.write(pubkey, 32, skein::hash::T_PUBKEY);
	h.write((uint8_t*)buf, strlen(buf), skein::hash::T_NONCE);
	auto var = request_hash(id, pubkey, _nonce, h, prereq);
	skein::zeroize(buf, sizeof(buf));
	skein::zeroize(prereq, sizeof(prereq));
	return var;
}

std::string dh25519_http_auth::request_hash::get_authorization()
{
	char buf[32];
	uint8_t response[32];
	sprintf(buf, "%u", nonce);
	h.read(response);
	auto var = "dh25519 id="+quote_field(id)+",key="+hex::b_to(pubkey,32)+",nonce="+identity(buf)+
		",response="+hex::b_to(response,32)+",response2="+hex::b_to(prereq,8)+",noprotocol=1";
	skein::zeroize(buf, sizeof(buf));
	skein::zeroize(response, sizeof(response));
	return var;
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
		hex::b_from(_challenge, challenge);
		curve25519(ssecret, privkey, _challenge);
		nonce = 0;
		reseeded = true;
		ready = true;
		skein::zeroize(_challenge, sizeof(_challenge));
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
