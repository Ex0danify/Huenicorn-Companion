#pragma once

#include <QPlainTextEdit>

class LogWindow : public QPlainTextEdit
{
  Q_OBJECT

public:
  explicit LogWindow(QWidget* parent = nullptr);

public slots:
  void append(const QString& line);

protected:
  void changeEvent(QEvent* event) override;
};
