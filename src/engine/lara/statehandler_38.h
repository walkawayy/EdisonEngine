#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "hid/inputstate.h"

namespace engine::lara
{
class StateHandler_38 final : public StateHandler_Pushable
{
public:
  explicit StateHandler_38(objects::LaraObject& lara)
      : StateHandler_Pushable{lara, LaraStateId::PushableGrab}
  {
  }

  void handleInput(CollisionInfo& collisionInfo, bool /*doPhysics*/) override
  {
    collisionInfo.policies &= ~CollisionInfo::SpazPushPolicy;
    setCameraRotationAroundLaraY(75_deg);
    if(!getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
    {
      setGoalAnimState(LaraStateId::Stop);
    }
  }
};
} // namespace engine::lara
