#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "hid/inputstate.h"

namespace engine::lara
{
class StateHandler_5 final : public AbstractStateHandler
{
public:
  explicit StateHandler_5(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::RunBack}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/) override
  {
    setGoalAnimState(LaraStateId::Stop);

    if(getWorld().getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Left)
    {
      subYRotationSpeed(core::SlowTurnSpeedAcceleration, -core::RunBackTurnSpeed);
    }
    else if(getWorld().getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Right)
    {
      addYRotationSpeed(core::SlowTurnSpeedAcceleration, core::RunBackTurnSpeed);
    }
  }

  void postprocessFrame(CollisionInfo& collisionInfo) override
  {
    auto& laraState = getLara().m_state;
    laraState.fallspeed = 0_spd;
    laraState.falling = false;
    collisionInfo.validFloorHeight = {-core::ClimbLimit2ClickMin, core::HeightLimit};
    collisionInfo.validCeilingHeightMin = 0_len;
    collisionInfo.policies |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.facingAngle = laraState.rotation.Y + 180_deg;
    setMovementAngle(collisionInfo.facingAngle);
    collisionInfo.initHeightInfo(laraState.location.position, getWorld(), core::LaraWalkHeight);
    if(stopIfCeilingBlocked(collisionInfo))
    {
      return;
    }

    if(collisionInfo.mid.floor.y > 200_len)
    {
      setAnimation(AnimationId::FREE_FALL_BACK);
      setGoalAnimState(LaraStateId::FallBackward);
      setCurrentAnimState(LaraStateId::FallBackward);
      getLara().m_state.fallspeed = 0_spd;
      getLara().m_state.falling = true;
      return;
    }

    if(checkWallCollision(collisionInfo))
    {
      setAnimation(AnimationId::STAY_SOLID);
    }
    placeOnFloor(collisionInfo);
  }
};
} // namespace engine::lara
