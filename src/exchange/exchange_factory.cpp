#include "exchange/exchange_interface.h"
#include "utils/logger.h"
#include "utils/stringsUtils.h"
#include <sstream>
#include <filesystem>
#include <dylib.hpp>


namespace fs = std::filesystem;

void ExchangeFactory::load_exchange_class()
{
    /*
    * Load exchange class lass from certain folder.
    */
    fs::path root_path;
    try {
        root_path = fs::path(getExecutablePath()).parent_path();
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("Failed to get executable path, falling back to current_path(): ") + ex.what());
        root_path = fs::current_path();
    }

    LOG_INFO("Loading exchange modules from: " + root_path.string());

    for (const auto& entry : fs::directory_iterator(root_path))
    {
        if (entry.is_directory())
        {
            continue;
        }

        fs::path file_path = entry.path();

        std::string ext = file_path.extension().string();
    #if defined(_WIN32) || defined(_WIN64)
        if (ext != ".dll")
        {
            continue;
        }
    #elif defined(__APPLE__)
        if (ext != ".dylib")
        {
            continue;
        }
    #else
        if (ext != ".so")
        {
            continue;
        }
    #endif

        std::string exchange_module_name = file_path.string();
        load_exchange_class_from_module(exchange_module_name);
    }
}

void ExchangeFactory::load_exchange_class_from_module(std::string module_name)
{
    /*
    * Load exchange class from module file.
    */

    LOG_INFO("Loading exchange module: " + module_name);

    try
    {
        dylib::library* lib = new dylib::library(module_name);

        if (lib->get_symbol(ExchangeClass) && lib->get_symbol(ExchangeInstance))
        {
            auto pGetExchangeClass = lib->get_function<const char* ()>(ExchangeClass);
            std::string exchange_class = pGetExchangeClass();

            this->loaded_libraries_[exchange_class] = std::shared_ptr<dylib::library>(lib);
        }
    }
    catch (const std::exception& ex)
    {
        std::stringstream ss;
        ss << "Policy file " << module_name << " failed to load, triggering an exception:\n" << ex.what();
        LOG_ERROR(ss.str());
    }
}

ExchangeFactory& ExchangeFactory::getInstance() {
    static ExchangeFactory instance;
    return instance;
}

ExchangeFactory::ExchangeFactory() {
    load_exchange_class();
}

std::shared_ptr<IExchange> ExchangeFactory::createExchange(
    std::string name,
    const std::map<std::string, std::string>& config) {

        if (loaded_libraries_.find(name) == loaded_libraries_.end()) {
            LOG_ERROR("Exchange class not found: " + name);
            return nullptr;
        }

        auto lib = loaded_libraries_[name];
        auto pGetExchangeInstance = lib->get_function<IExchange* (const std::map<std::string, std::string>&)>(ExchangeInstance);
        if (!pGetExchangeInstance) {
            LOG_ERROR("GetExchangeInstance function not found in exchange class: " + name);
            return nullptr;
        }

        IExchange* exchange_ptr = pGetExchangeInstance(config);
        if (!exchange_ptr) {
            LOG_ERROR("Failed to create exchange instance: " + name);
            return nullptr;
        }

        return std::shared_ptr<IExchange>(exchange_ptr);
}
