#include "cmd_task.h"
#include "base_cmd.h"
#include <memory>
#include <utility>

namespace pikiwidb {

std::string SetTask::GetCommand() {
  return "set " + key_ + " " + value_;
}

bool SetTask::DoInitial() {
  

}
  
void SetTask::Execute() {
  brpc_redis_->PushRedisTask(shared_from_this());
  brpc_redis_->Commit();
} 

void SetTask::CallBack() {
  
  
}

} // namespace pikiwidb

// TODO:
// 1. 解析 config 文件，知道后台有多少 pikiwidb 
// 2. flag 以 proxy 模式启动
// 3. client