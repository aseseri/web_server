#pragma once
#include "logger.h"
#include "request_handler.h"
#include <functional>
#include <string>
#include <unordered_map>

class Registry {
public:
    using RequestHandlerFactory = std::function<std::unique_ptr<RequestHandler>(
        const std::string& path,
        const std::unordered_map<std::string, std::string>& args
    )>;
    
    // Add public cleanup method for testing
    static void ResetForTesting() { GetFactories().clear(); }

    static void RegisterHandler(const std::string& name, RequestHandlerFactory factory);
    static RequestHandlerFactory GetHandlerFactory(const std::string& name);

private:
    static std::unordered_map<std::string, RequestHandlerFactory>& GetFactories();
};

#define REGISTER_HANDLER(Name, FactoryFunction) \
    struct Name##Registry { \
        Name##Registry() { \
            Registry::RegisterHandler(#Name, FactoryFunction); \
        } \
    } Name##_instance;
