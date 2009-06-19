// gui_widgets.h

#pragma once

#include "ui/Text.h"


namespace UI
{

class FpsCounter : public Text
{
	std::list<float> _dts;
	std::list<float> _dts_net;
	int _nSprites;
	int _nLights;
	int _nBatches;

public:
	FpsCounter(Window *parent, float x, float y, enumAlignText align);

protected:
	void OnVisibleChange(bool visible, bool inherited);
	void OnTimeStep(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class TimeElapsed : public Text
{
public:
	TimeElapsed(Window *parent, float x, float y, enumAlignText align);
protected:
	void OnVisibleChange(bool visible, bool inherited);
	void OnTimeStep(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class Oscilloscope : public Window
{
public:
	Oscilloscope(Window *parent, float x, float y);
	void Push(float value);
	void SetRange(float rmin, float rmax);
	void SetTitle(const string_t &title);
	void SetGrid(const float *data, size_t count);

protected:
	void DrawChildren(float sx, float sy) const;

private:
	size_t _barTexture;
	size_t _titleFont;
	string_t _title;
	std::vector<float> _vgrid;
	std::deque<float> _data;
	float _rangeMin;
	float _rangeMax;
};


///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file
