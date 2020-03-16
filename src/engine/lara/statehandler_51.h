#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"

namespace engine::lara
{
class StateHandler_51 final : public AbstractStateHandler
{
public:
  explicit StateHandler_51(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::MidasDeath}
  {
  }

  void handleInput(CollisionInfo& collisionInfo) override
  {
    getLara().m_state.falling = false;
    collisionInfo.policyFlags &= ~CollisionInfo::SpazPushPolicy;
    const auto& alternateLara = getEngine().findAnimatedModelForType(TR1ItemId::LaraShotgunAnim);
    if(alternateLara == nullptr)
      return;

    const auto frameOffs = getLara().getSkeleton()->frame_number - getLara().getSkeleton()->anim->firstFrame;
    switch(frameOffs.get())
    {
    case 5:
      getLara().getNode()->getChild(3)->setRenderable(alternateLara->renderMeshes[3].get());
      getLara().getNode()->getChild(6)->setRenderable(alternateLara->renderMeshes[6].get());
      break;
    case 70: getLara().getNode()->getChild(2)->setRenderable(alternateLara->renderMeshes[2].get()); break;
    case 90: getLara().getNode()->getChild(1)->setRenderable(alternateLara->renderMeshes[1].get()); break;
    case 100: getLara().getNode()->getChild(5)->setRenderable(alternateLara->renderMeshes[5].get()); break;
    case 120:
      getLara().getNode()->getChild(0)->setRenderable(alternateLara->renderMeshes[0].get());
      getLara().getNode()->getChild(4)->setRenderable(alternateLara->renderMeshes[4].get());
      break;
    case 135: getLara().getNode()->getChild(7)->setRenderable(alternateLara->renderMeshes[7].get()); break;
    case 150: getLara().getNode()->getChild(11)->setRenderable(alternateLara->renderMeshes[11].get()); break;
    case 163: getLara().getNode()->getChild(12)->setRenderable(alternateLara->renderMeshes[12].get()); break;
    case 174: getLara().getNode()->getChild(13)->setRenderable(alternateLara->renderMeshes[13].get()); break;
    case 186: getLara().getNode()->getChild(8)->setRenderable(alternateLara->renderMeshes[8].get()); break;
    case 195: getLara().getNode()->getChild(9)->setRenderable(alternateLara->renderMeshes[9].get()); break;
    case 218: getLara().getNode()->getChild(10)->setRenderable(alternateLara->renderMeshes[10].get()); break;
    case 225: getLara().getNode()->getChild(14)->setRenderable(alternateLara->renderMeshes[14].get()); break;
    default:
      // silence compiler
      break;
    }
    StateHandler_50::emitSparkles(getEngine());
  }

  void postprocessFrame(CollisionInfo& collisionInfo) override
  {
    collisionInfo.badPositiveDistance = core::ClimbLimit2ClickMin;
    collisionInfo.badNegativeDistance = -core::ClimbLimit2ClickMin;
    collisionInfo.badCeilingDistance = 0_len;
    setMovementAngle(getLara().m_state.rotation.Y);
    collisionInfo.policyFlags |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.facingAngle = getLara().m_state.rotation.Y;
    collisionInfo.initHeightInfo(getLara().m_state.position.position, getEngine(), core::LaraWalkHeight);
  }
};
} // namespace engine::lara
