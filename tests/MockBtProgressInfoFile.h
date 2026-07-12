#ifndef D_MOCK_BT_PROGRESS_INFO_FILE_H
#define D_MOCK_BT_PROGRESS_INFO_FILE_H

#include "BtProgressInfoFile.h"

namespace aria2 {

class MockBtProgressInfoFile : public BtProgressInfoFile {
private:
  std::string filename;

public:
  MockBtProgressInfoFile() {}
  virtual ~MockBtProgressInfoFile() {}

  virtual std::string getFilename() override { return filename; }

  void setFilename(const std::string& filename) { this->filename = filename; }

  virtual bool exists() override { return false; }

  virtual void save() override {}

  virtual void load() override {}

  virtual void removeFile() override {}

  virtual void updateFilename() override {}
};

} // namespace aria2

#endif // D_MOCK_BT_PROGRESS_INFO_FILE_H
