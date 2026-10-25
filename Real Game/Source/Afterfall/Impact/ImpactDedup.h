#pragma once
#include <cstdint>
#include <unordered_map>

namespace Afterfall
{
class FImpactDedup
{
public:
    explicit FImpactDedup(double InCooldownSec = 0.25, size_t InMaxEntries = 64)
        : CooldownSec(InCooldownSec), MaxEntries(InMaxEntries) {}

    static uint64_t MakeKey(uint32_t OtherId, uint32_t WallId)
    {
        return (static_cast<uint64_t>(OtherId) << 32) | WallId;
    }

    bool TryCommit(uint32_t OtherId, uint32_t WallId, double NowSec)
    {
        const uint64_t Key = MakeKey(OtherId, WallId);
        auto It = Last.find(Key);
        if (It != Last.end() && (NowSec - It->second) < CooldownSec)
            return false;
        if (It == Last.end() && Last.size() >= MaxEntries)
            Purge(NowSec);
        Last[Key] = NowSec;
        return true;
    }

    size_t Size() const { return Last.size(); }

private:
    void Purge(double NowSec)
    {
        for (auto It = Last.begin(); It != Last.end();)
            It = (NowSec - It->second) >= CooldownSec ? Last.erase(It) : ++It;
        if (Last.size() >= MaxEntries)
        {
            auto Oldest = Last.begin();
            for (auto It = Last.begin(); It != Last.end(); ++It)
                if (It->second < Oldest->second) Oldest = It;
            Last.erase(Oldest);
        }
    }

    double CooldownSec;
    size_t MaxEntries;
    std::unordered_map<uint64_t, double> Last;
};
}