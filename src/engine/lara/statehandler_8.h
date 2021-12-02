#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"

namespace engine::lara
{
class StateHandler_8 final : public AbstractStateHandler
{
public:
  explicit StateHandler_8(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::Death}
  {
  }

  void handleInput(CollisionInfo& collisionInfo, bool /*doPhysics*/) override
  {
    collisionInfo.policies &= ~CollisionInfo::SpazPushPolicy;
  }

  void postprocessFrame(CollisionInfo& collisionInfo, bool /*doPhysics*/) override
  {
    collisionInfo.validFloorHeight = {-core::ClimbLimit2ClickMin, core::ClimbLimit2ClickMin};
    collisionInfo.validCeilingHeightMin = 0_len;
    collisionInfo.collisionRadius = 400_len;
    collisionInfo.facingAngle = getLara().m_state.rotation.Y;
    setMovementAngle(collisionInfo.facingAngle);
    applyShift(collisionInfo);
    placeOnFloor(collisionInfo);
    collisionInfo.initHeightInfo(getLara().m_state.location.position, getWorld(), core::LaraWalkHeight);
    getLara().m_state.health = core::DeadHealth;
    setAir(-1_rframe);
  }
};
} // namespace engine::lara
