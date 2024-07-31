#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <thread>

#include "client.h"
#include "base_task.h"
#include "cmd_thread_pool.h"
#include "net/event_loop.h"
#include "net/http_client.h"
#include "net/http_server.h"
#include "base_cmd.h"
#include "cmd_table_manager.h"

namespace pikiwidb {


class TaskPool {
public:
  TaskPool() = default;
  ~TaskPool() = default;
  
  static size_t GetMaxWorkerNum() { return kMaxWorkers; }
  
  bool Init();
  void Run(int argc, char* argv[]);
  virtual void Exit();
  virtual void PushTask(std::shared_ptr<BaseTask> task);
  virtual void PushCmd(std::string cmd);
  bool IsExit() const;
  void SetName(const std::string& name);

  bool Listen(const char* ip, int port, const TcpTaskCallback& ccb);

  void Connect(const char* ip, int port, const TcpTaskCallback& ccb,
               const TcpConnectionFailCallback& fcb = TcpConnectionFailCallback(), EventLoop* loop = nullptr);
  
  EventLoop* BaseLoop();
  
  EventLoop* ChooseNextWorkerEventLoop();
  
  bool SetWorkerNum(size_t n);

  void Reset();
    
protected:
  virtual void StartWorkers();
  
  static const size_t kMaxWorkers;
  
  std::string name_;
    
  EventLoop base_;
  
  std::atomic<size_t> worker_num_{0};
  std::vector<std::thread> worker_threads_;
  std::vector<std::unique_ptr<EventLoop>> worker_loops_;
  mutable std::atomic<size_t> current_work_loop_{0};
  
  enum class State {
    kNone,
    kStarted,
    kStopped,
  };
  std::atomic<State> state_{State::kNone};

  pikiwidb::CmdTableManager cmd_table_manager_;
};

class CmdTaskManager : public TaskPool {
public:
  CmdTaskManager() = default;
  ~CmdTaskManager() = default;
  
  void Exit() override;
  void PushTask(std::shared_ptr<BaseTask> task) override;
  void PushCmd(std::string cmd) override;
private:
  void StartWorkers() override;
  
private:
  std::vector<std::thread> CmdThreads_;
  std::vector<std::unique_ptr<std::mutex>> CmdMutex_;
  std::vector<std::unique_ptr<std::condition_variable>> CmdCond_;
  std::vector<std::deque<std::string>> CmdQueue_;
  
  std::vector<std::thread> TaskThreads_;
  std::vector<std::unique_ptr<std::mutex>> TaskMutex_;
  std::vector<std::unique_ptr<std::condition_variable>> TaskCond_;
  std::vector<std::deque<std::shared_ptr<BaseTask>>> TaskQueue_;

  std::atomic<uint64_t> counter_ = 0;
  std::atomic<uint64_t> t_counter_ = 0;
  bool CmdRunning_ = true;
};
}