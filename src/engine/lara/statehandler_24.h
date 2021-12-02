#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "hid/inputstate.h"

namespace engine::lara
{
class StateHandler_24 final : public AbstractStateHandler
{
public:
  explicit StateHandler_24(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::SlideForward}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/, bool /*doPhysics*/) override
  {
    setCameraModifier(CameraModifier::AllowSteepSlants);
    setCameraRotationAroundLaraX(-45_deg);
    if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Jump))
    {
      setGoalAnimState(LaraStateId::JumpForward);
    }
  }

  void postprocessFrame(CollisionInfo& collisionInfo, bool doPhysics) override
  {
    setMovementAngle(getLara().m_state.rotation.Y);
    commonSlideHandling(collisionInfo, doPhysics);
  }
};
} // namespace engine::lara
