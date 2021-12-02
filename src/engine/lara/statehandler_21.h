#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "hid/inputstate.h"

namespace engine::lara
{
class StateHandler_21 final : public AbstractStateHandler
{
public:
  explicit StateHandler_21(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::StepRight}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/, bool /*doPhysics*/) override
  {
    if(getLara().isDead())
    {
      setGoalAnimState(LaraStateId::Stop);
      return;
    }

    const auto& inputHandler = getWorld().getPresenter().getInputHandler();
    if(inputHandler.getInputState().stepMovement != hid::AxisMovement::Right)
    {
      setGoalAnimState(LaraStateId::Stop);
    }

    if(inputHandler.getInputState().xMovement == hid::AxisMovement::Left)
    {
      subYRotationSpeed(core::SlowTurnSpeedAcceleration, -core::SlowTurnSpeed);
    }
    else if(inputHandler.getInputState().xMovement == hid::AxisMovement::Right)
    {
      addYRotationSpeed(core::SlowTurnSpeedAcceleration, core::SlowTurnSpeed);
    }
  }

  void postprocessFrame(CollisionInfo& collisionInfo, bool /*doPhysics*/) override
  {
    auto& laraState = getLara().m_state;
    collisionInfo.validFloorHeight = {-core::SteppableHeight, core::SteppableHeight};
    collisionInfo.validCeilingHeightMin = 0_len;
    collisionInfo.facingAngle = laraState.rotation.Y + 90_deg;
    collisionInfo.policies |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.initHeightInfo(laraState.location.position, getWorld(), core::LaraWalkHeight);

    setMovementAngle(collisionInfo.facingAngle);
    laraState.fallspeed = 0_spd;
    laraState.falling = false;

    if(stopIfCeilingBlocked(collisionInfo))
    {
      return;
    }

    if(checkWallCollision(collisionInfo))
    {
      setAnimation(AnimationId::STAY_SOLID);
      setGoalAnimState(LaraStateId::Stop);
      setCurrentAnimState(LaraStateId::Stop);
    }

    if(!tryStartSlide(collisionInfo))
    {
      placeOnFloor(collisionInfo);
    }
  }
};
} // namespace engine::lara
