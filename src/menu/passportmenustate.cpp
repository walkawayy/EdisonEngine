#include "passportmenustate.h"

#include "closepassportmenustate.h"
#include "core/angle.h"
#include "core/i18n.h"
#include "donemenustate.h"
#include "engine/audioengine.h"
#include "engine/items_tr1.h"
#include "engine/presenter.h"
#include "engine/soundeffects_tr1.h"
#include "engine/world/world.h"
#include "hid/actions.h"
#include "hid/inputhandler.h"
#include "hid/inputstate.h"
#include "idleringmenustate.h"
#include "menu/menuobject.h"
#include "menu/menustate.h"
#include "menudisplay.h"
#include "menuring.h"
#include "qs/qs.h"
#include "savegamelistmenustate.h"
#include "util.h"

#include <glm/vec2.hpp>
#include <gsl/gsl-lite.hpp>
#include <utility>

namespace menu
{
void PassportMenuState::handleObject(ui::Ui& /*ui*/,
                                     engine::world::World& /*world*/,
                                     MenuDisplay& display,
                                     MenuObject& object)
{
  if(&object != &display.getCurrentRing().getSelectedObject())
    zeroRotation(object, 256_au);
  else
    rotateForSelection(object);
}

std::optional<std::unique_ptr<MenuState>> PassportMenuState::showLoadGamePage(engine::world::World& world,
                                                                              MenuDisplay& display)
{
  const auto title = /* translators: TR charmap encoding */ _("Load Game");

  if(m_passportText == nullptr)
  {
    m_passportText = std::make_unique<ui::Text>(title);
  }

  if(world.getPresenter().getInputHandler().hasDebouncedAction(hid::Action::Action)
     || display.mode == InventoryMode::LoadMode)
  {
    return create<SavegameListMenuState>(std::move(display.m_currentState), title, world, true);
  }

  return std::nullopt;
}

std::optional<std::unique_ptr<MenuState>>
  PassportMenuState::showSaveGamePage(engine::world::World& world, MenuDisplay& display, bool isInGame)
{
  const auto title = m_allowSave && isInGame ? /* translators: TR charmap encoding */ _("Save Game")
                                             : /* translators: TR charmap encoding */ _("New Game");

  if(m_passportText == nullptr)
  {
    m_passportText = std::make_unique<ui::Text>(title);
  }

  if(world.getPresenter().getInputHandler().hasDebouncedAction(hid::Action::Action))
  {
    if(display.mode == InventoryMode::SaveMode || (m_allowSave && isInGame))
    {
      return create<SavegameListMenuState>(std::move(display.m_currentState), title, world, false);
    }
    else
    {
      return create<DoneMenuState>(MenuResult::NewGame);
    }
  }

  return std::nullopt;
}

void PassportMenuState::showExitGamePage(engine::world::World& world, MenuDisplay& display, bool returnToTitle)
{
  if(m_passportText == nullptr)
  {
    m_passportText
      = std::make_unique<ui::Text>(!returnToTitle ? /* translators: TR charmap encoding */ _("Exit Game")
                                                  : /* translators: TR charmap encoding */ _("Exit to Title"));
  }

  if(world.getPresenter().getInputHandler().hasDebouncedAction(hid::Action::Action))
  {
    display.result = !returnToTitle ? MenuResult::ExitGame : MenuResult::ExitToTitle;
  }
}

void PassportMenuState::prevPage(const core::Frame& minFrame, MenuObject& passport, engine::world::World& world)
{
  passport.goalFrame -= FramesPerPage;
  passport.animDirection = -1_rframe;
  if(const auto firstFrame = passport.openFrame + minFrame; passport.goalFrame < firstFrame)
  {
    passport.goalFrame = firstFrame;
  }
  else
  {
    world.getAudioEngine().playSoundEffect(engine::TR1SoundEffect::MenuGamePageTurn, nullptr);
    m_passportText.reset();
  }
}

void PassportMenuState::nextPage(MenuObject& passport, engine::world::World& world)
{
  passport.goalFrame += FramesPerPage;
  passport.animDirection = 1_rframe;
  if(const auto lastFrame = passport.lastMeshAnimFrame - FramesPerPage - 1_frame; passport.goalFrame > lastFrame)
  {
    passport.goalFrame = lastFrame;
  }
  else
  {
    world.getAudioEngine().playSoundEffect(engine::TR1SoundEffect::MenuGamePageTurn, nullptr);
    m_passportText.reset();
  }
}

std::unique_ptr<MenuState> PassportMenuState::onFrame(ui::Ui& ui, engine::world::World& world, MenuDisplay& display)
{
  auto& passport = display.getCurrentRing().getSelectedObject();
  passport.type = engine::TR1ItemId::PassportOpening;
  passport.initModel(world);

  if(passport.selectedRotationY == passport.rotationY && passport.animate())
    return nullptr;

  const bool hasSavedGames = world.hasSavedGames();

  const auto localFrame = passport.goalFrame - passport.openFrame;
  auto page = localFrame.cast<int>() / FramesPerPage.cast<int>();
  hid::AxisMovement forcePageTurn = hid::AxisMovement::Null;
  if(localFrame % FramesPerPage != 0_frame)
  {
    page = -1;
  }
  else if(m_forcePage.value_or(page) != page)
  {
    page = -1;
    if(page < *m_forcePage)
      forcePageTurn = hid::AxisMovement::Right;
    else if(page > *m_forcePage)
      forcePageTurn = hid::AxisMovement::Left;
    else
      m_forcePage.reset();
  }

  switch(page)
  {
  case LoadGamePage:
    if(!hasSavedGames || display.mode == InventoryMode::SaveMode)
    {
      forcePageTurn = hid::AxisMovement::Right;
      break;
    }
    if(auto tmp = showLoadGamePage(world, display))
      return std::move(*tmp);
    break;
  case SaveGamePage:
    if(!m_allowSave && display.mode != InventoryMode::TitleMode)
    {
      // can't save when dead, so just skip this page
      if(passport.animDirection == -1_rframe)
        forcePageTurn = hid::AxisMovement::Left;
      else
        forcePageTurn = hid::AxisMovement::Right;
      break;
    }
    if(auto tmp = showSaveGamePage(
         world, display, display.mode != InventoryMode::TitleMode && display.mode != InventoryMode::DeathMode))
      return std::move(*tmp);
    break;
  case ExitGamePage:
    showExitGamePage(world, display, display.mode != InventoryMode::TitleMode);
    break;
  default:
    Expects(page == -1);
    break;
  }

  if(m_passportText != nullptr)
    m_passportText->draw(
      ui, world.getPresenter().getTrFont(), {(ui.getSize().x - m_passportText->getWidth()) / 2, ui.getSize().y - 16});

  if(forcePageTurn == hid::AxisMovement::Left
     || world.getPresenter().getInputHandler().getInputState().xMovement.justChangedTo(hid::AxisMovement::Left))
  {
    if(hasSavedGames)
    {
      prevPage(0_frame, passport, world);
      return nullptr;
    }
    else if(m_allowSave || display.mode == InventoryMode::TitleMode)
    {
      prevPage(FramesPerPage, passport, world);
      return nullptr;
    }
  }
  else if(forcePageTurn == hid::AxisMovement::Right
          || world.getPresenter().getInputHandler().getInputState().xMovement.justChangedTo(hid::AxisMovement::Right))
  {
    nextPage(passport, world);
    return nullptr;
  }
  else if(world.getPresenter().getInputHandler().hasDebouncedAction(hid::Action::Menu))
  {
    if(!m_allowExit && display.mode != InventoryMode::TitleMode)
      return nullptr;

    return create<ClosePassportMenuState>(passport, create<IdleRingMenuState>(false));
  }
  else if(world.getPresenter().getInputHandler().hasDebouncedAction(hid::Action::Action))
  {
    return create<ClosePassportMenuState>(passport, create<IdleRingMenuState>(false));
  }

  return nullptr;
}

PassportMenuState::PassportMenuState(const std::shared_ptr<MenuRingTransform>& ringTransform,
                                     InventoryMode mode,
                                     bool allowSave)
    : MenuState{ringTransform}
    , m_allowExit{mode != InventoryMode::DeathMode && mode != InventoryMode::TitleMode}
    , m_allowSave{allowSave && (mode != InventoryMode::DeathMode && mode != InventoryMode::TitleMode)}
    , m_forcePage{mode == InventoryMode::LoadMode   ? std::optional<int>{0}
                  : mode == InventoryMode::SaveMode ? std::optional<int>{1}
                                                    : std::nullopt}
{
}
} // namespace menu
