#include "laraobject.h"

#include "core/boundingbox.h"
#include "core/interval.h"
#include "engine/audioengine.h"
#include "engine/cameracontroller.h"
#include "engine/heightinfo.h"
#include "engine/inventory.h"
#include "engine/items_tr1.h"
#include "engine/lara/abstractstatehandler.h"
#include "engine/objectmanager.h"
#include "engine/particle.h"
#include "engine/player.h"
#include "engine/presenter.h"
#include "engine/raycast.h"
#include "engine/skeletalmodelnode.h"
#include "engine/soundeffects_tr1.h"
#include "engine/world/animation.h"
#include "engine/world/rendermeshdata.h"
#include "engine/world/room.h"
#include "engine/world/skeletalmodeltype.h"
#include "engine/world/world.h"
#include "hid/actions.h"
#include "hid/inputhandler.h"
#include "loader/file/animation.h"
#include "modelobject.h"
#include "object.h"
#include "objectstate.h"
#include "render/scene/mesh.h" // IWYU pragma: keep
#include "serialization/objectreference.h"
#include "serialization/optional.h"
#include "serialization/quantity.h"
#include "serialization/serialization.h"
#include "serialization/vector_element.h"
#include "util/helpers.h"

#include <boost/assert.hpp>
#include <boost/log/trivial.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/throw_exception.hpp>
#include <cstdlib>
#include <exception>
#include <gl/api/gl.hpp>
#include <gl/program.h>
#include <gl/renderstate.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gslu.h>
#include <initializer_list>
#include <iosfwd>
#include <limits>
#include <map>
#include <set>
#include <stack>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
core::TRRotationXY getVectorAngles(const core::TRVec& co)
{
  return core::getVectorAngles(co.X, co.Y, co.Z);
}
} // namespace

namespace engine::objects
{
struct RangeXY
{
  core::Interval<core::Angle> y{};
  core::Interval<core::Angle> x{};
};

struct Weapon
{
  WeaponType type = WeaponType::None;
  RangeXY lockAngles{};
  RangeXY leftAngles{};
  RangeXY rightAngles{};
  core::RenderRotationSpeed aimSpeed = 0_deg / 1_rframe;
  core::Angle shotInaccuracy = 0_deg;
  core::Length weaponHeight = 0_len;
  core::Health damage = 0_hp;
  core::Length targetDist = 0_len;
  core::Frame recoilFrame = 0_frame;
  core::Frame flashTime = 0_frame;
  TR1SoundEffect shotSound = TR1SoundEffect::LaraFootstep;
};

namespace
{
const std::unordered_map<WeaponType, Weapon> weapons{{WeaponType::None, Weapon{}},
                                                     {WeaponType::Pistols,
                                                      Weapon{WeaponType::Pistols,
                                                             {{-60_deg, +60_deg}, {-60_deg, +60_deg}},
                                                             {{-170_deg, +60_deg}, {-80_deg, +80_deg}},
                                                             {{-60_deg, +170_deg}, {-80_deg, +80_deg}},
                                                             toRenderUnit(10_deg / 1_frame),
                                                             +4_deg,
                                                             650_len,
                                                             1_hp,
                                                             core::SectorSize * 8.0f,
                                                             9_frame,
                                                             3_frame,
                                                             TR1SoundEffect::LaraShootPistols}},
                                                     {WeaponType::Magnums,
                                                      Weapon{WeaponType::Magnums,
                                                             {{-60_deg, +60_deg}, {-60_deg, +60_deg}},
                                                             {{-170_deg, +60_deg}, {-80_deg, +80_deg}},
                                                             {{-60_deg, +170_deg}, {-80_deg, +80_deg}},
                                                             toRenderUnit(10_deg / 1_frame),
                                                             +4_deg,
                                                             650_len,
                                                             2_hp,
                                                             core::SectorSize * 8.0f,
                                                             9_frame,
                                                             3_frame,
                                                             TR1SoundEffect::CowboyShoot}},
                                                     {WeaponType::Uzis,
                                                      Weapon{WeaponType::Uzis,
                                                             {{-60_deg, +60_deg}, {-60_deg, +60_deg}},
                                                             {{-170_deg, +60_deg}, {-80_deg, +80_deg}},
                                                             {{-60_deg, +170_deg}, {-80_deg, +80_deg}},
                                                             toRenderUnit(10_deg / 1_frame),
                                                             +4_deg,
                                                             650_len,
                                                             1_hp,
                                                             core::SectorSize * 8.0f,
                                                             3_frame,
                                                             2_frame,
                                                             TR1SoundEffect::LaraShootUzis}},
                                                     {WeaponType::Shotgun,
                                                      Weapon{WeaponType::Shotgun,
                                                             {{-60_deg, +60_deg}, {-55_deg, +55_deg}},
                                                             {{-80_deg, +80_deg}, {-65_deg, +65_deg}},
                                                             {{-80_deg, +80_deg}, {-65_deg, +65_deg}},
                                                             toRenderUnit(10_deg / 1_frame),
                                                             0_deg,
                                                             500_len,
                                                             4_hp,
                                                             core::SectorSize * 8.0f,
                                                             9_frame,
                                                             3_frame,
                                                             TR1SoundEffect::LaraShootShotgun}}};

constexpr size_t BoneHips = 0;
constexpr size_t BoneThighR = 1;
constexpr size_t BoneCalfR = 2;
constexpr size_t BoneFootR = 3;
constexpr size_t BoneThighL = 4;
constexpr size_t BoneCalfL = 5;
constexpr size_t BoneFootL = 6;
constexpr size_t BoneTorso = 7;
constexpr size_t BoneArmL = 8;
constexpr size_t BoneForeArmL = 9;
constexpr size_t BoneHandL = 10;
constexpr size_t BoneArmR = 11;
constexpr size_t BoneForeArmR = 12;
constexpr size_t BoneHandR = 13;
constexpr size_t BoneHead = 14;
} // namespace

void LaraObject::setAnimation(AnimationId anim, const std::optional<core::Frame>& firstFrame)
{
  getSkeleton()->setAnimation(
    m_state.current_anim_state, gsl::not_null{&getWorld().getAnimation(anim)}, firstFrame.value_or(0_frame));
}

void LaraObject::handleLaraStateOnLand()
{
  CollisionInfo collisionInfo;
  collisionInfo.initialPosition = m_state.location.position;
  collisionInfo.collisionRadius = core::DefaultCollisionRadius;
  collisionInfo.policies = CollisionInfo::SpazPushPolicy;

  lara::AbstractStateHandler::create(getCurrentAnimState(), *this)
    ->handleInput(collisionInfo, isPhysicsFrame(getSkeleton()->getLocalFrame()));

  if(getWorld().getCameraController().getMode() != CameraMode::FreeLook)
  {
    if(abs(m_headRotation.X) >= 2_deg)
    {
      m_headRotation.X -= toRenderUnit(m_headRotation.X / 8 / 1_frame) * 1_rframe;
    }
    else
    {
      m_headRotation.X = 0_deg;
    }
    if(abs(m_headRotation.Y) >= 2_deg)
    {
      m_headRotation.Y -= toRenderUnit(m_headRotation.Y / 8 / 1_frame) * 1_rframe;
    }
    else
    {
      m_headRotation.Y = 0_deg;
    }
    m_torsoRotation = m_headRotation;
  }

  // "slowly" revert rotations to zero
  if(m_state.rotation.Z < -1_deg)
  {
    m_state.rotation.Z += toRenderUnit(1_deg / 1_frame) * 1_rframe;
    if(m_state.rotation.Z >= 0_deg)
    {
      m_state.rotation.Z = 0_deg;
    }
  }
  else if(m_state.rotation.Z > 1_deg)
  {
    m_state.rotation.Z -= toRenderUnit(1_deg / 1_frame) * 1_rframe;
    if(m_state.rotation.Z <= 0_deg)
    {
      m_state.rotation.Z = 0_deg;
    }
  }
  else
  {
    m_state.rotation.Z = 0_deg;
  }

  if(getYRotationSpeed() > core::TurnSpeedDeceleration * 1_rframe)
  {
    subYRotationSpeed(core::TurnSpeedDeceleration);
  }
  else if(getYRotationSpeed() < -core::TurnSpeedDeceleration * 1_rframe)
  {
    addYRotationSpeed(core::TurnSpeedDeceleration);
  }
  else
  {
    setYRotationSpeed(0_deg / 1_rframe);
  }

  m_state.rotation.Y += m_yRotationSpeed * 1_rframe;

  updateImpl();

  testInteractions(collisionInfo);

  lara::AbstractStateHandler::create(getCurrentAnimState(), *this)
    ->postprocessFrame(collisionInfo, isPhysicsFrame(getSkeleton()->getLocalFrame()));

  updateFloorHeight(-381_len);

  updateLarasWeaponsStatus();
  getWorld().handleCommandSequence(collisionInfo.mid.floor.lastCommandSequenceOrDeath, false);

  drawRoutine();
  applyTransform();
}

void LaraObject::handleLaraStateDiving()
{
  CollisionInfo collisionInfo;
  collisionInfo.initialPosition = m_state.location.position;
  collisionInfo.collisionRadius = core::DefaultCollisionRadiusUnderwater;
  collisionInfo.policies.reset_all();
  collisionInfo.validCeilingHeightMin = core::LaraDiveHeight;
  collisionInfo.validFloorHeight = {-core::LaraDiveHeight, core::HeightLimit};

  lara::AbstractStateHandler::create(getCurrentAnimState(), *this)
    ->handleInput(collisionInfo, isPhysicsFrame(getSkeleton()->getLocalFrame()));

  // "slowly" revert rotations to zero
  if(m_state.rotation.Z < -2_deg)
  {
    m_state.rotation.Z += toRenderUnit(2_deg / 1_frame) * 1_rframe;
  }
  else
  {
    if(m_state.rotation.Z > 2_deg)
    {
      m_state.rotation.Z -= toRenderUnit(2_deg / 1_frame) * 1_rframe;
    }
    else
    {
      m_state.rotation.Z = 0_deg;
    }
  }
  const core::Angle x = std::clamp(m_state.rotation.X, -100_deg, +100_deg);
  m_state.rotation.X = x;
  const core::Angle z = std::clamp(m_state.rotation.Z, -22_deg, +22_deg);
  m_state.rotation.Z = z;

  if(m_underwaterCurrentStrength != 0_len)
  {
    handleUnderwaterCurrent(collisionInfo);
  }

  updateImpl();

  m_state.location.move(util::yawPitch(m_state.fallspeed.nextFrame() / 4.0f, m_state.rotation));

  testInteractions(collisionInfo);

  lara::AbstractStateHandler::create(getCurrentAnimState(), *this)
    ->postprocessFrame(collisionInfo, isPhysicsFrame(getSkeleton()->getLocalFrame()));

  updateFloorHeight(0_len);
  updateLarasWeaponsStatus();
  getWorld().handleCommandSequence(collisionInfo.mid.floor.lastCommandSequenceOrDeath, false);
}

void LaraObject::handleLaraStateSwimming()
{
  CollisionInfo collisionInfo;
  collisionInfo.initialPosition = m_state.location.position;
  collisionInfo.collisionRadius = core::DefaultCollisionRadius;
  collisionInfo.policies.reset_all();
  collisionInfo.validCeilingHeightMin = core::DefaultCollisionRadius;
  collisionInfo.validFloorHeight = {-core::DefaultCollisionRadius, core::HeightLimit};

  setCameraRotationAroundLaraX(-22_deg);

  lara::AbstractStateHandler::create(getCurrentAnimState(), *this)
    ->handleInput(collisionInfo, isPhysicsFrame(getSkeleton()->getLocalFrame()));

  // "slowly" revert rotations to zero
  if(m_state.rotation.Z < 0_deg)
  {
    m_state.rotation.Z += toRenderUnit(2_deg / 1_frame) * 1_rframe;
  }
  else if(m_state.rotation.Z > 2_deg)
  {
    m_state.rotation.Z -= toRenderUnit(2_deg / 1_frame) * 1_rframe;
  }
  else
  {
    m_state.rotation.Z = 0_deg;
  }

  if(getWorld().getCameraController().getMode() != CameraMode::FreeLook)
  {
    m_headRotation.X -= toRenderUnit(m_headRotation.X / 8 / 1_frame) * 1_rframe;
    m_headRotation.Y -= toRenderUnit(m_headRotation.Y / 8 / 1_frame) * 1_rframe;
    m_torsoRotation.X = 0_deg;
    m_torsoRotation.Y /= 2.0f;
  }

  if(m_underwaterCurrentStrength != 0_len)
  {
    handleUnderwaterCurrent(collisionInfo);
  }

  updateImpl();

  m_state.location.move(util::pitch(m_state.fallspeed.nextFrame() / 4, getMovementAngle()).toRenderSystem());

  testInteractions(collisionInfo);

  lara::AbstractStateHandler::create(getCurrentAnimState(), *this)
    ->postprocessFrame(collisionInfo, isPhysicsFrame(getSkeleton()->getLocalFrame()));

  updateFloorHeight(core::DefaultCollisionRadius);
  updateLarasWeaponsStatus();
  getWorld().handleCommandSequence(collisionInfo.mid.floor.lastCommandSequenceOrDeath, false);
}

void LaraObject::placeOnFloor(const CollisionInfo& collisionInfo)
{
  m_state.location.position.Y += collisionInfo.mid.floor.y;
}

LaraObject::~LaraObject() = default;

void LaraObject::update()
{
  if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::DrawPistols))
    getWorld().getPlayer().getInventory().tryUse(*this, TR1ItemId::Pistols);
  else if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::DrawShotgun))
    getWorld().getPlayer().getInventory().tryUse(*this, TR1ItemId::Shotgun);
  else if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::DrawUzis))
    getWorld().getPlayer().getInventory().tryUse(*this, TR1ItemId::Uzis);
  else if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::DrawMagnums))
    getWorld().getPlayer().getInventory().tryUse(*this, TR1ItemId::Magnums);
  else if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::ConsumeSmallMedipack))
    getWorld().getPlayer().getInventory().tryUse(*this, TR1ItemId::SmallMedipack);
  else if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::ConsumeLargeMedipack))
    getWorld().getPlayer().getInventory().tryUse(*this, TR1ItemId::LargeMedipack);

