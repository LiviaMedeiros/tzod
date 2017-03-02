#include "MapSettings.h"
#include "ConfigBinding.h"
#include <gv/ThemeManager.h>
#include <gc/World.h>
#include <loc/Language.h>
#include <ui/Button.h>
#include <ui/Combo.h>
#include <ui/Edit.h>
#include <ui/EditableText.h>
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

MapSettingsDlg::MapSettingsDlg(TextureManager &texman, World &world/*, const ThemeManager &themeManager*/, LangCache &lang)
	: _world(world)
{
	Resize(512, 512);

	// Title
	auto text = std::make_shared<UI::Text>(texman);
	text->Move(GetWidth() / 2, 16);
	text->SetText(ConfBind(lang.map_title));
	text->SetAlign(alignTextCT);
	text->SetFont(texman, "font_default");
	AddFront(text);

	float x1 = 20;
	float x2 = x1 + 12;

	float y = 32;

	text = std::make_shared<UI::Text>(texman);
	text->Move(x1, y += 20);
	text->SetText(ConfBind(lang.map_author));
	AddFront(text);

	_author = std::make_shared<UI::Edit>(texman);
	_author->Move(x2, y += 15);
	_author->SetWidth(256);
	_author->GetEditable()->SetText(world._infoAuthor);
	AddFront(_author);

	text = std::make_shared<UI::Text>(texman);
	text->Move(x1, y += 20);
	text->SetText(ConfBind(lang.map_email));
	AddFront(text);

	_email = std::make_shared<UI::Edit>(texman);
	_email->Move(x2, y += 15);
	_email->SetWidth(256);
	_email->GetEditable()->SetText(world._infoEmail);
	AddFront(_email);

	text = std::make_shared<UI::Text>(texman);
	text->Move(x1, y += 20);
	text->SetText(ConfBind(lang.map_url));
	AddFront(text);

	_url = std::make_shared<UI::Edit>(texman);
	_url->Move(x2, y += 15);
	_url->SetWidth(256);
	_url->GetEditable()->SetText(world._infoUrl);
	AddFront(_url);

	text = std::make_shared<UI::Text>(texman);
	text->Move(x1, y += 20);
	text->SetText(ConfBind(lang.map_desc));
	AddFront(text);

	_desc = std::make_shared<UI::Edit>(texman);
	_desc->Move(x2, y += 15);
	_desc->SetWidth(256);
	_desc->GetEditable()->SetText(world._infoDesc);
	AddFront(_url);

	text = std::make_shared<UI::Text>(texman);
	text->Move(x1, y += 20);
	text->SetText(ConfBind(lang.map_init_script));
	AddFront(text);

	_onInit = std::make_shared<UI::Edit>(texman);
	_onInit->Move(x2, y += 15);
	_onInit->SetWidth(256);
	_onInit->GetEditable()->SetText(world._infoOnInit);
	AddFront(_onInit);

	text = std::make_shared<UI::Text>(texman);
	text->Move(x1, y += 20);
	text->SetText(ConfBind(lang.map_theme));
	AddFront(text);

	_theme = std::make_shared<DefaultComboBox>(texman);
	_theme->Move(x2, y += 15);
	_theme->SetWidth(256);
/*	for (size_t i = 0; i < themeManager.GetThemeCount(); i++)
	{
		_theme->GetData()->AddItem(themeManager.GetThemeName(i));
	}
	_theme->SetCurSel(FindTheme(themeManager, world._infoTheme));*/
	AddFront(_theme);


	//
	// OK & Cancel
	//
	auto btn = std::make_shared<UI::Button>(texman);
	btn->SetText(ConfBind(lang.common_ok));
	btn->Move(304, 480);
	btn->eventClick = std::bind(&MapSettingsDlg::OnOK, this);
	AddFront(btn);

	btn = std::make_shared<UI::Button>(texman);
	btn->SetText(ConfBind(lang.common_cancel));
	btn->Move(408, 480);
	btn->eventClick = std::bind(&MapSettingsDlg::OnCancel, this);
	AddFront(btn);
}

MapSettingsDlg::~MapSettingsDlg()
{
}

void MapSettingsDlg::OnOK()
{
	_world._infoAuthor = _author->GetEditable()->GetText();
	_world._infoEmail = _email->GetEditable()->GetText();
	_world._infoUrl = _url->GetEditable()->GetText();
	_world._infoDesc = _desc->GetEditable()->GetText();
	_world._infoOnInit = _onInit->GetEditable()->GetText();

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
