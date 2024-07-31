#pragma once 

#include <atomic>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "base_cmd.h"
#include "client.h"
#include "store.h"

namespace pikiwidb {

// 

class BaseTask : public std::enable_shared_from_this<BaseTask> {
public:
  BaseTask(std::shared_ptr<PClient> client) : client_(std::move(client)) {}
  virtual ~BaseTask() = default;
  void Run(BaseCmd *cmd);
  void CallBack();
  
  const std::string &CmdName();
  std::shared_ptr<PClient> Client();

private:
  std::shared_ptr<PClient> client_;
};

}