#ifndef NDEBUG
  if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::CheatDive))
    m_cheatDive = !m_cheatDive;
#endif

  if(m_underwaterState == UnderwaterState::OnLand && (m_cheatDive || m_state.location.room->isWaterRoom))
  {
    m_air = core::LaraAir;
    m_underwaterState = UnderwaterState::Diving;
    m_state.falling = false;
    m_state.location.position.Y += 100_len;
    updateFloorHeight(0_len);
    getWorld().getAudioEngine().stopSoundEffect(TR1SoundEffect::LaraScream, &m_state);
    if(getCurrentAnimState() == LaraStateId::SwandiveBegin)
    {
      m_state.rotation.X = -45_deg;
      setGoalAnimState(LaraStateId::UnderwaterDiving);
      updateImpl();
      m_state.fallspeed.velocity *= 2;
    }
    else if(getCurrentAnimState() == LaraStateId::SwandiveEnd)
    {
      m_state.rotation.X = -85_deg;
      setGoalAnimState(LaraStateId::UnderwaterDiving);
      updateImpl();
      m_state.fallspeed.velocity *= 2;
    }
    else
    {
      m_state.rotation.X = -45_deg;
      setAnimation(AnimationId::FREE_FALL_TO_UNDERWATER);
      setGoalAnimState(LaraStateId::UnderwaterForward);
      setCurrentAnimState(LaraStateId::UnderwaterDiving);
      m_state.fallspeed.velocity += m_state.fallspeed.velocity / 2;
    }

    resetHeadTorsoRotation();

    if(const auto waterSurfaceHeight = getWaterSurfaceHeight())
    {
      playSoundEffect(TR1SoundEffect::LaraFallIntoWater);

      auto surfaceLocation = m_state.location;
      surfaceLocation.updateRoom();
      for(int i = 0; i < 10; ++i)
      {
        surfaceLocation.position.X = m_state.location.position.X;
        surfaceLocation.position.Y = *waterSurfaceHeight;
        surfaceLocation.position.Z = m_state.location.position.Z;

        auto particle = gslu::make_nn_shared<SplashParticle>(surfaceLocation, getWorld(), false);
        setParent(particle, surfaceLocation.room->node);
        getWorld().getObjectManager().registerParticle(particle);
      }
    }
  }
  else if(m_underwaterState == UnderwaterState::Diving && !(m_cheatDive || m_state.location.room->isWaterRoom))
  {
    auto waterSurfaceHeight = getWaterSurfaceHeight();
    m_state.fallspeed = 0_spd;
    m_state.rotation.X = 0_deg;
    m_state.rotation.Z = 0_deg;
    resetHeadTorsoRotation();
    m_handStatus = HandStatus::None;

    if(!waterSurfaceHeight || abs(*waterSurfaceHeight - m_state.location.position.Y) >= core::QuarterSectorSize)
    {
      m_underwaterState = UnderwaterState::OnLand;
      setAnimation(AnimationId::FREE_FALL_FORWARD);
      setGoalAnimState(LaraStateId::JumpForward);
      setCurrentAnimState(LaraStateId::JumpForward);
      m_state.speed = std::exchange(m_state.fallspeed.velocity, 0_spd) / 4;
      m_state.falling = true;
    }
    else
    {
      m_underwaterState = UnderwaterState::Swimming;
      setAnimation(AnimationId::UNDERWATER_TO_ONWATER);
      setGoalAnimState(LaraStateId::OnWaterStop);
      setCurrentAnimState(LaraStateId::OnWaterStop);
      m_state.location.position.Y = *waterSurfaceHeight + 1_len;
      m_swimToDiveKeypressDuration = toAnimUnit(11_frame);
      updateFloorHeight(-381_len);
      playSoundEffect(TR1SoundEffect::LaraCatchingAir);
    }
  }
  else if(m_underwaterState == UnderwaterState::Swimming && !(m_cheatDive || m_state.location.room->isWaterRoom))
  {
    m_underwaterState = UnderwaterState::OnLand;
    setAnimation(AnimationId::FREE_FALL_FORWARD);
    setGoalAnimState(LaraStateId::JumpForward);
    setCurrentAnimState(LaraStateId::JumpForward);
    m_state.speed = std::exchange(m_state.fallspeed.velocity, 0_spd) / 4;
    m_state.falling = true;
    m_handStatus = HandStatus::None;
    m_state.rotation.X = 0_deg;
    m_state.rotation.Z = 0_deg;
    resetHeadTorsoRotation();
  }

  if(m_underwaterState == UnderwaterState::OnLand)
  {
    m_air = core::LaraAir;
    handleLaraStateOnLand();
  }
  else if(m_underwaterState == UnderwaterState::Diving)
  {
    if(!isDead() && !m_cheatDive)
    {
      m_air -= 1_rframe;
      if(m_air < 0_rframe)
      {
        m_air = -1_rframe;
        m_state.health -= 5_hp;
      }
    }
    handleLaraStateDiving();
  }
  else if(m_underwaterState == UnderwaterState::Swimming)
  {
    if(!isDead())
    {
      m_air = std::min(m_air + (core::RenderFrameRate * 1_sec / 3).cast<core::RenderFrame>(), core::LaraAir);
    }
    handleLaraStateSwimming();
  }
}

void LaraObject::updateImpl()
{
  const auto endOfAnim = getSkeleton()->advanceFrame(m_state);

  Expects(getSkeleton()->getAnim() != nullptr);
  if(endOfAnim)
  {
    if(getSkeleton()->getAnim()->animCommandCount > 0)
    {
      const auto* cmd = getSkeleton()->getAnim()->animCommands;
      for(uint16_t i = 0; i < getSkeleton()->getAnim()->animCommandCount; ++i)
      {
        Expects(cmd < &getWorld().getAnimCommands().back());
        const auto opcode = static_cast<AnimCommandOpcode>(*cmd);
        ++cmd;
        switch(opcode)
        {
        case AnimCommandOpcode::SetPosition:
          moveLocal(core::TRVec{core::Length{static_cast<core::Length::type>(cmd[0])},
                                core::Length{static_cast<core::Length::type>(cmd[1])},
                                core::Length{static_cast<core::Length::type>(cmd[2])}});
          cmd += 3;
          break;
        case AnimCommandOpcode::StartFalling:
          if(m_fallSpeedOverride != 0_spd)
          {
            m_state.fallspeed = std::exchange(m_fallSpeedOverride, 0_spd);
          }
          else
          {
            m_state.fallspeed = core::Speed{static_cast<core::Speed::type>(cmd[0])};
          }
          m_state.speed = core::Speed{static_cast<core::Speed::type>(cmd[1])};
          m_state.falling = true;
          cmd += 2;
          break;
        case AnimCommandOpcode::EmptyHands:
          setHandStatus(HandStatus::None);
          break;
          // NOLINTNEXTLINE(bugprone-branch-clone)
        case AnimCommandOpcode::PlaySound:
          cmd += 2;
          break;
        case AnimCommandOpcode::PlayEffect:
          cmd += 2;
          break;
        default:
          break;
        }
      }
    }

    getSkeleton()->setAnimation(m_state.current_anim_state,
                                gsl::not_null{getSkeleton()->getAnim()->nextAnimation},
                                getSkeleton()->getAnim()->nextFrame);
  }

  if(getSkeleton()->getAnim()->animCommandCount > 0)
  {
    const auto* cmd = getSkeleton()->getAnim()->animCommands;
    for(uint16_t i = 0; i < getSkeleton()->getAnim()->animCommandCount; ++i)
    {
      Expects(cmd < &getWorld().getAnimCommands().back());
      const auto opcode = static_cast<AnimCommandOpcode>(*cmd);
      ++cmd;
      switch(opcode)
      {
      case AnimCommandOpcode::SetPosition:
        cmd += 3;
        break;
      case AnimCommandOpcode::StartFalling:
        cmd += 2;
        break;
      case AnimCommandOpcode::PlaySound:
        if(getSkeleton()->getFrame() == toAnimUnit(core::Frame{gsl::narrow_cast<core::Frame::type>(cmd[0])}))
        {
          playSoundEffect(static_cast<TR1SoundEffect>(cmd[1]));
        }
        cmd += 2;
        break;
      case AnimCommandOpcode::PlayEffect:
        if(getSkeleton()->getFrame() == toAnimUnit(core::Frame{gsl::narrow_cast<core::Frame::type>(cmd[0])}))
        {
          BOOST_LOG_TRIVIAL(debug) << "Anim effect: " << int(cmd[1]);
          getWorld().runEffect(cmd[1], this);
        }
        cmd += 2;
        break;
      default:
        break;
      }
    }
  }

  applyMovement(true);
}

