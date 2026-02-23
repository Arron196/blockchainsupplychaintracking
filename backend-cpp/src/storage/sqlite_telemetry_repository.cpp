#include "storage/sqlite_telemetry_repository.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

namespace fs = std::filesystem;

namespace agri {

namespace {

void ThrowIfSqlError(int code, sqlite3* db, const std::string& prefix) {
    if (code == SQLITE_OK || code == SQLITE_DONE || code == SQLITE_ROW) {
        return;
    }
    const char* message = (db != nullptr) ? sqlite3_errmsg(db) : "sqlite error";
    throw std::runtime_error(prefix + ": " + (message != nullptr ? message : "unknown"));
}

void ExecOrThrow(sqlite3* db, const std::string& sql) {
    char* error = nullptr;
    const int code = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
    if (code != SQLITE_OK) {
        const std::string message = (error != nullptr) ? std::string(error) : std::string("sqlite exec error");
        if (error != nullptr) {
            sqlite3_free(error);
        }
        throw std::runtime_error(message);
    }
}

sqlite3_stmt* PrepareOrThrow(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* statement = nullptr;
    const int code = sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr);
    ThrowIfSqlError(code, db, "prepare failed");
    return statement;
}

std::optional<std::string> ReadNullableText(sqlite3_stmt* statement, int columnIndex) {
    if (sqlite3_column_type(statement, columnIndex) == SQLITE_NULL) {
        return std::nullopt;
    }
    const unsigned char* value = sqlite3_column_text(statement, columnIndex);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(value));
}

void BindTextOrThrow(sqlite3* db, sqlite3_stmt* statement, int index, const std::string& value) {
    const int code = sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
    ThrowIfSqlError(code, db, "bind text failed");
}

void BindInt64OrThrow(sqlite3* db, sqlite3_stmt* statement, int index, std::int64_t value) {
    const int code = sqlite3_bind_int64(statement, index, value);
    ThrowIfSqlError(code, db, "bind int64 failed");
}

}  

SQLiteTelemetryRepository::SQLiteTelemetryRepository(const std::string& databasePath) {
    const fs::path path(databasePath);
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
    }

    const int code = sqlite3_open_v2(
        databasePath.c_str(),
        &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    ThrowIfSqlError(code, db_, "open sqlite failed");

    EnsureSchema();
}

SQLiteTelemetryRepository::~SQLiteTelemetryRepository() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::uint64_t SQLiteTelemetryRepository::Save(const TelemetryPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql =
        "INSERT INTO telemetry_records "
        "(device_id, timestamp, telemetry_json, hash_hex, signature, pub_key_id, transport, batch_code) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* statement = PrepareOrThrow(db_, sql);

    BindTextOrThrow(db_, statement, 1, packet.deviceId);
    BindInt64OrThrow(db_, statement, 2, static_cast<std::int64_t>(packet.timestamp));
    BindTextOrThrow(db_, statement, 3, packet.telemetryJson);
    BindTextOrThrow(db_, statement, 4, packet.hashHex);
    BindTextOrThrow(db_, statement, 5, packet.signature);
    BindTextOrThrow(db_, statement, 6, packet.pubKeyId);
    BindTextOrThrow(db_, statement, 7, packet.transport);
    if (packet.batchCode.empty()) {
        ThrowIfSqlError(sqlite3_bind_null(statement, 8), db_, "bind batch code failed");
    } else {
        BindTextOrThrow(db_, statement, 8, packet.batchCode);
    }

    const int code = sqlite3_step(statement);
    ThrowIfSqlError(code, db_, "insert telemetry failed");
    sqlite3_finalize(statement);

    return static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
}

bool SQLiteTelemetryRepository::AttachReceipt(std::uint64_t recordId, const BlockchainReceipt& receipt) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql =
        "UPDATE telemetry_records SET tx_hash = ?, block_height = ?, submitted_at = ? WHERE record_id = ?;";
    sqlite3_stmt* statement = PrepareOrThrow(db_, sql);

    BindTextOrThrow(db_, statement, 1, receipt.txHash);
    BindInt64OrThrow(db_, statement, 2, static_cast<std::int64_t>(receipt.blockHeight));
    BindTextOrThrow(db_, statement, 3, receipt.submittedAtIso8601);
    BindInt64OrThrow(db_, statement, 4, static_cast<std::int64_t>(recordId));

    const int code = sqlite3_step(statement);
    ThrowIfSqlError(code, db_, "attach receipt failed");
    const int changes = sqlite3_changes(db_);
    sqlite3_finalize(statement);
    return changes > 0;
}

