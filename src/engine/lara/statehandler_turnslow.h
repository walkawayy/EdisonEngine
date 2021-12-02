#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"

namespace engine::lara
{
class StateHandler_TurnSlow : public AbstractStateHandler
{
protected:
  explicit StateHandler_TurnSlow(objects::LaraObject& lara, const LaraStateId id)
      : AbstractStateHandler{lara, id}
  {
  }

public:
  void postprocessFrame(CollisionInfo& collisionInfo, bool doPhysics) final
  {
    auto& laraState = getLara().m_state;
    collisionInfo.facingAngle = laraState.rotation.Y;
    collisionInfo.validFloorHeight = {-core::ClimbLimit2ClickMin, core::ClimbLimit2ClickMin};
    collisionInfo.validCeilingHeightMin = 0_len;
    collisionInfo.policies |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.initHeightInfo(laraState.location.position, getWorld(), core::LaraWalkHeight);

    if(!doPhysics)
      return;

    setMovementAngle(collisionInfo.facingAngle);
    laraState.fallspeed = 0_spd;
    laraState.falling = false;

    if(collisionInfo.mid.floor.y <= core::DefaultCollisionRadius)
    {
      if(!tryStartSlide(collisionInfo))
      {
        placeOnFloor(collisionInfo);
      }

      return;
    }

    setAnimation(AnimationId::FREE_FALL_FORWARD);
    setGoalAnimState(LaraStateId::JumpForward);
    setCurrentAnimState(LaraStateId::JumpForward);
    getLara().m_state.fallspeed = 0_spd;
    getLara().m_state.falling = true;
  }
};
} // namespace engine::lara
