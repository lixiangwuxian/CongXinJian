#include "i18n.hpp"

namespace {

const I18nStrings kChinese = {
    // tabTitle
    "从心剑",

    // Mode dropdown
    "禁用",
    "手动位移",
    "手动旋转",
    "手动6DOF",
    "自动位移",
    "自动旋转",

    // Button dropdown
    "摇杆按下",
    "扳机",
    "握持",
    "A/X",
    "B/Y",

    // Labels
    "启用",
    "调整",
    "按键绑定",
    "槽位",
    "槽位 %s",

    // Hand selector
    " 左手",
    " 右手",
    " 左手✔",
    " 右手✔",

    // Mirror
    "镜像到右手",
    "镜像到左手",

    // Section headers
    "<b>位移(cm)</b>",
    "<b>旋转(°)</b>",

    // Z offset
    "<color=#8888FF>轴向偏移</color>",

    // Buttons
    "重置偏移",

    // Mod settings
    "语言",
};

const I18nStrings kEnglish = {
    // tabTitle
    "CongXinJian",

    // Mode dropdown
    "Disabled",
    "Manual Pos",
    "Manual Rot",
    "Manual 6DOF",
    "Auto Pos",
    "Auto Rot",

    // Button dropdown
    "Thumbstick",
    "Trigger",
    "Grip",
    "A/X",
    "B/Y",

    // Labels
    "Enable",
    "Mode",
    "Button",
    "Slot",
    "Slot %s",

    // Hand selector
    " Left",
    " Right",
    " Left ✔",
    " Right ✔",

    // Mirror
    "Mirror→R",
    "Mirror→L",

    // Section headers
    "<b>Pos(cm)</b>",
    "<b>Rot(°)</b>",

    // Z offset
    "<color=#8888FF>Z Offset</color>",

    // Buttons
    "Reset",

    // Mod settings
    "Language",
};

Language g_language = Language::English;

}  // namespace

const I18nStrings& I18n() {
    return g_language == Language::Chinese ? kChinese : kEnglish;
}

Language GetLanguage() {
    return g_language;
}

void SetLanguage(Language lang) {
    g_language = lang;
}
