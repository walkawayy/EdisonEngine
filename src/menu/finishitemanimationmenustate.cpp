#include "finishitemanimationmenustate.h"

#include "core/angle.h"
#include "core/units.h"
#include "engine/items_tr1.h"
#include "menudisplay.h"
#include "menuobject.h"
#include "menuring.h"
#include "menustate.h"
#include "util.h"

namespace menu
{
std::unique_ptr<MenuState>
  FinishItemAnimationMenuState::onFrame(ui::Ui& /*ui*/, engine::world::World& world, MenuDisplay& display)
{
  auto& object = display.getCurrentRing().getSelectedObject();
  if(object.animate())
    return nullptr; // play full animation until its end

  if(object.type == engine::TR1ItemId::PassportOpening)
  {
    object.type = engine::TR1ItemId::PassportClosed;
    object.meshAnimFrame = 0_rframe;
    object.initModel(world);
  }

  return std::move(m_next);
}

void FinishItemAnimationMenuState::handleObject(ui::Ui& /*ui*/,
                                                engine::world::World& /*world*/,
                                                MenuDisplay& display,
                                                MenuObject& object)
{
  if(&object != &display.getCurrentRing().getSelectedObject())
    zeroRotation(object, 256_au);
  else
    rotateForSelection(object);
}
} // namespace menu
