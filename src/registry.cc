#include "registry.h"
#include <stdexcept>

std::unordered_map<std::string, Registry::RequestHandlerFactory>& Registry::GetFactories() {
    static std::unordered_map<std::string, RequestHandlerFactory> factories;
    return factories;
}

void Registry::RegisterHandler(const std::string& name, RequestHandlerFactory factory) {
    auto& factories = GetFactories();

    if (factories.find(name) != factories.end()) {
        throw std::runtime_error("Handler already registered: " + name);
    }

    factories[name] = factory;
    BOOST_LOG_TRIVIAL(info) << "Registered handler: " << name;
}

Registry::RequestHandlerFactory Registry::GetHandlerFactory(const std::string& name) {
    auto& factories = GetFactories();
    auto it = factories.find(name);
    return (it != factories.end()) ? it->second : nullptr;
}
