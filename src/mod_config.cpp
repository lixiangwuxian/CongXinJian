#include "mod_config.hpp"

#include "main.hpp"

#include "beatsaber-hook/shared/utils.hpp"

Configuration::Configuration(modloader::ModInfo const& info_)
    : info(info_), filePath(get_config_path(info_)) {}

void Configuration::Load() {
    if (readJson) return;
    if (!fileexists(filePath)) writefile(filePath, "{}");
    Reload();
}

void Configuration::Reload() {
    std::string text = readfile(filePath);
    readJson = !config.Parse(text.c_str()).HasParseError();
    EnsureObject();
}

void Configuration::Write() {
    EnsureObject();
    rapidjson::StringBuffer buf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
    config.Accept(writer);
    writefile(filePath, buf.GetString());
}

void Configuration::EnsureObject() {
    if (!config.IsObject()) {
        PaperLogger.warn("Config data was invalid! Clearing.");
        config.SetObject();
    }
}
