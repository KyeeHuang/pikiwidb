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

const std::string kTaskNamePing = "ping";

enum TaskFlags {
  kTaskFlagsWrite = (1 << 0),
  
};

class BaseTask : public std::enable_shared_from_this<BaseTask> {
public:
  enum class OpType {
    kGet,
    kSet,
    kDel,
    kIncr,
    kDecr,
    kUnknown,
  };

  BaseTask() = default;
  virtual ~BaseTask() = default;

  virtual void Execute() = 0;
  virtual void CallBack() = 0;
  
  virtual std::string GetCommand() = 0;

  OpType GetOpType() const { return op_type_; }

protected:
  OpType op_type_ = OpType::kUnknown;

private:
  virtual bool DoInitial() = 0;
};

}