void LaraObject::updateFloorHeight(const core::Length& dy)
{
  auto location = m_state.location;
  location.position.Y += dy;
  const auto sector = location.updateRoom();
  setCurrentRoom(m_state.location.room);
  const HeightInfo hi = HeightInfo::fromFloor(
    sector, m_state.location.position - core::TRVec{0_len, dy, 0_len}, getWorld().getObjectManager().getObjects());
  m_state.floor = hi.y;
  setCurrentRoom(location.room);
}

void LaraObject::setCameraRotationAroundLara(const core::Angle& x, const core::Angle& y)
{
  getWorld().getCameraController().setRotationAroundLara(x, y);
}

void LaraObject::setCameraRotationAroundLaraY(const core::Angle& y)
{
  getWorld().getCameraController().setRotationAroundLaraY(y);
}

void LaraObject::setCameraRotationAroundLaraX(const core::Angle& x)
{
  getWorld().getCameraController().setRotationAroundLaraX(x);
}

void LaraObject::setCameraDistance(const core::Length& d)
{
  getWorld().getCameraController().setDistance(d);
}

void LaraObject::setCameraModifier(const CameraModifier k)
{
  getWorld().getCameraController().setModifier(k);
}

void LaraObject::testInteractions(CollisionInfo& collisionInfo)
{
  m_state.is_hit = false;
  hit_direction.reset();

  if(isDead())
    return;

  std::set<gsl::not_null<const world::Room*>> rooms;
  rooms.insert(m_state.location.room);
  for(const world::Portal& p : m_state.location.room->portals)
    rooms.insert(p.adjoiningRoom);

  const auto execCollisions = [this, &rooms, &collisionInfo](const auto& range)
  {
    for(const auto& object : range)
    {
      if(!object->m_state.collidable || object->m_state.triggerState == TriggerState::Invisible)
        continue;

      if(rooms.find(object->m_state.location.room) == rooms.end())
        continue;

      const auto d = m_state.location.position - object->m_state.location.position;
      if(abs(d.X) >= 4 * core::SectorSize || abs(d.Y) >= 4 * core::SectorSize || abs(d.Z) >= 4 * core::SectorSize)
        continue;

      object->collide(collisionInfo);
    }
  };

  auto& objectManager = getWorld().getObjectManager();
  execCollisions(objectManager.getObjects() | boost::adaptors::map_values);
  execCollisions(objectManager.getDynamicObjects());

  auto& lara = objectManager.getLara();
  if(lara.explosionStumblingDuration != 0_rframe)
  {
    lara.updateExplosionStumbling();
  }
  if(!lara.hit_direction.has_value())
  {
    lara.hit_frame = 0_rframe;
  }
  // TODO selectedPuzzleKey = -1;
}

void LaraObject::handleUnderwaterCurrent(CollisionInfo& collisionInfo)
{
  if(m_cheatDive)
    return;

  core::TRVec targetPos;
  if(!m_underwaterRoute.calculateTarget(getWorld(), targetPos, m_state.location.position, m_state.getCurrentBox()))
    return;

  targetPos -= m_state.location.position;
  m_state.location.position.X
    += toRenderUnit(std::clamp(targetPos.X, -m_underwaterCurrentStrength, m_underwaterCurrentStrength) / 1_frame)
       * 1_rframe;
  m_state.location.position.Y
    += toRenderUnit(std::clamp(targetPos.Y, -m_underwaterCurrentStrength, m_underwaterCurrentStrength) / 1_frame)
       * 1_rframe;
  m_state.location.position.Z
    += toRenderUnit(std::clamp(targetPos.Z, -m_underwaterCurrentStrength, m_underwaterCurrentStrength) / 1_frame)
       * 1_rframe;

  m_underwaterCurrentStrength = 0_len;
  collisionInfo.facingAngle = angleFromAtan(m_state.location.position.X - collisionInfo.initialPosition.X,
                                            m_state.location.position.Z - collisionInfo.initialPosition.Z);

  collisionInfo.initHeightInfo(m_state.location.position + core::TRVec{0_len, core::LaraDiveGroundElevation, 0_len},
                               getWorld(),
                               core::LaraDiveHeight);
  switch(collisionInfo.collisionType)
  {
  case CollisionInfo::AxisColl::Front:
    if(m_state.rotation.X > 35_deg)
      m_state.rotation.X += toRenderUnit(2_deg / 1_frame) * 1_rframe;
    else if(m_state.rotation.X < -35_deg)
      m_state.rotation.X -= toRenderUnit(2_deg / 1_frame) * 1_rframe;
    break;
  case CollisionInfo::AxisColl::Top:
    m_state.rotation.X -= toRenderUnit(2_deg / 1_frame) * 1_rframe;
    break;
  case CollisionInfo::AxisColl::FrontTop:
    m_state.fallspeed = 0_spd;
    break;
  case CollisionInfo::AxisColl::FrontLeft:
    m_state.rotation.Y += toRenderUnit(5_deg / 1_frame) * 1_rframe;
    break;
  case CollisionInfo::AxisColl::FrontRight:
    m_state.rotation.Y -= toRenderUnit(5_deg / 1_frame) * 1_rframe;
    break;
  default:
    break;
  }

  if(collisionInfo.mid.floor.y < 0_len)
  {
    m_state.location.position.Y += collisionInfo.mid.floor.y;
    m_state.rotation.X += toRenderUnit(2_deg / 1_frame) * 1_rframe;
  }
  applyShift(collisionInfo);
  collisionInfo.initialPosition = m_state.location.position;
}

void LaraObject::updateLarasWeaponsStatus()
{
  if(leftArm.flashTimeout > 0_rframe)
  {
    leftArm.flashTimeout -= 1_rframe;
  }
  if(rightArm.flashTimeout > 0_rframe)
  {
    rightArm.flashTimeout -= 1_rframe;
  }

  auto& player = getWorld().getPlayer();

  bool doHolsterUpdate = false;
  if(isDead())
  {
    m_handStatus = HandStatus::None;
  }
  else
  {
    if(m_underwaterState != UnderwaterState::OnLand)
    {
      if(m_handStatus == HandStatus::Combat)
      {
        doHolsterUpdate = true;
      }
    }
    else if(player.requestedWeaponType == player.selectedWeaponType)
    {
      if(getWorld().getPresenter().getInputHandler().hasDebouncedAction(hid::Action::Holster))
      {
        doHolsterUpdate = true;
      }
    }
    else if(m_handStatus == HandStatus::Combat)
    {
      doHolsterUpdate = true;
    }
    else if(m_handStatus == HandStatus::None)
    {
      player.selectedWeaponType = player.requestedWeaponType;
      initWeaponAnimData();
      doHolsterUpdate = true;
    }
  }

  if(doHolsterUpdate && player.selectedWeaponType != WeaponType::None)
  {
    if(m_handStatus == HandStatus::None)
    {
      rightArm.frame = 0_rframe;
      leftArm.frame = 0_rframe;
      m_handStatus = HandStatus::DrawWeapon;
    }
    else if(m_handStatus == HandStatus::Combat)
    {
      m_handStatus = HandStatus::Holster;
    }
  }

  if(m_handStatus == HandStatus::DrawWeapon)
  {
    if(player.selectedWeaponType != WeaponType::None)
    {
      if(getWorld().getCameraController().getMode() != CameraMode::Cinematic
         && getWorld().getCameraController().getMode() != CameraMode::FreeLook)
      {
        getWorld().getCameraController().setMode(CameraMode::Combat);
      }

      if(player.selectedWeaponType != WeaponType::Shotgun)
      {
        drawWeapons(player.selectedWeaponType);
      }
      else
      {
        drawShotgun();
      }
    }
  }
  else if(m_handStatus == HandStatus::Holster)
  {
    {
      const auto& normalLara = *getWorld().findAnimatedModelForType(TR1ItemId::Lara);
      BOOST_ASSERT(normalLara.bones.size() == getSkeleton()->getBoneCount());
      getSkeleton()->setMeshPart(BoneHead, normalLara.bones[BoneHead].mesh);
      getSkeleton()->rebuildMesh();
    }

    switch(getWorld().getPlayer().selectedWeaponType)
    {
    case WeaponType::Pistols:
      [[fallthrough]];
    case WeaponType::Magnums:
      [[fallthrough]];
    case WeaponType::Uzis:
      holsterWeapons(getWorld().getPlayer().selectedWeaponType);
      break;
    case WeaponType::Shotgun:
      holsterShotgun();
      break;
    }
  }
  else if(m_handStatus == HandStatus::Combat)
  {
    {
      const auto& normalLara = *getWorld().findAnimatedModelForType(TR1ItemId::Lara);
      BOOST_ASSERT(normalLara.bones.size() == getSkeleton()->getBoneCount());
      getSkeleton()->setMeshPart(BoneHead, normalLara.bones[BoneHead].mesh);
    }

    switch(getWorld().getPlayer().selectedWeaponType)
    {
    case WeaponType::Pistols:
      if(getWorld().getPlayer().getInventory().getAmmo(WeaponType::Pistols).ammo != 0)
      {
        if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
        {
          const auto& uziLara = *getWorld().findAnimatedModelForType(TR1ItemId::LaraUzisAnim);
          BOOST_ASSERT(uziLara.bones.size() == getSkeleton()->getBoneCount());
          getSkeleton()->setMeshPart(BoneHead, uziLara.bones[BoneHead].mesh);
        }
      }
      if(getWorld().getCameraController().getMode() != CameraMode::Cinematic
         && getWorld().getCameraController().getMode() != CameraMode::FreeLook)
      {
        getWorld().getCameraController().setMode(CameraMode::Combat);
      }
      updateWeapons(getWorld().getPlayer().selectedWeaponType);
      break;
    case WeaponType::Magnums:
      if(getWorld().getPlayer().getInventory().getAmmo(WeaponType::Magnums).ammo != 0)
      {
        if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
        {
          const auto& uziLara = *getWorld().findAnimatedModelForType(TR1ItemId::LaraUzisAnim);
          BOOST_ASSERT(uziLara.bones.size() == getSkeleton()->getBoneCount());
          getSkeleton()->setMeshPart(BoneHead, uziLara.bones[BoneHead].mesh);
        }
      }
      if(getWorld().getCameraController().getMode() != CameraMode::Cinematic
         && getWorld().getCameraController().getMode() != CameraMode::FreeLook)
      {
        getWorld().getCameraController().setMode(CameraMode::Combat);
      }
      updateWeapons(getWorld().getPlayer().selectedWeaponType);
      break;
    case WeaponType::Uzis:
      if(getWorld().getPlayer().getInventory().getAmmo(WeaponType::Uzis).ammo != 0)
      {
        if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
        {
          const auto& uziLara = *getWorld().findAnimatedModelForType(TR1ItemId::LaraUzisAnim);
          BOOST_ASSERT(uziLara.bones.size() == getSkeleton()->getBoneCount());
          getSkeleton()->setMeshPart(BoneHead, uziLara.bones[BoneHead].mesh);
        }
      }
      if(getWorld().getCameraController().getMode() != CameraMode::Cinematic
         && getWorld().getCameraController().getMode() != CameraMode::FreeLook)
      {
        getWorld().getCameraController().setMode(CameraMode::Combat);
      }
      updateWeapons(getWorld().getPlayer().selectedWeaponType);
      break;
    case WeaponType::Shotgun:
      if(getWorld().getPlayer().getInventory().getAmmo(WeaponType::Shotgun).ammo != 0)
      {
        if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
        {
          const auto& uziLara = *getWorld().findAnimatedModelForType(TR1ItemId::LaraUzisAnim);
          BOOST_ASSERT(uziLara.bones.size() == getSkeleton()->getBoneCount());
          getSkeleton()->setMeshPart(BoneHead, uziLara.bones[BoneHead].mesh);
        }
      }
      if(getWorld().getCameraController().getMode() != CameraMode::Cinematic
         && getWorld().getCameraController().getMode() != CameraMode::FreeLook)
      {
        getWorld().getCameraController().setMode(CameraMode::Combat);
      }
      updateShotgun();
      break;
    default:
      break;
    }
    getSkeleton()->rebuildMesh();
  }
}

