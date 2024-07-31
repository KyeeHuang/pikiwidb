#include "base_task.h"
#include "log.h"
#include "client.h"

namespace pikiwidb {

void BaseTask::Run(BaseCmd *cmd) {
  cmd->Execute(client_.get());
}

void BaseTask::CallBack() {
  if (auto c = client_.get(); c)  {
    c->SendPacket(c->Message());
  }
}

const std::string &BaseTask::CmdName() {
  return client_->CmdName();
}

std::shared_ptr<PClient> BaseTask::Client() {
  return client_;
}

}