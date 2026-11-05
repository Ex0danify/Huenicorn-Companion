#pragma once

// Login autostart through an XDG .desktop entry
namespace Autostart
{
  bool isEnabled();
  bool setEnabled(bool enabled);
}
