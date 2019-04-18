#include "CampaignControls.h"
#include "Game.h"
#include "InputManager.h"
#include "MessageArea.h"
#include "ScoreTable.h"
#include "VehicleStateReader.h"
#include "inc/shell/Config.h"

#include <ctx/Deathmatch.h>
#include <ctx/GameContext.h>
#include <ctx/WorldController.h>
#include <gc/Player.h>
#include <gc/Vehicle.h>
#include <gc/World.h>
#include <loc/Language.h>
#include <plat/Input.h>
#include <plat/Keys.h>
#include <ui/DataSource.h>
#include <ui/InputContext.h>
#include <ui/GuiManager.h>
#include <ui/LayoutContext.h>
#include <ui/Rating.h>
#include <ui/StackLayout.h>
#include <video/RenderContext.h>
#include <video/TextureManager.h>

#include <sstream>
#include <iomanip>

namespace
{
	class TimerDisplay final
		: public UI::LayoutData<std::string_view>
	{
	public:
		TimerDisplay(World &world, const Deathmatch *deathmatch)
			: _world(world)
			, _deathmatch(deathmatch)
		{}

		// UI::LayoutData<std::string_view>
		std::string_view GetLayoutValue(const UI::DataContext &dc) const override
		{
			std::ostringstream text;
			if (_deathmatch && _deathmatch->GetTimeLimit() > 0)
			{
				int secondsLeft = (int)std::ceil(_deathmatch->GetTimeLimit() - _world.GetTime());
				text << (secondsLeft / 60) << ":" << std::setfill('0') << std::setw(2) << (secondsLeft % 60);
			}
			else
			{
				int seconds = (int)_world.GetTime();
				text << (seconds / 60) << ":";
				if (seconds % 60 < 10)
					text << "0";
				text << (seconds % 60);
			}

			_cachedString = text.str();

			return _cachedString;
		}

	private:
		World &_world;
		const Deathmatch *_deathmatch;
		mutable std::string _cachedString;
	};

	class DeathmatchRatingBinding final
		: public UI::RenderData<unsigned int>
	{
	public:
		DeathmatchRatingBinding(const Deathmatch &deathmatch)
			: _deathmatch(deathmatch)
		{}

		// UI::RenderData<unsigned int>
		unsigned int GetRenderValue(const UI::DataContext &dc, const UI::StateContext &sc) const override
		{
			return _deathmatch.GetRating();
		}

	private:
		const Deathmatch &_deathmatch;
	};
}

///////////////////////////////////////////////////////////////////////////////

GameLayout::GameLayout(UI::TimeStepManager &manager,
                       std::shared_ptr<GameContext> gameContext,
                       WorldView &worldView,
                       WorldController &worldController,
                       ShellConfig &conf,
                       LangCache &lang,
                       Plat::ConsoleBuffer &logger,
                       CampaignControlCommands campaignControlCommands)
  : UI::TimeStepping(manager)
  , _scoreAndControls(std::make_shared<UI::StackLayout>())
  , _gameContext(std::move(gameContext))
  , _gameViewHarness(_gameContext->GetWorld(), worldController)
  , _worldView(worldView)
  , _worldController(worldController)
  , _conf(conf)
  , _lang(lang)
  , _inputMgr(conf, logger)
{
	assert(_gameContext);

	_scoreAndControls->SetSpacing(20);
	_scoreAndControls->SetAlign(UI::Align::CT);
	AddFront(_scoreAndControls);

	_msg = std::make_shared<MessageArea>(manager, logger);
	AddFront(_msg);

	auto deathmatch = dynamic_cast<Deathmatch*>(_gameContext->GetGameplay());

	if (deathmatch)
	{
		_rating = std::make_shared<UI::Rating>();
		_rating->SetRating(std::make_shared<DeathmatchRatingBinding>(*deathmatch));
		_rating->SetVisible(false);
		_scoreAndControls->AddFront(_rating);
	}

	_score = std::make_shared<ScoreTable>(_gameContext->GetWorld(), deathmatch, _lang);
	_score->SetVisible(false);
	_scoreAndControls->AddFront(_score);

	if (deathmatch)
	{
		_campaignControls = std::make_shared<CampaignControls>(*deathmatch, std::move(campaignControlCommands));
		_campaignControls->SetVisible(false);
		_scoreAndControls->AddFront(_campaignControls);
		_scoreAndControls->SetFocus(_campaignControls);
	}

	_timerDisplay = std::make_shared<UI::Text>();
	_timerDisplay->SetAlign(alignTextRB);
	_timerDisplay->SetText(std::make_shared<TimerDisplay>(_gameContext->GetWorld(), deathmatch));
	AddFront(_timerDisplay);

	SetTimeStep(true);
	_gameContext->GetGameEventSource().AddListener(*this);
}

GameLayout::~GameLayout()
{
	_gameContext->GetGameEventSource().RemoveListener(*this);
}

