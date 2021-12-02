#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"

namespace engine::lara
{
class StateHandler_39 final : public AbstractStateHandler
{
public:
  explicit StateHandler_39(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::PickUp}
  {
  }

  void handleInput(CollisionInfo& collisionInfo, bool /*doPhysics*/) override
  {
    collisionInfo.policies &= ~CollisionInfo::SpazPushPolicy;
    setCameraRotationAroundLara(-15_deg, -130_deg);
    setCameraDistance(1024_len);
  }

  void postprocessFrame(CollisionInfo& collisionInfo, bool doPhysics) override
  {
    collisionInfo.facingAngle = getLara().m_state.rotation.Y;
    collisionInfo.validFloorHeight = {-core::ClimbLimit2ClickMin, core::ClimbLimit2ClickMin};
    collisionInfo.validCeilingHeightMin = 0_len;
    collisionInfo.policies |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.initHeightInfo(getLara().m_state.location.position, getWorld(), core::LaraWalkHeight);

    if(!doPhysics)
      return;

    setMovementAngle(collisionInfo.facingAngle);
  }
};
} // namespace engine::lara
