#include "../src/security_middleware.h"
#include <filesystem>
#include <iostream>
#include <string>

void test_path(const std::string& path, bool should_be_valid, const std::string& test_name) {
    std::string result = SecurityMiddleware::sanitize_path(path, std::filesystem::path("htdocs"));
    bool is_valid = !result.empty();

    std::cout << (is_valid == should_be_valid ? "PASS" : "FAIL") << ": " << test_name << std::endl;
    std::cout << "  Input: " << path << std::endl;
    std::cout << "  Result: " << (result.empty() ? "(blocked)" : result) << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Path Sanitization Tests ===" << std::endl << std::endl;

    // Attack vectors that should be blocked
    test_path("/../../../etc/passwd", false, "Basic traversal");
    test_path("/..\\..\\..\\etc\\passwd", false, "Windows-style traversal");
    test_path("/..%2f..%2f..%2fetc/passwd", false, "URL encoded traversal");
    test_path("/..%252f..%252fetc/passwd", false, "Double encoded traversal");
    test_path("/....//....//etc/passwd", false, "Dot-dot-slash-slash");
    test_path("/../src/security_middleware.cc", false, "Traverse to src");

    // Valid paths that should work
    test_path("/index.html", true, "Valid: index.html");
    test_path("/", true, "Valid: root");
    test_path("/subdir/file.html", true, "Valid: subdirectory");

    return 0;
}
