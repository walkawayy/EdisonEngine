#include "secrets.h"

#include "floordata.h"
#include "util/helpers.h"

#include <boost/assert.hpp>

namespace engine::floordata
{

std::bitset<16> getSecretsMask(const FloorDataValue* floorData)
{
  if(floorData == nullptr)
    return 0;

  std::bitset<16> result{};
  while(true)
  {
    FloorDataChunk chunkHeader{*floorData++};

    switch(chunkHeader.type)
    {
    case FloorDataChunkType::FloorSlant:
    case FloorDataChunkType::CeilingSlant:
    case FloorDataChunkType::BoundaryRoom:
      ++floorData;
      if(chunkHeader.isLast)
        return result;
      continue;
    case FloorDataChunkType::Death:
      if(chunkHeader.isLast)
        return result;
      continue;
    case FloorDataChunkType::CommandSequence:
      break;
    default:
      Expects(false);
    }

    ++floorData;
    while(true)
    {
      const Command command{*floorData++};
      switch(command.opcode)
      {
      case CommandOpcode::SwitchCamera:
        command.isLast = CameraParameters{*floorData++}.isLast;
        break;
      case CommandOpcode::Activate:
      case CommandOpcode::LookAt:
      case CommandOpcode::UnderwaterCurrent:
      case CommandOpcode::FlipMap:
      case CommandOpcode::FlipOn:
      case CommandOpcode::FlipOff:
      case CommandOpcode::FlipEffect:
      case CommandOpcode::EndLevel:
      case CommandOpcode::PlayTrack:
        break;
      case CommandOpcode::Secret:
        BOOST_ASSERT(command.parameter < 16);
        result.set(command.parameter);
        break;
      default:
        Expects(false);
      }

      if(command.isLast)
        break;
    }

    if(chunkHeader.isLast)
      break;
  }

  return result;
}
} // namespace engine::floordata