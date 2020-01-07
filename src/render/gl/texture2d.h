#pragma once

#include "texture.h"

namespace render::gl
{
template<typename _PixelT>
class Texture2D : public TextureImpl<::gl::TextureTarget::Texture2d, _PixelT>
{
public:
  explicit Texture2D(const glm::ivec2& size, const std::string& label = {})
      : Texture2D<_PixelT>{size, 1, label}
  {
  }

  explicit Texture2D(const glm::ivec2& size, int levels, const std::string& label = {})
      : TextureImpl<::gl::TextureTarget::Texture2d, _PixelT>{label}
      , m_size{size}
  {
    BOOST_ASSERT(levels > 0);
    BOOST_ASSERT(size.x > 0);
    BOOST_ASSERT(size.y > 0);
    GL_ASSERT(::gl::textureStorage2D(getHandle(), levels, Pixel::InternalFormat, size.x, size.y));
  }

  Texture2D<_PixelT>& assign(const gsl::not_null<const _PixelT*>& data, int level = 0)
  {
    const int levelDiv = 1 << level;

    GL_ASSERT(::gl::textureSubImage2D(getHandle(),
                                      level,
                                      0,
                                      0,
                                      glm::max(1, m_size.x / levelDiv),
                                      glm::max(1, m_size.y / levelDiv),
                                      Pixel::PixelFormat,
                                      Pixel::PixelType,
                                      data.get()));
    return *this;
  }

private:
  glm::ivec2 m_size{-1};
};
} // namespace render::gl
