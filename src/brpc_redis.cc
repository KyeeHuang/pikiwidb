#include "brpc_redis.h"
#include <bthread/bthread.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include "base_task.h"

namespace pikiwidb {

void* thread_entry(void* arg) {
  auto func = static_cast<std::function<void()>*>(arg);
  (*func)();
  delete func;
  return nullptr;
}

void BrpcRedis::PushRedisTask(const std::shared_ptr<BaseTask>& task) {
  std::lock_guard<std::mutex> lock(lock__);
  tasks_.push_back(task);
}

void SetResponse(const brpc::RedisResponse& response, const std::shared_ptr<BaseTask>& task, size_t index) {
  // TODO: write callback
  LOG(INFO) << response.reply(index);


}

void BrpcRedis::Commit() {
  brpc::RedisRequest request;
  brpc::RedisResponse response;
  brpc::Controller cntl;
  std::vector<std::shared_ptr<BaseTask>> task_batch;
  size_t batch_size = std::min((size_t) tasks_.size(), batch_size_);

  {
    std::lock_guard<std::mutex> lock(lock__);
    task_batch.assign(tasks_.begin(), tasks_.begin() + batch_size);
    tasks_.erase(tasks_.begin(), tasks_.begin() + batch_size);
  }

  for (auto& task : task_batch) {
    request.AddCommand(task->GetCommand());
  } 
  
  bthread_t bthread;
  
  auto callback = new std::function<void()>([&]() {
    channel_.CallMethod(nullptr, &cntl, &request, &response, nullptr);
    for (size_t i = 0; i < task_batch.size(); i++) {
      SetResponse(response, task_batch[i], i);
    }
  });

  bthread_start_background(&bthread, nullptr, thread_entry, callback);
}
  
} // namespace pikiwidb