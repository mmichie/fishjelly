#pragma once

#include <chrono>
#include <cstddef>

/**
 * Connection timeout constants and utilities for protecting against
 * slow HTTP attacks (Slowloris, Slow POST, Slow Read).
 *
 * This header defines per-stage timeouts and minimum data rate requirements
 * to prevent attackers from holding connections open indefinitely.
 *
 * Security Context:
 * - Slowloris: Attackers send partial headers slowly to exhaust connections
 * - Slow POST: Attackers send POST body data at minimal rate
 * - Slow Read: Attackers read response data very slowly
 *
 * Mitigation Strategy:
 * - Enforce strict timeouts at each stage of request processing
 * - Track data transfer rates and disconnect slow clients
 * - Prevent resource exhaustion via connection holdopen attacks
 */

namespace ConnectionTimeouts {

// Per-stage timeout constants (in seconds)

// Maximum time to read initial HTTP request headers
// Protects against Slowloris attacks where partial headers are sent slowly
static constexpr int READ_HEADER_TIMEOUT_SEC = 10;

// Maximum time to read request body (POST/PUT data)
// Protects against Slow POST attacks where body is sent at minimal rate
static constexpr int READ_BODY_TIMEOUT_SEC = 30;

// Maximum time to write response to client
// Protects against Slow Read attacks where client reads very slowly
static constexpr int WRITE_RESPONSE_TIMEOUT_SEC = 60;

// Timeout for SSL/TLS handshake
static constexpr int SSL_HANDSHAKE_TIMEOUT_SEC = 10;

// Timeout for keep-alive connections waiting for next request
static constexpr int KEEPALIVE_TIMEOUT_SEC = 5;

// Minimum data transfer rate (bytes per second)
// Connections transferring data slower than this will be terminated
// Set to 1KB/sec to allow slow connections while blocking attacks
static constexpr size_t MIN_DATA_RATE_BYTES_PER_SEC = 1024;

// Time window for measuring data rate (seconds)
// Rate is calculated over this rolling window
static constexpr int RATE_MEASUREMENT_WINDOW_SEC = 5;

/**
 * Tracks connection state for bandwidth monitoring
 * Used to detect and terminate connections with insufficient data rates
 */
class ConnectionState {
  public:
    ConnectionState() : start_time_(std::chrono::steady_clock::now()), bytes_transferred_(0) {}

    /**
     * Record bytes transferred in this connection
     */
    void add_bytes(size_t bytes) { bytes_transferred_ += bytes; }

    /**
     * Get total bytes transferred
     */
    size_t get_bytes_transferred() const { return bytes_transferred_; }

    /**
     * Get elapsed time since connection started
     */
    std::chrono::seconds get_elapsed_time() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    }

    /**
     * Check if connection is transferring data too slowly
     * Returns true if the average data rate is below MIN_DATA_RATE_BYTES_PER_SEC
     *
     * Note: Only checks after RATE_MEASUREMENT_WINDOW_SEC has elapsed to avoid
     * false positives during connection setup
     */
    bool is_too_slow() const {
        auto elapsed = get_elapsed_time();

        // Don't check rate until measurement window has passed
        if (elapsed.count() < RATE_MEASUREMENT_WINDOW_SEC) {
            return false;
        }

        // Avoid division by zero
        if (elapsed.count() == 0) {
            return false;
        }

        // Calculate average bytes per second
        size_t bytes_per_sec = bytes_transferred_ / elapsed.count();

        // Connection is too slow if below minimum rate
        return bytes_per_sec < MIN_DATA_RATE_BYTES_PER_SEC;
    }

    /**
     * Reset the connection state for a new request (keep-alive scenario)
     */
    void reset() {
        start_time_ = std::chrono::steady_clock::now();
        bytes_transferred_ = 0;
    }

  private:
    std::chrono::steady_clock::time_point start_time_;
    size_t bytes_transferred_;
};

} // namespace ConnectionTimeouts
