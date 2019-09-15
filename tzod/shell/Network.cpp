#include "gui.h"
#include "MapList.h"
#include "Network.h"
#include "inc/shell/Config.h"

//#include "md5.h"

//#include "network/TankServer.h"
//#include "network/TankClient.h"
//#include "network/LobbyClient.h"

#include <as/AppCfg.h>
#include <gc/Player.h>
#include <gc/World.h>
#include <gc/Macros.h>
#include <fs/FileSystem.h>
#include <loc/Language.h>
#include <plat/ConsoleBuffer.h>
#include <ui/Button.h>
#include <ui/Edit.h>
#include <ui/Combo.h>
#include <ui/Console.h>
#include <ui/DataSourceAdapters.h>
#include <ui/GuiManager.h>
#include <ui/Text.h>


#define MAX_NETWORKSPEED    60
#define MIN_NETWORKSPEED    10
#define AI_MAX_LEVEL   4U


CreateServerDlg::CreateServerDlg(UI::LayoutManager &manager, TextureManager &texman, World &world, FS::FileSystem &fs, ShellConfig &conf, LangCache &lang, Plat::ConsoleBuffer &logger)
  : Dialog(manager, texman, 770, 450)
  , _world(world)
  , _fs(fs)
  , _conf(conf)
  , _lang(lang)
  , _logger(logger)
{
	// Title
	auto text = std::make_shared<UI::Text>(manager, texman);
	text->Move(GetWidth() / 2, 16);
	text->SetText(texman, _lang.net_server_title.Get());
	text->SetAlign(alignTextCT);
	text->SetFont(texman, "font_default");
	AddFront(text);

	float x1 = 16;
	float x2 = x1 + 550;
	float x3 = x2 + 20;
	float x4 = x3 + 20;

	//
	// map list
	//

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(x1, 46);
	text->SetText(texman, _lang.choose_map.Get());
	AddFront(text);

	_maps = std::make_shared<MapListBox>(manager, texman, fs, _logger);
	_maps->Move(x1, 62);
	_maps->Resize(x2 - x1, 300);
	_maps->SetTabPos(0,   4); // name
	_maps->SetTabPos(1, 384); // size
	_maps->SetTabPos(2, 448); // theme
	_maps->SetCurSel(_maps->GetData()->FindItem(_conf.cl_map.Get()), false);
	_maps->SetScrollPos(_maps->GetCurSel() - (_maps->GetNumLinesVisible() - 1) * 0.5f);
	AddFront(_maps);
	SetFocus(_maps);


	//
	// settings
	//

	{
		float y =  56;

		_nightMode = std::make_shared<UI::CheckBox>(manager, texman);
		_nightMode->Move(x3, y);
		_nightMode->SetText(texman, _lang.night_mode.Get());
//		_nightMode->SetCheck( _conf.cl_nightmode.Get() );
		AddFront(_nightMode);


		text = std::make_shared<UI::Text>(manager, texman);
		text->Move(x3, y += 30);
		text->SetText(texman, _lang.game_speed.Get());
		AddFront(text);

		_gameSpeed = std::make_shared<UI::Edit>(manager, texman);
		_gameSpeed->Move(x4, y += 15);
		_gameSpeed->SetWidth(80);
		_gameSpeed->SetInt(_conf.cl_speed.GetInt());
		AddFront(_gameSpeed);

		text = std::make_shared<UI::Text>(manager, texman);
		text->Move(x3, y += 30);
		text->SetText(texman, _lang.frag_limit.Get());
		AddFront(text);

		_fragLimit = std::make_shared<UI::Edit>(manager, texman);
		_fragLimit->Move(x4, y += 15);
		_fragLimit->SetWidth(80);
//		_fragLimit->SetInt(_conf.cl_fraglimit.GetInt());
		AddFront(_fragLimit);

		text = std::make_shared<UI::Text>(manager, texman);
		text->Move(x3, y += 30);
		text->SetText(texman, _lang.time_limit.Get());
		AddFront(text);

		_timeLimit = std::make_shared<UI::Edit>(manager, texman);
		_timeLimit->Move(x4, y += 15);
		_timeLimit->SetWidth(80);
//		_timeLimit->SetInt(_conf.cl_timelimit.GetInt());
		AddFront(_timeLimit);

		text = std::make_shared<UI::Text>(manager, texman);
		text->Move(x3 + 30, y += 30);
		text->SetText(texman, _lang.zero_no_limits.Get());
		AddFront(text);

		text = std::make_shared<UI::Text>(manager, texman);
		text->Move(x3, y += 40);
		text->SetText(texman, _lang.net_server_fps.Get());
		AddFront(text);

		_svFps = std::make_shared<UI::Edit>(manager, texman);
		_svFps->Move(x4, y += 15);
		_svFps->SetWidth(100);
		_svFps->SetInt(_conf.sv_fps.GetInt());
		AddFront(_svFps);
	}


	//
	// Lobby
	//
	{
		_lobbyEnable = std::make_shared<UI::CheckBox>(manager, texman);
		_lobbyEnable->Move(32, 390);
		_lobbyEnable->SetText(texman, _lang.net_server_use_lobby.Get());
		_lobbyEnable->SetCheck(_conf.sv_use_lobby.Get());
		AddFront(_lobbyEnable);

		_lobbyList = std::make_shared<DefaultComboBox>(manager, texman);
		_lobbyList->Move(32, 415);
		_lobbyList->Resize(200);
		_lobbyList->SetEnabled(_conf.sv_use_lobby.Get());
		AddFront(_lobbyList);

		_lobbyAdd = std::make_shared<UI::Button>(manager, texman);
		_lobbyAdd->SetText(texman, _lang.net_server_add_lobby.Get());
		_lobbyAdd->Move(250, 410);
		_lobbyAdd->SetEnabled(_conf.sv_use_lobby.Get());
		AddFront(_lobbyAdd);

		for( size_t i = 0; i < _conf.lobby_servers.GetSize(); ++i )
		{
			const std::string &lobbyAddr = _conf.lobby_servers.GetStr(i).Get();
			_lobbyList->GetData()->AddItem(lobbyAddr);
			if( !i || _conf.sv_lobby.Get() == lobbyAddr )
			{
				_lobbyList->SetCurSel(i);
			}
		}
		_lobbyList->GetList()->AlignHeightToContent(128);

		_lobbyEnable->eventClick = std::bind(&CreateServerDlg::OnLobbyEnable, this);
	}

	auto btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, _lang.net_server_ok.Get());
	btn->Move(544, 410);
	btn->eventClick = std::bind(&CreateServerDlg::OnOK, this);
	AddFront(btn);

	btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, _lang.net_server_cancel.Get());
	btn->Move(656, 410);
	btn->eventClick = std::bind(&CreateServerDlg::OnCancel, this);
	AddFront(btn);
}

