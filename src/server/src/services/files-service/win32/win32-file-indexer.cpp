#include "win32-file-indexer.hpp"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <common/file-category.hpp>
#include <cstdint>
#include <filesystem>
#include <ranges>
#include <sqlcipher/sqlite3.h>
#include <string>
#include <string_view>
#include <vector>
#include "fuzzy/fzf.hpp"
#include "vicinae.hpp"

namespace fs = std::filesystem;

namespace {

constexpr int SCAN_BATCH_SIZE = 500;

bool shouldSkipEntry(const fs::directory_entry &entry) {
  auto path = entry.path();
  auto filename = path.filename().string();

  if (!filename.empty() && filename[0] == '.') return true;
  if (filename == "desktop.ini" || filename == "Thumbs.db") return true;

  std::error_code ec;
  if (entry.is_directory(ec)) {
    static constexpr std::string_view SKIP_DIRS[] = {"__pycache__",  ".git",       ".svn",
                                                     ".hg",         "node_modules", ".cache",
                                                     ".npm",        ".cargo",      "AppData",
                                                     ".local",      ".config",     ".vscode",
                                                     ".idea",       ".claude",     ".opencode"};
    for (auto &skip : SKIP_DIRS) {
      if (filename == skip) return true;
    }
  }

  return false;
}

int categoryToInt(vicinae::FileCategory cat) { return static_cast<int>(cat); }

vicinae::FileCategory intToCategory(int val) { return static_cast<vicinae::FileCategory>(val); }

std::string lastErrorMessage(sqlite3 *db) {
  auto *msg = sqlite3_errmsg(db);
  return msg ? std::string(msg) : "unknown error";
}

} // namespace

Win32FileIndexer::Win32FileIndexer() {
  m_dbPath = (Omnicast::stateDir() / "file-index.db").string();
  initDatabase();

  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
    auto dir = fs::path(path.toStdString());
    runIncrementalScan(dir);
  });
}

bool Win32FileIndexer::isAvailable() const { return true; }

void Win32FileIndexer::initDatabase() {
  fs::create_directories(fs::path(m_dbPath).parent_path());

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(m_dbPath.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) !=
      SQLITE_OK) {
    qWarning() << "Failed to open file index database:" << lastErrorMessage(db).c_str();
    if (db) sqlite3_close_v2(db);
    return;
  }

  sqlite3_exec(db, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
  sqlite3_exec(db, "PRAGMA busy_timeout=5000", nullptr, nullptr, nullptr);

  sqlite3_exec(db, R"(
    CREATE TABLE IF NOT EXISTS indexed_file (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      path TEXT UNIQUE NOT NULL,
      parent_id INT,
      last_modified_at INT,
      indexed_at INT NOT NULL DEFAULT (unixepoch()),
      type INT NOT NULL DEFAULT 0,
      category INT NOT NULL DEFAULT 0,
      size_bytes INT
    );
  )",
               nullptr, nullptr, nullptr);

  sqlite3_exec(db, R"(
    CREATE INDEX IF NOT EXISTS indexed_file_parent_id_idx ON indexed_file(parent_id);
  )",
               nullptr, nullptr, nullptr);

  sqlite3_exec(db, R"(
    CREATE INDEX IF NOT EXISTS indexed_file_category_idx ON indexed_file(category);
  )",
               nullptr, nullptr, nullptr);

  sqlite3_close_v2(db);
}

std::vector<fs::path> Win32FileIndexer::getDefaultSearchPaths() const {
  std::vector<fs::path> paths;

  auto dataLocations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for (const auto &loc : dataLocations) {
    auto p = fs::path(loc.toStdString());
    if (fs::is_directory(p)) paths.push_back(p);
  }

  auto userProfile = QDir::home();
  if (userProfile.exists()) {
    for (const auto &subdir : {"Documents", "Desktop", "Downloads"}) {
      auto p = fs::path(userProfile.absoluteFilePath(subdir).toStdString());
      if (fs::is_directory(p)) paths.push_back(p);
    }
  }

  return paths;
}

