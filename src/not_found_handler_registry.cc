#include "registry.h"
#include "not_found_handler.h"

static std::unique_ptr<RequestHandler> CreateNotFoundHandler(
    const std::string& path,
    const std::unordered_map<std::string, std::string>& args) {
    return std::make_unique<NotFoundHandler>();
}

REGISTER_HANDLER(NotFoundHandler, CreateNotFoundHandler)

void TouchNotFoundHandler() {
    // This function exists to force the registration of NotFoundHandler
    // It's called during server initialization
} 