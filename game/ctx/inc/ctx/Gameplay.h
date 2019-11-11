#pragma once

class SaveFile;

struct Gameplay
{
	virtual ~Gameplay() {}
	virtual void Step() = 0;
	virtual float GetGameEndTime() const = 0;
	virtual float GetTimeLimit() const = 0;
	virtual void Serialize(SaveFile &f) = 0;
};
