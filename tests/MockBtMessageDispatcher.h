#ifndef D_MOCK_BT_MESSAGE_DISPATCHER_H
#define D_MOCK_BT_MESSAGE_DISPATCHER_H

#include "BtMessageDispatcher.h"

#include <algorithm>

#include "BtMessage.h"
#include "Piece.h"

namespace aria2 {

class MockBtMessageDispatcher : public BtMessageDispatcher {
public:
  std::deque<std::unique_ptr<BtMessage>> messageQueue;

  virtual ~MockBtMessageDispatcher() {}

  virtual void
  addMessageToQueue(std::unique_ptr<BtMessage> btMessage) override
  {
    messageQueue.push_back(std::move(btMessage));
  }

  virtual void sendMessages() override {}

  virtual void doCancelSendingPieceAction(size_t index, int32_t begin,
                                          int32_t length) override
  {
  }

  virtual void
  doCancelSendingPieceAction(const std::shared_ptr<Piece>& piece) override
  {
  }

  virtual void doAbortOutstandingRequestAction(
      const std::shared_ptr<Piece>& piece) override
  {
  }

  virtual void doChokedAction() override {}

  virtual void doChokingAction() override {}

  virtual void checkRequestSlotAndDoNecessaryThing() override {}

  virtual bool isSendingInProgress() override { return false; }

  virtual size_t countMessageInQueue() override
  {
    return messageQueue.size();
  }

  virtual size_t countOutstandingRequest() override { return 0; }

  virtual bool isOutstandingRequest(size_t index,
                                    size_t blockIndex) override
  {
    return false;
  }

  virtual const RequestSlot*
  getOutstandingRequest(size_t index, int32_t begin,
                        int32_t length) override
  {
    return nullptr;
  }

  virtual void removeOutstandingRequest(const RequestSlot* slot) override
  {
  }

  virtual void
  addOutstandingRequest(std::unique_ptr<RequestSlot> slot) override
  {
  }

  virtual size_t countOutstandingUpload() override { return 0; }
};

} // namespace aria2

#endif // D_MOCK_BT_MESSAGE_DISPATCHER_H