CreateServerDlg::~CreateServerDlg()
{
}

void CreateServerDlg::OnOK()
{
	std::string fn;
	int index = _maps->GetCurSel();
	if( -1 != index )
	{
		fn = _maps->GetData()->GetItemText(index, 0);
	}
	else
	{
		return;
	}

//	std::shared_ptr<LobbyClient> announcer;
	_conf.sv_use_lobby.Set(_lobbyEnable->GetCheck());
	if( _lobbyEnable->GetCheck() )
	{
		if( -1 == _lobbyList->GetCurSel() )
		{
			return;
		}
		_conf.sv_lobby.Set(_lobbyList->GetData()->GetItemText(_lobbyList->GetCurSel(), 0));
//		announcer = new LobbyClient();
//		announcer->SetLobbyUrl(_conf.sv_lobby.Get());
	}

//	SAFE_DELETE(g_client);


	std::string path = DIR_MAPS;
	path += "/";
	path += fn + ".map";

//	GameInfo gi;
//    memset(&gi, 0, sizeof(gi));
//	gi.seed       = rand();
//	gi.fraglimit  = std::max(0, std::min(MAX_FRAGLIMIT, _fragLimit->GetInt()));
//	gi.timelimit  = std::max(0, std::min(MAX_TIMELIMIT, _timeLimit->GetInt()));
//	gi.server_fps = std::max(MIN_NETWORKSPEED, std::min(MAX_NETWORKSPEED, _svFps->GetInt()));
//	gi.nightmode  = _nightMode->GetCheck();
//	strcpy(gi.cMapName, fn.c_str());
//	strcpy(gi.cServerName, "ZOD Server");

	try
	{
		throw std::runtime_error("not implemented");

		//std::shared_ptr<FS::MemMap> m = _fs.Open(path)->QueryMap();
		//MD5_CTX md5;
		//MD5Init(&md5);
		//MD5Update(&md5, m->GetData(), m->GetSize());
		//MD5Final(&md5);
//		memcpy(gi.mapVer, md5.digest, 16);

	//	g_server = new TankServer(gi, announcer); // integrate into client instead
	}
	catch( const std::exception & )
	{
		throw;
//		_logger.Printf(1, "%s", e.what());
//		MessageBox(g_env.hMainWnd, _lang.net_server_error.Get().c_str(), TXT_VERSION, MB_OK|MB_ICONERROR);
		return;
	}

//	_conf.sv_latency.SetInt(1);

//	assert(false); // dont connect; integrate server into client instead
//	(new ConnectDlg(GetParent()))->eventClose = std::bind(&CreateServerDlg::OnCloseChild, this, _1, std::placeholders::_2);

//	PauseGame(true);


//	SAFE_DELETE(g_client);

//	assert(_world.IsEmpty());
//	new TankClient(_world);

	auto dlg = std::make_shared<WaitingForPlayersDlg>(GetManager(), GetManager().GetTextureManager(), _world, _conf, _lang);
	dlg->eventClose = std::bind(&CreateServerDlg::OnCloseChild, this, std::placeholders::_1, std::placeholders::_2);
	//GetParent()->
		AddFront(dlg);

	SetVisible(false);
}

