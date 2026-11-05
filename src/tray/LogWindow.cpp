#include "tray/LogWindow.hpp"

#include <QFontDatabase>

LogWindow::LogWindow(QWidget* parent)
  : QPlainTextEdit(parent)
{
  setWindowTitle(tr("Huenicorn log"));
  setReadOnly(true);
  setMaximumBlockCount(5000);
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  resize(760, 420);
}


void LogWindow::append(const QString& line)
{
  appendPlainText(line);
}
