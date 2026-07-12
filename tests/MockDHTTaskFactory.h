#ifndef D_MOCK_DHT_TASK_FACTORY_H
#define D_MOCK_DHT_TASK_FACTORY_H

#include "DHTTaskFactory.h"

namespace aria2 {

class MockDHTTaskFactory : public DHTTaskFactory {
public:
  virtual ~MockDHTTaskFactory() {}

  virtual std::shared_ptr<DHTTask>
  createPingTask(const std::shared_ptr<DHTNode>& remoteNode,
                 int numRetry = 0) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask>
  createNodeLookupTask(const unsigned char* targetID) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask> createBucketRefreshTask() override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask> createPeerLookupTask(
      const std::shared_ptr<DownloadContext>& ctx, uint16_t tcpPort,
      const std::shared_ptr<PeerStorage>& peerStorage) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask>
  createPeerAnnounceTask(const unsigned char* infoHash) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask>
  createReplaceNodeTask(const std::shared_ptr<DHTBucket>& bucket,
                        const std::shared_ptr<DHTNode>& newNode) override
  {
    return nullptr;
  }
};

} // namespace aria2

#endif // D_MOCK_DHT_TASK_FACTORY_H
