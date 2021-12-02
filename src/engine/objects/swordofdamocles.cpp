#include "swordofdamocles.h"

#include "core/magic.h"
#include "core/vec.h"
#include "engine/collisioninfo.h"
#include "engine/location.h"
#include "engine/objectmanager.h"
#include "engine/particle.h"
#include "engine/soundeffects_tr1.h"
#include "engine/world/world.h"
#include "laraobject.h"
#include "modelobject.h"
#include "objectstate.h"
#include "serialization/quantity.h"
#include "serialization/serialization.h"

#include <exception>

namespace engine::objects
{
void SwordOfDamocles::update()
{
  if(m_state.falling)
  {
    m_state.rotation.Y += m_rotateSpeed * 1_rframe;
    if(m_state.fallspeed >= core::TerminalSpeed)
    {
      m_state.fallspeed += core::TerminalGravity;
    }
    else
    {
      m_state.fallspeed += core::Gravity;
    }
    m_state.location.position.Y += m_state.fallspeed.nextFrame();
    m_state.location.position.X += m_dropSpeedX * 1_rframe;
    m_state.location.position.Z += m_dropSpeedZ * 1_rframe;
    if(m_state.location.position.Y > m_state.floor)
    {
      playSoundEffect(TR1SoundEffect::Clatter);
      m_state.location.position.Y = m_state.floor + 10_len;
      m_state.triggerState = TriggerState::Deactivated;
      deactivate();
      m_state.falling = false;
    }
  }
  else if(m_state.location.position.Y != m_state.floor)
  {
    m_state.rotation.Y += m_rotateSpeed * 1_rframe;
    const auto d = getWorld().getObjectManager().getLara().m_state.location.position - m_state.location.position;
    if(abs(d.X) <= 1536_len && abs(d.Z) <= 1536_len && d.Y > 0_len && d.Y < 3 * core::SectorSize)
    {
      m_dropSpeedX = d.X / toAnimUnit(32_frame);
      m_dropSpeedZ = d.Z / toAnimUnit(32_frame);
      m_state.falling = true;
    }
  }

  applyTransform();
}

void SwordOfDamocles::collide(CollisionInfo& collisionInfo)
{
  if(!isNear(getWorld().getObjectManager().getLara(), collisionInfo.collisionRadius))
    return;

  if(collisionInfo.policies.is_set(CollisionInfo::PolicyFlags::EnableBaddiePush))
    enemyPush(collisionInfo, false, true);

  if(!m_state.falling)
    return;

  getWorld().getObjectManager().getLara().m_state.health -= 100_hp;
  const auto tmp = getWorld().getObjectManager().getLara().m_state.location.position
                   + core::TRVec{util::rand15s(128_len), -util::rand15(745_len), util::rand15s(128_len)};
  auto fx = createBloodSplat(getWorld(),
                             Location{m_state.location.room, tmp},
                             getWorld().getObjectManager().getLara().m_state.speed.velocity,
                             util::rand15s(22.5_deg) + m_state.rotation.Y);
  getWorld().getObjectManager().registerParticle(fx);
}

void SwordOfDamocles::serialize(const serialization::Serializer<world::World>& ser)
{
  ModelObject::serialize(ser);
  ser(S_NV("rotateSpeed", m_rotateSpeed), S_NV("dropSpeedX", m_dropSpeedX), S_NV("dropSpeedZ", m_dropSpeedZ));
}
} // namespace engine::objects
