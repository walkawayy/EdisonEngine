#pragma once

#include "statehandler_onwater.h"

namespace engine::lara
{
class StateHandler_49 final : public StateHandler_OnWater
{
public:
  explicit StateHandler_49(objects::LaraObject& lara)
      : StateHandler_OnWater{lara, LaraStateId::OnWaterRight}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/) override
  {
    if(getLara().isDead())
    {
      setGoalAnimState(LaraStateId::WaterDeath);
      return;
    }

    setSwimToDiveKeypressDuration(0_frame);

    if(getWorld().getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Left)
    {
      getLara().m_state.rotation.Y -= 2_deg;
    }
    else if(getWorld().getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Right)
    {
      getLara().m_state.rotation.Y += 2_deg;
    }

    if(getWorld().getPresenter().getInputHandler().getInputState().stepMovement != hid::AxisMovement::Right)
    {
      setGoalAnimState(LaraStateId::OnWaterStop);
    }

    getLara().m_state.fallspeed
      = std::min(core::OnWaterMaxSpeed, getLara().m_state.fallspeed + core::OnWaterAcceleration * 1_frame);
  }

  void postprocessFrame(CollisionInfo& collisionInfo) override
  {
    setMovementAngle(getLara().m_state.rotation.Y + 90_deg);
    commonOnWaterHandling(collisionInfo);
  }
};
} // namespace engine::lara
