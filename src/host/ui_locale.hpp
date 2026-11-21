#pragma once

namespace sh {

inline constexpr int kUiLanguageCount = 2;
enum class UiLanguage : int { English = 0, Spanish = 1 };

/// Escape menu + F1 debug panel labels. Index with `int(UiLanguage)`.
struct MenuLocale {
  const char* title;
  const char* lead;
  const char* tab_video;
  const char* tab_audio;
  const char* tab_controls;
  const char* tab_system;
  const char* apply;
  const char* reset;
  const char* quit;
  const char* binds_reset;

  const char* bind_up;
  const char* bind_down;
  const char* bind_left;
  const char* bind_right;
  const char* bind_attack;
  const char* bind_jump;
  const char* bind_start;
  const char* bind_coin;

  const char* fullscreen_on;
  const char* fullscreen_off;
  const char* scale_fmt;
  const char* scale_prefix;
  const char* fit_stretch;
  const char* fit_integer;
  const char* filter_prefix;
  const char* vsync_on;
  const char* vsync_off;
  const char* uncap_on;
  const char* uncap_off;
  const char* volume_prefix;
  const char* mono_on;
  const char* mono_off;
  const char* language_prefix;
  const char* language_name;
  const char* map_keyboard;
  const char* map_gamepad;
  const char* pad_none;
  const char* pad_prefix;
  const char* none_key;
  const char* none_pad;
  const char* key_fallback;
  const char* button_fallback;
  const char* dpad_up;
  const char* dpad_down;
  const char* dpad_left;
  const char* dpad_right;
  const char* hint_capture_key;
  const char* hint_capture_pad;
  const char* hint_default;
  const char* conflict_fmt;
  const char* other_action;
  const char* filters[8];

  const char* debug_title;
  const char* debug_section_game;
  const char* debug_section_audio;
  const char* debug_stage_select;
  const char* debug_fast_boot;
  const char* debug_invincible;
  const char* debug_untouchable;
  const char* debug_uncap;
  const char* debug_mono;
};

inline const MenuLocale& menu_locale(int language) {
  static const MenuLocale kEn{
      "Options",
      "",
      "Video",
      "Audio",
      "Controls",
      "System",
      "Apply",
      "Restart",
      "Quit",
      "Reset controls",
      "Up",
      "Down",
      "Left",
      "Right",
      "Attack",
      "Jump",
      "Start",
      "Coin",
      "Fullscreen: On",
      "Fullscreen: Off",
      nullptr,
      "Scale: ",
      "Fit: Stretch",
      "Fit: Integer",
      "Filter: ",
      "VSync: On",
      "VSync: Off",
      "Unlock FPS: On",
      "Unlock FPS: Off",
      "Volume: ",
      "Mono: On",
      "Mono: Off",
      "Language: ",
      "English",
      "Input: Keyboard",
      "Input: Gamepad",
      "Gamepad: None",
      "Gamepad: ",
      "None",
      "None",
      "Key",
      "Button",
      "D-pad up",
      "D-pad down",
      "D-pad left",
      "D-pad right",
      "Press a key. Backspace clears. Escape cancels.",
      "Press a button. Backspace clears. Escape cancels.",
      "",
      "%s already on %s.",
      "another action",
      {"Sharp", "Smooth", "CRT lines", "CRT mask", "CRT phosphor", "Contrast",
       "Sharpen", "HD"},

      "Debug",
      "Game",
      "Audio",
      "Stage select",
      "Fast boot",
      "Invincible",
      "Untouchable",
      "Unlock FPS",
      "Mono",
  };
  static const MenuLocale kEs{
      "Opciones",
      "",
      "Video",
      "Audio",
      "Controles",
      "Sistema",
      "Aplicar",
      "Reiniciar",
      "Salir",
      "Restablecer controles",
      "Arriba",
      "Abajo",
      "Izquierda",
      "Derecha",
      "Ataque",
      "Salto",
      "Start",
      "Moneda",
      "Pantalla completa: Si",
      "Pantalla completa: No",
      nullptr,
      "Escala: ",
      "Ajuste: Estirar",
      "Ajuste: Entero",
      "Filtro: ",
      "VSync: Si",
      "VSync: No",
      "FPS sin limite: Si",
      "FPS sin limite: No",
      "Volumen: ",
      "Mono: Si",
      "Mono: No",
      "Idioma: ",
      "Espanol",
      "Entrada: Teclado",
      "Entrada: Mando",
      "Mando: Ninguno",
      "Mando: ",
      "Ninguna",
      "Ninguno",
      "Tecla",
      "Boton",
      "Cruz arriba",
      "Cruz abajo",
      "Cruz izq.",
      "Cruz der.",
      "Pulsa una tecla. Retroceso borra. Escape cancela.",
      "Pulsa un boton. Retroceso borra. Escape cancela.",
      "",
      "%s ya esta en %s.",
      "otra accion",
      {"Nitido", "Suave", "CRT lineas", "CRT mascara", "CRT fosforo", "Contraste",
       "Nitidez", "HD"},

      "Depuracion",
      "Juego",
      "Audio",
      "Selector de nivel",
      "Arranque rapido",
      "Invencible",
      "Intocable",
      "FPS sin limite",
      "Mono",
  };
  if (language == int(UiLanguage::Spanish))
    return kEs;
  return kEn;
}

inline const char* ui_language_name(int language) {
  if (language == int(UiLanguage::Spanish))
    return "Espanol";
  return "English";
}

} // namespace sh
