#pragma once

#include <functional>

#include <QTimer>

#include "installer/CommandRunner.hpp"

inline QString describe(const Command& command)
{
  return command.program + " " + command.arguments.join(' ');
}


// Records commands and answers them without starting any process
class FakeRunner : public CommandRunner
{
public:
  QList<Command> commands;
  // Decides exit code and output for each command
  std::function<int(const Command&, QString&)> handler;
  // Keeps the command running until it is cancelled
  bool hang{false};
  bool cancelled{false};

  void run(const Command& command) override
  {
    commands.append(command);
    if(hang){
      return;
    }

    QString output;
    const int code = handler ? handler(command, output) : 0;
    QTimer::singleShot(0, this, [this, code, output](){ emit finished(code, output); });
  }

  void cancel() override
  {
    cancelled = true;
    QTimer::singleShot(0, this, [this](){ emit finished(-1, {}); });
  }

  bool ran(const QString& prefix) const
  {
    for(const auto& command : commands){
      if(describe(command).startsWith(prefix)){
        return true;
      }
    }
    return false;
  }
};
