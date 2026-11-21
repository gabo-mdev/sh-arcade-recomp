#pragma once

#include "host/backends.hpp"

#include <memory>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace sh {

// In-window play menu drawn with RmlUi. Debug cheats stay on the F1 panel.
class PlayMenu {
public:
  PlayMenu();
  ~PlayMenu();

  bool init(SDL_Window* window, SDL_Renderer* renderer);
  void shutdown();

  bool ready() const;
  bool is_open() const;
  /// Gamepad row while the menu is open, or -1 when it is closed.
  int menu_pad_index() const;

  void show(HostOptions& current);
  void hide();
  void draw();
  void process(SDL_Event& event);

  void move(int direction);
  void horizontal(int direction);
  void activate();
  void cycle_tab(int direction);

  /// Preview filter while the menu is open; otherwise -1 (use HostOptions).
  int live_filter() const;
  bool capturing() const;
  bool capture_is_pad() const;
  void cancel_capture();
  void take_key(int scancode, bool clear);
  void take_pad(int button);

  bool take_quit();
  bool take_reset();
  /// Copies committed settings after Aplicar. Prefer immediate write-through
  /// from commit(); this still clears the applied latch for callers.
  bool take_applied(HostOptions& dst);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sh
