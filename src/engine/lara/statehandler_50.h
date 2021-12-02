#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "engine/particle.h"
#include "engine/world/skeletalmodeltype.h"

#include <gslu.h>

namespace engine::lara
{
class StateHandler_50 final : public AbstractStateHandler
{
public:
  explicit StateHandler_50(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::UseMidas}
  {
  }

  void handleInput(CollisionInfo& collisionInfo, bool doPhysics) override
  {
    collisionInfo.policies &= ~CollisionInfo::SpazPushPolicy;

    if(doPhysics)
    {
      emitSparkles(getWorld());
    }
  }

  void postprocessFrame(CollisionInfo& collisionInfo, bool doPhysics) override
  {
    collisionInfo.validFloorHeight = {-core::ClimbLimit2ClickMin, core::ClimbLimit2ClickMin};
    collisionInfo.validCeilingHeightMin = 0_len;
    collisionInfo.policies |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.facingAngle = getLara().m_state.rotation.Y;
    collisionInfo.initHeightInfo(getLara().m_state.location.position, getWorld(), core::LaraWalkHeight);

    if(!doPhysics)
      return;

    setMovementAngle(collisionInfo.facingAngle);
  }

  static void emitSparkles(world::World& world)
  {
    const auto spheres = world.getObjectManager().getLara().getSkeleton()->getBoneCollisionSpheres();

    const auto& normalLara = world.findAnimatedModelForType(TR1ItemId::Lara);
    Expects(normalLara != nullptr);
    for(size_t i = 0; i < spheres.size(); ++i)
    {
      if(world.getObjectManager().getLara().getSkeleton()->getMeshPart(i) == normalLara->bones[i].mesh)
        continue;

      const auto r = spheres[i].radius;
      auto p = core::TRVec{spheres[i].getCollisionPosition()};
      p.X += util::rand15s(r);
      p.Y += util::rand15s(r);
      p.Z += util::rand15s(r);
      auto fx = gslu::make_nn_shared<SparkleParticle>(
        Location{world.getObjectManager().getLara().m_state.location.room, p}, world);
      world.getObjectManager().registerParticle(fx);
    }
  }
};
} // namespace engine::lara
