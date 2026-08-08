#include "InkInputHandler.h"
#include "InkEngine.h"
#include "InkDisplayManager.h"
#include "InkStoryManager.h"
#include "os/AppSettings.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/SystemUI.h"
#include "GfxRenderer.h"

InkInputHandler::InkInputHandler(IInput& input) : _input(input) {}

InkInputHandler::~InkInputHandler() {}

void InkInputHandler::handleScroll(InkDisplayManager& display, int scrollAmount) {
    int newScroll = display.getScrollY() + scrollAmount;
    if (newScroll < 0) newScroll = 0;
    if (newScroll > display.getMaxScrollY()) newScroll = display.getMaxScrollY();
    display.setScrollY(newScroll);
}

void InkInputHandler::tickWaitingInput(InkEngine& engine, InkDisplayManager& display, InkStoryManager& story, class IDisplay& IDisplay, struct AppSettings& settings, IFrontlight* frontlight, BatteryWidget* batteryWidget) {
  ButtonEvent ev = _input.pollInput();
  int touchX = -1, touchY = -1;
  bool hasTouchTap = _input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0;

  if (ev == ButtonEvent::NONE && !hasTouchTap) return;

  int scrollAmount = IDisplay.getHeight() * 3 / 4;

  if (hasTouchTap) {
    if (settings.touchChoicesEnabled && display.isChoicesVisible() && display.getNumChoices() > 0 && display.isRevealComplete()) {
      int clickedIdx = display.getChoiceIndexAtY(touchY);
      if (clickedIdx >= 0 && clickedIdx < display.getNumChoices()) {
        display.setSelectedChoice(clickedIdx);
        display.markHistoryOld();
        if (story.runner()) {
          story.runner()->choose(static_cast<std::size_t>(clickedIdx));
        }
        engine.incrementRefreshCount();
        engine.setState(InkEngine::State::RUNNING_TEXT);
        return;
      }
    }
    
    // Wait, touch scroll could be implemented as a drag, but the user mentioned touch choice selection and touch scroll toggles.
    // For now, simple tap to scroll can be implemented if touch choices didn't trigger
    if (settings.touchScrollEnabled) {
      if (touchY < IDisplay.getHeight() / 3) {
         handleScroll(display, -scrollAmount);
         engine.requestRedraw();
         return;
      } else if (touchY > IDisplay.getHeight() * 2 / 3 && !display.isChoicesVisible()) {
         handleScroll(display, scrollAmount);
         engine.requestRedraw();
         return;
      }
    }
  }

  if (ev == ButtonEvent::TOP_EDGE_SWIPE) {
    BatteryMonitor *dummyBm = nullptr;
    BatteryWidget stackBw(*IDisplay.getRenderer(), *dummyBm);
    BatteryWidget &bw = batteryWidget ? *batteryWidget : stackBw;
    QuickMenuWidget menu(IDisplay, _input, bw, frontlight, settings);
    QuickMenuAction action = menu.show();
    if (action == QuickMenuAction::SLEEP_DEVICE) {
      engine.setShouldSleep(true);
    } else if (action == QuickMenuAction::OPEN_SETTINGS) {
      SettingsView view(IDisplay, _input, bw, settings);
      view.run();
      engine.reloadSettings();
    }
    engine.requestRedraw();
    return;
  }

  if (ev == ButtonEvent::SWIPE_DOWN) {
    handleScroll(display, -scrollAmount);
    engine.requestRedraw();
    return;
  } else if (ev == ButtonEvent::SWIPE_UP) {
    handleScroll(display, scrollAmount);
    engine.requestRedraw();
    return;
  }

  bool choicesVisible = display.isChoicesVisible();
  bool revealDone = display.isRevealComplete();

  switch (ev) {
  case ButtonEvent::LEFT:
  case ButtonEvent::UP: {
    if (choicesVisible && revealDone && display.getNumChoices() > 0 &&
        display.getSelectedChoice() > 0) {
      display.setSelectedChoice(display.getSelectedChoice() - 1);
      
      // Auto-scroll logic for hardware buttons
      GfxRenderer* renderer = IDisplay.getRenderer();
      if (renderer) {
          int marginY = settings.marginPx;
          int choiceLineHeight = renderer->getLineHeight(2);
          int choicePadding = choiceLineHeight / 3;
          int minChoiceHeight = settings.touchChoicesEnabled ? 44 : choiceLineHeight;
          
          int docY = marginY - display.getScrollY();
          for (const auto &w : display.getHistory()) { docY += w.getHeight(renderer); }
          docY += (marginY / 2) + 2 + (marginY / 2);
          
          for (int i = 0; i < display.getSelectedChoice(); ++i) {
             if (i > 0) docY += choicePadding;
             int lines = 1;
             int textHeight = lines * choiceLineHeight;
             docY += std::max(minChoiceHeight, textHeight);
          }
          if (docY < marginY) {
             // Scroll up
             display.setScrollY(display.getScrollY() + docY - marginY);
          }
      }
      
      engine.requestRedraw();
    } else {
      handleScroll(display, -scrollAmount);
      engine.requestRedraw();
    }
    break;
  }
  case ButtonEvent::RIGHT:
  case ButtonEvent::DOWN: {
    if (!choicesVisible || !revealDone) {
      handleScroll(display, scrollAmount);
      engine.requestRedraw();
    } else if (display.getNumChoices() > 0) {
      if (display.getSelectedChoice() < display.getNumChoices() - 1) {
          display.setSelectedChoice(display.getSelectedChoice() + 1);
          
          GfxRenderer* renderer = IDisplay.getRenderer();
          if (renderer) {
              int marginY = settings.marginPx;
              int choiceLineHeight = renderer->getLineHeight(2);
              int choicePadding = choiceLineHeight / 3;
              int minChoiceHeight = settings.touchChoicesEnabled ? 44 : choiceLineHeight;
              
              int docY = marginY - display.getScrollY();
              for (const auto &w : display.getHistory()) { docY += w.getHeight(renderer); }
              docY += (marginY / 2) + 2 + (marginY / 2);
              
              for (int i = 0; i < display.getSelectedChoice(); ++i) {
                 if (i > 0) docY += choicePadding;
                 int lines = 1;
                 int textHeight = lines * choiceLineHeight;
                 docY += std::max(minChoiceHeight, textHeight);
              }
              if (docY > IDisplay.getHeight() - marginY - minChoiceHeight) {
                 display.setScrollY(display.getScrollY() + (docY - (IDisplay.getHeight() - marginY - minChoiceHeight)));
              }
          }
          
          engine.requestRedraw();
      } else {
          // cycle back to top or do nothing? User said "allow cycling through all options anyway"
          // Let's cycle back to 0
          display.setSelectedChoice(0);
          engine.requestRedraw();
      }
    }
    break;
  }
  case ButtonEvent::CONFIRM: {
    if (!choicesVisible || !revealDone) {
      handleScroll(display, scrollAmount);
      engine.requestRedraw();
    } else if (display.getNumChoices() > 0) {
      display.markHistoryOld();
      if (story.runner()) {
         story.runner()->choose(static_cast<std::size_t>(display.getSelectedChoice()));
      }
      engine.incrementRefreshCount();
      engine.setState(InkEngine::State::RUNNING_TEXT);
    }
    break;
  }
  case ButtonEvent::BACK:
  case ButtonEvent::QUIT: {
#ifdef PLATFORM_NATIVE
    engine.setShouldSleep(false);
    engine.setState(InkEngine::State::DONE);
#else
    SystemUI ui(IDisplay);
    if (ui.showConfirmDialog(_input, "Exit Story", "Return to the menu?")) {
      engine.setShouldSleep(false);
      engine.setState(InkEngine::State::DONE);
    } else {
      engine.requestRedraw();
    }
#endif
    break;
  }
  case ButtonEvent::SLEEP:
    engine.setShouldSleep(true);
    break;
  default:
    break;
  }
}