void Win32FileIndexer::start() {
  if (m_running) return;
  m_running = true;

  m_searchPaths = getDefaultSearchPaths();
  updateWatchedPaths();

  QtConcurrent::run([this]() {
    for (const auto &path : m_searchPaths) {
      runFullScan(path);
    }
  });
}

void Win32FileIndexer::rebuildIndex() {
  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(m_dbPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return;

  sqlite3_exec(db, "DELETE FROM indexed_file", nullptr, nullptr, nullptr);
  sqlite3_close_v2(db);

  QtConcurrent::run([this]() {
    for (const auto &path : m_searchPaths) {
      runFullScan(path);
    }
  });
}

void Win32FileIndexer::preferenceValuesChanged(const QJsonObject &preferences) {
  std::vector<fs::path> newPaths;
  auto indexingPaths = preferences.value("indexingPaths").toArray();
  for (const auto &v : indexingPaths) {
    if (v.isString()) newPaths.emplace_back(v.toString().toStdString());
  }

  if (newPaths.empty()) {
    newPaths = getDefaultSearchPaths();
  }

  m_excludedPaths.clear();
  auto excludedPaths = preferences.value("excludedIndexingPaths").toArray();
  for (const auto &v : excludedPaths) {
    if (v.isString()) m_excludedPaths.emplace_back(v.toString().toStdString());
  }

  m_searchPaths = std::move(newPaths);
  updateWatchedPaths();

  QtConcurrent::run([this]() {
    rebuildIndex();
  });
}

void Win32FileIndexer::updateWatchedPaths() {
  QStringList dirs;
  for (const auto &path : m_searchPaths) {
    if (fs::is_directory(path)) {
      dirs.append(QString::fromStdString(path.string()));
    }
  }

  auto currentDirs = m_watcher.directories();
  if (!currentDirs.isEmpty()) {
    m_watcher.removePaths(currentDirs);
  }
  if (!dirs.isEmpty()) {
    m_watcher.addPaths(dirs);
  }
}

void Win32FileIndexer::runFullScan(const fs::path &root) {
  if (!fs::is_directory(root)) return;

  int scanId = ++m_scanCounter;

  ScanStatus status{.scanId = scanId, .kind = ScanKind::Full, .state = ScanState::Started, .entrypoint = root, .processedFileCount = 0};
  emit scanStatusChanged(status);

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(m_dbPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
    ScanStatus fail{.scanId = scanId, .kind = ScanKind::Full, .state = ScanState::Failed, .entrypoint = root, .processedFileCount = 0};
    emit scanStatusChanged(fail);
    return;
  }

  sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);

  std::error_code ec;
  auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_close_v2(db);
    ScanStatus fail{.scanId = scanId, .kind = ScanKind::Full, .state = ScanState::Failed, .entrypoint = root, .processedFileCount = 0};
    emit scanStatusChanged(fail);
    return;
  }

  auto end = fs::recursive_directory_iterator();

  sqlite3_stmt *insertStmt = nullptr;
  sqlite3_prepare_v2(db,
                     "INSERT OR REPLACE INTO indexed_file (path, last_modified_at, type, category, "
                     "size_bytes) VALUES (?1, ?2, ?3, ?4, ?5)",
                     -1, &insertStmt, nullptr);

  int64_t count = 0;

  for (; it != end; it.increment(ec)) {
    if (ec) continue;

    const auto &entry = *it;
    if (shouldSkipEntry(entry)) {
      if (entry.is_directory(ec)) it.disable_recursion_pending();
      continue;
    }

    auto pathStr = entry.path().string();
    auto lastWrite = fs::last_write_time(entry, ec);
    auto lastWriteSec = std::chrono::duration_cast<std::chrono::seconds>(lastWrite.time_since_epoch()).count();
    auto fileSize = entry.is_regular_file(ec) ? static_cast<int64_t>(entry.file_size(ec)) : 0;
    auto isDir = entry.is_directory(ec);
    auto cat = vicinae::fileCategoryFor(entry.path(), isDir);

    sqlite3_bind_text(insertStmt, 1, pathStr.c_str(), static_cast<int>(pathStr.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStmt, 2, static_cast<sqlite3_int64>(lastWriteSec));
    sqlite3_bind_int(insertStmt, 3, isDir ? 1 : 0);
    sqlite3_bind_int(insertStmt, 4, categoryToInt(cat));
    sqlite3_bind_int64(insertStmt, 5, fileSize);
    sqlite3_step(insertStmt);
    sqlite3_reset(insertStmt);
    sqlite3_clear_bindings(insertStmt);

    count++;
    if (count % SCAN_BATCH_SIZE == 0) {
      ScanStatus progress{.scanId = scanId, .kind = ScanKind::Full, .state = ScanState::Started, .entrypoint = root, .processedFileCount = static_cast<size_t>(count)};
      emit scanStatusChanged(progress);
    }
  }

  sqlite3_finalize(insertStmt);
  sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
  sqlite3_close_v2(db);

  ScanStatus done{.scanId = scanId, .kind = ScanKind::Full, .state = ScanState::Succeeded, .entrypoint = root, .processedFileCount = static_cast<size_t>(count)};
  emit scanStatusChanged(done);
}

