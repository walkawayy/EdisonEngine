#pragma once

#include "menuringtransform.h"
#include "ui/label.h"

namespace menu
{
enum class InventoryMode
{
  GameMode,
  TitleMode,
  KeysMode,
  SaveMode,
  LoadMode,
  DeathMode
};

struct MenuObject;
struct MenuRing;
class MenuState;

struct MenuDisplay
{
  explicit MenuDisplay(InventoryMode mode, engine::World& world);
  ~MenuDisplay();

  const InventoryMode mode;
  std::array<std::unique_ptr<ui::Label>, 7> objectTexts;
  std::optional<engine::TR1ItemId> inventoryChosen{};
  int musicVolume = 8; // range 0..10
  bool allowMenuClose = true;

  std::shared_ptr<MenuRingTransform> ringTransform = std::make_shared<MenuRingTransform>();
  std::unique_ptr<MenuState> m_currentState;

  void display(gl::Image<gl::SRGBA8>& img, engine::World& world);
  bool isDone = false;

  std::vector<gsl::not_null<std::unique_ptr<MenuRing>>> rings;
  size_t currentRingIndex = 0;
  bool passOpen = false;
  bool doOptions(gl::Image<gl::SRGBA8>& img, engine::World& world, MenuObject& object);
  void updateMenuObjectDescription(engine::World& world, const MenuObject& object);
  void clearMenuObjectDescription();
  void updateRingTitle();

  [[nodiscard]] MenuRing& getCurrentRing()
  {
    return *rings.at(currentRingIndex);
  }

  [[nodiscard]] const MenuRing& getCurrentRing() const
  {
    return *rings.at(currentRingIndex);
  }

private:
  [[nodiscard]] static std::vector<MenuObject> getOptionRingObjects(const engine::World& world, bool withHomePolaroid);
  [[nodiscard]] static std::vector<MenuObject> getMainRingObjects(const engine::World& world);
  [[nodiscard]] static std::vector<MenuObject> getKeysRingObjects(const engine::World& world);
};
} // namespace menu