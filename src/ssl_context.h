#ifndef FISHJELLY_SSL_CONTEXT_H
#define FISHJELLY_SSL_CONTEXT_H

#include <boost/asio/ssl.hpp>
#include <string>

namespace ssl = boost::asio::ssl;

/**
 * SSL/TLS Context Manager
 * Handles SSL context configuration, certificate loading, and security settings
 */
class SSLContext {
  private:
    ssl::context ctx_;
    std::string cert_file_;
    std::string key_file_;
    std::string dh_file_;
    bool configured_;

    void configure_security_options();
    void set_default_ciphers();

  public:
    /**
     * Constructor - Creates SSL context with TLS server method
     */
    SSLContext();

    /**
     * Load server certificate from PEM file
     * @param cert_file Path to certificate file
     * @throws std::runtime_error if file cannot be loaded
     */
    void load_certificate(const std::string& cert_file);

    /**
     * Load private key from PEM file
     * @param key_file Path to private key file
     * @throws std::runtime_error if file cannot be loaded
     */
    void load_private_key(const std::string& key_file);

    /**
     * Load DH parameters from PEM file
     * @param dh_file Path to DH parameters file
     * @throws std::runtime_error if file cannot be loaded
     */
    void load_dh_params(const std::string& dh_file);

    /**
     * Set custom cipher list
     * @param ciphers Cipher suite string (OpenSSL format)
     */
    void set_cipher_list(const std::string& ciphers);

    /**
     * Get the underlying ASIO SSL context
     * @return Reference to boost::asio::ssl::context
     */
    ssl::context& get_context();

    /**
     * Check if SSL context is properly configured
     * @return true if certificate and key are loaded
     */
    bool is_configured() const;

    /**
     * Get certificate file path
     */
    std::string get_cert_file() const { return cert_file_; }

    /**
     * Get private key file path
     */
    std::string get_key_file() const { return key_file_; }
};

#endif // FISHJELLY_SSL_CONTEXT_H
