#pragma once

// Minimal replacement for the Configuration class that beatsaber-hook
// removed in v8 (config/config-utils.hpp). Keeps the same on-disk location
// (/sdcard/ModData/<app>/Configs/<id>.json) so existing configs carry over.

#include <string>

#include "beatsaber-hook/shared/rapidjson.hpp"
#include "scotland2/shared/loader.hpp"

class Configuration {
  public:
    explicit Configuration(modloader::ModInfo const& info_);

    modloader::ModInfo const info;
    rapidjson::Document config;

    // Loads the JSON config once; creates an empty object if missing.
    void Load();
    // Re-reads the JSON config from disk.
    void Reload();
    // Writes the JSON config to disk.
    void Write();

  private:
    void EnsureObject();

    std::string filePath;
    bool readJson = false;
};
