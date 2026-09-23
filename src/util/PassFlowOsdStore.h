#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

// Process-wide last pass-flow totals so OSD survives preview reconnect.
struct PassFlowOsdStore {
    static void Put(const std::string& taskId, int enter, int leave) {
        auto& s = Inst();
        std::lock_guard<std::mutex> g(s.mu);
        s.m[taskId] = {enter, leave};
    }
    static std::pair<int, int> Get(const std::string& taskId) {
        auto& s = Inst();
        std::lock_guard<std::mutex> g(s.mu);
        auto it = s.m.find(taskId);
        return it == s.m.end() ? std::pair<int, int>{0, 0} : it->second;
    }

private:
    struct State {
        std::mutex mu;
        std::unordered_map<std::string, std::pair<int, int>> m;
    };
    static State& Inst() {
        static State s;
        return s;
    }
};
