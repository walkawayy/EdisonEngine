#include "dartgun.h"

#include "core/angle.h"
#include "core/id.h"
#include "core/units.h"
#include "core/vec.h"
#include "dart.h"
#include "engine/items_tr1.h"
#include "engine/location.h"
#include "engine/objectmanager.h"
#include "engine/particle.h"
#include "engine/skeletalmodelnode.h"
#include "engine/soundeffects_tr1.h"
#include "engine/world/room.h"
#include "engine/world/world.h"
#include "modelobject.h"
#include "objectstate.h"
#include "qs/quantity.h"
#include "render/scene/node.h"

#include <gsl/gsl-lite.hpp>
#include <gslu.h>
#include <memory>
#include <type_traits>
#include <utility>

void engine::objects::DartGun::update()
{
  if(m_state.updateActivationTimeout())
  {
    if(m_state.current_anim_state == 0_as)
    {
      m_state.goal_anim_state = 1_as;
    }
  }
  else if(m_state.current_anim_state == 1_as)
  {
    m_state.goal_anim_state = 0_as;
  }

  if(m_state.current_anim_state != 1_as || getSkeleton()->getLocalFrame() != 0_rframe)
  {
    ModelObject::update();
    return;
  }

  auto axis = axisFromAngle(m_state.rotation.Y);

  core::TRVec d(0_len, 512_len, 0_len);

  switch(axis)
  {
  case core::Axis::PosZ:
    d.Z += 412_len;
    break;
  case core::Axis::PosX:
    d.X += 412_len;
    break;
  case core::Axis::NegZ:
    d.Z -= 412_len;
    break;
  case core::Axis::NegX:
    d.X -= 412_len;
    break;
  default:
    break;
  }

  auto dart = getWorld().createDynamicObject<Dart>(
    TR1ItemId::Dart, m_state.location.room, m_state.rotation.Y, m_state.location.position - d, 0);
  dart->activate();
  auto& dartState = dart->m_state;
  dartState.triggerState = TriggerState::Active;

  auto particle = gslu::make_nn_shared<SmokeParticle>(dartState.location, getWorld(), dartState.rotation);
  setParent(particle, dartState.location.room->node);
  getWorld().getObjectManager().registerParticle(std::move(particle));

  playSoundEffect(TR1SoundEffect::DartgunShoot);
  ModelObject::update();
}
