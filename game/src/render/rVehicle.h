#pragma once
#include "inc/render/ObjectView.h"
#include <stddef.h>

class TextureManager;

class R_Vehicle : public ObjectRFunc
{
public:
	R_Vehicle(TextureManager &tm);
	void Draw(const World &world, const GC_Actor &actor, RenderContext &rc) const override;

private:
	TextureManager &_tm;
	size_t _nameFont;
};

