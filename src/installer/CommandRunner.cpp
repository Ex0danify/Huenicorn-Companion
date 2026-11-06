#include "installer/CommandRunner.hpp"

#include <QStandardPaths>
#include <QTimer>

#include "installer/Requirements.hpp"

namespace
{
  constexpr int MaxTailChars = 16000;
  constexpr int KillGraceMs = 3000;
}


ProcessRunner::ProcessRunner(QObject* parent)
  : CommandRunner(parent)
{
  m_process.setProcessChannelMode(QProcess::MergedChannels);

  connect(&m_process, &QProcess::readyReadStandardOutput, this, [this](){
    m_buffer += m_process.readAllStandardOutput();
    flush(false);
  });

  connect(&m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status){
    flush(true);
    if(!m_reported){
      m_reported = true;
      emit finished(status == QProcess::NormalExit ? exitCode : -1, m_tail);
    }
  });

  connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error){
    if(error == QProcess::FailedToStart && !m_reported){
      m_reported = true;
      emit finished(-1, tr("Could not start %1").arg(m_process.program()));
    }
  });
}


void ProcessRunner::run(const Command& command)
{
  m_buffer.clear();
  m_tail.clear();
  m_reported = false;

  // QProcess resolves bare names against our own PATH, so look them up in the clean one
  auto program = command.program;
  if(!program.contains('/')){
    const auto found = QStandardPaths::findExecutable(program, Requirements::systemPath());
    if(!found.isEmpty()){
      program = found;
    }
  }

  m_process.setProcessEnvironment(Requirements::cleanEnvironment());
  m_process.setWorkingDirectory(command.workDir);
  m_process.start(program, command.arguments);
}


void ProcessRunner::cancel()
{
  if(m_process.state() == QProcess::NotRunning){
    return;
  }

  m_process.terminate();
  QTimer::singleShot(KillGraceMs, &m_process, [this](){
    if(m_process.state() != QProcess::NotRunning){
      m_process.kill();
    }
  });
}


void ProcessRunner::flush(bool final)
{
  qsizetype newline;
  while((newline = m_buffer.indexOf('\n')) >= 0){
    const auto line = QString::fromUtf8(m_buffer.left(newline)).trimmed();
    m_buffer.remove(0, newline + 1);
    m_tail += line + '\n';
    emit outputLine(line);
  }

  if(final && !m_buffer.isEmpty()){
    const auto line = QString::fromUtf8(m_buffer).trimmed();
    m_buffer.clear();
    m_tail += line + '\n';
    emit outputLine(line);
  }

  if(m_tail.size() > MaxTailChars){
    m_tail = m_tail.right(MaxTailChars);
  }
}