void CreateServerDlg::OnCancel()
{
	Close(_resultCancel);
}

void CreateServerDlg::OnLobbyEnable()
{
	_lobbyList->SetEnabled(_lobbyEnable->GetCheck());
	_lobbyAdd->SetEnabled(_lobbyEnable->GetCheck());
}

void CreateServerDlg::OnCloseChild(std::shared_ptr<UI::Dialog> sender, int result)
{
	if( _resultCancel == result )
	{
		SetVisible(true);
	}

	if( _resultOK == result )
	{
		Close(_resultOK);
	}
	UnlinkChild(*sender);
}

///////////////////////////////////////////////////////////////////////////////

ConnectDlg::ConnectDlg(UI::LayoutManager &manager, TextureManager &texman, const std::string &defaultName, World &world, ShellConfig &conf, LangCache &lang)
  : Dialog(manager, texman, 512, 384)
  , _world(world)
  , _conf(conf)
  , _lang(lang)
{
	// Title
	auto text = std::make_shared<UI::Text>(manager, texman);
	text->Move(GetWidth() / 2, 16);
	text->SetText(texman, _lang.net_connect_title.Get());
	text->SetAlign(alignTextCT);
	text->SetFont(texman, "font_default");
	AddFront(text);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 65);
	text->SetText(texman, _lang.net_connect_address.Get());
	AddFront(text);

	_name = std::make_shared<UI::Edit>(manager, texman);
	_name->Move(25, 80);
	_name->SetWidth(300);
	_name->SetText(texman, defaultName);
	AddFront(_name);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 105);
	text->SetText(texman, _lang.net_connect_status.Get());
	AddFront(text);

	_status = std::make_shared<DefaultListBox>(manager, texman);
	_status->Move(25, 120);
	_status->Resize(400, 180);
	AddFront(_status);

	_btnOK = std::make_shared<UI::Button>(manager, texman);;
	_btnOK->SetText(texman, _lang.net_connect_ok.Get());
	_btnOK->Move(312, 350);
	_btnOK->eventClick = std::bind(&ConnectDlg::OnOK, this);
	AddFront(_btnOK);

	auto btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, _lang.net_connect_cancel.Get());
	btn->Move(412, 350);
	btn->eventClick = std::bind(&ConnectDlg::OnCancel, this);
	AddFront(btn);

	SetFocus(_name);
}

