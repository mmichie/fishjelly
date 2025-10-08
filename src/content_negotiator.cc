#include "content_negotiator.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

/**
 * MediaType constructor
 */
MediaType::MediaType(std::string t, double q) : type(std::move(t)), quality(q) {
    // Calculate specificity based on wildcards
    if (type == "*/*") {
        specificity = 1;
    } else if (type.find("/*") != std::string::npos) {
        specificity = 2;
    } else {
        specificity = 3;
    }
}

/**
 * Check if this media type pattern matches a content type
 */
bool MediaType::matches(std::string_view content_type) const {
    if (type == "*/*") {
        return true;
    }

    size_t slash_pos = type.find('/');
    if (slash_pos == std::string::npos) {
        return false;
    }

    // Handle type/* patterns
    if (type.substr(slash_pos + 1) == "*") {
        std::string type_prefix = type.substr(0, slash_pos + 1);
        return content_type.substr(0, slash_pos + 1) == type_prefix;
    }

    // Exact match (case-insensitive)
    if (type.length() != content_type.length()) {
        return false;
    }

    for (size_t i = 0; i < type.length(); ++i) {
        if (std::tolower(type[i]) != std::tolower(content_type[i])) {
            return false;
        }
    }

    return true;
}

/**
 * Trim whitespace from both ends of a string
 */
std::string ContentNegotiator::trim(std::string_view str) {
    size_t start = 0;
    size_t end = str.length();

    while (start < end && std::isspace(str[start])) {
        ++start;
    }

    while (end > start && std::isspace(str[end - 1])) {
        --end;
    }

    return std::string(str.substr(start, end - start));
}

/**
 * Extract quality value from parameter string
 */
double ContentNegotiator::extractQuality(std::string_view params) {
    size_t q_pos = params.find("q=");
    if (q_pos == std::string::npos) {
        return 1.0;
    }

    size_t value_start = q_pos + 2;
    size_t value_end = params.find(';', value_start);
    if (value_end == std::string::npos) {
        value_end = params.length();
    }

    std::string q_value = trim(params.substr(value_start, value_end - value_start));

    try {
        double quality = std::stod(q_value);
        return std::max(0.0, std::min(1.0, quality)); // Clamp to [0.0, 1.0]
    } catch (...) {
        return 1.0;
    }
}

/**
 * Parse a single media type with parameters
 */
MediaType ContentNegotiator::parseMediaType(std::string_view media_type_str) {
    size_t semicolon_pos = media_type_str.find(';');
    std::string type;
    double quality = 1.0;

    if (semicolon_pos != std::string::npos) {
        type = trim(media_type_str.substr(0, semicolon_pos));
        std::string params = std::string(media_type_str.substr(semicolon_pos + 1));
        quality = extractQuality(params);
    } else {
        type = trim(media_type_str);
    }

    return MediaType(type, quality);
}

/**
 * Parse Accept header into list of media types with quality values
 */
std::vector<MediaType> ContentNegotiator::parseAcceptHeader(std::string_view accept_header) {
    std::vector<MediaType> media_types;

    if (accept_header.empty()) {
        // Default to accepting anything
        media_types.emplace_back("*/*", 1.0);
        return media_types;
    }

    // Split by comma
    size_t start = 0;
    while (start < accept_header.length()) {
        size_t comma_pos = accept_header.find(',', start);
        if (comma_pos == std::string::npos) {
            comma_pos = accept_header.length();
        }

        std::string_view media_type_str = accept_header.substr(start, comma_pos - start);
        media_types.push_back(parseMediaType(media_type_str));

        start = comma_pos + 1;
    }

    // Sort by quality (descending), then by specificity (descending)
    std::sort(media_types.begin(), media_types.end(), [](const MediaType& a, const MediaType& b) {
        if (std::abs(a.quality - b.quality) > 0.001) {
            return a.quality > b.quality;
        }
        return a.specificity > b.specificity;
    });

    return media_types;
}

/**
 * Get MIME type from file extension
 */
std::string ContentNegotiator::getMimeType(std::string_view extension) {
    // Common MIME types mapping
    static const std::map<std::string, std::string> mime_map = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"txt", "text/plain"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"svg", "image/svg+xml"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"gz", "application/gzip"},
    };

    std::string ext_lower(extension);
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = mime_map.find(ext_lower);
    if (it != mime_map.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

/**
 * Find all file variants for a base path
 */
std::map<std::string, std::string> ContentNegotiator::findVariants(std::string_view base_path) {
    std::map<std::string, std::string> variants;

    // Remove leading slash if present
    std::string clean_path(base_path);
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }

    std::filesystem::path base(clean_path);
    std::filesystem::path parent = base.parent_path();
    std::string filename = base.filename().string();

    // Try to find files matching the pattern: filename.extension
    // Common extensions to check
    static const std::vector<std::string> extensions = {"html", "json", "xml", "txt",  "pdf", "png",
                                                        "jpg",  "jpeg", "gif", "webp", "svg"};

    for (const auto& ext : extensions) {
        std::filesystem::path variant_path = parent / (filename + "." + ext);

        if (std::filesystem::exists(variant_path)) {
            std::string content_type = getMimeType(ext);
            variants[variant_path.string()] = content_type;
        }
    }

    return variants;
}

/**
 * Score a content type against client preferences
 */
double ContentNegotiator::scoreContentType(std::string_view content_type,
                                           const std::vector<MediaType>& preferences) {
    double best_score = 0.0;

    for (const auto& pref : preferences) {
        if (pref.matches(content_type)) {
            // Score based on quality and specificity
            // More specific matches are better (with same quality)
            double score = pref.quality;
            if (pref.specificity == 3) {
                score += 0.0; // Exact match
            } else if (pref.specificity == 2) {
                score -= 0.001; // type/* match
            } else {
                score -= 0.002; // */* match
            }

            best_score = std::max(best_score, score);
        }
    }

    return best_score;
}

/**
 * Select the best matching file variant
 */
std::string ContentNegotiator::selectBestMatch(std::string_view base_path,
                                               std::string_view accept_header) {
    auto variants = findVariants(base_path);

    if (variants.empty()) {
        return ""; // No variants found
    }

    auto preferences = parseAcceptHeader(accept_header);

    std::vector<FileVariant> scored_variants;
    for (const auto& [path, content_type] : variants) {
        double score = scoreContentType(content_type, preferences);
        if (score > 0.0) {
            scored_variants.push_back({path, content_type, score});
        }
    }

    if (scored_variants.empty()) {
        return ""; // No acceptable variants
    }

    // Sort by score (descending)
    std::sort(scored_variants.begin(), scored_variants.end());

    return scored_variants[0].path;
}
