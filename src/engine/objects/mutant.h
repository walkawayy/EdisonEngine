#pragma once

#include "aiagent.h"
#include "engine/ai/ai.h"

namespace engine::objects
{
class FlyingMutant : public AIAgent
{
public:
  FlyingMutant(const gsl::not_null<world::World*>& world, const Location& location)
      : AIAgent{world, location}
  {
  }

  FlyingMutant(const gsl::not_null<world::World*>& world,
               const gsl::not_null<const world::Room*>& room,
               const loader::file::Item& item,
               const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
      : AIAgent{world, room, item, animatedModel}
  {
  }

  void update() final;

  void serialize(const serialization::Serializer<world::World>& ser) override;

private:
  bool m_shootBullet = false;
  bool m_throwGrenade = false;
  bool m_flying = false;
  bool m_lookingAround = false;
};

class WalkingMutant final : public FlyingMutant
{
public:
  WalkingMutant(const gsl::not_null<world::World*>& world, const Location& location)
      : FlyingMutant{world, location}
  {
  }

  WalkingMutant(const gsl::not_null<world::World*>& world,
                const gsl::not_null<const world::Room*>& room,
                const loader::file::Item& item,
                const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
      : FlyingMutant{world, room, item, animatedModel}
  {
    for(size_t i = 0; i < getSkeleton()->getBoneCount(); ++i)
    {
      getSkeleton()->setVisible(i, (0xffe07fffu >> i) & 1u);
    }
    getSkeleton()->rebuildMesh();
  }
};

class CentaurMutant final : public AIAgent
{
public:
  CentaurMutant(const gsl::not_null<world::World*>& world, const Location& location)
      : AIAgent{world, location}
  {
  }

  CentaurMutant(const gsl::not_null<world::World*>& world,
                const gsl::not_null<const world::Room*>& room,
                const loader::file::Item& item,
                const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
      : AIAgent{world, room, item, animatedModel}
  {
  }

  void update() override;
};

class TorsoBoss final : public AIAgent
{
public:
  TorsoBoss(const gsl::not_null<world::World*>& world, const Location& location)
      : AIAgent{world, location}
  {
  }

  TorsoBoss(const gsl::not_null<world::World*>& world,
            const gsl::not_null<const world::Room*>& room,
            const loader::file::Item& item,
            const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
      : AIAgent{world, room, item, animatedModel}
  {
  }

  void update() override;

  void serialize(const serialization::Serializer<world::World>& ser) override;

private:
  bool m_hasHitLara = false;
  core::Frame m_turnStartFrame = 0_frame;
};

} // namespace engine::objects