void Win32FileIndexer::runIncrementalScan(const fs::path &root) {
  if (!fs::is_directory(root)) return;

  int scanId = ++m_scanCounter;

  ScanStatus status{.scanId = scanId, .kind = ScanKind::Incremental, .state = ScanState::Started, .entrypoint = root, .processedFileCount = 0};
  emit scanStatusChanged(status);

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(m_dbPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
    ScanStatus fail{.scanId = scanId, .kind = ScanKind::Incremental, .state = ScanState::Failed, .entrypoint = root, .processedFileCount = 0};
    emit scanStatusChanged(fail);
    return;
  }

  sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);

  std::error_code ec;
  auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_close_v2(db);
    ScanStatus fail{.scanId = scanId, .kind = ScanKind::Incremental, .state = ScanState::Failed, .entrypoint = root, .processedFileCount = 0};
    emit scanStatusChanged(fail);
    return;
  }

  auto end = fs::recursive_directory_iterator();

  sqlite3_stmt *insertStmt = nullptr;
  sqlite3_prepare_v2(db,
                     "INSERT OR REPLACE INTO indexed_file (path, last_modified_at, type, category, "
                     "size_bytes) VALUES (?1, ?2, ?3, ?4, ?5)",
                     -1, &insertStmt, nullptr);

  sqlite3_stmt *lookupStmt = nullptr;
  sqlite3_prepare_v2(db, "SELECT last_modified_at FROM indexed_file WHERE path = ?1", -1, &lookupStmt,
                     nullptr);

  int64_t count = 0;

  for (; it != end; it.increment(ec)) {
    if (ec) continue;

    const auto &entry = *it;
    if (shouldSkipEntry(entry)) {
      if (entry.is_directory(ec)) it.disable_recursion_pending();
      continue;
    }

    auto pathStr = entry.path().string();
    auto lastWrite = fs::last_write_time(entry, ec);
    auto lastWriteSec = std::chrono::duration_cast<std::chrono::seconds>(lastWrite.time_since_epoch()).count();

    sqlite3_bind_text(lookupStmt, 1, pathStr.c_str(), static_cast<int>(pathStr.size()), SQLITE_TRANSIENT);
    bool needsUpdate = true;
    if (sqlite3_step(lookupStmt) == SQLITE_ROW) {
      auto existingMod = sqlite3_column_int64(lookupStmt, 0);
      needsUpdate = (existingMod != lastWriteSec);
    }
    sqlite3_reset(lookupStmt);
    sqlite3_clear_bindings(lookupStmt);

    if (needsUpdate) {
      auto fileSize = entry.is_regular_file(ec) ? static_cast<int64_t>(entry.file_size(ec)) : 0;
      auto isDir = entry.is_directory(ec);
      auto cat = vicinae::fileCategoryFor(entry.path(), isDir);

      sqlite3_bind_text(insertStmt, 1, pathStr.c_str(), static_cast<int>(pathStr.size()), SQLITE_TRANSIENT);
      sqlite3_bind_int64(insertStmt, 2, static_cast<sqlite3_int64>(lastWriteSec));
      sqlite3_bind_int(insertStmt, 3, isDir ? 1 : 0);
      sqlite3_bind_int(insertStmt, 4, categoryToInt(cat));
      sqlite3_bind_int64(insertStmt, 5, fileSize);
      sqlite3_step(insertStmt);
      sqlite3_reset(insertStmt);
      sqlite3_clear_bindings(insertStmt);
      count++;
    }
  }

  sqlite3_finalize(insertStmt);
  sqlite3_finalize(lookupStmt);
  sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
  sqlite3_close_v2(db);

  ScanStatus done{.scanId = scanId, .kind = ScanKind::Incremental, .state = ScanState::Succeeded, .entrypoint = root, .processedFileCount = static_cast<size_t>(count)};
  emit scanStatusChanged(done);
}

