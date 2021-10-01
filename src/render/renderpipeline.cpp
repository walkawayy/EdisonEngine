#include "renderpipeline.h"

#include "pass/effectpass.h"
#include "pass/geometrypass.h"
#include "pass/hbaopass.h"
#include "pass/portalpass.h"
#include "pass/uipass.h"
#include "pass/worldcompositionpass.h"
#include "rendersettings.h"

#include <boost/assert.hpp>
#include <gl/texturedepth.h>
#include <glm/vec2.hpp>
#include <gsl/gsl-lite.hpp>

namespace render
{
RenderPipeline::RenderPipeline(scene::MaterialManager& materialManager, const glm::ivec2& viewport)
{
  resize(materialManager, viewport, true);
}

void RenderPipeline::worldCompositionPass(const bool inWater)
{
  BOOST_ASSERT(m_portalPass != nullptr);
  if(m_renderSettings.waterDenoise)
    m_portalPass->renderBlur();
  BOOST_ASSERT(m_hbaoPass != nullptr);
  if(m_renderSettings.hbao)
    m_hbaoPass->render();
  BOOST_ASSERT(m_worldCompositionPass != nullptr);

  m_worldCompositionPass->render(inWater);
  auto finalOutput = m_worldCompositionPass->getFramebuffer();
  for(const auto& effect : m_effects)
  {
    effect->render(inWater);
    finalOutput = effect->getFramebuffer();
  }
  GL_ASSERT(gl::api::blitNamedFramebuffer(finalOutput->getHandle(),
                                          0,
                                          0,
                                          0,
                                          m_size.x - 1,
                                          m_size.y - 1,
                                          0,
                                          0,
                                          m_size.x - 1,
                                          m_size.y - 1,
                                          gl::api::ClearBufferMask::ColorBufferBit,
                                          gl::api::BlitFramebufferFilter::Nearest));
}

void RenderPipeline::updateCamera(const gsl::not_null<std::shared_ptr<scene::Camera>>& camera)
{
  BOOST_ASSERT(m_worldCompositionPass != nullptr);
  m_worldCompositionPass->updateCamera(camera);
  BOOST_ASSERT(m_hbaoPass != nullptr);
  if(m_renderSettings.hbao)
    m_hbaoPass->updateCamera(camera);
}

void RenderPipeline::apply(const RenderSettings& renderSettings, scene::MaterialManager& materialManager)
{
  m_renderSettings = renderSettings;
  resize(materialManager, m_size, true);
}

void RenderPipeline::resize(scene::MaterialManager& materialManager, const glm::ivec2& viewport, bool force)
{
  if(!force && m_size == viewport)
  {
    return;
  }

  m_size = viewport;

  m_geometryPass = std::make_shared<pass::GeometryPass>(viewport);
  m_portalPass = std::make_shared<pass::PortalPass>(materialManager, m_geometryPass->getDepthBuffer(), viewport);
  m_hbaoPass = std::make_shared<pass::HBAOPass>(materialManager, viewport, *m_geometryPass);
  m_worldCompositionPass = std::make_shared<pass::WorldCompositionPass>(
    materialManager, m_renderSettings, viewport, *m_geometryPass, *m_portalPass, *m_hbaoPass);

  auto fxSource = m_worldCompositionPass->getColorBuffer();
  auto addEffect =
    [this, &fxSource](const std::string& name, const gsl::not_null<std::shared_ptr<render::scene::Material>>& material)
  {
    auto fx = std::make_shared<pass::EffectPass>("fx:" + name, material, fxSource);
    m_effects.emplace_back(fx);
    fxSource = fx->getOutput();
  };

  m_effects.clear();
  if(m_renderSettings.fxaa)
    addEffect("fxaa", materialManager.getFXAA());
  if(m_renderSettings.lensDistortion)
    addEffect("lens", materialManager.getLensDistortion());
  if(m_renderSettings.velvia)
    addEffect("velvia", materialManager.getVelvia());
  if(m_renderSettings.filmGrain)
    addEffect("filmGrain", materialManager.getFilmGrain());
  if(m_renderSettings.crt)
    addEffect("crt", materialManager.getCRT());
  m_uiPass = std::make_shared<pass::UIPass>(materialManager, viewport);
}

gl::RenderState RenderPipeline::bindPortalFrameBuffer()
{
  BOOST_ASSERT(m_portalPass != nullptr);
  BOOST_ASSERT(m_geometryPass != nullptr);
  return m_portalPass->bind(*m_geometryPass->getPositionBuffer());
}

void RenderPipeline::bindUiFrameBuffer()
{
  BOOST_ASSERT(m_uiPass != nullptr);
  m_uiPass->bind();
}

void RenderPipeline::bindGeometryFrameBuffer(float farPlane)
{
  BOOST_ASSERT(m_geometryPass != nullptr);
  m_geometryPass->getColorBuffer()->getTexture()->clear({0, 0, 0});
  m_geometryPass->getPositionBuffer()->getTexture()->clear({0.0f, 0.0f, -farPlane});
  m_geometryPass->getDepthBuffer()->clear(gl::ScalarDepth{1.0f});
  m_geometryPass->bind();
}

void RenderPipeline::renderUiFrameBuffer(float alpha)
{
  BOOST_ASSERT(m_uiPass != nullptr);
  m_uiPass->render(alpha);
}
} // namespace render
