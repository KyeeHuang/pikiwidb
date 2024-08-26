#pragma once
#include "base_task.h"
#include "brpc_redis.h"

namespace pikiwidb {


class SetTask : public BaseTask {
 public:
  enum SetCondition { kNONE, kNX, kXX, kEXORPX };
  SetTask(std::string key, std::string value) : key_(key), value_(value) {};
  
 protected:
  void Execute() override;
  void CallBack() override;
  bool DoInitial() override;
  std::string GetCommand() override;
  
 private:
  std::string key_;
  std::string value_;
  int64_t sec_ = 0;
  
  SetTask::SetCondition condition_{kNONE};

  std::unique_ptr<BrpcRedis> brpc_redis_;
};
  

} // namespace pikiwidb