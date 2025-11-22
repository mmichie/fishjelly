#!/bin/bash
#
# Generate self-signed SSL certificate for development/testing
# For production, use Let's Encrypt or purchase a certificate from a CA
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$SCRIPT_DIR/../base"
SSL_DIR="$BASE_DIR/ssl"

# Configuration
DAYS=365
KEY_SIZE=4096
COUNTRY="US"
STATE="California"
CITY="San Francisco"
ORG="Fishjelly"
OU="Development"
CN="localhost"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cn)
            CN="$2"
            shift 2
            ;;
        --days)
            DAYS="$2"
            shift 2
            ;;
        --org)
            ORG="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --cn HOSTNAME     Common Name (default: localhost)"
            echo "  --days DAYS       Certificate validity in days (default: 365)"
            echo "  --org NAME        Organization name (default: Fishjelly)"
            echo "  --help            Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                              # Generate cert for localhost"
            echo "  $0 --cn example.com --days 730  # Generate cert for example.com valid for 2 years"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "========================================="
echo "  Fishjelly SSL Certificate Generator"
echo "========================================="
echo ""
echo "Generating self-signed certificate..."
echo "  Common Name: $CN"
echo "  Organization: $ORG"
echo "  Valid for: $DAYS days"
echo ""

# Create SSL directory if it doesn't exist
mkdir -p "$SSL_DIR"

# Generate private key
echo "Generating $KEY_SIZE-bit RSA private key..."
openssl genrsa -out "$SSL_DIR/server-key.pem" $KEY_SIZE 2>/dev/null

# Generate certificate
echo "Generating self-signed certificate..."
openssl req -new -x509 \
    -key "$SSL_DIR/server-key.pem" \
    -out "$SSL_DIR/server-cert.pem" \
    -days $DAYS \
    -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORG/OU=$OU/CN=$CN" \
    2>/dev/null

# Generate DH parameters (this takes a while)
echo "Generating DH parameters (this may take a minute)..."
openssl dhparam -out "$SSL_DIR/dhparam.pem" 2048 2>/dev/null

# Set appropriate permissions
chmod 600 "$SSL_DIR/server-key.pem"
chmod 644 "$SSL_DIR/server-cert.pem"
chmod 644 "$SSL_DIR/dhparam.pem"

echo ""
echo "✓ Certificate generated successfully!"
echo ""
echo "Files created:"
echo "  Certificate: $SSL_DIR/server-cert.pem"
echo "  Private Key: $SSL_DIR/server-key.pem"
echo "  DH Params:   $SSL_DIR/dhparam.pem"
echo ""
echo "Certificate details:"
openssl x509 -in "$SSL_DIR/server-cert.pem" -noout -subject -dates
echo ""
echo "To use with Fishjelly:"
echo "  ./shelob --asio --ssl --ssl-port 8443"
echo ""
echo "To test:"
echo "  curl --insecure https://localhost:8443/"
echo "  openssl s_client -connect localhost:8443 -showcerts"
echo ""
echo "WARNING: This is a self-signed certificate for development only."
echo "         Browsers will show a security warning."
echo "         For production, use Let's Encrypt or a commercial CA."
echo ""