void LaraObject::updateShotgun()
{
  if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
  {
    updateAimingState(weapons.at(WeaponType::Shotgun));
  }
  else
  {
    aimAt = nullptr;
  }
  if(aimAt == nullptr)
  {
    findTarget(weapons.at(WeaponType::Shotgun));
  }
  updateAimAngles(weapons.at(WeaponType::Shotgun), leftArm);
  if(leftArm.aiming)
  {
    m_torsoRotation.X = leftArm.aimRotation.X / 2;
    m_torsoRotation.Y = leftArm.aimRotation.Y / 2;
    m_headRotation.X = 0_deg;
    m_headRotation.Y = 0_deg;
  }
  updateAnimShotgun();
}

void LaraObject::updateWeapons(WeaponType weaponType)
{
  const auto& weapon = weapons.at(weaponType);
  if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
  {
    updateAimingState(weapon);
  }
  else
  {
    aimAt = nullptr;
  }
  if(aimAt == nullptr)
  {
    findTarget(weapon);
  }
  updateAimAngles(weapon, leftArm);
  updateAimAngles(weapon, rightArm);
  if(leftArm.aiming && !rightArm.aiming)
  {
    m_headRotation.Y = m_torsoRotation.Y = leftArm.aimRotation.Y / 2;
    m_headRotation.X = m_torsoRotation.X = leftArm.aimRotation.X / 2;
  }
  else if(rightArm.aiming && !leftArm.aiming)
  {
    m_headRotation.Y = m_torsoRotation.Y = rightArm.aimRotation.Y / 2;
    m_headRotation.X = m_torsoRotation.X = rightArm.aimRotation.X / 2;
  }
  else if(leftArm.aiming && rightArm.aiming)
  {
    m_headRotation.Y = m_torsoRotation.Y = (leftArm.aimRotation.Y + rightArm.aimRotation.Y) / 4;
    m_headRotation.X = m_torsoRotation.X = (leftArm.aimRotation.X + rightArm.aimRotation.X) / 4;
  }

  updateAnimNotShotgun(weaponType);
}

void LaraObject::updateAimingState(const Weapon& weapon)
{
  if(aimAt == nullptr)
  {
    rightArm.aiming = false;
    leftArm.aiming = false;
    m_weaponTargetVector.X = 0_deg;
    m_weaponTargetVector.Y = 0_deg;
    return;
  }

  Location weaponLocation{m_state.location};
  weaponLocation.position.Y -= weapon.weaponHeight;
  const auto enemyChestPos = getUpperThirdBBoxCtr(*aimAt);
  auto targetVector = getVectorAngles(enemyChestPos.position - weaponLocation.position);
  targetVector.X = normalizeAngle(targetVector.X - m_state.rotation.X);
  targetVector.Y = normalizeAngle(targetVector.Y - m_state.rotation.Y);
  if(!raycastLineOfSight(weaponLocation, enemyChestPos.position, getWorld().getObjectManager()).first)
  {
    rightArm.aiming = false;
    leftArm.aiming = false;
  }
  else if(!weapon.lockAngles.y.contains(targetVector.Y) || !weapon.lockAngles.x.contains(targetVector.X))
  {
    if(leftArm.aiming)
    {
      if(!weapon.leftAngles.y.contains(targetVector.Y) || !weapon.leftAngles.x.contains(targetVector.X))
      {
        leftArm.aiming = false;
      }
    }
    if(rightArm.aiming)
    {
      if(!weapon.rightAngles.y.contains(targetVector.Y) || !weapon.rightAngles.x.contains(targetVector.X))
      {
        rightArm.aiming = false;
      }
    }
  }
  else
  {
    rightArm.aiming = true;
    leftArm.aiming = true;
  }
  m_weaponTargetVector = targetVector;
}

void LaraObject::initWeaponAnimData()
{
  leftArm.reset();
  rightArm.reset();

  rightArm.flashTimeout = 0_rframe;
  leftArm.flashTimeout = 0_rframe;
  aimAt = nullptr;
  switch(getWorld().getPlayer().selectedWeaponType)
  {
  case WeaponType::None:
    leftArm.weaponAnimData = rightArm.weaponAnimData = getWorld().findAnimatedModelForType(TR1ItemId::Lara)->frames;
    break;
  case WeaponType::Pistols:
    [[fallthrough]];
  case WeaponType::Magnums:
    [[fallthrough]];
  case WeaponType::Uzis:
    leftArm.weaponAnimData = rightArm.weaponAnimData
      = getWorld().findAnimatedModelForType(TR1ItemId::LaraPistolsAnim)->frames;
    if(m_handStatus != HandStatus::None && m_handStatus != HandStatus::Grabbing)
    {
      overrideLaraMeshesDrawWeapons(getWorld().getPlayer().selectedWeaponType);
    }
    break;
  case WeaponType::Shotgun:
    leftArm.weaponAnimData = rightArm.weaponAnimData
      = getWorld().findAnimatedModelForType(TR1ItemId::LaraShotgunAnim)->frames;
    if(m_handStatus != HandStatus::None && m_handStatus != HandStatus::Grabbing)
    {
      overrideLaraMeshesDrawShotgun();
    }
    break;
  }
}

Location LaraObject::getUpperThirdBBoxCtr(const ModelObject& object)
{
  const auto kf = object.getSkeleton()->getInterpolationInfo().getNearestFrame();
  const auto bbox = kf->bbox.toBBox();

  const auto ctrX = bbox.x.mid();
  const auto ctrZ = bbox.z.mid();
  const auto ctrY3 = bbox.y.size() / 3 + bbox.y.min;

  Location result{object.m_state.location};
  result.position += util::pitch(core::TRVec{ctrX, ctrY3, ctrZ}, object.m_state.rotation.Y);
  return result;
}

void LaraObject::drawWeapons(WeaponType weaponType)
{
  auto nextFrame = leftArm.frame + 1_rframe;
  if(nextFrame < toAnimUnit(5_frame) || nextFrame > toAnimUnit(23_frame))
  {
    nextFrame = toAnimUnit(5_frame);
  }
  else if(nextFrame == toAnimUnit(13_frame))
  {
    overrideLaraMeshesDrawWeapons(weaponType);
    playSoundEffect(TR1SoundEffect::LaraDrawWeapon);
  }
  else if(nextFrame == toAnimUnit(23_frame))
  {
    initAimInfoPistol();
    nextFrame = 0_rframe;
  }

  leftArm.frame = nextFrame;
  rightArm.frame = nextFrame;
}

void LaraObject::findTarget(const Weapon& weapon)
{
  Location weaponLocation{m_state.location};
  weaponLocation.position.Y -= weapons.at(WeaponType::Shotgun).weaponHeight;
  aimAt.reset();
  core::Angle bestYAngle{std::numeric_limits<core::Angle::type>::max()};
  for(const auto& currentEnemy : getWorld().getObjectManager().getObjects() | boost::adaptors::map_values)
  {
    if(currentEnemy->m_state.isDead() || currentEnemy.get() == getWorld().getObjectManager().getLaraPtr())
      continue;

    const auto modelEnemy = std::dynamic_pointer_cast<ModelObject>(currentEnemy.get());
    if(modelEnemy == nullptr)
    {
      BOOST_LOG_TRIVIAL(warning) << "Ignoring non-model object " << currentEnemy->getNode()->getName();
      continue;
    }

    if(!modelEnemy->getNode()->isVisible() || !modelEnemy->m_isActive)
      continue;

    const auto d = currentEnemy->m_state.location.position - weaponLocation.position;
    if(abs(d.X) > weapon.targetDist)
      continue;

    if(abs(d.Y) > weapon.targetDist)
      continue;

    if(abs(d.Z) > weapon.targetDist)
      continue;

    if(util::square(d.X) + util::square(d.Y) + util::square(d.Z) >= util::square(weapon.targetDist))
      continue;

    auto enemyPos = getUpperThirdBBoxCtr(*std::dynamic_pointer_cast<const ModelObject>(currentEnemy.get()));
    const auto canShoot = raycastLineOfSight(weaponLocation, enemyPos.position, getWorld().getObjectManager()).first;
    if(!canShoot)
      continue;

    auto aimAngle = getVectorAngles(enemyPos.position - weaponLocation.position);
    aimAngle.X = normalizeAngle(aimAngle.X - (m_torsoRotation.X + m_state.rotation.X));
    aimAngle.Y = normalizeAngle(aimAngle.Y - (m_torsoRotation.Y + m_state.rotation.Y));
    if(!weapon.lockAngles.y.contains(aimAngle.Y) || !weapon.lockAngles.x.contains(aimAngle.X))
      continue;

    const auto absY = abs(aimAngle.Y);
    if(absY >= bestYAngle)
      continue;

    bestYAngle = absY;
    aimAt = modelEnemy;
  }
  updateAimingState(weapon);
}

void LaraObject::initAimInfoPistol()
{
  leftArm.reset();
  rightArm.reset();

  m_handStatus = HandStatus::Combat;
  m_torsoRotation.Y = 0_deg;
  m_torsoRotation.X = 0_deg;
  m_headRotation.Y = 0_deg;
  m_headRotation.X = 0_deg;
  aimAt = nullptr;

  rightArm.weaponAnimData = getWorld().findAnimatedModelForType(TR1ItemId::LaraPistolsAnim)->frames;
  leftArm.weaponAnimData = rightArm.weaponAnimData;
}

