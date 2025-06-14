#include "footer_middleware.h"
#include "global.h"
#include <iostream>

void FooterMiddleware::process(RequestContext& ctx, std::function<void()> next) {
    // Call next middleware first
    next();

    // Only process if response not sent and it's HTML content
    if (!ctx.response_sent && ctx.status_code == 200) {
        // Check if this is an HTML response that should have footer
        bool should_add_footer = false;

        // Check file extension from path
        std::string path = ctx.path;
        if (path.find(".shtml") != std::string::npos || path.find(".shtm") != std::string::npos) {
            should_add_footer = true;
        } else if (ctx.content_type == "text/html") {
            // Also add to any HTML response if configured
            // (This could be controlled by middleware configuration)
            should_add_footer = false; // For now, only .shtml files
        }

        if (should_add_footer) {
            auto filter_index = ctx.response_body.find("</body>");

            if (DEBUG) {
                if (filter_index != std::string::npos) {
                    std::cout << "FooterMiddleware: Found </body> at " << filter_index << std::endl;
                } else {
                    std::cout << "FooterMiddleware: Didn't find </body>" << std::endl;
                }
            }

            if (filter_index != std::string::npos) {
                // Insert footer before </body>
                ctx.response_body.insert(filter_index, footer_html);
            } else {
                // No </body> found, append footer at the end
                ctx.response_body.append(footer_html);
            }
        }
    }
}