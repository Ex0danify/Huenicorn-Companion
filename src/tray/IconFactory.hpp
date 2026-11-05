#pragma once

#include <QIcon>

#include "daemon/DaemonController.hpp"

namespace IconFactory
{
  // Horn silhouette tinted by the daemon state
  QIcon forState(DaemonController::State state);

  QColor colorFor(DaemonController::State state);
}