vec2d GameLayout::GetDragDirection() const
{
	vec2d dragDirection{ 0, 0 };

	if (!_activeDrags.empty())
	{
		float maxDragLength = 0;
		for (auto &drag : _activeDrags)
		{
			vec2d dir = drag.second.second - drag.second.first;
			maxDragLength = std::max(maxDragLength, dir.sqr());
		}
		maxDragLength = std::sqrt(maxDragLength);

		// only account for len > max / 2
		unsigned int count = 0;
		for (auto &drag : _activeDrags)
		{
			vec2d dir = drag.second.second - drag.second.first;
			if (dir.len() > maxDragLength/2)
			{
				dragDirection += dir;
				count++;
			}
		}
		dragDirection /= (float)count;
	}
	return dragDirection;
}

unsigned int GameLayout::GetEffectiveDragCount() const
{
	float maxDragLength = 0;
	for (auto &drag : _activeDrags)
	{
		vec2d dir = drag.second.second - drag.second.first;
		maxDragLength = std::max(maxDragLength, dir.sqr());
	}
	maxDragLength = std::sqrt(maxDragLength);
	
	// only account for len > max / 2
	unsigned int count = 0;
	for (auto &drag : _activeDrags)
	{
		vec2d dir = drag.second.second - drag.second.first;
		if (dir.len() > maxDragLength/2)
		{
			count++;
		}
	}
	return count;
}

void GameLayout::OnTimeStep(const UI::InputContext &ic, float dt)
{
	bool tab = ic.GetInput().IsKeyPressed(Plat::Key::Tab);
	bool gameOver = _gameContext->GetGameplay() ? _gameContext->GetGameplay()->IsGameOver() : false;
	bool allDead = !_gameContext->GetWorldController().GetLocalPlayers().empty();
	float lastDieTime = 0;
	for (auto player : _gameContext->GetWorldController().GetLocalPlayers())
	{
		allDead &= !player->GetVehicle();
		lastDieTime = std::max(lastDieTime, player->GetDieTime());
	}
	float time = _gameContext->GetWorld().GetTime();
	constexpr float showScoreDelay = 0.3f;
	_score->SetVisible(tab || gameOver || (allDead && time > PLAYER_RESPAWN_DELAY && time > lastDieTime + showScoreDelay));
	if (_campaignControls)
		_campaignControls->SetVisible(gameOver);
	if (_rating)
		_rating->SetVisible(gameOver);

	_gameViewHarness.Step(dt);

	std::vector<GC_Player*> players = _worldController.GetLocalPlayers();
	for (auto player : players)
		player->SetIsActive(ic.GetFocused());

	if (ic.GetFocused())
	{
		WorldController::ControllerStateMap controlStates;

		vec2d dragDirection = GetDragDirection();
		bool reversing = GetEffectiveDragCount() > 1;
		
		for (unsigned int playerIndex = 0; playerIndex != players.size(); ++playerIndex)
		{
			if(VehicleStateReader *vehicleStateReader = _inputMgr.GetVehicleStateReader(playerIndex) )
			{
				vehicleStateReader->Step(dt);
				if( GC_Vehicle *vehicle = players[playerIndex]->GetVehicle() )
				{
					VehicleState vs;
					vehicleStateReader->ReadVehicleState(_gameViewHarness, *vehicle, playerIndex,
						ic.GetInput(), dragDirection, reversing, vs);
					controlStates.insert(std::make_pair(vehicle->GetId(), vs));
				}
			}
		}

		_worldController.SendControllerStates(std::move(controlStates));
	}
}

void GameLayout::Draw(const UI::DataContext &dc, const UI::StateContext &sc, const UI::LayoutContext &lc, const UI::InputContext &ic, RenderContext &rc, TextureManager &texman, float time) const
{
	int pxWidth = (int)lc.GetPixelSize().x;
	int pxHeight = (int)lc.GetPixelSize().y;
	float scale = std::min(lc.GetPixelSize().x / 1024.f, lc.GetPixelSize().y / 768.f); // lc.GetScale()
	const_cast<GameViewHarness&>(_gameViewHarness).SetCanvasSize(pxWidth, pxHeight, scale);

	_gameViewHarness.RenderGame(rc, _worldView, _conf.d_field.Get(), _conf.d_path.Get() ? &_gameContext->GetAIManager() : nullptr);

	// On-screen controls
	vec2d dir = GetDragDirection();
	bool reversing = GetEffectiveDragCount() > 1;
	std::vector<GC_Player*> players = _worldController.GetLocalPlayers();
	for (unsigned int playerIndex = 0; playerIndex != players.size(); ++playerIndex)
	{
		// Draw touch drag indicator
		if (!dir.IsZero())
		{
			if (const GC_Vehicle *vehicle = players[playerIndex]->GetVehicle())
			{
				vec2d pos = _gameViewHarness.WorldToCanvas(playerIndex, vehicle->GetPos());
				pos += dir;
				uint32_t opacity = uint32_t(std::min(dir.len() / 200.f, 1.f) * 255.f) & 0xff;
				uint32_t rgb = reversing ? opacity : opacity << 8;
				rc.DrawSprite(_texDrag.GetTextureId(texman), 0, rgb | (opacity << 24), pos, dir.Norm());
			}
		}
		
		// Draw tap target
		if (const VehicleStateReader *vehicleStateReader = _inputMgr.GetVehicleStateReader(playerIndex))
		{
			float time = vehicleStateReader->GetRemainingFireTime();
			if (time > 0)
			{
				vec2d pos = _gameViewHarness.WorldToCanvas(playerIndex, vehicleStateReader->GetFireTarget());
				rc.DrawSprite(_texTarget.GetTextureId(texman), 0, 0xff00ff00, pos, Vec2dDirection(_gameContext->GetWorld().GetTime()*3));
			}
		}
	}
}