ConnectDlg::~ConnectDlg()
{
}

void ConnectDlg::OnOK()
{
	_status->GetData()->DeleteAllItems();

	_btnOK->SetEnabled(false);
	_name->SetEnabled(false);

//	SAFE_DELETE(g_client);

//	assert(_world.IsEmpty());
//	TankClient *cl = new TankClient(_world);
//	_clientSubscribtion = cl->AddListener(this);
//	cl->Connect(_name->GetText());
}

void ConnectDlg::OnCancel()
{
	Close(_resultCancel);
}

void ConnectDlg::OnConnected()
{
	_conf.cl_server.Set(_name->GetText());
	auto dlg = std::make_shared<WaitingForPlayersDlg>(GetManager(), GetManager().GetTextureManager(), _world, _conf, _lang);
	dlg->eventClose = eventClose;
	//GetParent()->
		AddFront(dlg);
	Close(-1); // close with any code except ok and cancel
}

void ConnectDlg::OnErrorMessage(const std::string &msg)
{
	_status->GetData()->AddItem(msg);
//	SAFE_DELETE(g_client);
	_btnOK->SetEnabled(true);
	_name->SetEnabled(true);
}

void ConnectDlg::OnTextMessage(const std::string &msg)
{
	_status->GetData()->AddItem(msg);
}

void ConnectDlg::OnClientDestroy()
{
//	_clientSubscribtion.reset();
}

///////////////////////////////////////////////////////////////////////////////

InternetDlg::InternetDlg(UI::LayoutManager &manager, TextureManager &texman, World &world, ShellConfig &conf, LangCache &lang)
  : Dialog(manager, texman, 450, 384)
  , _world(world)
  , _conf(conf)
  , _lang(lang)
//  , _client(new LobbyClient())
{
//	_client->eventError.bind(&InternetDlg::OnLobbyError, this);
//	_client->eventServerListReply.bind(&InternetDlg::OnLobbyList, this);

	// Title
	auto text = std::make_shared<UI::Text>(manager, texman);
	text->Move(GetWidth() / 2, 16);
	text->SetText(texman, _lang.net_internet_title.Get());
	text->SetAlign(alignTextCT);
	text->SetFont(texman, "font_default");
	AddFront(text);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 65);
	text->SetText(texman, _lang.net_internet_lobby_address.Get());
	AddFront(text);

	_name = std::make_shared<UI::Edit>(manager, texman);
	_name->Move(25, 80);
	_name->SetWidth(300);
	_name->SetText(texman, "tzod.fatal.ru/lobby/");
	AddFront(_name);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 105);
	text->SetText(texman, _lang.net_internet_server_list.Get());
	AddFront(text);

	_servers = std::make_shared<DefaultListBox>(manager, texman);
	_servers->Move(25, 120);
	_servers->Resize(400, 180);
	_servers->eventChangeCurSel = std::bind(&InternetDlg::OnSelectServer, this, std::placeholders::_1);
	AddFront(_servers);

	_status = std::make_shared<UI::Text>(manager, texman);
	_status->Move(_servers->GetWidth() / 2, _servers->GetHeight() / 2);
	_status->SetAlign(alignTextCC);
	_status->SetFontColor(0x7f7f7f7f);
	_servers->AddFront(_status);

	_btnRefresh = std::make_shared<UI::Button>(manager, texman);
	_btnRefresh->SetText(texman, _lang.net_internet_refresh.Get());
	_btnRefresh->Move(25, 320);
	_btnRefresh->eventClick = std::bind(&InternetDlg::OnRefresh, this);
	AddFront(_btnRefresh);

	_btnConnect = std::make_shared<UI::Button>(manager, texman);
	_btnConnect->SetText(texman, _lang.net_internet_connect.Get());
	_btnConnect->Move(175, 320);
	_btnConnect->eventClick = std::bind(&InternetDlg::OnConnect, this);
	_btnConnect->SetEnabled(false);
	AddFront(_btnConnect);

	auto btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, _lang.net_internet_cancel.Get());
	btn->Move(325, 320);
	btn->eventClick = std::bind(&InternetDlg::OnCancel, this);
	AddFront(btn);

	SetFocus(_name);

	OnRefresh();
}

