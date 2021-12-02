#include "audioengine.h"

#include "audio/bufferhandle.h"
#include "audio/buffervoice.h"
#include "audio/device.h"
#include "audio/soundengine.h"
#include "audio/streamsource.h"
#include "audio/streamvoice.h"
#include "audio/wadstreamsource.h"
#include "objects/laraobject.h"
#include "script/reflection.h"
#include "script/scriptengine.h"
#include "serialization/chrono.h"
#include "serialization/map.h"
#include "serialization/optional.h"
#include "serialization/serialization.h"
#include "tracks_tr1.h"
#include "util/helpers.h"
#include "video/ffmpegstreamsource.h"
#include "world/world.h"

#include <boost/format.hpp>

namespace engine
{
void AudioEngine::triggerCdTrack(const script::ScriptEngine& scriptEngine,
                                 TR1TrackId trackId,
                                 const floordata::ActivationState& activationRequest,
                                 const floordata::SequenceCondition triggerType)
{
  if(trackId >= TR1TrackId::Sentinel)
    return;

  if(trackId < TR1TrackId::LaraTalk2)
  { // NOLINT(bugprone-branch-clone)
    // 1..27
    triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId == TR1TrackId::LaraTalk2)
  {
    // 28
    if(m_cdTrackActivationStates[trackId].isOneshot()
       && m_world.getObjectManager().getLara().getCurrentAnimState() == loader::file::LaraStateId::JumpUp)
    {
      trackId = TR1TrackId::LaraTalk3;
    }
    triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId < TR1TrackId::LaraTalk15)
  {
    // 29..40
    if(trackId != TR1TrackId::LaraTalk11)
      triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId == TR1TrackId::LaraTalk15)
  { // NOLINT(bugprone-branch-clone)
    // 41
    if(m_world.getObjectManager().getLara().getCurrentAnimState() == loader::file::LaraStateId::Hang)
      triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId == TR1TrackId::LaraTalk16)
  {
    // 42
    if(m_world.getObjectManager().getLara().getCurrentAnimState() == loader::file::LaraStateId::Hang)
      triggerNormalCdTrack(scriptEngine, TR1TrackId::LaraTalk17, activationRequest, triggerType);
    else
      triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId < TR1TrackId::LaraTalk23)
  {
    // 43..48
    triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId == TR1TrackId::LaraTalk23)
  {
    // 49
    if(m_world.getObjectManager().getLara().getCurrentAnimState() == loader::file::LaraStateId::OnWaterStop)
      triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
  else if(trackId == TR1TrackId::LaraTalk24)
  {
    // LaraTalk24 "Right. Now I better take off these wet clothes"
    if(m_cdTrackActivationStates[trackId].isOneshot())
    {
      m_cdTrack50time += 1_rframe;
      if(m_cdTrack50time == (core::RenderFrameRate * 4_sec).cast<core::RenderFrame>())
      {
        m_world.finishLevel();
        m_cdTrack50time = 0_rframe;
        triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
      }
    }
    else if(m_world.getObjectManager().getLara().getCurrentAnimState() == loader::file::LaraStateId::OnWaterExit)
    {
      triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
    }
  }
  else
  {
    // 51..64
    triggerNormalCdTrack(scriptEngine, trackId, activationRequest, triggerType);
  }
}

void AudioEngine::triggerNormalCdTrack(const script::ScriptEngine& scriptEngine,
                                       const TR1TrackId trackId,
                                       const floordata::ActivationState& activationRequest,
                                       const floordata::SequenceCondition triggerType)
{
  if(trackId >= TR1TrackId::Sentinel)
    return;

  auto& trackState = m_cdTrackActivationStates[trackId];
  if(trackState.isOneshot())
    return;

  if(triggerType == floordata::SequenceCondition::ItemActivated)
    trackState ^= activationRequest.getActivationSet();
  else if(triggerType == floordata::SequenceCondition::LaraOnGroundInverted)
    trackState &= ~activationRequest.getActivationSet();
  else
    trackState |= activationRequest.getActivationSet();

  if(!trackState.isFullyActivated())
  {
    playStopCdTrack(scriptEngine, trackId, true);
    return;
  }

  if(activationRequest.isOneshot())
    trackState.setOneshot(true);

  if(!m_currentTrack.has_value() || *m_currentTrack != trackId)
    playStopCdTrack(scriptEngine, trackId, false);
}

void AudioEngine::playStopCdTrack(const script::ScriptEngine& scriptEngine, const TR1TrackId trackId, bool stop)
{
  const auto trackInfo = scriptEngine.getTrackInfo(trackId);

  switch(trackInfo.type)
  {
  case audio::TrackType::AmbientEffect:
    if(!stop)
    {
      BOOST_LOG_TRIVIAL(debug) << "playStopCdTrack - play sound effect " << toString(trackId);
      playSoundEffect(core::SoundEffectId{trackInfo.id}, nullptr);
    }
    else
    {
      BOOST_LOG_TRIVIAL(debug) << "playStopCdTrack - stop effect " << toString(trackId);
      stopSoundEffect(core::SoundEffectId{trackInfo.id}, nullptr);
    }
    break;
  case audio::TrackType::Ambient:
    m_soundEngine->getDevice().removeStream(m_ambientStream);
    m_ambientStreamId.reset();
    m_currentTrack.reset();

    if(!stop)
    {
      BOOST_LOG_TRIVIAL(debug) << "playStopCdTrack - play ambient " << toString(trackId);
      const auto stream = playStream(trackInfo.id.get());
      stream->setLooping(true);
      m_ambientStream = stream.get();
      m_ambientStreamId = trackInfo.id.get();
      m_soundEngine->getDevice().removeStream(m_interceptStream);
      m_interceptStreamId.reset();
      m_currentTrack = trackId;
    }
    break;
  case audio::TrackType::Interception:
    m_soundEngine->getDevice().removeStream(m_interceptStream);
    m_interceptStreamId.reset();
    m_currentTrack.reset();

    if(!stop)
    {
      BOOST_LOG_TRIVIAL(debug) << "playStopCdTrack - play interception " << toString(trackId);
      auto stream = playStream(trackInfo.id.get());
      stream->setLooping(false);
      m_interceptStream = stream.get();
      m_interceptStreamId = trackInfo.id.get();
      m_currentTrack = trackId;
    }
    break;
  }
}

gsl::not_null<std::shared_ptr<audio::StreamVoice>>
  AudioEngine::playStream(size_t trackId, const std::chrono::milliseconds& initialPosition)
{
  static constexpr size_t DefaultBufferSize = 8192;
  static constexpr size_t DefaultBufferCount = 4;

  if(std::filesystem::is_regular_file(m_rootPath / "CDAUDIO.WAD"))
  {
    auto stream = m_soundEngine->getDevice().createStream(audio::createWadStream(m_rootPath / "CDAUDIO.WAD", trackId),
                                                          DefaultBufferSize,
                                                          DefaultBufferCount,
                                                          initialPosition);
    m_music.add(stream);
    stream->play();
    return stream;
  }
  else
  {
    auto stream
      = m_soundEngine->getDevice().createStream(std::make_unique<video::FfmpegStreamSource>(util::ensureFileExists(
                                                  m_rootPath / (boost::format("%03d.ogg") % trackId).str())),
                                                DefaultBufferSize,
                                                DefaultBufferCount,
                                                initialPosition);
    m_music.add(stream);
    stream->play();
    return stream;
  }
}

std::shared_ptr<audio::Voice> AudioEngine::playSoundEffect(const core::SoundEffectId& id, audio::Emitter* emitter)
{
  const auto soundEffectIt = m_soundEffects.find(id.get());
  if(soundEffectIt == m_soundEffects.end())
  {
    BOOST_LOG_TRIVIAL(warning) << "Sound effect " << toString(id.get_as<TR1SoundEffect>()) << " not found";
    return nullptr;
  }

  const auto soundEffect = soundEffectIt->second;
  if(soundEffect->chance != 0 && util::rand15() > soundEffect->chance)
    return nullptr;

  size_t sample = soundEffect->sample.get();
  if(soundEffect->getSampleCount() > 1)
    sample += util::rand15(soundEffect->getSampleCount());

  float pitch = 1;
  if(soundEffect->useRandomPitch())
    pitch = 0.9f + util::rand15(0.2f);

  float volume = std::clamp(static_cast<float>(soundEffect->volume) / 0x7fff, 0.0f, 1.0f);
  if(soundEffect->useRandomVolume())
    volume -= util::rand15(0.25f);
  if(volume <= 0)
    return nullptr;

  const auto& buffer = m_samples.at(sample);
  switch(soundEffect->getPlaybackType(loader::file::level::Engine::TR1))
  {
  case loader::file::PlaybackType::Looping:
    // BOOST_LOG_TRIVIAL(trace) << "Play looping sound effect " << toString(id.get_as<TR1SoundEffect>());
    if(auto voices = m_soundEngine->getVoicesForBuffer(emitter, sample); !voices.empty())
    {
      Expects(voices.size() == 1);
      return voices[0];
    }
    else
    {
      auto voice = m_soundEngine->playBuffer(buffer, sample, pitch, volume, emitter);
      m_sfx.add(voice);
      voice->setLooping(true);
      voice->play();
      return voice.get();
    }
  case loader::file::PlaybackType::Restart:
    // BOOST_LOG_TRIVIAL(trace) << "Play restarting sound effect " << toString(id.get_as<TR1SoundEffect>());
    if(auto voices = m_soundEngine->getVoicesForBuffer(emitter, sample); !voices.empty())
    {
      Expects(voices.size() == 1);
      auto voice = voices[0];
      voice->pause();
      voice->setPitch(pitch);
      voice->setLocalGain(volume);
      if(emitter != nullptr)
        voice->setPosition(emitter->getPosition());
      voice->rewind();
      m_sfx.add(voice);
      voice->play();
      return voice;
    }
    else
    {
      auto voice = m_soundEngine->playBuffer(buffer, sample, pitch, volume, emitter);
      m_sfx.add(voice);
      return voice.get();
    }
  case loader::file::PlaybackType::Wait:
    // BOOST_LOG_TRIVIAL(trace) << "Play single-instance sound effect " << toString(id.get_as<TR1SoundEffect>());
    if(auto voices = m_soundEngine->getVoicesForBuffer(emitter, sample); !voices.empty())
    {
      BOOST_ASSERT(voices.size() == 1);
      return voices[0];
    }
    else
    {
      auto voice = m_soundEngine->playBuffer(buffer, sample, pitch, volume, emitter);
      m_sfx.add(voice);
      return voice.get();
    }
  default:
  {
    auto handle = m_soundEngine->playBuffer(buffer, sample, pitch, volume, emitter);
    m_sfx.add(handle);
    return handle.get();
  }
  }
}

void AudioEngine::stopSoundEffect(const core::SoundEffectId& id, audio::Emitter* emitter)
{
  const auto soundEffectIt = m_soundEffects.find(id.get());
  if(soundEffectIt == m_soundEffects.end())
    return;

  const auto soundEffect = soundEffectIt->second;
  const size_t first = soundEffect->sample.get();
  const size_t last = first + soundEffect->getSampleCount();

  bool anyStopped = false;
  for(size_t i = first; i < last; ++i)
  {
    anyStopped |= m_soundEngine->stopBuffer(i, emitter);
  }

  if(!anyStopped)
    BOOST_LOG_TRIVIAL(debug) << "Attempting to stop sound effect " << toString(id.get_as<TR1SoundEffect>())
                             << " (samples " << first << ".." << (last - 1) << ") didn't stop any sample";
  else
    BOOST_LOG_TRIVIAL(debug) << "Stopped samples of sound effect " << toString(id.get_as<TR1SoundEffect>());
}

void AudioEngine::setUnderwater(bool underwater)
{
  if(underwater)
  {
    if(m_underwaterAmbience == nullptr)
    {
      m_soundEngine->getDevice().setFilter(m_soundEngine->getDevice().getUnderwaterFilter());
      m_underwaterAmbience = playSoundEffect(TR1SoundEffect::UnderwaterAmbience, nullptr);
      m_underwaterAmbience->setLooping(true);
    }
  }
  else if(m_underwaterAmbience != nullptr)
  {
    m_soundEngine->getDevice().setFilter(nullptr);
    stopSoundEffect(TR1SoundEffect::UnderwaterAmbience, nullptr);
    m_underwaterAmbience.reset();
  }
}

void AudioEngine::addWav(const gsl::not_null<const uint8_t*>& buffer)
{
  auto handle = std::make_shared<audio::BufferHandle>();
  handle->fillFromWav(buffer.get());
  m_samples.emplace_back(std::move(handle));
}

std::shared_ptr<audio::Voice> AudioEngine::playSoundEffect(const core::SoundEffectId& id, const glm::vec3& pos)
{
  auto voice = playSoundEffect(id, nullptr);
  if(voice != nullptr)
    voice->setPosition(pos);
  return voice;
}

void AudioEngine::serialize(const serialization::Serializer<world::World>& ser)
{
  static const auto getStreamPos = [](const std::weak_ptr<audio::StreamVoice>& stream)
  {
    const auto locked = stream.lock();
    return locked == nullptr ? std::chrono::milliseconds{0} : locked->getStreamPosition();
  };

  if(!ser.loading)
  {
    if(const auto stream = m_ambientStream.lock(); stream == nullptr || stream->isStopped())
      m_ambientStreamId.reset();
    if(const auto stream = m_interceptStream.lock(); stream == nullptr || stream->isStopped())
      m_interceptStreamId.reset();
  }

  std::chrono::milliseconds ambientPosition = getStreamPos(m_ambientStream);
  std::chrono::milliseconds interceptPosition = getStreamPos(m_interceptStream);

  ser(S_NV("currentTrack", m_currentTrack),
      S_NV("cdTrackActivationStates", m_cdTrackActivationStates),
      S_NV("ambientStreamId", m_ambientStreamId),
      S_NV("ambientStreamPosition", ambientPosition),
      S_NV("interceptStreamId", m_interceptStreamId),
      S_NV("interceptStreamPosition", interceptPosition));

  if(ser.loading)
  {
    m_soundEngine->getDevice().removeStream(m_ambientStream);

    if(m_ambientStreamId.has_value())
    {
      const auto stream = playStream(*m_ambientStreamId, ambientPosition);
      stream->setLooping(true);
      m_ambientStream = stream.get();
    }

    m_soundEngine->getDevice().removeStream(m_interceptStream);

    if(m_interceptStreamId.has_value())
    {
      m_interceptStream = playStream(*m_interceptStreamId, interceptPosition).get();
    }
  }
}

AudioEngine::AudioEngine(world::World& world,
                         std::filesystem::path rootPath,
                         std::shared_ptr<audio::SoundEngine> soundEngine)
    : m_world{world}
    , m_rootPath{std::move(rootPath)}
    , m_soundEngine{std::move(soundEngine)}
{
}

void AudioEngine::init(const std::vector<loader::file::SoundEffectProperties>& soundEffectProperties,
                       const std::vector<int16_t>& soundEffects)
{
  m_soundEffectProperties = soundEffectProperties;
  m_soundEffects.clear();
  for(size_t i = 0; i < soundEffects.size(); ++i)
  {
    if(soundEffects[i] < 0)
      continue;

    m_soundEffects[gsl::narrow<int>(i)] = &m_soundEffectProperties.at(soundEffects[i]);
  }
  m_cdTrackActivationStates.clear();
  m_cdTrack50time = 0_rframe;
  m_underwaterAmbience.reset();
  m_soundEngine->getDevice().removeStream(m_ambientStream);
  m_ambientStreamId.reset();
  m_soundEngine->getDevice().removeStream(m_interceptStream);
  m_interceptStreamId.reset();
  m_currentTrack.reset();
}
} // namespace engine
