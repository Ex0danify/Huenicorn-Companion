#include <QtTest>

#include "tray/IconFactory.hpp"

class TestIconFactory : public QObject
{
  Q_OBJECT

private slots:
  void everyStateHasADistinctColor()
  {
    using State = DaemonController::State;
    QSet<QRgb> colors;
    for(auto state : {State::Stopped, State::Starting, State::Running, State::Error}){
      colors.insert(IconFactory::colorFor(state).rgb());
    }
    QCOMPARE(colors.size(), 4);
    QCOMPARE(IconFactory::colorFor(State::Starting), IconFactory::colorFor(State::Stopping));
  }

  void hornIsTintedByStateWithoutBackground()
  {
    for(auto state : {DaemonController::State::Running, DaemonController::State::Error}){
      const QImage image = IconFactory::forState(state).pixmap(32, 32).toImage();
      const QColor expected = IconFactory::colorFor(state);

      // A filled disc would color the corners, the horn leaves them empty
      QCOMPARE(qAlpha(image.pixel(1, 1)), 0);
      QCOMPARE(qAlpha(image.pixel(30, 30)), 0);

      // The horn body carries the state color
      bool tinted = false;
      for(int y = 0; y < image.height() && !tinted; ++y){
        for(int x = 0; x < image.width(); ++x){
          const QColor pixel = image.pixelColor(x, y);
          if(pixel.alpha() == 255 && pixel.rgb() == expected.rgb()){
            tinted = true;
            break;
          }
        }
      }
      QVERIFY(tinted);
    }
  }
};

QTEST_MAIN(TestIconFactory)
#include "TestIconFactory.moc"