InternetDlg::~InternetDlg()
{
}

void InternetDlg::OnRefresh()
{
	_servers->GetData()->DeleteAllItems();
	_status->SetText(GetManager().GetTextureManager(), _lang.net_internet_searching.Get());

	_btnRefresh->SetEnabled(false);
	_name->SetEnabled(false);

//	_client->SetLobbyUrl(_name->GetText());
//	_client->RequestServerList();
}

void InternetDlg::OnConnect()
{
	if( -1 != _servers->GetCurSel() )
	{
		const std::string &addr = _servers->GetData()->GetItemText(_servers->GetCurSel(), 0);
		auto dlg = std::make_shared<ConnectDlg>(GetManager(), GetManager().GetTextureManager(), addr, _world, _conf, _lang);
		dlg->eventClose = std::bind(&InternetDlg::OnCloseChild, this, std::placeholders::_1, std::placeholders::_2);
		//GetParent()->
			AddFront(dlg);
		SetVisible(false);
	}
}

void InternetDlg::OnCancel()
{
	Close(_resultCancel);
}

void InternetDlg::OnSelectServer(int idx)
{
	_btnConnect->SetEnabled(-1 != idx);
}

void InternetDlg::OnLobbyError(const std::string &msg)
{
	Error(msg.c_str());
}

void InternetDlg::OnLobbyList(const std::vector<std::string> &result)
{
	_status->SetText(GetManager().GetTextureManager(), result.empty() ? _lang.net_internet_not_found.Get() : "");
	for( size_t i = 0; i < result.size(); ++i )
	{
		_servers->GetData()->AddItem(result[i]);
	}
	_btnRefresh->SetEnabled(true);
	_name->SetEnabled(true);
}

void InternetDlg::Error(const char *msg)
{
	_status->SetText(GetManager().GetTextureManager(), msg);
	_btnRefresh->SetEnabled(true);
	_name->SetEnabled(true);
}

void InternetDlg::OnCloseChild(std::shared_ptr<UI::Dialog> sender, int result)
{
	if( _resultCancel == result )
	{
		SetVisible(true);
	}

	if( _resultOK == result )
	{
		Close(_resultOK);
	}
	UnlinkChild(*sender);
}

///////////////////////////////////////////////////////////////////////////////

WaitingForPlayersDlg::WaitingForPlayersDlg(UI::LayoutManager &manager, TextureManager &texman, World &world, ShellConfig &conf, LangCache &lang)
  : Dialog(manager, texman, 680, 512)
  , _players(nullptr)
  , _bots(nullptr)
  , _chat(nullptr)
  , _btnOK(nullptr)
  , _btnProfile(nullptr)
  , _buf(new Plat::ConsoleBuffer(80, 500))
  , _world(world)
  , _conf(conf)
  , _lang(lang)