void LaraObject::initAimInfoShotgun()
{
  leftArm.reset();
  rightArm.reset();

  m_handStatus = HandStatus::Combat;
  m_torsoRotation.Y = 0_deg;
  m_torsoRotation.X = 0_deg;
  m_headRotation.Y = 0_deg;
  m_headRotation.X = 0_deg;
  aimAt = nullptr;

  rightArm.weaponAnimData = getWorld().findAnimatedModelForType(TR1ItemId::LaraShotgunAnim)->frames;
  leftArm.weaponAnimData = rightArm.weaponAnimData;
}

void LaraObject::overrideLaraMeshesDrawWeapons(WeaponType weaponType)
{
  TR1ItemId id;
  switch(weaponType)
  {
  case WeaponType::Pistols:
    id = TR1ItemId::LaraPistolsAnim;
    break;
  case WeaponType::Magnums:
    id = TR1ItemId::LaraMagnumsAnim;
    break;
  case WeaponType::Uzis:
    id = TR1ItemId::LaraUzisAnim;
    break;
  default:
    BOOST_THROW_EXCEPTION(std::domain_error("weaponType"));
  }

  const auto& src = getWorld().findAnimatedModelForType(id);
  Expects(src != nullptr);
  BOOST_ASSERT(src->bones.size() == getSkeleton()->getBoneCount());
  const auto& normalLara = *getWorld().findAnimatedModelForType(TR1ItemId::Lara);
  BOOST_ASSERT(normalLara.bones.size() == getSkeleton()->getBoneCount());
  getSkeleton()->setMeshPart(leftArm.handBoneId, src->bones[leftArm.handBoneId].mesh);
  getSkeleton()->setMeshPart(leftArm.thighBoneId, normalLara.bones[leftArm.thighBoneId].mesh);
  getSkeleton()->setMeshPart(rightArm.handBoneId, src->bones[rightArm.handBoneId].mesh);
  getSkeleton()->setMeshPart(rightArm.thighBoneId, normalLara.bones[rightArm.thighBoneId].mesh);
  getSkeleton()->rebuildMesh();
}

void LaraObject::overrideLaraMeshesDrawShotgun()
{
  const auto& src = *getWorld().findAnimatedModelForType(TR1ItemId::LaraShotgunAnim);
  BOOST_ASSERT(src.bones.size() == getSkeleton()->getBoneCount());
  const auto& normalLara = *getWorld().findAnimatedModelForType(TR1ItemId::Lara);
  BOOST_ASSERT(normalLara.bones.size() == getSkeleton()->getBoneCount());
  getSkeleton()->setMeshPart(BoneTorso, normalLara.bones[BoneTorso].mesh);
  getSkeleton()->setMeshPart(BoneHandL, src.bones[BoneHandL].mesh);
  getSkeleton()->setMeshPart(BoneHandR, src.bones[BoneHandR].mesh);
  getSkeleton()->rebuildMesh();
}

void LaraObject::drawShotgun()
{
  auto nextFrame = leftArm.frame + 1_rframe;
  if(nextFrame < toAnimUnit(5_frame) || nextFrame > toAnimUnit(47_frame))
  {
    nextFrame = toAnimUnit(13_frame);
  }
  else if(nextFrame == toAnimUnit(23_frame))
  {
    overrideLaraMeshesDrawShotgun();

    playSoundEffect(TR1SoundEffect::LaraDrawWeapon);
  }
  else if(nextFrame == toAnimUnit(47_frame))
  {
    initAimInfoShotgun();
    nextFrame = 0_rframe;
  }

  leftArm.frame = nextFrame;
  rightArm.frame = nextFrame;
}

void LaraObject::updateAimAngles(const Weapon& weapon, AimInfo& aimInfo) const
{
  core::TRRotationXY targetRot{};
  if(aimInfo.aiming)
  {
    targetRot = m_weaponTargetVector;
  }

  if(aimInfo.aimRotation.X >= targetRot.X - weapon.aimSpeed * 1_rframe
     && aimInfo.aimRotation.X <= targetRot.X + weapon.aimSpeed * 1_rframe)
  {
    aimInfo.aimRotation.X = targetRot.X;
  }
  else if(aimInfo.aimRotation.X >= targetRot.X)
  {
    aimInfo.aimRotation.X -= weapon.aimSpeed * 1_rframe;
  }
  else
  {
    aimInfo.aimRotation.X += weapon.aimSpeed * 1_rframe;
  }

  if(aimInfo.aimRotation.Y >= targetRot.Y - weapon.aimSpeed * 1_rframe
     && aimInfo.aimRotation.Y <= weapon.aimSpeed * 1_rframe + targetRot.Y)
  {
    aimInfo.aimRotation.Y = targetRot.Y;
  }
  else if(aimInfo.aimRotation.Y >= targetRot.Y)
  {
    aimInfo.aimRotation.Y -= weapon.aimSpeed * 1_rframe;
  }
  else
  {
    aimInfo.aimRotation.Y += weapon.aimSpeed * 1_rframe;
  }
}

void LaraObject::updateAnimShotgun()
{
  auto aimingFrame = leftArm.frame;
  if(leftArm.aiming)
  {
    if(leftArm.frame >= toAnimUnit(0_frame) && leftArm.frame < toAnimUnit(13_frame))
    {
      aimingFrame = leftArm.frame + 1_rframe;
      if(leftArm.frame == toAnimUnit(12_frame))
      {
        aimingFrame = toAnimUnit(47_frame);
      }
    }
    else if(leftArm.frame == toAnimUnit(47_frame))
    {
      if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
      {
        tryShootShotgun();
        rightArm.frame = leftArm.frame + toAnimUnit(1_frame);
        leftArm.frame = leftArm.frame + toAnimUnit(1_frame);
        return;
      }
    }
    else if(leftArm.frame > toAnimUnit(47_frame) && leftArm.frame < toAnimUnit(80_frame))
    {
      aimingFrame = leftArm.frame + 1_rframe;
      if(leftArm.frame == toAnimUnit(79_frame))
      {
        rightArm.frame = toAnimUnit(47_frame);
        leftArm.frame = toAnimUnit(47_frame);
        return;
      }
      else if(leftArm.frame == toAnimUnit(56_frame))
      {
        playSoundEffect(TR1SoundEffect::LaraHolsterWeapons);
        rightArm.frame = aimingFrame;
        leftArm.frame = aimingFrame;
        return;
      }
    }
    else if(leftArm.frame >= toAnimUnit(114_frame) && leftArm.frame <= toAnimUnit(126_frame))
    {
      aimingFrame = leftArm.frame + 1_rframe;
      if(leftArm.frame == toAnimUnit(126_frame))
      {
        rightArm.frame = 0_rframe;
        leftArm.frame = 0_rframe;
        return;
      }
    }

    rightArm.frame = aimingFrame;
    leftArm.frame = aimingFrame;
    return;
  }

  if(leftArm.frame == 0_rframe && getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
  {
    leftArm.frame += 1_rframe;
    rightArm.frame += 1_rframe;
    return;
  }

  if(leftArm.frame > toAnimUnit(0_frame) && leftArm.frame < toAnimUnit(13_frame))
  {
    aimingFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(12_frame))
    {
      if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
      {
        rightArm.frame = toAnimUnit(47_frame);
        leftArm.frame = toAnimUnit(47_frame);
        return;
      }

      rightArm.frame = toAnimUnit(114_frame);
      leftArm.frame = toAnimUnit(114_frame);
      return;
    }
  }
  else if(leftArm.frame == toAnimUnit(47_frame))
  {
    if(getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
    {
      tryShootShotgun();
      rightArm.frame = aimingFrame + 1_rframe;
      leftArm.frame = aimingFrame + 1_rframe;
      return;
    }

    rightArm.frame = toAnimUnit(114_frame);
    leftArm.frame = toAnimUnit(114_frame);
    return;
  }
  else if(leftArm.frame >= toAnimUnit(47_frame) && leftArm.frame < toAnimUnit(80_frame))
  {
    aimingFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(59_frame))
    {
      rightArm.frame = 0_rframe;
      leftArm.frame = 0_rframe;
      return;
    }
    if(leftArm.frame == toAnimUnit(79_frame))
    {
      rightArm.frame = toAnimUnit(114_frame);
      leftArm.frame = toAnimUnit(114_frame);
      return;
    }
    else if(leftArm.frame == toAnimUnit(56_frame))
    {
      playSoundEffect(TR1SoundEffect::LaraHolsterWeapons);
      rightArm.frame = aimingFrame;
      leftArm.frame = aimingFrame;
      return;
    }

    rightArm.frame = toAnimUnit(114_frame);
    leftArm.frame = toAnimUnit(114_frame);
    return;
  }
  else if(leftArm.frame >= toAnimUnit(114_frame) && leftArm.frame < toAnimUnit(127_frame))
  {
    aimingFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(126_frame))
    {
      aimingFrame = 0_rframe;
    }
    else if(leftArm.frame == toAnimUnit(56_frame))
    {
      playSoundEffect(TR1SoundEffect::LaraHolsterWeapons);
      rightArm.frame = aimingFrame;
      leftArm.frame = aimingFrame;
      return;
    }
  }
  else if(leftArm.frame >= toAnimUnit(114_frame) && leftArm.frame < toAnimUnit(127_frame))
  {
    aimingFrame = leftArm.frame + toAnimUnit(1_frame);
    if(leftArm.frame == toAnimUnit(126_frame))
    {
      aimingFrame = 0_rframe;
    }
  }

  rightArm.frame = aimingFrame;
  leftArm.frame = aimingFrame;
}

void LaraObject::tryShootShotgun()
{
  bool fireShotgun = false;
  const auto rounds = getWorld().getPlayer().getInventory().getAmmo(WeaponType::Shotgun).roundsPerShot;
  for(size_t i = 0; i < rounds; ++i)
  {
    core::TRRotationXY aimAngle;
    aimAngle.Y = util::rand15s(+20_deg) + m_state.rotation.Y + leftArm.aimRotation.Y;
    aimAngle.X = util::rand15s(+20_deg) + leftArm.aimRotation.X;
    if(shootBullet(WeaponType::Shotgun, aimAt, *this, aimAngle))
    {
      fireShotgun = true;
    }
  }
  if(fireShotgun)
  {
    playSoundEffect(weapons.at(WeaponType::Shotgun).shotSound);
  }
}

