#include "UITestDesktop.h"
#include <ui/Button.h>
#include <ui/DataSource.h>
#include <ui/LayoutContext.h>
#include <ui/Rectangle.h>
#include <ui/Text.h>

using namespace UI::DataSourceAliases;


UITestDesktop::UITestDesktop(TextureManager &texman)
	: _testRect(std::make_shared<UI::Rectangle>())
	, _testText(std::make_shared<UI::Text>(texman))
	, _testButton(std::make_shared<UI::Button>(texman))
{
	_testRect->SetTexture(texman, "gui_splash", false);
	AddFront(_testRect);

	_testText->SetText("Hello World!"_txt);
	AddFront(_testText);

	_testButton->SetText("Push me"_txt);
	AddFront(_testButton);
}

FRECT UITestDesktop::GetChildRect(TextureManager &texman, const UI::LayoutContext &lc, const UI::DataContext &dc, const UI::Window &child) const
{
	float scale = lc.GetScale();
	vec2d size = lc.GetPixelSize();

	if (_testRect.get() == &child)
	{
		return UI::CanvasLayout({ 0, 42 }, { size.x, 384 }, scale);
	}
	else if (_testButton.get() == &child)
	{
		return MakeRectWH(Vec2dFloor((size - child.GetSize() * scale) / 2), Vec2dFloor(child.GetSize() * scale));
	}

	return UI::Window::GetChildRect(texman, lc, dc, child);
}