FRECT GameLayout::GetChildRect(TextureManager &texman, const UI::LayoutContext &lc, const UI::DataContext &dc, const UI::Window &child) const
{
	float scale = lc.GetScale();
	vec2d size = lc.GetPixelSize();

	if (_scoreAndControls.get() == &child)
	{
		vec2d pxChildSize = child.GetContentSize(texman, dc, lc.GetScale(), DefaultLayoutConstraints(lc));
		return MakeRectWH(Vec2dFloor((size - pxChildSize) / 2), pxChildSize);
	}
	if (_timerDisplay.get() == &child)
	{
		return MakeRectWH(size - vec2d{1, 1}, {});
	}
	if (_msg.get() == &child)
	{
		return UI::CanvasLayout(vec2d{ 50, size.y / scale - 50 }, _msg->GetSize(), scale);
	}
	return UI::Window::GetChildRect(texman, lc, dc, child);
}

std::shared_ptr<UI::Window> GameLayout::GetFocus() const
{
	return _scoreAndControls;
}

bool GameLayout::OnPointerDown(UI::InputContext &ic, UI::LayoutContext &lc, TextureManager &texman, UI::PointerInfo pi, int button)
{
	if (Plat::PointerType::Touch == pi.type)
	{
		_activeDrags[pi.id].first = pi.position;
		_activeDrags[pi.id].second = pi.position;
		return true;
	}
	return false;
}

void GameLayout::OnPointerUp(UI::InputContext &ic, UI::LayoutContext &lc, TextureManager &texman, UI::PointerInfo pi, int button)
{
	_activeDrags.erase(pi.id);
}

void GameLayout::OnPointerMove(UI::InputContext &ic, UI::LayoutContext &lc, TextureManager &texman, UI::PointerInfo pi, bool captured)
{
	if( captured )
	{
		auto &drag = _activeDrags[pi.id];
		drag.second = pi.position;
		vec2d dir = drag.second - drag.first;
		const float maxDragLength = 100;
		if (dir.len() > maxDragLength)
		{
			drag.first = drag.second - dir.Norm() * maxDragLength;
		}
	}
}

void GameLayout::OnTap(UI::InputContext &ic, UI::LayoutContext &lc, TextureManager &texman, vec2d pointerPosition)
{
	std::vector<GC_Player*> players = _worldController.GetLocalPlayers();
	for (unsigned int playerIndex = 0; playerIndex != players.size(); ++playerIndex)
	{
		if( VehicleStateReader *vehicleStateReader = _inputMgr.GetVehicleStateReader(playerIndex) )
		{
			auto c2w = _gameViewHarness.CanvasToWorld(playerIndex, (int)pointerPosition.x, (int)pointerPosition.y);
			if (c2w.visible)
			{
				vehicleStateReader->OnTap(c2w.worldPos);
			}
		}
	}
}

void GameLayout::OnMurder(GC_Player &victim, GC_Player *killer, MurderType murderType)
{
	char msg[256] = { 0 };
	switch (murderType)
	{
	default:
		assert(false);
	case MurderType::Accident:
		// TODO: remove string allocations
		snprintf(msg, sizeof(msg), std::string(_lang.msg_player_x_died.Get()).c_str(), std::string(victim.GetNick()).c_str());
		break;
	case MurderType::Enemy:
		assert(killer);
		// TODO: remove string allocations
		snprintf(msg, sizeof(msg), std::string(_lang.msg_player_x_killed_his_enemy_x.Get()).c_str(), std::string(killer->GetNick()).c_str(), std::string(victim.GetNick()).c_str());
		break;
	case MurderType::Friend:
		assert(killer);
		// TODO: remove string allocations
		snprintf(msg, sizeof(msg), std::string(_lang.msg_player_x_killed_his_friend_x.Get()).c_str(), std::string(killer->GetNick()).c_str(), std::string(victim.GetNick()).c_str());
		break;
	case MurderType::Suicide:
		// TODO: remove string allocations
		snprintf(msg, sizeof(msg), std::string(_lang.msg_player_x_killed_him_self.Get()).c_str(), std::string(victim.GetNick()).c_str());
		break;
	}
	_msg->WriteLine(msg);
}
