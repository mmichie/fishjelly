#include "ssl_context.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

SSLContext::SSLContext() : ctx_(ssl::context::tlsv12_server), configured_(false) {
    configure_security_options();
    set_default_ciphers();
}

void SSLContext::configure_security_options() {
    // Disable old/insecure protocols
    ctx_.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                     ssl::context::no_sslv3 | ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1 |
                     ssl::context::single_dh_use);

    // Enable session caching for performance
    SSL_CTX_set_session_cache_mode(ctx_.native_handle(),
                                   SSL_SESS_CACHE_SERVER | SSL_SESS_CACHE_NO_INTERNAL);

    // Set session timeout (2 hours)
    SSL_CTX_set_timeout(ctx_.native_handle(), 7200);
}

void SSLContext::set_default_ciphers() {
    // Mozilla "Intermediate" cipher suite configuration
    // Balance between security and compatibility
    // Supports TLS 1.2 and 1.3
    const char* cipher_list =
        // TLS 1.3 ciphers (OpenSSL 1.1.1+)
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256:"
        // TLS 1.2 ciphers with forward secrecy
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        // Fallback ciphers
        "DHE-RSA-AES128-GCM-SHA256:"
        "DHE-RSA-AES256-GCM-SHA384";

    SSL_CTX_set_cipher_list(ctx_.native_handle(), cipher_list);

    // TLS 1.3 cipher suites (separate API)
#ifdef TLS1_3_VERSION
    const char* tls13_ciphers = "TLS_AES_128_GCM_SHA256:"
                                "TLS_AES_256_GCM_SHA384:"
                                "TLS_CHACHA20_POLY1305_SHA256";
    SSL_CTX_set_ciphersuites(ctx_.native_handle(), tls13_ciphers);
#endif
}

void SSLContext::load_certificate(const std::string& cert_file) {
    // Check if file exists
    std::ifstream cert_test(cert_file);
    if (!cert_test.good()) {
        throw std::runtime_error("Certificate file not found: " + cert_file);
    }
    cert_test.close();

    try {
        ctx_.use_certificate_chain_file(cert_file);
        cert_file_ = cert_file;
        std::cout << "Loaded certificate: " << cert_file << std::endl;
    } catch (const boost::system::system_error& e) {
        throw std::runtime_error("Failed to load certificate: " + std::string(e.what()));
    }
}

void SSLContext::load_private_key(const std::string& key_file) {
    // Check if file exists
    std::ifstream key_test(key_file);
    if (!key_test.good()) {
        throw std::runtime_error("Private key file not found: " + key_file);
    }
    key_test.close();

    try {
        ctx_.use_private_key_file(key_file, ssl::context::pem);
        key_file_ = key_file;
        configured_ = true;
        std::cout << "Loaded private key: " << key_file << std::endl;
    } catch (const boost::system::system_error& e) {
        throw std::runtime_error("Failed to load private key: " + std::string(e.what()));
    }
}

void SSLContext::load_dh_params(const std::string& dh_file) {
    // Check if file exists
    std::ifstream dh_test(dh_file);
    if (!dh_test.good()) {
        std::cerr << "Warning: DH parameters file not found: " << dh_file << std::endl;
        std::cerr << "Continuing without DH parameters (DHE ciphers will not be available)"
                  << std::endl;
        return;
    }
    dh_test.close();

    try {
        ctx_.use_tmp_dh_file(dh_file);
        dh_file_ = dh_file;
        std::cout << "Loaded DH parameters: " << dh_file << std::endl;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Warning: Failed to load DH parameters: " << e.what() << std::endl;
        std::cerr << "Continuing without DH parameters" << std::endl;
    }
}

void SSLContext::set_cipher_list(const std::string& ciphers) {
    if (SSL_CTX_set_cipher_list(ctx_.native_handle(), ciphers.c_str()) != 1) {
        throw std::runtime_error("Failed to set cipher list: " + ciphers);
    }
    std::cout << "Set custom cipher list: " << ciphers << std::endl;
}

ssl::context& SSLContext::get_context() { return ctx_; }

bool SSLContext::is_configured() const { return configured_; }
