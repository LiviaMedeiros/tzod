// gui.cpp

#include "stdafx.h"
#include "gui.h"
#include "GuiManager.h"

#include "functions.h"
#include "options.h"
#include "level.h"
#include "macros.h"

#include "video/TextureManager.h"

#include "fs/FileSystem.h"
#include "fs/MapFile.h"
#include "gc/Player.h"



namespace UI
{
///////////////////////////////////////////////////////////////////////////////

MainMenu::MainMenu(Window *parent) : Window(parent, 0, 0, "gui_splash")
{
//	ClipChildren(true);
	OnParentSize(parent->GetWidth(), parent->GetHeight());

	(new Button(this,   0, 256, "����� ����"))->eventClick.bind(&MainMenu::OnNewGame, this);
//	(new Button(this,  96, 256, "���������"))->eventClick.bind(&MainMenu::OnNewGame, this);
//	(new Button(this, 192, 256, "���������"))->eventClick.bind(&MainMenu::OnNewGame, this);
//	(new Button(this, 288, 256, "���������"))->eventClick.bind(&MainMenu::OnNewGame, this);
	(new Button(this, 416, 256, "�����"))->eventClick.bind(&MainMenu::OnExit, this);
}

void MainMenu::OnNewGame()
{
	NewGameDlg *dlg = new NewGameDlg(this);
	dlg->eventClose.bind(&MainMenu::OnCloseChild, this);
}

void MainMenu::OnExit()
{
	DestroyWindow(g_env.hMainWnd);
}

void MainMenu::OnParentSize(float width, float height)
{
	Move( (width - GetWidth()) * 0.5f, (height - GetHeight()) * 0.5f );
}

void MainMenu::OnCloseChild(int result)
{
	if( Dialog::_resultOK == result )
	{
		Destroy();
	}
}

void MainMenu::OnRawChar(int c)
{
	if( VK_ESCAPE == c )
	{
		Destroy();
	}
}

bool MainMenu::OnFocus(bool focus)
{
	return true;
}


///////////////////////////////////////////////////////////////////////////////

NewGameDlg::NewGameDlg(Window *parent)
  : Dialog(parent, 0, 0, 768, 512, true)
{
	Move( (parent->GetWidth() - GetWidth()) / 2,
		(parent->GetHeight() - GetHeight()) / 2 );


	//
	// map list
	//

	_maps = new List(this, 16, 16, 448, 224);
	_maps->SetTabPos(0,   4); // name
	_maps->SetTabPos(1, 256); // size
	_maps->SetTabPos(2, 320); // theme
	GetManager()->SetFocusWnd(_maps);


	SafePtr<IFileSystem> dir = g_fs->GetFileSystem(DIR_MAPS);
	if( dir )
	{
		std::set<string_t> files;
		if( dir->EnumAllFiles(files, TEXT("*.map")) )
		{
			int lastMapIndex = -1;

			for( std::set<string_t>::iterator it = files.begin(); it != files.end(); ++it )
			{
				string_t tmp = DIR_MAPS;
				tmp += "/";
				tmp += *it;

				MapFile file;
				if( file.Open(tmp.c_str(), false) )
				{
					it->erase(it->length() - 4); // cut out the file extension
					int index = _maps->AddItem(it->c_str());

					if( *it == OPT(cMapName) )
						lastMapIndex = index;

					char size[64];
					int h = 0, w = 0;
					file.getMapAttribute("width", w);
					file.getMapAttribute("height", h);
					wsprintf(size, "%3d*%d", w, h);
					_maps->SetItemText(index, 1, size);

					if( file.getMapAttribute("theme", tmp) )
					{
						_maps->SetItemText(index, 2, tmp.c_str());
					}
				}
			}

			_maps->SetCurSel(lastMapIndex, true);
		}
		else
		{
			_ASSERT(FALSE); // EnumAllFiles has returned error...
		}
	}


	//
	// settings
	//

	{
		float x = 480;
		float y =  16;

		_nightMode = new CheckBox(this, x, y, "������ �����");
		_nightMode->SetCheck( OPT(bNightMode) );


		new Text(this, x, y+=30, "�������� ����, %", alignTextLT);
		_gameSpeed = new Edit(this, x+20, y+=15, 80);
		_gameSpeed->SetInt(OPT(gameSpeed));

		new Text(this, x, y+=30, "����� ������", alignTextLT);
		_fragLimit = new Edit(this, x+20, y+=15, 80);
		_fragLimit->SetInt(OPT(fraglimit));

		new Text(this, x, y+=30, "����� �������", alignTextLT);
		_timeLimit = new Edit(this, x+20, y+=15, 80);
		_timeLimit->SetInt(OPT(timelimit));

		new Text(this, x+30, y+=40, "(0 - ��� ������)", alignTextLT);
	}



	//
	// players
	//

	_players = new List(this, 16, 256, 448, 128);
	_players->SetTabPos(0,   4); // name
	_players->SetTabPos(1, 192); // skin
	_players->SetTabPos(2, 256); // class
	_players->SetTabPos(3, 320); // team

	RefreshPlayersList();


	Button *btn;

	btn = new Button(this, 480, 256, "��������");
	btn->eventClick.bind(&NewGameDlg::OnAddPlayer, this);
//	btn->Enable(false);

	btn = new Button(this, 480, 286, "�������");
	btn->eventClick.bind(&NewGameDlg::OnAddPlayer, this);
	btn->Enable(false);

	btn = new Button(this, 480, 316, "��������");
	btn->eventClick.bind(&NewGameDlg::OnAddPlayer, this);
	btn->Enable(false);


	(new Button(this, 544, 472, "�������!"))->eventClick.bind(&NewGameDlg::OnOK, this);
	(new Button(this, 656, 472, "������"))->eventClick.bind(&NewGameDlg::OnCancel, this);
}

void NewGameDlg::RefreshPlayersList()
{
	_players->DeleteAllItems();

	for( int i = 0; i < OPT(dm_nPlayers); ++i )
	{
		int index = _players->AddItem(  OPT(dm_pdPlayers)[i].name);
		_players->SetItemText(index, 1, OPT(dm_pdPlayers)[i].skin);
		_players->SetItemText(index, 2, OPT(dm_pdPlayers)[i].cls);

		char s[16];
		if( 0 != OPT(dm_pdPlayers)[i].team ) 
			wsprintf(s, "%d", OPT(dm_pdPlayers)[i].team);
		else wsprintf(s, "[���]");

		_players->SetItemText(index, 3, s);
	}
}

void NewGameDlg::OnAddPlayer()
{
	new EditPlayerDlg(this);
}

void NewGameDlg::OnRemovePlayer()
{
	new EditPlayerDlg(this);
}

void NewGameDlg::OnEditPlayer()
{
	new EditPlayerDlg(this);
}

void NewGameDlg::OnClosePlayerDlg(int result)
{
	if( _resultOK == result )
	{
		RefreshPlayersList();
	}
}

void NewGameDlg::OnOK()
{
	string_t fn;
	int index = _maps->GetCurSel();
	if( -1 != index )
	{
		fn = _maps->GetItemText(index, 0);
	}
	else
	{
//		MessageBoxT(NULL, "�������� �����", MB_OK|MB_ICONHAND);
		return;
	}

	string_t path = DIR_MAPS;
	path += "\\";
	path += fn + ".map";

	OPT(gameSpeed)  = __max(MIN_GAMESPEED, __min(MAX_GAMESPEED, _gameSpeed->GetInt()));
	OPT(fraglimit)  = __max(0, __min(MAX_FRAGLIMIT, _fragLimit->GetInt()));
	OPT(timelimit)  = __max(0, __min(MAX_TIMELIMIT, _timeLimit->GetInt()));
	OPT(bNightMode) = _nightMode->GetCheck();

	SAFE_DELETE(g_level);
	g_level = new Level();

	if( g_level->init_newdm(path.c_str()) )
	{
		g_level->_seed = rand();
		strcpy(OPT(cMapName), fn.c_str());

		// ��������� ������� � �������� �������
		for( int i = OPT(dm_nPlayers) - 1; i >= 0; --i )
		{
			GC_Player *player = new GC_Player(g_options.dm_pdPlayers[i].team);
			player->SetController( g_options.dm_pdPlayers[i].type);
			strcpy(player->_name,  g_options.dm_pdPlayers[i].name);
			strcpy(player->_skin,  g_options.dm_pdPlayers[i].skin);
			strcpy(player->_class, g_options.dm_pdPlayers[i].cls);
		}
	}
	else
	{
		SAFE_DELETE(g_level);
//		MessageBoxT(NULL, "������ ��� �������� �����", MB_OK|MB_ICONHAND);
		return;
	}

//	g_gui->Show(false);
	Close(_resultOK);
}

void NewGameDlg::OnCancel()
{
	Close(_resultCancel);
}

bool NewGameDlg::OnFocus(bool focus)
{
	return Dialog::OnFocus(focus);
}

///////////////////////////////////////////////////////////////////////////////

EditPlayerDlg::EditPlayerDlg(Window *parent)
  : Dialog(parent, 0, 0, 512, 256)
{
	Move((parent->GetWidth() - GetWidth()) * 0.5f,
	     (parent->GetHeight() - GetHeight()) * 0.5f);
	SetEasyMove(true);

	float x = 64;
	float y =  8;

	new Text(this, 8, y, "���", alignTextLT);
	new Edit(this, x, y-=1, 120);

	new Text(this, 8, y+=20, "���", alignTextLT);
	new ComboBox(this, x, y-=1, 120);

	new Text(this, 8, y+=20, "����", alignTextLT);
	new ComboBox(this, x, y-=1, 120);

	new Text(this, 8, y+=20, "�����", alignTextLT);
	new ComboBox(this, x, y-=1, 120);

	(new Button(this, 408, 192, "OK"))->eventClick.bind(&EditPlayerDlg::OnOk, this);
	(new Button(this, 408, 224, "������"))->eventClick.bind(&EditPlayerDlg::OnCancel, this);
}

void EditPlayerDlg::OnOk()
{
//	Close(_resultOK);

	new SkinSelectorDlg(this);

}

void EditPlayerDlg::OnCancel()
{
	Close(_resultCancel);
}

///////////////////////////////////////////////////////////////////////////////

SkinSelectorDlg::SkinSelectorDlg(Window *parent)
  : Dialog(parent, 0, 0, 512, 256)
{
	std::vector<string_t> names;
	g_texman->GetTextureNames(names, TEXT("skin/"));

	float x = 2;
	float y = 2;

	for( size_t i = 0; i < names.size(); ++i )
	{
		Window *wnd = new Window(this, x, y, names[i].c_str());
		x += wnd->GetWidth() + 4;
		if( x > GetWidth() )
		{
			y += wnd->GetHeight() + 4;
			x = 2;
		}
	}
}

void SkinSelectorDlg::OnOk()
{
	Close(_resultOK);
}

void SkinSelectorDlg::OnCancel()
{
	Close(_resultCancel);
}


///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file
