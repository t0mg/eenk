#include "InkInputHandler.h"
#include "GfxRenderer.h"
#include "InkDisplayManager.h"
#include "InkEngine.h"
#include "InkStoryManager.h"
#include "os/AppSettings.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/SystemUI.h"

InkInputHandler::InkInputHandler(IInput &input) : _input(input) {}

InkInputHandler::~InkInputHandler() {}

void InkInputHandler::handleScroll(InkDisplayManager &display,
                                   int scrollAmount) {
  int newScroll = display.getScrollY() + scrollAmount;
  if (newScroll < 0)
    newScroll = 0;
  if (newScroll > display.getMaxScrollY())
    newScroll = display.getMaxScrollY();
  display.setScrollY(newScroll);
}

void InkInputHandler::handleCommonNavigation(
    bool isStoryEnded, InkEngine &engine, InkDisplayManager &display,
    InkStoryManager &story, class IDisplay &IDisplay,
    struct AppSettings &settings, IFrontlight *frontlight,
    BatteryWidget *batteryWidget) {
  ButtonEvent ev = _input.pollInput();
  int touchX = -1, touchY = -1;
  bool hasTouchTap =
      _input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0;

  if (ev == ButtonEvent::NONE && !hasTouchTap)
    return;

  _input.resetActivityTimer();

  int scrollAmount = IDisplay.getHeight() * 3 / 4;

  bool choicesRevealed = display.isChoicesRevealed();

  if (hasTouchTap) {
    if (settings.touchChoicesEnabled && choicesRevealed &&
        display.getNumChoices() > 0) {
      int clickedIdx = display.getChoiceIndexAtY(touchY);
      if (clickedIdx >= 0 && clickedIdx < display.getNumChoices()) {
        display.setSelectedChoice(clickedIdx);
        ev = ButtonEvent::CONFIRM;
      }
    }

    if (settings.touchScrollEnabled && ev != ButtonEvent::CONFIRM) {
      if (touchY < IDisplay.getHeight() / 3 && display.getScrollY() > 0) {
        handleScroll(display, -scrollAmount);
        engine.requestRedraw();
        return;
      } else {
        if (!choicesRevealed) {
          if (display.getScrollY() >= display.getMaxScrollY()) {
            display.revealChoices();
          } else {
            handleScroll(display, scrollAmount);
          }
          engine.requestRedraw();
          return;
        } else if (touchY > IDisplay.getHeight() * 2 / 3 &&
                   !display.isChoicesVisible()) {
          handleScroll(display, scrollAmount);
          engine.requestRedraw();
          return;
        }
      }
    }
  }

  if (ev == ButtonEvent::TOP_EDGE_SWIPE) {
    BatteryMonitor *dummyBm = nullptr;
    BatteryWidget stackBw(*IDisplay.getRenderer(), *dummyBm);
    BatteryWidget &bw = batteryWidget ? *batteryWidget : stackBw;
    QuickMenuWidget menu(IDisplay, _input, bw, frontlight, settings);
    QuickMenuAction action = menu.show();
    engine.reloadSettings();

    if (action == QuickMenuAction::SLEEP_DEVICE) {
      engine.setShouldSleep(true);
    } else if (action == QuickMenuAction::OPEN_SETTINGS) {
      SettingsView view(IDisplay, _input, bw, frontlight, settings);
      view.run();
      engine.reloadSettings();
    }
    engine.requestRedraw();
    return;
  }

  if (settings.touchScrollEnabled) {
    if (ev == ButtonEvent::SWIPE_DOWN) {
      handleScroll(display, -scrollAmount);
      engine.requestRedraw();
      return;
    } else if (ev == ButtonEvent::SWIPE_UP) {
      if (!choicesRevealed) {
        if (display.getScrollY() >= display.getMaxScrollY()) {
          display.revealChoices();
        } else {
          handleScroll(display, scrollAmount);
        }
      } else {
        handleScroll(display, scrollAmount);
      }
      engine.requestRedraw();
      return;
    }
  }

  switch (ev) {
  case ButtonEvent::LEFT:
  case ButtonEvent::UP: {
    if (choicesRevealed && display.getNumChoices() > 0 &&
        display.getSelectedChoice() > 0) {
      display.setSelectedChoice(display.getSelectedChoice() - 1);
      display.scrollToSelectedChoice();
      engine.requestRedraw();
    } else {
      handleScroll(display, -scrollAmount);
      engine.requestRedraw();
    }
    break;
  }
  case ButtonEvent::RIGHT:
  case ButtonEvent::DOWN: {
    if (!choicesRevealed) {
      if (display.getScrollY() >= display.getMaxScrollY()) {
        display.revealChoices();
      } else {
        handleScroll(display, scrollAmount);
      }
      engine.requestRedraw();
    } else if (display.getNumChoices() > 0) {
      if (display.getSelectedChoice() < display.getNumChoices() - 1) {
        display.setSelectedChoice(display.getSelectedChoice() + 1);
        display.scrollToSelectedChoice();
        engine.requestRedraw();
      } else {
        display.setSelectedChoice(0);
        display.scrollToSelectedChoice();
        engine.requestRedraw();
      }
    }
    break;
  }
  case ButtonEvent::CONFIRM: {
    if (!choicesRevealed) {
      if (display.getScrollY() >= display.getMaxScrollY()) {
        display.revealChoices();
      } else {
        handleScroll(display, scrollAmount);
      }
      engine.requestRedraw();
    } else if (display.getNumChoices() > 0) {
      if (!isStoryEnded) {
        display.flashChoiceActivation(story, settings,
                                      display.getSelectedChoice());
        display.markHistoryOld();
        if (story.runner()) {
          story.runner()->choose(
              static_cast<std::size_t>(display.getSelectedChoice()));
        }
        engine.incrementRefreshCount();
        engine.setState(InkEngine::State::RUNNING_TEXT);
      } else {
        showStoryMenu(engine, display, story, IDisplay, settings, frontlight,
                      batteryWidget);
      }
    }
    break;
  }
  case ButtonEvent::BACK:
  case ButtonEvent::QUIT: {
    showStoryMenu(engine, display, story, IDisplay, settings, frontlight,
                  batteryWidget);
    break;
  }
  case ButtonEvent::SLEEP:
    engine.setShouldSleep(true);
    break;
  default:
    break;
  }
}

