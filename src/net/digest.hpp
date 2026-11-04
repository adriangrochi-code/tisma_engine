#pragma once

// HTTP Digest access authentication (RFC 7616, qop "auth", algorithm MD5)
// and the MD5 it needs (RFC 1321). For printers that ask for a user and a
// password (PrusaLink). Own implementation from the RFCs; see PROVENANCE.md.

#include <string>
#include <string_view>

namespace tisma::net {

// The MD5 digest of the bytes, as 32 lowercase hex digits.
std::string md5_hex(std::string_view data);

// The fields of a WWW-Authenticate: Digest challenge; false when it is not one.
struct DigestChallenge {
    std::string realm, nonce, opaque, qop, algorithm;
};
bool parse_digest_challenge(std::string_view header, DigestChallenge& out);

// The Authorization header's value for this request (method and the URI's
// path with its query), with a client nonce and the request count.
std::string digest_authorization(const DigestChallenge& challenge, const std::string& user, const std::string& password,
                                 const std::string& method, const std::string& uri, const std::string& cnonce,
                                 unsigned count = 1);

} // namespace tisma::net
