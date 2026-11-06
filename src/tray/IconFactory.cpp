#include "tray/IconFactory.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace
{
  // Coordinates live on a 64 unit grid and are scaled to the icon size
  constexpr double Grid = 64.0;

  QPainterPath monitorFrame()
  {
    QPainterPath frame;
    frame.setFillRule(Qt::OddEvenFill);
    frame.addRoundedRect(QRectF(4, 6, 56, 42), 5, 5);
    frame.addRoundedRect(QRectF(8.5, 10.5, 47, 33), 2, 2);
    return frame;
  }


  QPainterPath monitorStand()
  {
    QPainterPath stand;
    stand.addPolygon(QPolygonF({{27, 48}, {37, 48}, {39, 54}, {25, 54}}));
    stand.addRoundedRect(QRectF(17, 54, 30, 4), 2, 2);
    return stand;
  }


  // The neck runs into the bezel so the head reads as sitting inside the screen
  QPainterPath unicornHead()
  {
    QPainterPath head;
    head.setFillRule(Qt::OddEvenFill);
    head.moveTo(7, 42.5);
    head.lineTo(7.5, 30.5);
    head.lineTo(11.3, 30);
    head.lineTo(9.9, 27.4);
    head.lineTo(13.7, 26.9);
    head.lineTo(12.3, 24.3);
    head.lineTo(16.1, 23.8);
    head.lineTo(14.7, 21.2);
    head.lineTo(18.5, 20.7);
    head.lineTo(17.1, 18.1);
    head.lineTo(20.9, 17.6);
    head.lineTo(23.5, 16.2);
    head.lineTo(26, 16);
    head.cubicTo(27.5, 16.4, 29, 15.8, 30.5, 14.1);
    head.cubicTo(31.5, 12.5, 33, 11.4, 34.7, 11.4);
    head.lineTo(33.2, 15.6);
    head.lineTo(35.6, 16.2);
    head.lineTo(44, 10.9);
    head.lineTo(38.3, 19);
    head.cubicTo(40, 22, 41.8, 25, 43.5, 28);
    head.cubicTo(44.8, 29, 45.2, 31, 44.4, 34);
    head.cubicTo(43, 35.2, 41.3, 35.2, 40, 34.5);
    head.cubicTo(38.6, 34, 37.8, 33, 37, 32.3);
    head.cubicTo(34, 31.8, 31, 30.5, 29.6, 27.8);
    head.cubicTo(27, 29.2, 25.8, 31.5, 26, 34.5);
    head.cubicTo(26.2, 38, 27.8, 39.5, 27, 40.5);
    head.lineTo(27, 42.5);
    head.closeSubpath();
    head.addEllipse(QPointF(35.2, 25.8), 1.3, 1.3);
    head.translate(0, 3.5);
    return head;
  }


  QPixmap render(int size, const QColor& color)
  {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / Grid, size / Grid);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(monitorFrame());
    painter.drawPath(monitorStand());
    painter.drawPath(unicornHead());

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
