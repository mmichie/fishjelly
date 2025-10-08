#ifndef SHELOB_CONTENT_NEGOTIATOR_H
#define SHELOB_CONTENT_NEGOTIATOR_H 1

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "global.h"

/**
 * Media type with quality value from Accept header
 */
struct MediaType {
    std::string type; // e.g., "text/html", "application/json", "*\/*"
    double quality;   // q-value from 0.0 to 1.0 (default 1.0)
    int specificity;  // 3 for type/subtype, 2 for type/*, 1 for *\/*

    MediaType(std::string t, double q = 1.0);
    bool matches(std::string_view content_type) const;
};

/**
 * File variant with its content type and score
 */
struct FileVariant {
    std::string path;
    std::string content_type;
    double score;

    bool operator<(const FileVariant& other) const { return score > other.score; }
};

/**
 * Content negotiation handler for HTTP Accept headers
 */
class ContentNegotiator {
  public:
    /**
     * Parse Accept header and extract media types with quality values
     * Example: "text/html, application/json;q=0.9, star/star;q=0.1"
     */
    std::vector<MediaType> parseAcceptHeader(std::string_view accept_header);

    /**
     * Find all file variants for a given base path
     * Example: For "/api/data", finds data.json, data.xml, data.html
     * Returns map of file path to content type
     */
    std::map<std::string, std::string> findVariants(std::string_view base_path);

    /**
     * Select the best matching file variant based on Accept header
     * Returns the file path of the best match, or empty string if no match
     */
    std::string selectBestMatch(std::string_view base_path, std::string_view accept_header);

    /**
     * Score a content type against client preferences
     * Returns the quality value (0.0 to 1.0) for the content type
     */
    double scoreContentType(std::string_view content_type,
                            const std::vector<MediaType>& preferences);

  private:
    /**
     * Get MIME type from file extension
     */
    std::string getMimeType(std::string_view extension);

    /**
     * Calculate specificity of a media type pattern
     * Exact type/subtype = 3, type/star = 2, star/star = 1
     */
    int calculateSpecificity(std::string_view type);

    /**
     * Trim whitespace from string
     */
    std::string trim(std::string_view str);

    /**
     * Parse a single media type with parameters (e.g., "text/html;q=0.9")
     */
    MediaType parseMediaType(std::string_view media_type_str);

    /**
     * Extract q-value from parameters string
     * Example: "q=0.9;charset=utf-8" -> 0.9
     */
    double extractQuality(std::string_view params);
};

#endif /* !SHELOB_CONTENT_NEGOTIATOR_H */