//  , _clientSubscribtion(g_client->AddListener(this))
{
	// Title
	auto text = std::make_shared<UI::Text>(manager, texman);
	text->Move(GetWidth() / 2, 16);
	text->SetText(texman, _lang.net_chatroom_title.Get());
	text->SetAlign(alignTextCT);
	text->SetFont(texman, "font_default");
	AddFront(text);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 50);
	text->SetText(texman, _lang.net_chatroom_players.Get());
	AddFront(text);

	_players = std::make_shared<DefaultListBox>(manager, texman);
	_players->Move(20, 65);
	_players->Resize(512, 70);
	_players->SetTabPos(1, 200);
	_players->SetTabPos(2, 300);
	_players->SetTabPos(3, 400);
	AddFront(_players);

	_btnProfile = std::make_shared<UI::Button>(manager, texman);
	_btnProfile->SetText(texman, _lang.net_chatroom_my_profile.Get());
	_btnProfile->Move(560, 65);
	_btnProfile->eventClick = std::bind(&WaitingForPlayersDlg::OnChangeProfileClick, this);
	AddFront(_btnProfile);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 150);
	text->SetText(texman, _lang.net_chatroom_bots.Get());
	AddFront(text);

	_bots = std::make_shared<DefaultListBox>(manager, texman);
	_bots->Move(20, 165);
	_bots->Resize(512, 100);
	_bots->SetTabPos(1, 200);
	_bots->SetTabPos(2, 300);
	_bots->SetTabPos(3, 400);
	AddFront(_bots);

	auto btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, _lang.net_chatroom_bot_new.Get());
	btn->Move(560, 180);
	btn->eventClick = std::bind(&WaitingForPlayersDlg::OnAddBotClick, this);
	AddFront(btn);

	text = std::make_shared<UI::Text>(manager, texman);
	text->Move(20, 285);
	text->SetText(texman, _lang.net_chatroom_chat_window.Get());
	AddFront(text);

	_chat = UI::Console::Create(this, texman, 20, 300, 512, 200, _buf.get());
	_chat->SetTexture(texman, "ui/list", false);
	_chat->SetEcho(false);
	_chat->eventOnSendCommand = std::bind(&WaitingForPlayersDlg::OnSendMessage, this, std::placeholders::_1);


	_btnOK = std::make_shared<UI::Button>(manager, texman);
	_btnOK->SetText(texman, _lang.net_chatroom_ready_button.Get());
	_btnOK->Move(560, 450);
	_btnOK->eventClick = std::bind(&WaitingForPlayersDlg::OnOK, this);
	_btnOK->SetEnabled(false);
	AddFront(_btnOK);

	btn = std::make_shared<UI::Button>(manager, texman);
	btn->SetText(texman, _lang.common_cancel.Get());
	btn->Move(560, 480);
	btn->eventClick = std::bind(&WaitingForPlayersDlg::OnCancel, this);
	AddFront(btn);


	//
	// send player info
	//

//	dynamic_cast<TankClient*>(g_client)->SendPlayerInfo(GetPlayerDescFromConf(_conf.cl_playerinfo));

	// send ping request
//	DWORD t = timeGetTime();
//	g_client->SendDataToServer(DataWrap(t, DBTYPE_PING));
}

WaitingForPlayersDlg::~WaitingForPlayersDlg()
{
}

void WaitingForPlayersDlg::OnCloseProfileDlg(std::shared_ptr<UI::Dialog> sender, int result)
{
	_btnProfile->SetEnabled(true);
	_btnOK->SetEnabled(true);

	if( _resultOK == result )
	{
//		dynamic_cast<TankClient*>(g_client)->SendPlayerInfo(GetPlayerDescFromConf(_conf.cl_playerinfo));
	}
	UnlinkChild(*sender);
}

void WaitingForPlayersDlg::OnChangeProfileClick()
{
	_btnProfile->SetEnabled(false);
	_btnOK->SetEnabled(false);
	auto dlg = std::make_shared<EditPlayerDlg>(GetManager(), GetManager().GetTextureManager(), _conf.cl_playerinfo, _conf, _lang);
	dlg->eventClose = std::bind(&WaitingForPlayersDlg::OnCloseProfileDlg, this, std::placeholders::_1, std::placeholders::_2);
	//GetParent()->
		AddFront(dlg);
}

void WaitingForPlayersDlg::OnAddBotClick()
{
	auto dlg = std::make_shared<EditBotDlg>(GetManager(), GetManager().GetTextureManager(), _conf.ui_netbotinfo, _lang);
	dlg->eventClose = std::bind(&WaitingForPlayersDlg::OnAddBotClose, this, std::placeholders::_1, std::placeholders::_2);
	AddFront(dlg);
}