std::optional<TelemetryRecord> SQLiteTelemetryRepository::LatestByDevice(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql =
        "SELECT record_id, device_id, timestamp, telemetry_json, hash_hex, signature, pub_key_id, transport, "
        "batch_code, tx_hash, block_height, submitted_at "
        "FROM telemetry_records WHERE device_id = ? ORDER BY timestamp DESC, record_id DESC LIMIT 1;";
    sqlite3_stmt* statement = PrepareOrThrow(db_, sql);
    BindTextOrThrow(db_, statement, 1, deviceId);

    const int code = sqlite3_step(statement);
    if (code == SQLITE_ROW) {
        const TelemetryRecord record = RowToRecord(statement);
        sqlite3_finalize(statement);
        return record;
    }
    ThrowIfSqlError(code, db_, "latest by device query failed");
    sqlite3_finalize(statement);
    return std::nullopt;
}

std::optional<TelemetryRecord> SQLiteTelemetryRepository::FindByTransaction(const std::string& txHash) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql =
        "SELECT record_id, device_id, timestamp, telemetry_json, hash_hex, signature, pub_key_id, transport, "
        "batch_code, tx_hash, block_height, submitted_at "
        "FROM telemetry_records WHERE tx_hash = ? LIMIT 1;";
    sqlite3_stmt* statement = PrepareOrThrow(db_, sql);
    BindTextOrThrow(db_, statement, 1, txHash);

    const int code = sqlite3_step(statement);
    if (code == SQLITE_ROW) {
        const TelemetryRecord record = RowToRecord(statement);
        sqlite3_finalize(statement);
        return record;
    }
    ThrowIfSqlError(code, db_, "find by transaction query failed");
    sqlite3_finalize(statement);
    return std::nullopt;
}

std::vector<TelemetryRecord> SQLiteTelemetryRepository::FindByBatch(const std::string& batchCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TelemetryRecord> result;

    const std::string sql =
        "SELECT record_id, device_id, timestamp, telemetry_json, hash_hex, signature, pub_key_id, transport, "
        "batch_code, tx_hash, block_height, submitted_at "
        "FROM telemetry_records WHERE batch_code = ? ORDER BY timestamp ASC, record_id ASC;";
    sqlite3_stmt* statement = PrepareOrThrow(db_, sql);
    BindTextOrThrow(db_, statement, 1, batchCode);

    int code = sqlite3_step(statement);
    while (code == SQLITE_ROW) {
        result.push_back(RowToRecord(statement));
        code = sqlite3_step(statement);
    }
    ThrowIfSqlError(code, db_, "find by batch query failed");
    sqlite3_finalize(statement);
    return result;
}

std::uint64_t SQLiteTelemetryRepository::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql = "SELECT COUNT(1) FROM telemetry_records;";
    sqlite3_stmt* statement = PrepareOrThrow(db_, sql);
    const int code = sqlite3_step(statement);
    ThrowIfSqlError(code, db_, "size query failed");

    const std::uint64_t count = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 0));
    sqlite3_finalize(statement);
    return count;
}

void SQLiteTelemetryRepository::EnsureSchema() {
    const std::string sql =
        "CREATE TABLE IF NOT EXISTS telemetry_records ("
        "record_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_id TEXT NOT NULL,"
        "timestamp INTEGER NOT NULL,"
        "telemetry_json TEXT NOT NULL,"
        "hash_hex TEXT NOT NULL,"
        "signature TEXT NOT NULL,"
        "pub_key_id TEXT NOT NULL,"
        "transport TEXT NOT NULL,"
        "batch_code TEXT,"
        "tx_hash TEXT,"
        "block_height INTEGER,"
        "submitted_at TEXT,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_telemetry_device_time ON telemetry_records(device_id, timestamp DESC);"
        "CREATE INDEX IF NOT EXISTS idx_telemetry_batch ON telemetry_records(batch_code);"
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_telemetry_tx_hash ON telemetry_records(tx_hash);";
    ExecOrThrow(db_, sql);
}

TelemetryRecord SQLiteTelemetryRepository::RowToRecord(::sqlite3_stmt* statement) {

    TelemetryRecord record;
    record.recordId = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 0));
    record.packet.deviceId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    record.packet.timestamp = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 2));
    record.packet.telemetryJson = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    record.packet.hashHex = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
    record.packet.signature = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
    record.packet.pubKeyId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 6));
    record.packet.transport = reinterpret_cast<const char*>(sqlite3_column_text(statement, 7));

    const std::optional<std::string> batchCode = ReadNullableText(statement, 8);
    record.packet.batchCode = batchCode.value_or("");

    const std::optional<std::string> txHash = ReadNullableText(statement, 9);
    if (txHash.has_value()) {
        BlockchainReceipt receipt;
        receipt.txHash = *txHash;
        receipt.blockHeight = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 10));
        receipt.submittedAtIso8601 = ReadNullableText(statement, 11).value_or("");
        record.receipt = receipt;
    }

    return record;
}

}
