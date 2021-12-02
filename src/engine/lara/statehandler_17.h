#pragma once

#include "statehandler_underwater.h"

namespace engine::lara
{
class StateHandler_17 final : public StateHandler_Underwater
{
public:
  explicit StateHandler_17(objects::LaraObject& lara)
      : StateHandler_Underwater{lara, LaraStateId::UnderwaterForward}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/, bool /*doPhysics*/) override
  {
    if(getLara().isDead())
    {
      setGoalAnimState(LaraStateId::WaterDeath);
      return;
    }

    handleDiveRotationInput();

    if(!getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Jump))
    {
      setGoalAnimState(LaraStateId::UnderwaterInertia);
    }

    getLara().m_state.fallspeed += 8_spd / 1_frame;
    getLara().m_state.fallspeed = std::min(getLara().m_state.fallspeed.velocity, 200_spd);
  }
};
} // namespace engine::lara
