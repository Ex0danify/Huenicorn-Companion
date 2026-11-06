#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

struct Command
{
  QString program;
  QStringList arguments;
  QString workDir;
};

// Runs one command at a time. Tests replace it so no real build tool is started.
class CommandRunner : public QObject
{
  Q_OBJECT

public:
  using QObject::QObject;

  virtual void run(const Command& command) = 0;
  virtual void cancel() = 0;

signals:
  void outputLine(const QString& line);
  // The tail holds the end of the combined output, enough to explain a failure
  void finished(int exitCode, const QString& tail);
};


class ProcessRunner : public CommandRunner
{
  Q_OBJECT

public:
  explicit ProcessRunner(QObject* parent = nullptr);

  void run(const Command& command) override;
  void cancel() override;

private:
  void flush(bool final);

  QProcess m_process;
  QByteArray m_buffer;
  QString m_tail;
  bool m_reported{false};
};
