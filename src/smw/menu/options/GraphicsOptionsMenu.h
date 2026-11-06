#pragma once

#include "Localization.h"
#include "uimenu.h"

#include <string>
#include <vector>

class MI_Button;
class MI_Image;
class MI_PacksField;
class MI_Text;
template<typename T> class MI_SelectField;


/*
    You can switch between windowed and fullscreen mode,
    as well as change the graphic packs here.
*/
class UI_GraphicsOptionsMenu : public UI_Menu {
public:
    UI_GraphicsOptionsMenu();

    /// The font file selected in the menu, or empty for the pixel fonts
    std::string selectedFont() const;
    /// Shows the font field only for the smooth font style
    void refreshFontFields();
    /// The code of the language selected in the menu
    std::string selectedLanguage() const;

private:
    MI_SelectField<bool>* miTopLayerField;
    MI_SelectField<short>* miFrameLimiterField;
    MI_SelectField<bool>* miFullscreenField;

    MI_PacksField* miMenuGraphicsPackField;
    MI_PacksField* miWorldGraphicsPackField;
    MI_PacksField* miGameGraphicsPackField;
    MI_SelectField<bool>* miFontStyleField;
    MI_SelectField<short>* miFontField;
    std::vector<std::string> mFontFiles;
    MI_SelectField<short>* miLanguageField;
    std::vector<l10n::LocaleInfo> mLanguages;
    MI_Button* miGraphicsOptionsMenuBackButton;

    MI_Image* miGraphicsOptionsMenuLeftHeaderBar;
    MI_Image* miGraphicsOptionsMenuRightHeaderBar;
    MI_Text* miGraphicsOptionsMenuHeaderText;
};
