#pragma once
#include <ui/Window.h>
#include <string>
#include <deque>

class ConfCache;

namespace UI
{
	class ConsoleBuffer;
}

class MessageArea : public UI::Window
{
public:
	MessageArea(UI::Window *parent, ConfCache &conf, UI::ConsoleBuffer &logger);

	void WriteLine(const std::string &text);
	void Clear();

	// UI::Window
	void OnTimeStep(float dt) override;
	void Draw(DrawingContext &dc) const override;

private:
	struct Line
	{
		float time;
		std::string str;
	};
	typedef std::deque<Line> LineList;
	LineList _lines;
	size_t _fontTexture;
	ConfCache &_conf;
	UI::ConsoleBuffer &_logger;
};
