#include "GraphicsOptionsMenu.h"

#include "FileList.h"
#include "GameValues.h"
#include "ResourceManager.h"
#include "ui/MI_Button.h"
#include "ui/MI_Image.h"
#include "ui/MI_PacksField.h"
#include "ui/MI_SelectField.h"
#include "ui/MI_Text.h"
#include "path.h"

#include <string>

extern CResourceManager* rm;
extern CGameValues game_values;
extern GraphicsList* menugraphicspacklist;
extern GraphicsList* worldgraphicspacklist;
extern GraphicsList* gamegraphicspacklist;


UI_GraphicsOptionsMenu::UI_GraphicsOptionsMenu()
    : UI_Menu()
{
    miTopLayerField = new MI_SelectField<bool>(&rm->spr_selectfield, 70, 120, "Draw Top Layer", 500, 220);
    miTopLayerField->add("Background", false);
    miTopLayerField->add("Foreground", true);
    miTopLayerField->setOutputPtr(&game_values.toplayer);
    miTopLayerField->setCurrentValue(game_values.toplayer ? 1 : 0);
    miTopLayerField->setAutoAdvance(true);

    miFrameLimiterField = new MI_SelectField<short>(&rm->spr_selectfield, 70, 160, "Frame Limit", 500, 220);
    miFrameLimiterField->add("10 FPS", 100);
    miFrameLimiterField->add("15 FPS", 67);
    miFrameLimiterField->add("20 FPS", 50);
    miFrameLimiterField->add("25 FPS", 40);
    miFrameLimiterField->add("30 FPS", 33);
    miFrameLimiterField->add("35 FPS", 28);
    miFrameLimiterField->add("40 FPS", 25);
    miFrameLimiterField->add("45 FPS", 22);
    miFrameLimiterField->add("50 FPS", 20);
    miFrameLimiterField->add("55 FPS", 18);
    miFrameLimiterField->add("62 FPS (Normal)", 16);
    miFrameLimiterField->add("66 FPS", 15);
    miFrameLimiterField->add("71 FPS", 14);
    miFrameLimiterField->add("77 FPS", 13);
    miFrameLimiterField->add("83 FPS", 12);
    miFrameLimiterField->add("90 FPS", 11);
    miFrameLimiterField->add("100 FPS", 10);
    miFrameLimiterField->add("111 FPS", 9);
    miFrameLimiterField->add("125 FPS", 8);
    miFrameLimiterField->add("142 FPS", 7);
    miFrameLimiterField->add("166 FPS", 6);
    miFrameLimiterField->add("200 FPS", 5);
    miFrameLimiterField->add("250 FPS", 4);
    miFrameLimiterField->add("333 FPS", 3);
    miFrameLimiterField->add("500 FPS", 2);
    miFrameLimiterField->add("No Limit", 0);
    miFrameLimiterField->setOutputPtr(&game_values.framelimiter);
    miFrameLimiterField->setCurrentValue(game_values.framelimiter);

    miFullscreenField = new MI_SelectField<bool>(&rm->spr_selectfield, 70, 200, "Screen Size", 500, 220);
    miFullscreenField->add("Windowed", false);
    miFullscreenField->add("Fullscreen", true);
    miFullscreenField->setOutputPtr(&game_values.fullscreen);
    miFullscreenField->setCurrentValue(game_values.fullscreen ? 1 : 0);
    miFullscreenField->setAutoAdvance(true);
    miFullscreenField->setItemChangedCode(MENU_CODE_TOGGLE_FULLSCREEN);

    miMenuGraphicsPackField = new MI_PacksField(&rm->spr_selectfield, 70, 240, "Menu Graphics", 500, 220, menugraphicspacklist, MENU_CODE_MENU_GRAPHICS_PACK_CHANGED);
    miWorldGraphicsPackField = new MI_PacksField(&rm->spr_selectfield, 70, 280, "World Graphics", 500, 220, worldgraphicspacklist, MENU_CODE_WORLD_GRAPHICS_PACK_CHANGED);
    miGameGraphicsPackField = new MI_PacksField(&rm->spr_selectfield, 70, 320, "Game Graphics", 500, 220, gamegraphicspacklist, MENU_CODE_GAME_GRAPHICS_PACK_CHANGED);

    // Pixel: the fonts of the graphics packs; otherwise the TTF/OTF fonts found in the fonts directory
    mFontFiles = CResourceManager::listFontFiles();
    miFontField = new MI_SelectField<short>(&rm->spr_selectfield, 70, 360, "Font Style", 500, 220);
    miFontField->add("Pixel", -1);
    short iSelectedFont = -1;
    for (size_t i = 0; i < mFontFiles.size(); i++) {
        const std::string name = gfxFont::fontFamilyName(convertPath("fonts/" + mFontFiles[i]));
        miFontField->add(name.empty() ? mFontFiles[i] : name, static_cast<short>(i));
        if (mFontFiles[i] == game_values.font)
            iSelectedFont = static_cast<short>(i);
    }
    miFontField->setCurrentValue(iSelectedFont);
    miFontField->setItemChangedCode(MENU_CODE_FONT_CHANGED);

    miGraphicsOptionsMenuBackButton = new MI_Button(&rm->spr_selectfield, 544, 432, "Back", 80, TextAlign::CENTER);
    miGraphicsOptionsMenuBackButton->SetCode(MENU_CODE_BACK_TO_OPTIONS_MENU);

    miGraphicsOptionsMenuLeftHeaderBar = new MI_Image(&rm->menu_plain_field, 0, 0, 0, 0, 320, 32, 1, 1, 0);
    miGraphicsOptionsMenuRightHeaderBar = new MI_Image(&rm->menu_plain_field, 320, 0, 192, 0, 320, 32, 1, 1, 0);
    miGraphicsOptionsMenuHeaderText = new MI_HeaderText("Graphics Options Menu", 320, 5);

    AddControl(miTopLayerField, miGraphicsOptionsMenuBackButton, miFrameLimiterField, NULL, miGraphicsOptionsMenuBackButton);

    AddControl(miFrameLimiterField, miTopLayerField, miFullscreenField, NULL, miGraphicsOptionsMenuBackButton);
    AddControl(miFullscreenField, miFrameLimiterField, miMenuGraphicsPackField, NULL, miGraphicsOptionsMenuBackButton);
    AddControl(miMenuGraphicsPackField, miFullscreenField, miWorldGraphicsPackField, NULL, miGraphicsOptionsMenuBackButton);

    AddControl(miWorldGraphicsPackField, miMenuGraphicsPackField, miGameGraphicsPackField, NULL, miGraphicsOptionsMenuBackButton);
    AddControl(miGameGraphicsPackField, miWorldGraphicsPackField, miFontField, NULL, miGraphicsOptionsMenuBackButton);
    AddControl(miFontField, miGameGraphicsPackField, miGraphicsOptionsMenuBackButton, NULL, miGraphicsOptionsMenuBackButton);
    AddControl(miGraphicsOptionsMenuBackButton, miFontField, miTopLayerField, miFontField, NULL);

    AddNonControl(miGraphicsOptionsMenuLeftHeaderBar);
    AddNonControl(miGraphicsOptionsMenuRightHeaderBar);
    AddNonControl(miGraphicsOptionsMenuHeaderText);

    setInitialFocus(miTopLayerField);
    SetCancelCode(MENU_CODE_BACK_TO_OPTIONS_MENU);
};

std::string UI_GraphicsOptionsMenu::selectedFont() const
{
    const short iFont = miFontField->currentValue();
    return iFont >= 0 && iFont < static_cast<short>(mFontFiles.size()) ? mFontFiles[iFont] : std::string();
}