void InkInputHandler::showStoryMenu(
    InkEngine &engine, InkDisplayManager &display, InkStoryManager &story,
    class IDisplay &IDisplay, struct AppSettings &settings,
    IFrontlight *frontlight, BatteryWidget *batteryWidget) {
  SystemUI ui(IDisplay);
  StorySaveManager &saveMgr = engine.getSaveManager();

  enum Action { EXIT_TO_MENU, REWIND_CHECKPOINT, REWIND_NAMED, RESTART_STORY };

  std::vector<std::string> menuOptions;
  std::vector<Action> optionActions;

  menuOptions.push_back("Save and exit");
  optionActions.push_back(EXIT_TO_MENU);

  if (saveMgr.hasUnnamedCheckpoint()) {
    menuOptions.push_back("Rewind to last checkpoint");
    optionActions.push_back(REWIND_CHECKPOINT);
  }

  if (saveMgr.hasNamedCheckpoints()) {
    menuOptions.push_back("Rewind to...");
    optionActions.push_back(REWIND_NAMED);
  }

  menuOptions.push_back("Restart story");
  optionActions.push_back(RESTART_STORY);

  int menuW = 0, menuH = 0;
  int choice = ui.showMenuModal(_input, "Story Menu", menuOptions, 0, "", 0, 0,
                                true, &menuW, &menuH);
  if (choice < 0 || choice >= static_cast<int>(optionActions.size())) {
    engine.requestRedraw();
    return;
  }

  Action action = optionActions[choice];
  switch (action) {
  case EXIT_TO_MENU: {
    size_t snapLen = 0;
    const unsigned char *snap = engine.createSnapshot(&snapLen);
    if (snap && snapLen > 0) {
      saveMgr.saveMainProgress(snap, snapLen, engine.getHistory());
      saveMgr.writeSaveFile(engine.getStorage());
    }
    engine.setShouldSleep(false);
    engine.setState(InkEngine::State::DONE);
    break;
  }
  case REWIND_CHECKPOINT: {
    if (ui.showConfirmDialog(_input, "Confirm Rewind",
                             "Are you sure you want to rewind to the last "
                             "checkpoint?\n\nCurrent "
                             "progress will be lost.",
                             "", menuW, menuH, false)) {
      int cpIdx = saveMgr.getUnnamedCheckpointIndex();
      if (cpIdx >= 0 && saveMgr.restoreCheckpoint(cpIdx, story, display)) {
        saveMgr.writeSaveFile(engine.getStorage());
        engine.incrementRefreshCount();
        engine.setState(InkEngine::State::RUNNING_TEXT);
      }
    }
    engine.requestRedraw();
    break;
  }
  case REWIND_NAMED: {
    std::vector<std::pair<size_t, std::string>> namedCheckpoints;
    saveMgr.getNamedCheckpoints(namedCheckpoints);
    std::vector<std::string> submenuItems;
    for (const auto &nc : namedCheckpoints) {
      submenuItems.push_back(nc.second);
    }
    int subW = 0, subH = 0;
    int subChoice = ui.showMenuModal(_input, "Rewind to...", submenuItems,
                                     static_cast<int>(submenuItems.size()) - 1,
                                     "", menuW, menuH, false, &subW, &subH);
    if (subChoice >= 0 &&
        subChoice < static_cast<int>(namedCheckpoints.size())) {
      std::string confirmMsg = "Are you sure you want to rewind to \"" +
                               namedCheckpoints[subChoice].second +
                               "\"?\n\nProgress after this point will be lost.";
      if (ui.showConfirmDialog(_input, "Confirm Rewind", confirmMsg.c_str(), "",
                               subW, subH, false)) {
        size_t actualIdx = namedCheckpoints[subChoice].first;
        if (saveMgr.restoreCheckpoint(actualIdx, story, display)) {
          saveMgr.writeSaveFile(engine.getStorage());
          engine.incrementRefreshCount();
          engine.setState(InkEngine::State::RUNNING_TEXT);
        }
      }
    }
    engine.requestRedraw();
    break;
  }
  case RESTART_STORY: {
    if (ui.showConfirmDialog(
            _input, "Confirm Restart",
            "Are you sure you want to restart ?\n\nProgress will be lost.", "",
            menuW, menuH, false)) {
      saveMgr.clearAll(engine.getStorage());
      if (story.getStory()) {
        story.globals() = story.getStory()->new_globals();
        story.runner() = story.getStory()->new_runner(story.globals());
      }
      display.clearHistory();
      display.setScrollY(0);
      engine.incrementRefreshCount();
      engine.setState(InkEngine::State::RUNNING_TEXT);
    }
    engine.requestRedraw();
    break;
  }
  }
}

void InkInputHandler::tickWaitingInput(
    InkEngine &engine, InkDisplayManager &display, InkStoryManager &story,
    class IDisplay &IDisplay, struct AppSettings &settings,
    IFrontlight *frontlight, BatteryWidget *batteryWidget) {
  handleCommonNavigation(false, engine, display, story, IDisplay, settings,
                         frontlight, batteryWidget);
}

void InkInputHandler::tickStoryEnded(
    InkEngine &engine, InkDisplayManager &display, InkStoryManager &story,
    class IDisplay &IDisplay, struct AppSettings &settings,
    IFrontlight *frontlight, BatteryWidget *batteryWidget) {
  handleCommonNavigation(true, engine, display, story, IDisplay, settings,
                         frontlight, batteryWidget);
}
