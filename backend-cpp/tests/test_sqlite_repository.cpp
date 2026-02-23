#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "storage/sqlite_telemetry_repository.h"

namespace fs = std::filesystem;

namespace {

agri::TelemetryPacket BuildPacket() {
    agri::TelemetryPacket packet;
    packet.deviceId = "stm32-node-sqlite";
    packet.timestamp = 1700002000;
    packet.telemetryJson = "{\"temperature\":23.1}";
    packet.hashHex = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    packet.signature = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    packet.pubKeyId = "pubkey-1";
    packet.transport = "wifi";
    packet.batchCode = "BATCH-SQLITE-01";
    return packet;
}

void TestSqliteRepositoryRoundTrip() {
    const fs::path dbPath = fs::path("/tmp") / "agri_sqlite_repository_test.db";
    std::error_code ec;
    fs::remove(dbPath, ec);

    agri::SQLiteTelemetryRepository repository(dbPath.string());
    const agri::TelemetryPacket packet = BuildPacket();

    const std::uint64_t recordId = repository.Save(packet);
    assert(recordId == 1);
    assert(repository.Size() == 1);

    agri::BlockchainReceipt receipt;
    receipt.txHash = "0xtesttxhash";
    receipt.blockHeight = 12345;
    receipt.submittedAtIso8601 = "2026-02-23T00:00:00Z";
    assert(repository.AttachReceipt(recordId, receipt));

    const auto latest = repository.LatestByDevice(packet.deviceId);
    assert(latest.has_value());
    assert(latest->recordId == recordId);
    assert(latest->receipt.has_value());
    assert(latest->receipt->txHash == receipt.txHash);

    const auto byTx = repository.FindByTransaction(receipt.txHash);
    assert(byTx.has_value());
    assert(byTx->recordId == recordId);

    const auto byBatch = repository.FindByBatch(packet.batchCode);
    assert(byBatch.size() == 1);
    assert(byBatch[0].recordId == recordId);

    assert(repository.Delete(recordId));
    assert(repository.Size() == 0);
    assert(!repository.Delete(recordId));

    fs::remove(dbPath, ec);
}

}

int main() {
    TestSqliteRepositoryRoundTrip();
    std::cout << "test_sqlite_repository passed" << std::endl;
    return 0;
}
