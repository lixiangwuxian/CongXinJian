#include "ui/mod_settings.hpp"

#include "config.hpp"
#include "i18n.hpp"
#include "main.hpp"
#include "ui/settings_tab.hpp"

#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"
#include "bsml/shared/BSML/Settings/BSMLSettings.hpp"

#include "HMUI/ViewController.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"

#include <array>
#include <functional>
#include <string_view>

namespace {

std::array<std::string_view, 2> kLangNames = {"中文", "English"};

void BuildModSettings(HMUI::ViewController* vc, bool firstActivation,
                      bool /*addedToHierarchy*/, bool /*screenSystemEnabling*/) {
    if (!firstActivation) return;

    auto parent = vc->get_transform();

    auto* vert = BSML::Lite::CreateVerticalLayoutGroup(parent);
    auto vt = vert->get_transform();

    int curLang = static_cast<int>(GetLanguage());

    BSML::Lite::CreateDropdown(vt, StringW(I18n().languageLabel),
        StringW(kLangNames[curLang]),
        kLangNames,
        std::function<void(StringW)>([](StringW val) {
            std::string s(val);
            int lang = (s == "中文") ? 0 : 1;
            SetLanguage(static_cast<Language>(lang));
            GetTweakConfig().language = lang;
            SaveTweakConfig();
            RebuildSettingsTab();
        })
    );
}

}  // namespace

void RegisterModSettings() {
    auto* settings = BSML::BSMLSettings::get_instance();
    if (!settings) {
        PaperLogger.error("BSMLSettings null");
        return;
    }

    bool ok = settings->TryAddSettingsMenu(
        std::function<void(HMUI::ViewController*, bool, bool, bool)>(&BuildModSettings),
        "CongXinJian");
    PaperLogger.info("BSMLSettings AddMenu: {}", ok);
}
