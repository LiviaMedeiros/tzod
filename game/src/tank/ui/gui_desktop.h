// gui_desktop.h

#pragma once

#include "Window.h"

namespace UI
{
	class EditorLayout;
	class Console;
	class ScoreTable;

	// widgets
	class FpsCounter;
	class TimeElapsed;

///////////////////////////////////////////////////////////////////////////////

class MessageArea : public Window
{
private:
	struct Line
	{
		float time;
		string_t str;
	};
	typedef std::deque<Line> LineList;
	LineList _lines;
	Text *_blankText; // used for text drawing

public:
	MessageArea(Window *parent, float x, float y);
	~MessageArea();

	void WriteLine(const string_t &text);
	void Clear();

	virtual void OnTimeStep(float dt);
	virtual void DrawChildren(float sx, float sy) const;

private:
	void OnToggleVisible();
};

///////////////////////////////////////////////////////////////////////////////

class Desktop : public Window
{
	EditorLayout *_editor;
	Console      *_con;
	MessageArea  *_msg;
	ScoreTable   *_score;

	FpsCounter   *_fps;
	TimeElapsed  *_time;

public:
	Desktop(GuiManager* manager);
	~Desktop();

	void ShowDesktopBackground(bool show);
	void ShowConsole(bool show);
	void ShowEditor(bool show);

	void OnCloseChild(int result);

	MessageArea* GetMsgArea() const;

protected:
	virtual void OnRawChar(int c);
	virtual bool OnFocus(bool focus);
	virtual void OnSize(float width, float height);

private:
	void OnChangeShowFps();
	void OnChangeShowTime();

	void OnCommand(const string_t &cmd);
	bool OnCompleteCommand(const string_t &cmd, string_t &result);
};


///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file
