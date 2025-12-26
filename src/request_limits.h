#ifndef SHELOB_REQUEST_LIMITS_H
#define SHELOB_REQUEST_LIMITS_H 1

#include <cstddef>

/**
 * Request size limits to prevent memory exhaustion DoS attacks
 *
 * These limits protect against attackers sending:
 * - Giant headers to exhaust memory
 * - Unlimited request bodies to fill memory
 * - Too many headers to slow down parsing
 * - Excessively large chunked data
 *
 * HTTP/1.1 Limits
 */
namespace RequestLimits {

// HTTP/1.1 Header Limits
constexpr size_t MAX_HEADER_SIZE = 8192;  // 8KB total headers size
constexpr size_t MAX_HEADER_LINE = 8192;  // 8KB per header line
constexpr size_t MAX_REQUEST_LINE = 8192; // 8KB for request line (GET /path HTTP/1.1)
constexpr size_t MAX_HEADERS_COUNT = 100; // Maximum number of header fields

// HTTP/1.1 Body Limits
constexpr size_t MAX_BODY_SIZE = 10485760;    // 10MB request body
constexpr size_t MAX_UPLOAD_SIZE = 104857600; // 100MB for file uploads (PUT)
constexpr size_t MAX_CHUNK_SIZE = 1048576;    // 1MB per chunk in chunked encoding

// HTTP/2 Specific Limits (per RFC 7540 recommendations)
constexpr int MAX_STREAMS_PER_CONN = 100;      // Maximum concurrent streams
constexpr size_t MAX_FRAME_SIZE = 16384;       // 16KB max frame size (RFC 7540 default)
constexpr size_t MAX_HEADER_LIST_SIZE = 16384; // 16KB max total header size
constexpr size_t MAX_HEADER_NAME_SIZE = 256;   // 256 bytes per header name
constexpr size_t MAX_HEADER_VALUE_SIZE = 8192; // 8KB per header value

// HPACK dynamic table size limits (protection against HPACK bomb attacks)
// RFC 7540 default is 4096 bytes; we use the same to prevent memory exhaustion
// from maliciously crafted compressed headers that expand to huge sizes
constexpr size_t MAX_HPACK_TABLE_SIZE = 4096; // 4KB HPACK dynamic table

} // namespace RequestLimits

#endif /* !SHELOB_REQUEST_LIMITS_H */
