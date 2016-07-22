#include "MapSettings.h"
#include <gv/ThemeManager.h>
#include <gc/World.h>
#include <loc/Language.h>
#include <ui/Button.h>
#include <ui/Combo.h>
#include <ui/Edit.h>
#include <ui/DataSourceAdapters.h>
#include <ui/Text.h>
#include <ui/List.h>

static size_t FindTheme(const ThemeManager &themeManager, const std::string &name)
{
	for (size_t i = 0; i < themeManager.GetThemeCount(); ++i)
	{
		if (themeManager.GetThemeName(i) == name)
		{
			return i;
		}
	}
	return 0;
}

MapSettingsDlg::MapSettingsDlg(UI::LayoutManager &manager, TextureManager &texman, World &world/*, const ThemeManager &themeManager*/, LangCache &lang)
	: Dialog(manager, texman, 512, 512)
	, _world(world)
{
	SetEasyMove(true);

	// Title
	auto text = std::make_shared<UI::Text>(manager, texman);
	text->Move(GetWidth() / 2, 16);
	text->SetText(texman, lang.map_title.Get());
	text->SetAlign(alignTextCT);
	text->SetFont(texman, "font_default");
	AddFront(text);

	float x1 = 20;
	float x2 = x1 + 12;

	float y = 32;

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, y += 20);
	text->SetText(texman, lang.map_author.Get());
	AddFront(text);

	_author = std::make_shared<UI::Edit>(manager, texman);
	_author->Move(x2, y += 15);
	_author->SetWidth(256);
	_author->SetText(texman, world._infoAuthor);
	AddFront(_author);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, y += 20);
	text->SetText(texman, lang.map_email.Get());
	AddFront(text);

	_email = std::make_shared<UI::Edit>(manager, texman);
	_email->Move(x2, y += 15);
	_email->SetWidth(256);
	_email->SetText(texman, world._infoEmail);
	AddFront(_email);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, y += 20);
	text->SetText(texman, lang.map_url.Get());
	AddFront(text);

	_url = std::make_shared<UI::Edit>(manager, texman);
	_url->Move(x2, y += 15);
	_url->SetWidth(256);
	_url->SetText(texman, world._infoUrl);
	AddFront(_url);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, y += 20);
	text->SetText(texman, lang.map_desc.Get());
	AddFront(text);

	_desc = std::make_shared<UI::Edit>(manager, texman);
	_desc->Move(x2, y += 15);
	_desc->SetWidth(256);
	_desc->SetText(texman, world._infoDesc);
	AddFront(_url);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, y += 20);
	text->SetText(texman, lang.map_init_script.Get());
	AddFront(text);

	_onInit = std::make_shared<UI::Edit>(manager, texman);
	_onInit->Move(x2, y += 15);
	_onInit->SetWidth(256);
	_onInit->SetText(texman, world._infoOnInit);
	AddFront(_onInit);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, y += 20);
	text->SetText(texman, lang.map_theme.Get());
	AddFront(text);

	_theme = std::make_shared<DefaultComboBox>(manager, texman);
	_theme->Move(x2, y += 15);
	_theme->Resize(256);
/*	for (size_t i = 0; i < themeManager.GetThemeCount(); i++)
	{
		_theme->GetData()->AddItem(themeManager.GetThemeName(i));
	}
	_theme->SetCurSel(FindTheme(themeManager, world._infoTheme));*/
	AddFront(_theme);


	//
	// OK & Cancel
	//
	auto btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, lang.common_ok.Get());
	btn->Move(304, 480);
	btn->eventClick = std::bind(&MapSettingsDlg::OnOK, this);
	AddFront(btn);

	btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, lang.common_cancel.Get());
	btn->Move(408, 480);
	btn->eventClick = std::bind(&MapSettingsDlg::OnCancel, this);
	AddFront(btn);
}

MapSettingsDlg::~MapSettingsDlg()
{
}

void MapSettingsDlg::OnOK()
{
	_world._infoAuthor = _author->GetText();
	_world._infoEmail = _email->GetText();
	_world._infoUrl = _url->GetText();
	_world._infoDesc = _desc->GetText();
	_world._infoOnInit = _onInit->GetText();

	int i = _theme->GetCurSel();
	if (0 != i)
	{
		_world._infoTheme = _theme->GetData()->GetItemText(i, 0);
	}
	else
	{
		_world._infoTheme.clear();
	}

	Close(_resultOK);
}

void MapSettingsDlg::OnCancel()
{
	Close(_resultCancel);
}
