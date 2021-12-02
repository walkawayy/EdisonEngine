#include "menuobject.h"

#include "core/id.h"
#include "core/magic.h"
#include "core/vec.h"
#include "engine/objectmanager.h"
#include "engine/objects/laraobject.h"
#include "engine/objects/objectstate.h"
#include "engine/skeletalmodelnode.h"
#include "engine/world/skeletalmodeltype.h"
#include "engine/world/world.h"
#include "menuringtransform.h"
#include "qs/qs.h"
#include "render/scene/node.h"
#include "render/scene/renderable.h"
#include "render/scene/rendercontext.h"
#include "render/scene/rendermode.h"
#include "util/helpers.h"

#include <boost/log/trivial.hpp>
#include <cstddef>
#include <cstdint>
#include <gl/program.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gsl/gsl-lite.hpp>
#include <optional>
#include <utility>

namespace render::scene
{
class Mesh;
}

namespace menu
{
bool MenuObject::animate()
{
  for(int i = 0; i < 2; ++i)
  {
    if(meshAnimFrame == toAnimUnit(goalFrame))
    {
      updateMeshRenderMask();
      return false;
    }

    if(animStretchCounter != 0_frame)
    {
      animStretchCounter -= 1_frame;
    }
    else
    {
      animStretchCounter = animStretch;
      meshAnimFrame += animDirection;
      if(meshAnimFrame >= toAnimUnit(lastMeshAnimFrame))
      {
        meshAnimFrame = 0_rframe;
      }
      else if(meshAnimFrame < 0_rframe)
      {
        meshAnimFrame = toAnimUnit(lastMeshAnimFrame) - 1_rframe;
      }
    }
    updateMeshRenderMask();
  }
  return true;
}

void MenuObject::updateMeshRenderMask()
{
  auto setMask = [this](uint32_t mask)
  {
    if(std::exchange(meshRenderMask, mask) == mask)
      return;

    for(size_t i = 0; i < node->getChildren().size(); ++i)
    {
      node->setVisible(i, meshRenderMask.test(i));
    }
    node->rebuildMesh();
  };

  if(type == engine::TR1ItemId::PassportOpening)
  {
    if(meshAnimFrame <= toAnimUnit(14_frame))
    {
      setMask(0x57);
    }
    else if(meshAnimFrame <= toAnimUnit(18_frame))
    {
      setMask(0x5f);
    }
    else if(meshAnimFrame == toAnimUnit(19_frame))
    {
      setMask(0x5b);
    }
    else if(meshAnimFrame <= toAnimUnit(23_frame))
    {
      setMask(0x7b);
    }
    else if(meshAnimFrame <= toAnimUnit(28_frame))
    {
      setMask(0x3b);
    }
    else if(meshAnimFrame == toAnimUnit(29_frame))
    {
      setMask(0x13);
    }
  }
  else if(type == engine::TR1ItemId::Compass
          && (meshAnimFrame == toAnimUnit(0_frame) || meshAnimFrame >= toAnimUnit(18_frame)))
  {
    setMask(defaultMeshRenderMask.to_ulong());
  }
  else
  {
    setMask(0xffffffff);
  }
}

void MenuObject::initModel(const engine::world::World& world)
{
  const auto& obj = world.findAnimatedModelForType(type);
  Expects(obj != nullptr);
  node = std::make_shared<engine::SkeletalModelNode>("menu-object", gsl::not_null{&world}, gsl::not_null{obj.get()});
  node->bind("u_lightAmbient",
             [](const render::scene::Node* /*node*/, const render::scene::Mesh& /*mesh*/, gl::Uniform& uniform)
             {
               uniform.set(0.5f);
             });
  core::AnimStateId animState{0_as};
  engine::SkeletalModelNode::buildMesh(node, animState);
}

void MenuObject::draw(const engine::world::World& world,
                      const MenuRingTransform& ringTransform,
                      const core::Angle& ringItemAngle) const
{
  glm::mat4 nodeMatrix
    = ringTransform.getModelMatrix() * core::TRRotation{0_deg, ringItemAngle, 0_deg}.toMatrix()
      * glm::translate(glm::mat4{1.0f}, core::TRVec{ringTransform.radius, 0_len, 0_len}.toRenderSystem())
      * core::TRRotation{baseRotationX, 90_deg, 0_deg}.toMatrix()
      * glm::translate(glm::mat4{1.0f}, core::TRVec{0_len, 0_len, positionZ}.toRenderSystem())
      * core::TRRotation{rotationX, rotationY, 0_deg}.toMatrix();

  if(const auto& spriteSequence = world.findSpriteSequenceForType(type))
  {
    BOOST_LOG_TRIVIAL(warning) << "Menu Sprite: " << toString(type);
    // TODO drawSprite
  }
  else if(const auto& obj = world.findAnimatedModelForType(type))
  {
    node->setLocalMatrix(nodeMatrix);
    core::AnimStateId animState{0_as};
    node->setAnimation(animState, gsl::not_null{&obj->animations[0]}, toRenderUnit(meshAnimFrame));

    if(type == engine::TR1ItemId::Compass)
    {
      const auto delta = (rotationY + world.getObjectManager().getLara().m_state.rotation.Y + compassNeedleRotation
                          + util::rand15s(10_deg))
                         / 50;
      compassNeedleRotationMomentum = compassNeedleRotationMomentum * 19 / 20 - delta;
      compassNeedleRotation += compassNeedleRotationMomentum;
      node->patchBone(1, core::TRRotation{0_deg, compassNeedleRotation, 0_deg}.toMatrix());
    }

    node->updatePose();

    render::scene::RenderContext context{render::scene::RenderMode::Full, std::nullopt};
    node->getRenderable()->render(node.get(), context);
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "No sprite or model found for " << toString(type);
  }
}
} // namespace menu