void LaraObject::holsterShotgun()
{
  auto aimFrame = leftArm.frame;
  if(leftArm.frame == 0_rframe)
  {
    aimFrame = toAnimUnit(80_frame);
  }
  else if(leftArm.frame >= toAnimUnit(0_frame) && leftArm.frame < toAnimUnit(13_frame))
  {
    aimFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(12_frame))
    {
      aimFrame = toAnimUnit(114_frame);
    }
  }
  else if(leftArm.frame == toAnimUnit(47_frame))
  {
    aimFrame = toAnimUnit(114_frame);
  }
  else if(leftArm.frame >= toAnimUnit(47_frame) && leftArm.frame < toAnimUnit(80_frame))
  {
    aimFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(59_frame))
    {
      aimFrame = 0_rframe;
    }
    else if(aimFrame == toAnimUnit(80_frame))
    {
      aimFrame = toAnimUnit(114_frame);
    }
  }
  else if(leftArm.frame >= toAnimUnit(114_frame) && leftArm.frame < toAnimUnit(127_frame))
  {
    aimFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(126_frame))
    {
      aimFrame = toAnimUnit(80_frame);
    }
  }
  else if(leftArm.frame >= toAnimUnit(80_frame) && leftArm.frame < toAnimUnit(114_frame))
  {
    aimFrame = leftArm.frame + 1_rframe;
    if(leftArm.frame == toAnimUnit(100_frame))
    {
      const auto& src = *getWorld().findAnimatedModelForType(TR1ItemId::LaraShotgunAnim);
      BOOST_ASSERT(src.bones.size() == getSkeleton()->getBoneCount());
      const auto& normalLara = *getWorld().findAnimatedModelForType(TR1ItemId::Lara);
      BOOST_ASSERT(normalLara.bones.size() == getSkeleton()->getBoneCount());
      getSkeleton()->setMeshPart(BoneTorso, src.bones[BoneTorso].mesh);
      getSkeleton()->setMeshPart(BoneHandL, normalLara.bones[BoneHandL].mesh);
      getSkeleton()->setMeshPart(BoneHandR, normalLara.bones[BoneHandR].mesh);
      getSkeleton()->rebuildMesh();

      playSoundEffect(TR1SoundEffect::LaraDrawWeapon);
    }
    else if(leftArm.frame == toAnimUnit(113_frame))
    {
      aimFrame = 0_rframe;
      m_handStatus = HandStatus::None;
      aimAt = nullptr;
      rightArm.aiming = false;
      leftArm.aiming = false;
    }
  }

  rightArm.frame = aimFrame;
  leftArm.frame = aimFrame;

  m_torsoRotation.X /= 2;
  m_torsoRotation.Y /= 2;
  m_headRotation.X = 0_deg;
  m_headRotation.Y = 0_deg;
}

void LaraObject::holsterWeapons(WeaponType weaponType)
{
  leftArm.holsterWeapons(*this, weaponType);
  rightArm.holsterWeapons(*this, weaponType);

  if(leftArm.frame == toAnimUnit(5_frame) && rightArm.frame == toAnimUnit(5_frame))
  {
    m_handStatus = HandStatus::None;
    leftArm.frame = 0_rframe;
    rightArm.frame = 0_rframe;
    aimAt = nullptr;
    rightArm.aiming = false;
    leftArm.aiming = false;
  }

  m_headRotation.X = (rightArm.aimRotation.X + leftArm.aimRotation.X) / 4;
  m_headRotation.Y = rightArm.aimRotation.Y / 4;
  m_torsoRotation.X = (rightArm.aimRotation.X + leftArm.aimRotation.X) / 4;
  m_torsoRotation.Y = rightArm.aimRotation.Y / 4;
}

void LaraObject::updateAnimNotShotgun(const WeaponType weaponType)
{
  const auto& weapon = weapons.at(weaponType);

  rightArm.update(*this, weapon);
  leftArm.update(*this, weapon);
}

bool LaraObject::shootBullet(const WeaponType weaponType,
                             const std::shared_ptr<ModelObject>& targetObject,
                             const ModelObject& weaponHolder,
                             const core::TRRotationXY& aimAngle)
{
  Expects(weaponType != WeaponType::None);

  auto& ammo = getWorld().getPlayer().getInventory().getAmmo(weaponType);

  if(ammo.ammo == 0)
  {
    playSoundEffect(TR1SoundEffect::EmptyAmmo);
    getWorld().getPlayer().requestedWeaponType = WeaponType::Pistols;
    return false;
  }

  --ammo.ammo;
  const auto weapon = &weapons.at(weaponType);
  core::TRVec weaponPosition = weaponHolder.m_state.location.position;
  weaponPosition.Y -= weapon->weaponHeight;
  const core::TRRotation shootVector{
    util::rand15s(weapon->shotInaccuracy) + aimAngle.X, util::rand15s(weapon->shotInaccuracy) + aimAngle.Y, +0_deg};

  std::vector<SkeletalModelNode::Sphere> spheres;
  if(targetObject != nullptr)
  {
    spheres = targetObject->getSkeleton()->getBoneCollisionSpheres();
  }
  bool hasHit = false;
  glm::vec3 hitPos;
  const auto bulletDir = normalize(glm::vec3(shootVector.toMatrix()[2])); // +Z is our shooting direction
  for(const auto& sphere : spheres)
  {
    hitPos = weaponPosition.toRenderSystem()
             + bulletDir * dot(sphere.getCollisionPosition() - weaponPosition.toRenderSystem(), bulletDir);

    if(core::Length{static_cast<core::Length::type>(length(hitPos - sphere.getPosition()))} <= sphere.radius)
    {
      hasHit = true;
      break;
    }
  }

  if(!hasHit)
  {
    ++ammo.misses;

    static constexpr float VeryLargeDistanceProbablyClipping = 1u << 14u;

    const auto aimHitPos
      = raycastLineOfSight(Location{weaponHolder.m_state.location.room, weaponPosition},
                           weaponPosition + core::TRVec{-bulletDir * VeryLargeDistanceProbablyClipping},
                           getWorld().getObjectManager())
          .second;
    emitRicochet(aimHitPos);
  }
  else
  {
    BOOST_ASSERT(targetObject != nullptr);
    ++ammo.hits;
    hitTarget(*targetObject, core::TRVec{hitPos}, weapon->damage);
  }

  return true;
}

void LaraObject::hitTarget(ModelObject& object, const core::TRVec& hitPos, const core::Health& damage)
{
  if(!object.m_state.isDead() && damage >= object.m_state.health)
  {
    ++getWorld().getPlayer().kills;
  }
  object.m_state.is_hit = true;
  object.m_state.health -= damage;
  auto fx = createBloodSplat(getWorld(),
                             Location{object.m_state.location.room, hitPos},
                             object.m_state.speed.velocity,
                             object.m_state.rotation.Y);
  getWorld().getObjectManager().registerParticle(fx);
  if(object.m_state.isDead())
    return;

  TR1SoundEffect soundEffect;
  switch(object.m_state.type.get_as<TR1ItemId>())
  {
  case TR1ItemId::Wolf:
    soundEffect = TR1SoundEffect::WolfHurt;
    break;
  case TR1ItemId::Bear:
    soundEffect = TR1SoundEffect::BearHurt;
    break;
  case TR1ItemId::LionMale:
  case TR1ItemId::LionFemale:
    soundEffect = TR1SoundEffect::LionHurt;
    break;
  case TR1ItemId::RatOnLand:
    soundEffect = TR1SoundEffect::RatHurt;
    break;
  case TR1ItemId::SkateboardKid:
    soundEffect = TR1SoundEffect::SkateboardKidHurt;
    break;
  case TR1ItemId::TorsoBoss:
    soundEffect = TR1SoundEffect::TorsoBossHurt;
    break;
  default:
    return;
  }

  object.playSoundEffect(soundEffect);
}

namespace
{
class MatrixStack
{
private:
  std::stack<glm::mat4> m_stack;

public:
  explicit MatrixStack()
  {
    m_stack.push(glm::mat4{1.0f});
  }

  void push()
  {
    m_stack.push(m_stack.top());
  }

  void pop()
  {
    m_stack.pop();
  }

  [[nodiscard]] const glm::mat4& top() const
  {
    return m_stack.top();
  }

  glm::mat4& top()
  {
    return m_stack.top();
  }

  void rotate(const glm::mat4& m)
  {
    m_stack.top() *= m;
  }

  void rotate(const core::TRRotation& r)
  {
    rotate(r.toMatrix());
  }

  void rotate(const core::TRRotationXY& r)
  {
    rotate(r.toMatrix());
  }

  void resetRotation()
  {
    top()[0] = glm::vec4{1, 0, 0, 0};
    top()[1] = glm::vec4{0, 1, 0, 0};
    top()[2] = glm::vec4{0, 0, 1, 0};
  }

  void rotate(const uint32_t packed)
  {
    m_stack.top() *= core::fromPackedAngles(packed);
  }

  void translate(const glm::vec3& c)
  {
    m_stack.top() = glm::translate(m_stack.top(), c);
  }

  void transform(const std::initializer_list<size_t>& indices,
                 const std::vector<world::SkeletalModelType::Bone>& bones,
                 const gsl::span<const uint32_t>& angleData,
                 const std::shared_ptr<SkeletalModelNode>& skeleton)
  {
    for(auto idx : indices)
      transform(idx, bones, angleData, skeleton);
  }

  void transform(const size_t idx,
                 const std::vector<world::SkeletalModelType::Bone>& bones,
                 const gsl::span<const uint32_t>& angleData,
                 const std::shared_ptr<SkeletalModelNode>& skeleton)
  {
    BOOST_ASSERT(idx > 0);
    translate(bones.at(idx).position);
    rotate(angleData[idx]);
    apply(skeleton, idx);
  }

  void apply(const std::shared_ptr<SkeletalModelNode>& skeleton, const size_t idx)
  {
    BOOST_ASSERT(skeleton != nullptr);
    skeleton->setMeshMatrix(idx, m_stack.top());
  }
};

class DualMatrixStack
{
private:
  MatrixStack m_stack1{};
  MatrixStack m_stack2{};
  const float m_bias;

public:
  explicit DualMatrixStack(const float bias)
      : m_bias{bias}
  {
  }

  void push()
  {
    m_stack1.push();
    m_stack2.push();
  }

  void pop()
  {
    m_stack1.pop();
    m_stack2.pop();
  }

  [[nodiscard]] glm::mat4 itop() const
  {
    return util::mix(m_stack1.top(), m_stack2.top(), m_bias);
  }

  void rotate(const glm::mat4& m)
  {
    m_stack1.top() *= m;
    m_stack2.top() *= m;
  }

  void rotate(const core::TRRotation& r)
  {
    rotate(r.toMatrix());
  }

  void rotate(const core::TRRotationXY& r)
  {
    rotate(r.toMatrix());
  }

  void rotate(const uint32_t packed1, const uint32_t packed2)
  {
    m_stack1.top() *= core::fromPackedAngles(packed1);
    m_stack2.top() *= core::fromPackedAngles(packed2);
  }

  void resetRotation()
  {
    m_stack1.resetRotation();
    m_stack2.resetRotation();
  }

  void translate(const glm::vec3& v1, const glm::vec3& v2)
  {
    m_stack1.top() = glm::translate(m_stack1.top(), v1);
    m_stack2.top() = glm::translate(m_stack2.top(), v2);
  }

  void translate(const glm::vec3& v)
  {
    translate(v, v);
  }

  void transform(const std::initializer_list<size_t>& indices,
                 const std::vector<world::SkeletalModelType::Bone>& bones,
                 const gsl::span<const uint32_t>& angleData1,
                 const gsl::span<const uint32_t>& angleData2,
                 const std::shared_ptr<SkeletalModelNode>& skeleton)
  {
    for(auto idx : indices)
      transform(idx, bones, angleData1, angleData2, skeleton);
  }

  void transform(const size_t idx,
                 const std::vector<world::SkeletalModelType::Bone>& bones,
                 const gsl::span<const uint32_t>& angleData1,
                 const gsl::span<const uint32_t>& angleData2,
                 const std::shared_ptr<SkeletalModelNode>& skeleton)
  {
    BOOST_ASSERT(idx > 0);
    translate(bones.at(idx).position);
    rotate(angleData1[idx], angleData2[idx]);
    apply(skeleton, idx);
  }

