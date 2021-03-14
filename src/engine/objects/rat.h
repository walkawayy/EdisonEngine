#pragma once

#include "aiagent.h"
#include "engine/ai/ai.h"

namespace engine::objects
{
class Rat final : public AIAgent
{
public:
  Rat(const gsl::not_null<world::World*>& world, const core::RoomBoundPosition& position)
      : AIAgent{world, position}
  {
  }

  Rat(const gsl::not_null<world::World*>& world,
      const gsl::not_null<const loader::file::Room*>& room,
      const loader::file::Item& item,
      const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
      : AIAgent{world, room, item, animatedModel}
  {
  }

  void update() override;
};
} // namespace engine::objects