void InkInputHandler::tickStoryEnded(InkEngine& engine, InkDisplayManager& display, InkStoryManager& story, class IDisplay& IDisplay, struct AppSettings& settings, IFrontlight* frontlight, BatteryWidget* batteryWidget) {
  ButtonEvent ev = _input.pollInput();
  
  int touchX = -1, touchY = -1;
  bool hasTouchTap = _input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0;

  if (ev == ButtonEvent::NONE && !hasTouchTap) return;
  
  if (hasTouchTap && settings.touchChoicesEnabled) {
      int clickedIdx = display.getChoiceIndexAtY(touchY);
      if (clickedIdx >= 0 && clickedIdx < display.getNumChoices()) {
          display.setSelectedChoice(clickedIdx);
          ev = ButtonEvent::CONFIRM;
      }
  }

  if (ev == ButtonEvent::CONFIRM) {
    if (display.getSelectedChoice() == 0) {
#ifdef PLATFORM_NATIVE
      engine.setShouldSleep(false);
      engine.setState(InkEngine::State::DONE);
#else
      SystemUI ui(IDisplay);
      if (ui.showConfirmDialog(_input, "Exit Story", "Return to the menu?")) {
        engine.setShouldSleep(false);
        engine.setState(InkEngine::State::DONE);
      } else {
        engine.requestRedraw();
      }
#endif
    } else if (display.getSelectedChoice() == 1) {
      SystemUI ui(IDisplay);
      if (ui.showConfirmDialog(_input, "Restart Story", "Begin the story again?")) {
        if (story.getStory()) {
            story.globals() = story.getStory()->new_globals();
            story.runner() = story.getStory()->new_runner(story.globals());
        }
        display.clearHistory();
        display.setScrollY(0);
        engine.setState(InkEngine::State::RUNNING_TEXT);
      } else {
        engine.requestRedraw();
      }
    }
  } else if (ev == ButtonEvent::LEFT || ev == ButtonEvent::UP) {
    if (display.getSelectedChoice() > 0) {
      display.setSelectedChoice(display.getSelectedChoice() - 1);
      engine.requestRedraw();
    } else {
      int scrollAmount = IDisplay.getHeight() / 4;
      handleScroll(display, -scrollAmount);
      engine.requestRedraw();
    }
  } else if (ev == ButtonEvent::RIGHT || ev == ButtonEvent::DOWN) {
    if (display.getSelectedChoice() < display.getNumChoices() - 1) {
      display.setSelectedChoice(display.getSelectedChoice() + 1);
      engine.requestRedraw();
    } else {
      int scrollAmount = IDisplay.getHeight() / 4;
      handleScroll(display, scrollAmount);
      engine.requestRedraw();
    }
  } else if (ev == ButtonEvent::BACK || ev == ButtonEvent::QUIT) {
#ifdef PLATFORM_NATIVE
    engine.setShouldSleep(false);
    engine.setState(InkEngine::State::DONE);
#else
    SystemUI ui(IDisplay);
    if (ui.showConfirmDialog(_input, "Exit Story", "Return to the menu?")) {
      engine.setShouldSleep(false);
      engine.setState(InkEngine::State::DONE);
    } else {
      engine.requestRedraw();
    }
#endif
  } else if (ev == ButtonEvent::SLEEP) {
    engine.setShouldSleep(true);
  }
}
