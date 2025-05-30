#ifndef SHELOB_FOOTER_MIDDLEWARE_H
#define SHELOB_FOOTER_MIDDLEWARE_H 1

#include "middleware.h"
#include <string>

/**
 * Middleware that adds a footer to HTML responses
 * (Replaces the old Filter class)
 */
class FooterMiddleware : public Middleware {
private:
    std::string footer_html;
    
public:
    FooterMiddleware(const std::string& footer = "") {
        if (footer.empty()) {
            // Default footer content
            footer_html = "<hr><p><h1>The spice is vital to space travel.</h1></p>"
                         "</ul><a href=\"/index.html\">Return to Main Page</a>";
        } else {
            footer_html = footer;
        }
    }
    
    void process(RequestContext& ctx, std::function<void()> next) override;
};

#endif /* !SHELOB_FOOTER_MIDDLEWARE_H */