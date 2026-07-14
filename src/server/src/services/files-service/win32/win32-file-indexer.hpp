#pragma once
#include "services/files-service/abstract-file-indexer.hpp"
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

class Win32FileIndexer : public AbstractFileIndexer {
  Q_OBJECT

public:
  Win32FileIndexer();

  void start() override;
  void rebuildIndex() override;
  void preferenceValuesChanged(const QJsonObject &preferences) override;
  QFuture<std::vector<IndexerFileResult>> queryAsync(std::string_view query,
                                                     const IndexerQueryParams &params = {}) override;
  bool isAvailable() const override;

private:
  void initDatabase();
  void runFullScan(const std::filesystem::path &root);
  void runIncrementalScan(const std::filesystem::path &root);
  void updateWatchedPaths();
  std::vector<std::filesystem::path> getDefaultSearchPaths() const;

  QFileSystemWatcher m_watcher;
  std::vector<std::filesystem::path> m_searchPaths;
  std::string m_dbPath;
  std::atomic<int> m_scanCounter = 0;
  bool m_running = false;
};
