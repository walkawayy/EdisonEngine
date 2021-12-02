#include "applyitemtransformmenustate.h"

#include "core/angle.h"
#include "menu/menuobject.h"
#include "menudisplay.h"
#include "menuring.h"
#include "qs/qs.h"
#include "selectedmenustate.h"
#include "util.h"

namespace menu
{
class MenuState;

void ApplyItemTransformMenuState::handleObject(ui::Ui& /*ui*/,
                                               engine::world::World& /*world*/,
                                               MenuDisplay& display,
                                               MenuObject& object)
{
  if(&object != &display.getCurrentRing().getSelectedObject())
  {
    zeroRotation(object, 256_au);
    return;
  }

  object.baseRotationX = exactScale(object.selectedBaseRotationX, m_duration, Duration);
  object.rotationX = exactScale(object.selectedRotationX, m_duration, Duration);
  object.positionZ = exactScale(object.selectedPositionZ, m_duration, Duration);

  if(object.rotationY != object.selectedRotationY)
  {
    if(const auto dy = object.selectedRotationY - object.rotationY; dy > 0_deg && dy < 180_deg)
    {
      object.rotationY += toRenderUnit(1024_au / 1_frame) * 1_rframe;
    }
    else
    {
      object.rotationY -= toRenderUnit(1024_au / 1_frame) * 1_rframe;
    }
    object.rotationY -= object.rotationY % (toRenderUnit(1024_au / 1_frame) * 1_rframe);
  }
}

std::unique_ptr<MenuState>
  ApplyItemTransformMenuState::onFrame(ui::Ui& /*ui*/, engine::world::World& /*world*/, MenuDisplay& /*display*/)
{
  if(m_duration != Duration)
  {
    m_duration += 1_rframe;
    return nullptr;
  }

  return create<SelectedMenuState>();
}
} // namespace menu
