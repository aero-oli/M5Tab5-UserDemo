#include <cassert>
#include <vector>
#include <apps/app_hardware_gpio/gpio_layout.h>

static bool overlaps(const app_ui::Rect& a, const app_ui::Rect& b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void verifyLayout(int pinCount)
{
    const auto layout = app_hardware_gpio::makeGpioLayout(pinCount);
    const std::vector<app_ui::Rect> fixed = {layout.statusText, layout.errorText, layout.releaseAll, layout.grid};
    for (size_t i = 0; i < fixed.size(); ++i) {
        assert(app_ui::containsLocal(layout.content, fixed[i]));
        for (size_t j = i + 1; j < fixed.size(); ++j) assert(!overlaps(fixed[i], fixed[j]));
    }
    const app_ui::Rect inner = {0, 0, layout.cardW - 34, layout.cardH - 34};
    const std::vector<app_ui::Rect> children = {layout.title, layout.role, layout.state, layout.toggle, layout.action};
    for (size_t i = 0; i < children.size(); ++i) {
        assert(app_ui::containsLocal(inner, children[i]));
        for (size_t j = i + 1; j < children.size(); ++j) assert(!overlaps(children[i], children[j]));
    }
    assert(layout.action.h >= 44 && layout.releaseAll.h >= 44);
    std::vector<app_ui::Rect> cards;
    for (int i = 0; i < pinCount; ++i) {
        const auto card = app_hardware_gpio::gpioCardRect(layout, i);
        assert(card.x >= 0 && card.x + card.w <= layout.grid.w);
        assert(card.y + card.h <= layout.scrollContentHeight);
        for (const auto& previous : cards) assert(!overlaps(card, previous));
        cards.push_back(card);
    }
    if (cards.empty()) assert(layout.scrollContentHeight == 0);
    else assert(layout.scrollContentHeight == cards.back().y + layout.cardH + 24);
    if (pinCount == 24) assert(layout.scrollContentHeight > layout.grid.h);
}

int main()
{
    for (int count : {0, 1, 24, 25}) verifyLayout(count);
}
