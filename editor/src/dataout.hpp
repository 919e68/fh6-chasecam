// FH6 Data Out: the game's official telemetry, one UDP packet per frame while
// driving. Enable in Settings > HUD and Gameplay > Data Out (IP 127.0.0.1 and
// the port shown in the editor; avoid 5200-5300, the game uses those).
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace fh6 {

struct Telemetry {
    int32_t raceOn = 0;
    uint32_t timestampMs = 0;
    float accel[3] = {}, vel[3] = {}, angVel[3] = {}; // car space: x right, y up, z forward
    float yaw = 0, pitch = 0, roll = 0;               // radians
    float slipRatio[4] = {}, slipAngle[4] = {}, combinedSlip[4] = {}; // FL FR RL RR; > 1 = grip lost
    int32_t carOrdinal = 0;
    float pos[3] = {};                                // world, metres
    float speed = 0;                                  // m/s
    uint8_t throttle = 0, brake = 0, handbrake = 0, gear = 0;
    int8_t steer = 0;
};

// Packet is 324 bytes (323 bytes of fields + 1 pad). Returns false if too short.
bool parseDataOut(const uint8_t* data, size_t size, Telemetry& out);

// Listens on 127.0.0.1:<port> in a background thread.
class Receiver {
public:
    ~Receiver() { stop(); }
    bool start(int port);   // false + error() if the port can't be opened
    void stop();
    int port() const { return port_; }
    std::string error() const { std::lock_guard<std::mutex> l(m_); return err_; }
    // Latest packet; age in seconds since it arrived (large if none yet)
    bool latest(Telemetry& t, double& ageSec) const;
    float rate() const { return rate_.load(); } // packets per second
private:
    void run();
    mutable std::mutex m_;
    Telemetry last_;
    bool have_ = false;
    double lastTime_ = 0;
    std::string err_;
    std::thread th_;
    std::atomic<bool> running_{ false };
    std::atomic<float> rate_{ 0 };
    intptr_t sock_ = -1;
    int port_ = 0;
};

double nowSec();

} // namespace fh6
