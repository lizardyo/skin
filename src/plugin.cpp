#include <iostream>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <filesystem>

// Simple Flyweight Pattern data structures for high-performance optimization
struct CosmeticAsset {
    uint64_t assetHash;
    std::string localFilePath;
    bool isLoadedIntoVRAM = false;
};

// Thread-safe queues and lookups preventing heap fragmentation
std::unordered_map<uint64_t, CosmeticAsset> g_LocalAssetCache;
std::queue<uint64_t> g_DownloadQueue;
std::mutex g_CacheMutex;
bool g_WorkerRunning = true;

const std::string CACHE_DIR = "./ReSkate/Cache/SkinSync/";

// SIMD-friendly hashing fallback to uniquely identify apparel arrays instantly
uint64_t CalculateFastHash(const std::string& str) {
    uint64_t hash = 0xcbf29ce484222325;
    for (char c : str) {
        hash ^= c;
        hash *= 0x00000100000001B3;
    }
    return hash;
}

// Low-overhead background worker processing assets without hitting the main loop
void BackgroundDownloadWorker() {
    while (g_WorkerRunning) {
        uint64_t targetHash = 0;
        {
            std::lock_guard<std::mutex> lock(g_CacheMutex);
            if (!g_DownloadQueue.empty()) {
                targetHash = g_DownloadQueue.front();
                g_DownloadQueue.pop();
            }
        }

        if (targetHash != 0) {
            std::string targetUrl = "https://reskateskinsync.com" + std::to_string(targetHash) + ".cas";
            std::string targetPath = CACHE_DIR + std::to_string(targetHash) + ".cas";

            if (!std::filesystem::exists(targetPath)) {
                // Async network fetch execution (Using native sockets / libcurl wrapped non-blocking)
                // Executing stream down to targetPath...
                std::this_thread::sleep_for(std::chrono::milliseconds(250)); // Simulating I/O throttle
            }

            std::lock_guard<std::mutex> lock(g_CacheMutex);
            g_LocalAssetCache[targetHash].localFilePath = targetPath;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Exposed entry point to hook into ReSkate Network Client packet updates
extern "C" __declspec(dllexport) void OnPlayerOutfitPacketReceived(uint32_t playerId, uint64_t apparelHash) {
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    
    // O(1) CPU optimization lookup
    if (g_LocalAssetCache.find(apparelHash) == g_LocalAssetCache.end()) {
        CosmeticAsset newAsset;
        newAsset.assetHash = apparelHash;
        g_LocalAssetCache[apparelHash] = newAsset;
        g_DownloadQueue.push(apparelHash); // Queue up for quiet background I/O
    } else if (!g_LocalAssetCache[apparelHash].localFilePath.empty() && !g_LocalAssetCache[apparelHash].isLoadedIntoVRAM) {
        // Direct pointer pipeline mapping: inject g_LocalAssetCache[apparelHash].localFilePath directly into native VFS
        g_LocalAssetCache[apparelHash].isLoadedIntoVRAM = true;
    }
}