QFuture<std::vector<IndexerFileResult>> Win32FileIndexer::queryAsync(std::string_view query,
                                                                    const IndexerQueryParams &params) {
  return QtConcurrent::run([this, q = std::string{query}, params]() -> std::vector<IndexerFileResult> {
    if (q.empty()) return {};

    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(m_dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return {};

    std::string sql = "SELECT path, category FROM indexed_file WHERE path LIKE ?1";
    std::vector<std::string> bindValues;

    std::string likePattern = "%" + q + "%";
    bindValues.push_back(likePattern);

    if (params.category) {
      sql += " AND category = ?2";
      bindValues.push_back(std::to_string(categoryToInt(*params.category)));
    }

    sql += " LIMIT 1000";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr) != SQLITE_OK) {
      sqlite3_close_v2(db);
      return {};
    }

    for (size_t i = 0; i < bindValues.size(); i++) {
      sqlite3_bind_text(stmt, static_cast<int>(i + 1), bindValues[i].c_str(),
                        static_cast<int>(bindValues[i].size()), SQLITE_TRANSIENT);
    }

    struct Candidate {
      std::filesystem::path path;
      vicinae::FileCategory category;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(1000);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      auto *pathText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      auto cat = intToCategory(sqlite3_column_int(stmt, 1));

      if (pathText) {
        candidates.push_back(Candidate{.path = fs::path(pathText), .category = cat});
      }
    }

    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);

    const auto &matcher = fzf::threadLocalMatcher();
    struct Scored {
      std::filesystem::path path;
      int score = 0;
      vicinae::FileCategory category;
    };

    std::vector<Scored> scored;
    scored.reserve(candidates.size());

    for (auto &cand : candidates) {
      auto filename = cand.path.filename().string();
      auto parentStr = cand.path.parent_path().string();
      auto score = matcher.fuzzy_match_v2_score_query(filename, q);
      if (score <= 0) {
        score = static_cast<int>(matcher.fuzzy_match_v2_score_query(parentStr, q) * 0.7);
      }
      if (score > 0) {
        scored.push_back(Scored{.path = std::move(cand.path), .score = score, .category = cand.category});
      }
    }

    std::ranges::stable_sort(scored, [](const Scored &a, const Scored &b) { return a.score > b.score; });

    int limit = std::max(0, params.limit);
    size_t end = std::min(static_cast<size_t>(limit), scored.size());

    std::vector<IndexerFileResult> results;
    results.reserve(end);

    for (size_t i = 0; i < end; i++) {
      results.emplace_back(IndexerFileResult{.path = std::move(scored[i].path),
                                              .rank = static_cast<double>(scored[i].score),
                                              .category = scored[i].category,
                                              .mimeType = std::nullopt});
    }

    return results;
  });
}