  // NOLINTNEXTLINE(readability-make-member-function-const)
  void apply(const std::shared_ptr<SkeletalModelNode>& skeleton, const size_t idx)
  {
    BOOST_ASSERT(skeleton != nullptr);
    skeleton->setMeshMatrix(idx, itop());
  }
};
} // namespace

void LaraObject::drawRoutine()
{
  const auto interpolationInfo = getSkeleton()->getInterpolationInfo();
  if(!hit_direction.has_value() && interpolationInfo.firstFrame != interpolationInfo.secondFrame)
  {
    drawRoutineInterpolated(interpolationInfo);
    return;
  }

  const auto& objInfo = *getWorld().findAnimatedModelForType(m_state.type);
  const loader::file::AnimFrame* frame;
  if(!hit_direction.has_value())
  {
    frame = interpolationInfo.firstFrame;
  }
  else
  {
    switch(*hit_direction)
    {
    case core::Axis::PosX:
      frame = getWorld().getAnimation(AnimationId::AH_LEFT).frames;
      break;
    case core::Axis::NegZ:
      frame = getWorld().getAnimation(AnimationId::AH_BACKWARD).frames;
      break;
    case core::Axis::NegX:
      frame = getWorld().getAnimation(AnimationId::AH_RIGHT).frames;
      break;
    default:
      frame = getWorld().getAnimation(AnimationId::AH_FORWARD).frames;
      break;
    }
    frame = frame->next(toRenderUnit(hit_frame).cast<size_t>().get());
  }

  MatrixStack matrixStack;

  matrixStack.push();
  matrixStack.translate(frame->pos.toGl());
  const auto angleData = frame->getAngleData();
  matrixStack.rotate(angleData[BoneHips]);
  matrixStack.apply(getSkeleton(), BoneHips);

  matrixStack.push();
  matrixStack.transform({BoneThighR, BoneCalfR, BoneFootR}, objInfo.bones, angleData, getSkeleton());

  matrixStack.pop();
  matrixStack.push();
  matrixStack.transform({BoneThighL, BoneCalfL, BoneFootL}, objInfo.bones, angleData, getSkeleton());

  matrixStack.pop();
  matrixStack.translate(objInfo.bones[BoneTorso].position);
  matrixStack.rotate(angleData[BoneTorso]);
  matrixStack.rotate(m_torsoRotation);
  matrixStack.apply(getSkeleton(), BoneTorso);

  matrixStack.push();
  matrixStack.translate(objInfo.bones[BoneHead].position);
  matrixStack.rotate(angleData[BoneHead]);
  matrixStack.rotate(m_headRotation);
  matrixStack.apply(getSkeleton(), BoneHead);

  WeaponType activeWeaponType = WeaponType::None;
  if(m_handStatus == HandStatus::Combat || m_handStatus == HandStatus::DrawWeapon
     || m_handStatus == HandStatus::Holster)
  {
    activeWeaponType = getWorld().getPlayer().selectedWeaponType;
  }

  matrixStack.pop();
  gsl::span<const uint32_t> armAngleData;
  switch(activeWeaponType)
  {
  case WeaponType::None:
    matrixStack.push();
    matrixStack.transform({BoneArmL, BoneForeArmL, BoneHandL}, objInfo.bones, angleData, getSkeleton());

    matrixStack.pop();
    matrixStack.push();
    matrixStack.transform({BoneArmR, BoneForeArmR, BoneHandR}, objInfo.bones, angleData, getSkeleton());

    m_muzzleFlashLeft->setVisible(false);
    m_muzzleFlashRight->setVisible(false);
    break;
  case WeaponType::Pistols:
  case WeaponType::Magnums:
  case WeaponType::Uzis:
    matrixStack.push();
    matrixStack.translate(objInfo.bones[BoneArmL].position);
    matrixStack.resetRotation();
    matrixStack.rotate(rightArm.aimRotation);

    armAngleData = rightArm.weaponAnimData->next(toRenderUnit(rightArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.rotate(armAngleData[BoneArmL]);
    matrixStack.apply(getSkeleton(), BoneArmL);

    matrixStack.transform(BoneForeArmL, objInfo.bones, armAngleData, getSkeleton());
    matrixStack.transform(BoneHandL, objInfo.bones, armAngleData, getSkeleton());

    renderMuzzleFlash(activeWeaponType, matrixStack.top(), m_muzzleFlashRight, rightArm.flashTimeout != 0_rframe);
    matrixStack.pop();
    matrixStack.push();
    matrixStack.translate(objInfo.bones[BoneArmR].position);
    matrixStack.resetRotation();
    matrixStack.rotate(leftArm.aimRotation);
    armAngleData = leftArm.weaponAnimData->next(toRenderUnit(leftArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.rotate(armAngleData[BoneArmR]);
    matrixStack.apply(getSkeleton(), BoneArmR);

    matrixStack.transform({BoneForeArmR, BoneHandR}, objInfo.bones, armAngleData, getSkeleton());

    renderMuzzleFlash(activeWeaponType, matrixStack.top(), m_muzzleFlashLeft, leftArm.flashTimeout != 0_rframe);
    break;
  case WeaponType::Shotgun:
    matrixStack.push();
    armAngleData = rightArm.weaponAnimData->next(toRenderUnit(rightArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.transform({BoneArmL, BoneForeArmL, BoneHandL}, objInfo.bones, armAngleData, getSkeleton());

    matrixStack.pop();
    matrixStack.push();
    armAngleData = leftArm.weaponAnimData->next(toRenderUnit(leftArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.transform({BoneArmR, BoneForeArmR, BoneHandR}, objInfo.bones, armAngleData, getSkeleton());
    break;
  default:
    break;
  }
}

void LaraObject::drawRoutineInterpolated(const InterpolationInfo& interpolationInfo)
{
  const auto& objInfo = *getWorld().findAnimatedModelForType(m_state.type);

  DualMatrixStack matrixStack{interpolationInfo.bias};

  matrixStack.push();
  matrixStack.translate(interpolationInfo.firstFrame->pos.toGl(), interpolationInfo.secondFrame->pos.toGl());
  const auto angleDataA = interpolationInfo.firstFrame->getAngleData();
  const auto angleDataB = interpolationInfo.secondFrame->getAngleData();
  matrixStack.rotate(angleDataA[BoneHips], angleDataB[BoneHips]);
  matrixStack.apply(getSkeleton(), 0);

  matrixStack.push();
  matrixStack.transform({BoneThighR, BoneCalfR, BoneFootR}, objInfo.bones, angleDataA, angleDataB, getSkeleton());

  matrixStack.pop();
  matrixStack.push();
  matrixStack.transform({BoneThighL, BoneCalfL, BoneFootL}, objInfo.bones, angleDataA, angleDataB, getSkeleton());

  matrixStack.pop();
  matrixStack.translate(objInfo.bones[BoneTorso].position);
  matrixStack.rotate(angleDataA[BoneTorso], angleDataB[BoneTorso]);
  matrixStack.rotate(m_torsoRotation);
  matrixStack.apply(getSkeleton(), BoneTorso);

  matrixStack.push();
  matrixStack.translate(objInfo.bones[14].position);
  matrixStack.rotate(angleDataA[BoneHead], angleDataB[BoneHead]);
  matrixStack.rotate(m_headRotation);
  matrixStack.apply(getSkeleton(), BoneHead);

  WeaponType activeWeaponType = WeaponType::None;
  if(m_handStatus == HandStatus::Combat || m_handStatus == HandStatus::DrawWeapon
     || m_handStatus == HandStatus::Holster)
  {
    activeWeaponType = getWorld().getPlayer().selectedWeaponType;
  }

  matrixStack.pop();
  gsl::span<const uint32_t> armAngleData;
  switch(activeWeaponType)
  {
  case WeaponType::None:
    matrixStack.push();
    matrixStack.transform({BoneArmL, BoneForeArmL, BoneHandL}, objInfo.bones, angleDataA, angleDataB, getSkeleton());

    matrixStack.pop();
    matrixStack.push();
    matrixStack.transform({BoneArmR, BoneForeArmR, BoneHandR}, objInfo.bones, angleDataA, angleDataB, getSkeleton());

    m_muzzleFlashLeft->setVisible(false);
    m_muzzleFlashRight->setVisible(false);
    break;
  case WeaponType::Pistols:
  case WeaponType::Magnums:
  case WeaponType::Uzis:
    matrixStack.push();
    matrixStack.translate(objInfo.bones[8].position);
    matrixStack.resetRotation();
    matrixStack.rotate(rightArm.aimRotation);

    armAngleData = rightArm.weaponAnimData->next(toRenderUnit(rightArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.rotate(armAngleData[BoneArmL], armAngleData[8]);
    matrixStack.apply(getSkeleton(), BoneArmL);

    matrixStack.transform(BoneForeArmL, objInfo.bones, armAngleData, armAngleData, getSkeleton());
    matrixStack.transform(BoneHandL, objInfo.bones, armAngleData, armAngleData, getSkeleton());

    renderMuzzleFlash(activeWeaponType, matrixStack.itop(), m_muzzleFlashRight, rightArm.flashTimeout != 0_rframe);
    matrixStack.pop();
    matrixStack.push();
    matrixStack.translate(objInfo.bones[11].position);
    matrixStack.resetRotation();
    matrixStack.rotate(leftArm.aimRotation);
    armAngleData = leftArm.weaponAnimData->next(toRenderUnit(leftArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.rotate(armAngleData[BoneArmR], armAngleData[BoneArmR]);
    matrixStack.apply(getSkeleton(), BoneArmR);

    matrixStack.transform({BoneForeArmR, BoneHandR}, objInfo.bones, armAngleData, armAngleData, getSkeleton());

    renderMuzzleFlash(activeWeaponType, matrixStack.itop(), m_muzzleFlashLeft, leftArm.flashTimeout != 0_rframe);
    break;
  case WeaponType::Shotgun:
    matrixStack.push();
    armAngleData = rightArm.weaponAnimData->next(toRenderUnit(rightArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.transform(
      {BoneArmL, BoneForeArmL, BoneHandL}, objInfo.bones, armAngleData, armAngleData, getSkeleton());

    matrixStack.pop();
    matrixStack.push();
    armAngleData = leftArm.weaponAnimData->next(toRenderUnit(leftArm.frame).cast<size_t>().get())->getAngleData();
    matrixStack.transform(
      {BoneArmR, BoneForeArmR, BoneHandR}, objInfo.bones, armAngleData, armAngleData, getSkeleton());
    break;
  default:
    break;
  }
}

void LaraObject::renderMuzzleFlash(const WeaponType weaponType,
                                   glm::mat4 m,
                                   const gsl::not_null<std::shared_ptr<render::scene::Node>>& muzzleFlashNode,
                                   const bool visible) const
{
  if(!visible)
  {
    muzzleFlashNode->setVisible(false);
    return;
  }

  core::Shade shade{core::Shade::type{0}};
  core::Length dy = 0_len;
  switch(weaponType)
  {
  case WeaponType::None:
  case WeaponType::Pistols:
    shade = core::Shade{core::Shade::type{5120}};
    dy = 155_len;
    break;
  case WeaponType::Magnums:
    shade = core::Shade{core::Shade::type{4096}};
    dy = 155_len;
    break;
  case WeaponType::Uzis:
    shade = core::Shade{core::Shade::type{2560}};
    dy = 180_len;
    break;
  case WeaponType::Shotgun:
    shade = core::Shade{core::Shade::type{5120}};
    dy = 155_len;
    break;
  default:
    BOOST_THROW_EXCEPTION(std::domain_error("WeaponType"));
  }

  m = translate(m, core::TRVec{0_len, dy, 55_len}.toRenderSystem());
  m *= core::TRRotation(-90_deg, 0_deg, util::rand15s(180_deg) * 2).toMatrix();

  muzzleFlashNode->setVisible(true);
  setParent(muzzleFlashNode, getNode()->getParent().lock());
  muzzleFlashNode->setLocalMatrix(getNode()->getLocalMatrix() * m);

  muzzleFlashNode->bind("u_lightAmbient",
                        [brightness = toBrightness(shade)](const render::scene::Node* /*node*/,
                                                           const render::scene::Mesh& /*mesh*/,
                                                           gl::Uniform& uniform)
                        {
                          uniform.set(brightness.get());
                        });
}

void LaraObject::burnIfAlive()
{
  if(isDead())
    return;

  const auto sector = m_state.location.moved({}).updateRoom();
  if(HeightInfo::fromFloor(sector,
                           {m_state.location.position.X, 32000_len, m_state.location.position.Z},
                           getWorld().getObjectManager().getObjects())
       .y
     != m_state.floor)
    return;

  m_state.health = core::DeadHealth;
  m_state.is_hit = true;

  for(size_t i = 0; i < 10; ++i)
  {
    auto particle = gslu::make_nn_shared<FlameParticle>(m_state.location, getWorld(), true);
    setParent(particle, m_state.location.room->node);
    getWorld().getObjectManager().registerParticle(particle);
  }
}

void LaraObject::serialize(const serialization::Serializer<world::World>& ser)
{
  ModelObject::serialize(ser);
  ser(S_NV("yRotationSpeed", m_yRotationSpeed),
      S_NV("fallSpeedOverride", m_fallSpeedOverride),
      S_NV("movementAngle", m_movementAngle),
      S_NV("air", m_air),
      S_NV("currentSlideAngle", m_currentSlideAngle),
      S_NV("handStatus", m_handStatus),
      S_NV("underwaterState", m_underwaterState),
      S_NV("swimToDiveKeypressDuration", m_swimToDiveKeypressDuration),
      S_NV("headRotation", m_headRotation),
      S_NV("torsoRotation", m_torsoRotation),
      S_NV("underwaterCurrentStrength", m_underwaterCurrentStrength),
      S_NV("underwaterRoute", m_underwaterRoute),
      S_NV("hitDirection", hit_direction),
      S_NV("hitFrame", hit_frame),
      S_NV("explosionStumblingDuration", explosionStumblingDuration),
      // FIXME S_NVP(forceSourcePosition),
      S_NV("leftArm", leftArm),
      S_NV("rightArm", rightArm),
      S_NV("weaponTargetVector", m_weaponTargetVector));

  ser.lazy(
    [this](const serialization::Serializer<world::World>& ser)
    {
      ser(S_NV("aimAt", serialization::ObjectReference{aimAt}));
    });

  if(ser.loading)
  {
    forceSourcePosition = nullptr;
    getSkeleton()->getRenderState().setScissorTest(false);
  }
}

LaraObject::LaraObject(const std::string& name,
                       const gsl::not_null<world::World*>& world,
                       const gsl::not_null<const world::Room*>& room,
                       const loader::file::Item& item,
                       const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
    : ModelObject(name, world, room, item, false, animatedModel)
{
  m_underwaterRoute.step = core::SectorSize * 20;
  m_underwaterRoute.drop = -core::SectorSize * 20;
  m_underwaterRoute.fly = core::QuarterSectorSize;

  m_state.health = core::LaraHealth;
  m_state.collidable = true;

  if(m_state.location.room->isWaterRoom)
  {
    m_underwaterState = UnderwaterState::Diving;
    setAnimation(AnimationId::UNDERWATER_IDLE);
    setCurrentAnimState(LaraStateId::UnderwaterStop);
    setGoalAnimState(LaraStateId::UnderwaterStop);
  }
  else
  {
    m_underwaterState = UnderwaterState::OnLand;
    setAnimation(AnimationId::STAY_SOLID);
    setCurrentAnimState(LaraStateId::Stop);
    setGoalAnimState(LaraStateId::Stop);
  }

  initMuzzleFlashes();

  auto& player = world->getPlayer();
  if(player.getInventory().count(TR1ItemId::Shotgun) > 0)
  {
    const auto& src = *getWorld().findAnimatedModelForType(TR1ItemId::LaraShotgunAnim);
    BOOST_ASSERT(src.bones.size() == getSkeleton()->getBoneCount());
    getSkeleton()->setMeshPart(BoneTorso, src.bones[BoneTorso].mesh);
    getSkeleton()->rebuildMesh();
  }

  if(auto weaponType = player.selectedWeaponType; weaponType != WeaponType::None && weaponType != WeaponType::Shotgun)
  {
    leftArm.overrideHolsterWeaponsMeshes(*this, player.selectedWeaponType);
    rightArm.overrideHolsterWeaponsMeshes(*this, player.selectedWeaponType);
  }

  getSkeleton()->getRenderState().setScissorTest(false);
}

void LaraObject::initMuzzleFlashes()
{
  const auto& muzzleFlashModel = getWorld().findAnimatedModelForType(TR1ItemId::MuzzleFlash);
  if(muzzleFlashModel == nullptr)
    return;

  world::RenderMeshDataCompositor compositor;
  compositor.append(*muzzleFlashModel->bones[0].mesh);
  auto mdl = compositor.toMesh(*getWorld().getPresenter().getMaterialManager(), false, {});

  m_muzzleFlashLeft->setRenderable(mdl);
  m_muzzleFlashLeft->setVisible(false);

  m_muzzleFlashRight->setRenderable(mdl);
  m_muzzleFlashRight->setVisible(false);
}

void LaraObject::updateExplosionStumbling()
{
  const auto rot = angleFromAtan(forceSourcePosition->X - m_state.location.position.X,
                                 forceSourcePosition->Z - m_state.location.position.Z)
                   - 180_deg;
  hit_direction = axisFromAngle(m_state.rotation.Y - rot);
  Expects(hit_direction.has_value());
  if(hit_frame == 0_rframe)
  {
    playSoundEffect(TR1SoundEffect::LaraOof);
  }

  hit_frame += 1_rframe;
  if(hit_frame > toAnimUnit(34_frame))
  {
    hit_frame = toAnimUnit(34_frame);
  }
  explosionStumblingDuration -= 1_rframe;
}

void LaraObject::AimInfo::serialize(const serialization::Serializer<world::World>& ser)
{
  auto ptr = reinterpret_cast<const int16_t*>(weaponAnimData);
  ser(S_NV_VECTOR_ELEMENT("weaponAnimData", ser.context.getPoseFrames(), ptr),
      S_NV("frame", frame),
      S_NV("aiming", aiming),
      S_NV("aimRotation", aimRotation),
      S_NV("flashTimeout", flashTimeout));
  weaponAnimData = reinterpret_cast<const loader::file::AnimFrame*>(ptr);
}

void LaraObject::AimInfo::update(LaraObject& lara, const Weapon& weapon)
{
  if(!aiming
     && (lara.aimAt != nullptr || !lara.getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action)))
  {
    if(frame >= toAnimUnit(24_frame))
    {
      frame = toAnimUnit(4_frame);
    }
    else if(frame > toAnimUnit(0_frame) && frame <= toAnimUnit(4_frame))
    {
      frame -= 1_rframe;
    }
  }
  else if(frame >= toAnimUnit(0_frame) && frame < toAnimUnit(4_frame))
  {
    frame += 1_rframe;
  }
  else if(frame == toAnimUnit(4_frame)
          && lara.getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
  {
    core::TRRotationXY aimAngle;
    aimAngle.X = aimRotation.X;
    aimAngle.Y = lara.m_state.rotation.Y + aimRotation.Y;
    if(lara.shootBullet(weapon.type, lara.aimAt, lara, aimAngle))
    {
      flashTimeout = toAnimUnit(weapon.flashTime);
      lara.playSoundEffect(weapon.shotSound);
    }
    frame = toAnimUnit(24_frame);
  }
  else if(frame >= toAnimUnit(24_frame))
  {
    frame += 1_rframe;
    if(frame == toAnimUnit(weapon.recoilFrame + 24_frame))
    {
      frame = toAnimUnit(4_frame);
    }
  }
}

void LaraObject::AimInfo::holsterWeapons(LaraObject& lara, WeaponType weaponType)
{
  if(frame >= toAnimUnit(24_frame))
  {
    frame = toAnimUnit(4_frame);
  }
  else if(frame > toAnimUnit(0_frame) && frame < toAnimUnit(5_frame))
  {
    aimRotation.X -= aimRotation.X / frame * 1_rframe;
    aimRotation.Y -= aimRotation.Y / frame * 1_rframe;
    frame -= 1_rframe;
  }
  else if(frame == toAnimUnit(0_frame))
  {
    aimRotation.X = 0_deg;
    aimRotation.Y = 0_deg;
    frame = toAnimUnit(23_frame);
  }
  else if(frame > toAnimUnit(5_frame) && frame < toAnimUnit(24_frame))
  {
    frame -= 1_rframe;
    if(frame == toAnimUnit(12_frame))
    {
      overrideHolsterWeaponsMeshes(lara, weaponType);
      lara.playSoundEffect(TR1SoundEffect::LaraHolster);
    }
  }
}

void LaraObject::AimInfo::overrideHolsterWeaponsMeshes(LaraObject& lara, WeaponType weaponType)
{
  TR1ItemId srcId;
  switch(weaponType)
  {
  case WeaponType::Pistols:
    srcId = TR1ItemId::LaraPistolsAnim;
    break;
  case WeaponType::Magnums:
    srcId = TR1ItemId::LaraMagnumsAnim;
    break;
  case WeaponType::Uzis:
    srcId = TR1ItemId::LaraUzisAnim;
    break;
  default:
    BOOST_THROW_EXCEPTION(std::domain_error("weaponType"));
  }

  const auto& src = *lara.getWorld().findAnimatedModelForType(srcId);
  BOOST_ASSERT(src.bones.size() == lara.getSkeleton()->getBoneCount());
  const auto& normalLara = *lara.getWorld().findAnimatedModelForType(TR1ItemId::Lara);
  BOOST_ASSERT(normalLara.bones.size() == lara.getSkeleton()->getBoneCount());
  lara.getSkeleton()->setMeshPart(handBoneId, normalLara.bones[handBoneId].mesh);
  lara.getSkeleton()->setMeshPart(thighBoneId, src.bones[thighBoneId].mesh);
  lara.getSkeleton()->rebuildMesh();
}
} // namespace engine::objects
