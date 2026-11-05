#include "tray/IconFactory.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace
{
  QPixmap render(int size, const QColor& color)
  {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size, size);
    painter.translate(0.5, 0.5);
    painter.rotate(24);

    // Slightly concave sides like a real horn, bold enough for a 16px panel
    QPainterPath horn;
    horn.moveTo(0.0, -0.48);
    horn.cubicTo(0.05, -0.20, 0.16, 0.12, 0.25, 0.42);
    horn.lineTo(-0.25, 0.42);
    horn.cubicTo(-0.16, 0.12, -0.05, -0.20, 0.0, -0.48);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(horn);

    // Spiral grooves cut out of the horn, too fine to survive small sizes
    if(size >= 48){
      painter.setClipPath(horn);
      painter.setCompositionMode(QPainter::CompositionMode_Clear);
      painter.setPen(QPen(Qt::black, 0.03, Qt::SolidLine, Qt::RoundCap));
      for(double y : {-0.22, -0.02, 0.18, 0.38}){
        painter.drawLine(QPointF(-0.3, y + 0.09), QPointF(0.3, y - 0.09));
      }
    }

    return pixmap;
  }
}


QColor IconFactory::colorFor(DaemonController::State state)
{
  switch(state){
    case DaemonController::State::Running: return QColor(46, 160, 90);
    case DaemonController::State::Starting:
    case DaemonController::State::Stopping: return QColor(224, 150, 20);
    case DaemonController::State::Error: return QColor(210, 55, 55);
    case DaemonController::State::Stopped: break;
  }
  return QColor(120, 120, 128);
}


QIcon IconFactory::forState(DaemonController::State state)
{
  QIcon icon;
  for(int size : {16, 22, 32, 48, 64}){
    icon.addPixmap(render(size, colorFor(state)));
  }
  return icon;
}
