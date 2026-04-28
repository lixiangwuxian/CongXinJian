#pragma once

enum class Language : int { Chinese = 0, English = 1 };

struct I18nStrings {
    // Tab / mod name
    const char* tabTitle;

    // Mode dropdown options
    const char* modeDisabled;
    const char* modeManualPos;
    const char* modeManualRot;
    const char* modeManual6DOF;
    const char* modeAutoPos;
    const char* modeAutoRot;

    // Button dropdown options
    const char* btnThumbstick;
    const char* btnTrigger;
    const char* btnGrip;
    const char* btnPrimary;   // A/X
    const char* btnSecondary; // B/Y

    // Labels
    const char* enable;
    const char* adjustment;
    const char* assignedButton;
    const char* slotLabel;      // dropdown title: "Config Slot" / "配置槽位"
    const char* slotNameFmt;    // format for each slot entry — "Slot %s" / "槽位 %s"

    // Hand selector
    const char* leftHand;
    const char* rightHand;
    const char* leftHandCheck;
    const char* rightHandCheck;

    // Mirror
    const char* mirrorToRight;
    const char* mirrorToLeft;

    // Section headers
    const char* posHeader;
    const char* rotHeader;

    // Z offset
    const char* zOffsetLabel;

    // Buttons
    const char* resetOffset;

    // Mod settings
    const char* languageLabel;
};

/// Returns the string table for the currently active language.
const I18nStrings& I18n();

/// Get / set the active language. SetLanguage does NOT rebuild UI.
Language GetLanguage();
void SetLanguage(Language lang);
