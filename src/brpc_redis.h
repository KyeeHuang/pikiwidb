#pragma once
#include <brpc/channel.h>
#include <brpc/redis.h>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "brpc/redis_reply.h"
#include "redis.h"
#include "base_task.h"

namespace pikiwidb {

class BrpcRedis : public Redis {
public:
  void Init() { options.protocol = brpc::PROTOCOL_REDIS; }
  
  void Open();
  
  void PushRedisTask(const std::shared_ptr<BaseTask>& task);

  void Commit();
  
private:
  void SetResponse(const brpc::RedisResponse& resp, const std::shared_ptr<BaseTask>& task, size_t index);

  brpc::Channel channel_;
  brpc::ChannelOptions options;
  std::mutex lock__;
  std::vector<std::shared_ptr<BaseTask>> tasks_;
  size_t batch_size_ = 5; 
};
}

