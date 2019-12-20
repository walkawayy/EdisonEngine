#include "door.h"

#include "laraobject.h"
#include "serialization/box_ptr.h"

namespace engine::objects
{
// #define NO_DOOR_BLOCK

Door::Door(const gsl::not_null<Engine*>& engine,
           const gsl::not_null<const loader::file::Room*>& room,
           const loader::file::Item& item,
           const gsl::not_null<const loader::file::SkeletalModelType*>& animatedModel)
    : ModelObject{engine, room, item, true, animatedModel}
{
#ifndef NO_DOOR_BLOCK
  core::Length dx = 0_len, dz = 0_len;
  const auto axis = axisFromAngle(m_state.rotation.Y, 45_deg);
  Expects(axis.has_value());
  switch(*axis)
  {
  case core::Axis::PosZ: dz = -core::SectorSize; break;
  case core::Axis::PosX: dx = -core::SectorSize; break;
  case core::Axis::NegZ: dz = core::SectorSize; break;
  case core::Axis::NegX: dx = core::SectorSize; break;
  }

  m_wingsPosition = m_state.position.position + core::TRVec{dx, 0_len, dz};

  m_info.init(*m_state.position.room, m_wingsPosition);
  if(m_info.originalSector.portalTarget != nullptr)
  {
    m_target.init(*m_info.originalSector.portalTarget, m_state.position.position);
  }

  if(m_state.position.room->alternateRoom.get() >= 0)
  {
    m_alternateInfo.init(getEngine().getRooms().at(m_state.position.room->alternateRoom.get()), m_wingsPosition);
    if(m_alternateInfo.originalSector.portalTarget != nullptr)
    {
      m_alternateTarget.init(*m_alternateInfo.originalSector.portalTarget, m_state.position.position);
    }
  }
#endif
}

void Door::update()
{
  if(m_state.updateActivationTimeout())
  {
    if(m_state.current_anim_state == 0_as)
    {
      m_state.goal_anim_state = 1_as;
    }
    else
    {
#ifndef NO_DOOR_BLOCK
      m_info.open();
      m_target.open();
      m_alternateInfo.open();
      m_alternateTarget.open();
#endif
    }
  }
  else
  {
    if(m_state.current_anim_state == 1_as)
    {
      m_state.goal_anim_state = 0_as;
    }
    else
    {
#ifndef NO_DOOR_BLOCK
      m_info.close();
      m_target.close();
      m_alternateInfo.close();
      m_alternateTarget.close();
#endif
    }
  }

  ModelObject::update();
}

void Door::collide(CollisionInfo& collisionInfo)
{
#ifndef NO_DOOR_BLOCK
  if(!isNear(getEngine().getLara(), collisionInfo.collisionRadius))
    return;

  if(!testBoneCollision(getEngine().getLara()))
    return;

  if(!collisionInfo.policyFlags.is_set(CollisionInfo::PolicyFlags::EnableBaddiePush))
    return;

  if(m_state.current_anim_state == m_state.goal_anim_state)
  {
    enemyPush(collisionInfo, false, true);
  }
  else
  {
    const auto enableSpaz = collisionInfo.policyFlags.is_set(CollisionInfo::PolicyFlags::EnableSpaz);
    enemyPush(collisionInfo, enableSpaz, true);
  }
#endif
}

void Door::serialize(const serialization::Serializer& ser)
{
  ModelObject::serialize(ser);
  ser(S_NV("info", m_info),
      S_NV("alternateInfo", m_alternateInfo),
      S_NV("target", m_target),
      S_NV("alternateTarget", m_alternateTarget),
      S_NV("wingsPosition", m_wingsPosition));

  if(ser.loading)
  {
    ser.lazy([this](const serialization::Serializer& ser) {
      m_info.sector
        = const_cast<loader::file::Sector*>(m_state.position.room->getSectorByAbsolutePosition(m_wingsPosition));
      if(m_info.originalSector.portalTarget != nullptr)
      {
        m_target.sector = const_cast<loader::file::Sector*>(
          m_info.originalSector.portalTarget->getSectorByAbsolutePosition(m_state.position.position));
      }

      if(m_state.position.room->alternateRoom.get() >= 0)
      {
        m_alternateInfo.sector = const_cast<loader::file::Sector*>(ser.engine.getRooms()
                                                                     .at(m_state.position.room->alternateRoom.get())
                                                                     .getSectorByAbsolutePosition(m_wingsPosition));
        if(m_alternateInfo.originalSector.portalTarget != nullptr)
        {
          m_alternateTarget.sector = const_cast<loader::file::Sector*>(
            m_alternateInfo.originalSector.portalTarget->getSectorByAbsolutePosition(m_state.position.position));
        }
      }
    });
  }
}

void Door::Info::open()
{
  if(sector == nullptr)
    return;

  *sector = originalSector;
  if(box != nullptr)
    box->blocked = false;
}

void Door::Info::close()
{
  if(sector == nullptr)
    return;

  sector->reset();
  if(box != nullptr)
    box->blocked = true;
}

void Door::Info::init(const loader::file::Room& room, const core::TRVec& wingsPosition)
{
  sector = const_cast<loader::file::Sector*>(room.getSectorByAbsolutePosition(wingsPosition));
  Expects(sector != nullptr);
  originalSector = *sector;

  if(sector->portalTarget == nullptr)
  {
    box = const_cast<loader::file::Box*>(sector->box);
  }
  else
  {
    box = const_cast<loader::file::Box*>(sector->portalTarget->getSectorByAbsolutePosition(wingsPosition)->box);
  }
  if(box != nullptr && !box->blockable)
  {
    box = nullptr;
  }

  close();
}

void Door::Info::serialize(const serialization::Serializer& ser)
{
  ser(S_NV("originalSector", originalSector), S_NV("box", box));
  if(ser.loading)
  {
    sector = nullptr;
    ser.lazy([this](const serialization::Serializer& ser) {
      originalSector.updateCaches(ser.engine.getRooms(), ser.engine.getBoxes(), ser.engine.getFloorData());
    });
  }
}
} // namespace engine::objects