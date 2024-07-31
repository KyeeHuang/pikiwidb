#include <cassert>
#include <unique_ptr.h>
#include <condition_variable>

#include "cmd_task_manager.h"
#include "pstd/log.h"
#include "util.h"

namespace pikiwidb {

const size_t TaskPool::kMaxWorkers = 8;

bool TaskPool::SetWorkerNum(size_t num) {
  if (num <= 1) return true;
  if (state_ != State::kNone) {
    ERROR("can only called before application run");
    return false;
  }
  if (!worker_loops_.empty()) {
    ERROR("can only called once, not empty loops size: {}", worker_loops_.size());
    return false;
  }
  worker_num_.store(num);
  worker_threads_.reserve(num);
  worker_loops_.reserve(num);
  return true;
}

bool TaskPool::Init() {
  auto f = [this] { return ChooseNextWorkerEventLoop(); };
  
  base_.Init();
  INFO("base loop {} {}, g_baseLoop {}", base_.GetName(), static_cast<void*>(&base_)),
    static_cast<void*>(pikiwidb::EventLoop::Self());

  return true;
}

void TaskPool::Run(int ac, char* av[]) {
  assert(state_ == State::kNone);
  INFO("Process {} starting...", name_);
  
  StartWorkers();
  base_.Run();

  for (auto& w : worker_threads_) {
    if (w.joinable()) {
      w.join();
    }
  }

  worker_threads_.clear();
  
  INFO("Process {} stopped, goodbye...", name_);
}

void TaskPool::Exit() {
  state_ = State::kStopped;

  BaseLoop()->Stop();
  for (const auto& worker_loops : worker_loops_) {
    EventLoop* loop = worker_loops.get();
    loop->Stop();
  }
}

bool TaskPool::IsExit() const { return state_ == State::kStopped; }

EventLoop* TaskPool::BaseLoop() { return &base_; }

EventLoop* TaskPool::ChooseNextWorkerEventLoop() {
  if (worker_loops_.empty())
    return BaseLoop();
  
  auto& loop = worker_loops_[current_work_loop_++ % worker_loops_.size()];
  return loop.get();
}

void TaskPool::StartWorkers() {
  assert(state_ == State::kNone);
  
  size_t index = 1;
  while (worker_loops_.size() < worker_num_) {
    std::unique_ptr<EventLoop> loop = std::make_unique<EventLoop>();
    if (!name_.empty()) {
      loop->SetName(name_ + std::to_string(index++));
      INFO("loop {}, name {}", static_cast<void*>(loop.get()), loop->GetName().c_str());
    }
    worker_loops_.push_back(std::move(loop));
  }

  for (index = 0; index < worker_loops_.size(); ++index) {
    EventLoop* loop = worker_loops_[index].get();
    std::thread t([loop]() {
      loop->Init();
      loop->Run();
    });
    INFO("thread {}, thread loop {}, loop name {}", index, static_cast<void*>(loop), loop->GetName().c_str());
    worker_threads_.push_back(std::move(t));
  }

  state_ = State::kStarted;
}

void TaskPool::SetName(const std::string& name) { name_ = name; }

void TaskPool::Reset() {
  state_ = State::kNone;
  BaseLoop()->Reset();
}

bool TaskPool::Listen(const char* ip, int port, const TcpTaskCallback& ccb) {
  auto f = [this] { return ChooseNextWorkerEventLoop(); };
  auto loop = BaseLoop();
  return loop->Execute([loop, ip, port, ccb, f]() {return loop->Listen(ip, port, ccb, f); }).get();
}

void TaskPool::Connect(const char* ip, int port, const NewTcpConnectionCallback& ccb,
                       const TcpConnectionFailCallback& fcb, EventLoop* loop) {
  if (!loop) {
    loop = ChooseNextWorkerEventLoop();
  }

  std::string ipStr(ip);
  loop->Execute([loop, ipStr, port, ccb, fcb]() { loop->Connect(ipStr.c_str(), port, ccb, fcb); });
}

void CmdTaskManager::PushTask(std::shared_ptr<BaseTask> task) {
  auto pos = ++t_counter_ % worker_num_;
  std::unique_lock lock(*TaskMutex_[pos]);
  
  TaskQueue_[pos].emplace_back(task);
  TaskCond_[pos]->notify_one();
}

void CmdTaskManager::PushCmd(const std::string cmd) {
  auto pos = ++counter_ % worker_num_;
  std::unique_lock lock(*CmdMutex_[pos]);
  
  CmdQueue_[pos].emplace_back(cmd);
  CmdCond_[pos]->notify_one();
}

void CmdTaskManager::StartWorkers() {
  assert(state_ == State::kNone);
  
  TaskPool::StartWorkers();
  
  CmdMutex_.reserve(worker_num_);
  CmdCond_.reserve(worker_num_);
  CmdQueue_.reserve(worker_num_);
  TaskMutex_.reserve(worker_num_);
  TaskCond_.reserve(worker_num_);
  TaskQueue_.reserve(worker_num_);

  for (size_t index = 0; index < worker_num_; ++index) {
    CmdMutex_.emplace_back(std::make_unqiue<std::mutex>());
    CmdCond_.emplace_back(std::make_unqiue<std::condition_variable>());
    TaskMutex_.emplace_back(std::make_unqiue<std::mutex>());
    TaskCond_.emplace_back(std::make_unqiue<std::condition_variable>());
    CmdQueue_.emplace_back();
    TaskQueue_.emplace_back();

    std::thread t([this, index]() {
      while (CmdRunning_) {
        std::unique_lock lock(*CmdMutex_[index]);
        std::unique_lock lock(*TaskMutex_[index]);
        while (CmdQueue_[index].empty()) {
          if (!CmdRunning_) {
            break;
          }
          CmdCond_[index]->wait(lock);
        }
        if (!CmdRunning_) {
          break;
        }
        auto task = TaskQueue_[index].front();
        auto cmd = CmdQueue_[index].front();
        
        if (task->Client()->State() != ClientState::kOK) {
          continue;
        }
        
        auto [cmdPtr, ret] = cmd_table_manager_.GetCommand(cmd, task->Client().get());
        
        if (!cmdPtr) {
          if (ret == CmdRes::kInvalidParameter) {
            task->Client()->SetRes(CmdRes::kInvalidParameter);
          } else {
            task->Client()->SetRes(CmdRes::kSyntaxErr, "unknown command '" + cmd + "'");
          }
          continue;
        }

        if (!cmdPtr->CheckArg(task->Client()->ParamsSize())) {
          task->Client()->SetRes(CmdRes::kWrongNum, cmd);
          continue;
        }
        
        task->Run(cmdPtr);
        task->CallBack();
        
        TaskQueue_[index].pop_front();
        CmdQueue_[index].pop_front();
      }
      INFO("worker write thread {}, goodbye...", index);
    }); 

    INFO("worker write thread {}, starting...", index);
  }
}

void CmdTaskManager::Exit() {
  TaskPool::Exit();
  
  CmdRunning_ = false;
  int i = 0;
  for (auto& cond : CmdCond_) {
    std::unique_lock lock(*CmdMutex_[i++]);
    cond->notify_all();
  }
  for (auto& wt : CmdThreads_) {
    if (wt.joinable()) {
      wt.join();
    }
  }
  for (auto& cond : TaskCond_) {
    std::unique_lock lock(*TaskMutex_[i++]);
    cond->notify_all();
  }
  for (auto& wt : TaskThreads_) {
    if (wt.joinable()) {
      wt.join();
    }
  }
  CmdThreads_.clear();
  CmdCond_.clear();
  CmdQueue_.clear();
  CmdMutex_.clear();
  TaskThreads_.clear();
  TaskCond_.clear();
  TaskQueue_.clear();
  TaskMutex_.clear();
}

}