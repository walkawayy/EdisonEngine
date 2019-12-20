#include "color.h"

#include "io/sdlreader.h"

namespace loader::file
{
ByteColor ByteColor::read(io::SDLReader& reader, const bool withAlpha)
{
  ByteColor color;
  color.r = reader.readU8() << 2u;
  color.g = reader.readU8() << 2u;
  color.b = reader.readU8() << 2u;
  if(withAlpha)
    color.a = reader.readU8() << 2u;
  else
    color.a = 255;
  return color;
}

std::unique_ptr<Palette> Palette::readTr1(io::SDLReader& reader)
{
  std::unique_ptr<Palette> palette{new Palette()};
  for(auto& c : gsl::span<ByteColor>(palette->colors))
    c = ByteColor::readTr1(reader);
  return palette;
}

std::unique_ptr<Palette> Palette::readTr2(io::SDLReader& reader)
{
  std::unique_ptr<Palette> palette{new Palette()};
  for(auto& c : gsl::span<ByteColor>(palette->colors))
    c = ByteColor::readTr2(reader);
  return palette;
}
} // namespace loader::file