void WaitingForPlayersDlg::OnAddBotClose(std::shared_ptr<UI::Dialog> sender, int result)
{
	if( _resultOK == result )
	{
//		BotDesc bd;
//		bd.pd = GetPlayerDescFromConf(_conf.ui_netbotinfo);
//		bd.level = _conf.ui_netbotinfo.level.GetInt();
//		dynamic_cast<TankClient*>(g_client)->SendAddBot(bd);
	}
	UnlinkChild(*sender);
}

void WaitingForPlayersDlg::OnOK()
{
//	dynamic_cast<TankClient*>(g_client)->SendPlayerReady(true);
}

void WaitingForPlayersDlg::OnCancel()
{
//	SAFE_DELETE(g_client);

	Close(_resultCancel);
}

void WaitingForPlayersDlg::OnSendMessage(const std::string &msg)
{
	if( !msg.empty() )
	{
//		if( 0 == strcmp(msg, "/ping") )
//		{
//			DWORD t = timeGetTime();
//			g_client->SendDataToServer(DataWrap(t, DBTYPE_PING));
//		}
//		else
		{
//			dynamic_cast<TankClient*>(g_client)->SendTextMessage(msg);
		}
	}
}

void WaitingForPlayersDlg::OnErrorMessage(const std::string &msg)
{
	_players->GetData()->DeleteAllItems();
	_bots->GetData()->DeleteAllItems();
	_btnOK->SetEnabled(false);
	_buf->WriteLine(0, msg);
}

void WaitingForPlayersDlg::OnTextMessage(const std::string &msg)
{
	_buf->WriteLine(0, msg);
}

void WaitingForPlayersDlg::OnPlayerReady(size_t idx, bool ready)
{
	int count = _world.GetList(LIST_players).size();
	assert(_players->GetData()->GetItemCount() <= count); // count includes bots

//	for( int index = 0; index < count; ++index )
//	{
//		GC_Player *player = (GC_Player *) _players->GetData()->GetItemData(index);
//		assert(player);
//
//		if( _world.GetList(LIST_players).IndexOf(player) == idx )
//		{
//			_players->GetData()->SetItemText(index, 3, ready ? _lang.net_chatroom_player_ready.Get() : "");
//			return;
//		}
//	}
	assert(false);
}

void WaitingForPlayersDlg::OnPlayersUpdate()
{
	// TODO: implement via the ListDataSource interface
	_players->GetData()->DeleteAllItems();
	_bots->GetData()->DeleteAllItems();
	FOREACH(_world.GetList(LIST_players), GC_Player, player)
	{
		if( !player->GetIsHuman() )
		{
			// nick & skin
			int index = _bots->GetData()->AddItem(player->GetNick());
			_bots->GetData()->SetItemText(index, 1, player->GetSkin());

			// team
			std::ostringstream tmp;
			tmp << _lang.net_chatroom_team.Get() << player->GetTeam();
			_bots->GetData()->SetItemText(index, 2, tmp.str());

			// level
		//	assert(player->GetLevel() <= AI_MAX_LEVEL);
		//	_bots->GetData()->SetItemText(index, 3, _lang->GetStr(EditBotDlg::levels[player->GetLevel()], "")->Get());
		}
		else
		{
			int index = _players->GetData()->AddItem(player->GetNick(), (size_t) player);
			_players->GetData()->SetItemText(index, 1, player->GetSkin());

			std::ostringstream tmp;
			tmp << _lang.net_chatroom_team.Get() << player->GetTeam();
			_players->GetData()->SetItemText(index, 2, tmp.str());
		}

//		_buf->printf(_lang.net_chatroom_player_x_disconnected.Get().c_str(), player->GetNick().c_str());
//		_buf->printf(_lang.net_chatroom_player_x_connected.Get().c_str(), player->GetNick().c_str());
//		_buf->printf("\n");
	}

	_btnOK->SetEnabled(_players->GetData()->GetItemCount() > 0);
}

void WaitingForPlayersDlg::OnStartGame()
{
	Close(_resultOK);
}

void WaitingForPlayersDlg::OnClientDestroy()
{
//	_clientSubscribtion.reset();
}
