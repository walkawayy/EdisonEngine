#pragma once

#include "spriteobject.h"

namespace engine::objects
{
class PickupObject final : public SpriteObject
{
public:
  PickupObject(const gsl::not_null<world::World*>& world, const Location& location, std::string name)
      : SpriteObject{world, location, std::move(name)}
  {
  }

  PickupObject(const gsl::not_null<world::World*>& world,
               const std::string& name,
               const gsl::not_null<const world::Room*>& room,
               const loader::file::Item& item,
               const gsl::not_null<const world::Sprite*>& sprite)
      : SpriteObject{world, name, room, item, true, sprite}
  {
  }

  void collide(CollisionInfo& collisionInfo) override;
};
} // namespace engine::objects
