#pragma once

#include "core/magic.h"
#include "core/units.h"
#include "core/vec.h"
#include "engine/floordata/types.h"
#include "qs/qs.h"

#include <cstdint>
#include <gsl/gsl-lite.hpp>
#include <gslu.h>
#include <map>
#include <memory>

namespace engine::world
{
struct Sector;
}

namespace engine::objects
{
class Object;
}

namespace engine
{
enum class SlantClass
{
  None,
  Max512,
  Steep
};

struct HeightInfo
{
  core::Length y = 0_len;
  SlantClass slantClass = SlantClass::None;
  const floordata::FloorDataValue* lastCommandSequenceOrDeath = nullptr;

  static bool skipSteepSlants;

  static HeightInfo fromFloor(gsl::not_null<const world::Sector*> roomSector,
                              const core::TRVec& pos,
                              const std::map<uint16_t, gslu::nn_shared<objects::Object>>& objects);

  static HeightInfo fromCeiling(gsl::not_null<const world::Sector*> roomSector,
                                const core::TRVec& pos,
                                const std::map<uint16_t, gslu::nn_shared<objects::Object>>& objects);

  HeightInfo() = default;
};

struct VerticalDistances
{
  //! Floor distance relative to the object
  HeightInfo floor;
  //! Ceiling distance relative to the object's top
  HeightInfo ceiling;

  void init(const gsl::not_null<const world::Sector*>& roomSector,
            const core::TRVec& position,
            const std::map<uint16_t, gslu::nn_shared<objects::Object>>& objects,
            const core::Length& objectY,
            const core::Length& objectHeight)
  {
    floor = HeightInfo::fromFloor(roomSector, position, objects);
    if(floor.y != core::InvalidHeight)
      floor.y -= objectY;

    ceiling = HeightInfo::fromCeiling(roomSector, position, objects);
    if(ceiling.y != core::InvalidHeight)
      ceiling.y -= objectY - objectHeight;
  }
};
} // namespace engine
