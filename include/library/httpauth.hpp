#ifndef _library__httpauth__hpp__included__
#define _library__httpauth__hpp__included__

#include "skein.hpp"
#include "text.hpp"
#include <string>
#include <cstring>
#include <list>
#include <map>

/**
 * DH25519 HTTP auth class.
 */
class dh25519_http_auth
{
public:
/**
 * Internal hashing instance.
 */
	class request_hash
	{
	public:
/**
 * Construct.
 */
		request_hash(const text& _id, const uint8_t* _key, unsigned _nonce, skein::hash _h,
			const uint8_t* _prereq)
			: id(_id), nonce(_nonce), h(_h)
		{
			memcpy(pubkey, _key, 32);
			memcpy(prereq, _prereq, 8);
		}
/**
 * Append data to hash.
 */
		void hash(const uint8_t* data, size_t datalen)
		{
			h.write(data, datalen);
		}
/**
 * Read the final Authorization header.
 */
		text get_authorization();
	private:
		text id;
		uint8_t pubkey[32];
		uint8_t prereq[8];
		unsigned nonce;
		skein::hash h;
	};
/**
 * Create a new instance.
 *
 * Parameter privkey: The private key (32 bytes).
 */
	dh25519_http_auth(const uint8_t* privkey);
/**
 * Dtor.
 */
	~dh25519_http_auth();
/**
 * Format a session creation request
 *
 * Returns: The value for Authorization header.
 */
	text format_get_session_request();
/**
 * Start request hash computation. Hashes in the shared secret and nonce. The nonce is incremented.
 *
 * Parameter url: The notional URL.
 * Returns: The skein hash instance.
 */
	request_hash start_request(const text& url, const text& verb);
/**
 * Parse session auth response. If it contains new session parameters, the session is updated.
 *
 * Parameter response: The response from server (WWW-Authenticate).
 */
	void parse_auth_response(const text& response);
/**
 * Is the session ready?
 */
	bool is_ready() { return ready; }
/**
 * Output pubkey.
 */
	void get_pubkey(uint8_t* pubkey);
private:
	void parse_auth_response(std::map<text, text> pparse);
	unsigned char privkey[32];
	unsigned char pubkey[32];
	unsigned char ssecret[32];
	text id;
	unsigned nonce;
	bool ready; //id&ssecret is valid.
};

#endif
