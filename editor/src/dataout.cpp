#include "dataout.hpp"
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
static void closeSock(intptr_t s) { closesocket((SOCKET)s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
static void closeSock(intptr_t s) { close((int)s); }
#endif

namespace fh6 {

double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

template <class T> static T rd(const uint8_t* d, size_t off) { T v; std::memcpy(&v, d + off, sizeof v); return v; }
static void rd3(const uint8_t* d, size_t off, float* v) { for (int i = 0; i < 3; i++) v[i] = rd<float>(d, off + 4 * i); }
static void rd4(const uint8_t* d, size_t off, float* v) { for (int i = 0; i < 4; i++) v[i] = rd<float>(d, off + 4 * i); }

// Byte offsets of the FH6 Data Out packet (little endian), from Playground's
// documentation: Forza Motorsport's layout plus CarGroup, SmashableVelDiff and
// SmashableMass after NumCylinders.
bool parseDataOut(const uint8_t* d, size_t n, Telemetry& t) {
    if (n < 323) return false;
    t.raceOn = rd<int32_t>(d, 0);
    t.timestampMs = rd<uint32_t>(d, 4);
    rd3(d, 20, t.accel);
    rd3(d, 32, t.vel);
    rd3(d, 44, t.angVel);
    t.yaw = rd<float>(d, 56);
    t.pitch = rd<float>(d, 60);
    t.roll = rd<float>(d, 64);
    rd4(d, 84, t.slipRatio);       // after NormalizedSuspensionTravel (68)
    rd4(d, 164, t.slipAngle);      // after WheelRotationSpeed, RumbleStrip, Puddle, SurfaceRumble
    rd4(d, 180, t.combinedSlip);
    t.carOrdinal = rd<int32_t>(d, 212);
    rd3(d, 244, t.pos);            // after CarGroup, SmashableVelDiff, SmashableMass (232..243)
    t.speed = rd<float>(d, 256);
    t.throttle = d[315];
    t.brake = d[316];
    t.handbrake = d[318];
    t.gear = d[319];
    t.steer = (int8_t)d[320];
    return true;
}

bool Receiver::start(int port) {
    stop();
#ifdef _WIN32
    static bool wsa = false;
    if (!wsa) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); wsa = true; }
#endif
    intptr_t s = (intptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (s < 0 || bind((decltype(socket(0, 0, 0)))s, (sockaddr*)&a, sizeof a) != 0) {
        if (s >= 0) closeSock(s);
        std::lock_guard<std::mutex> l(m_);
        err_ = "Port " + std::to_string(port) + " is in use. Pick another port here and in FH6.";
        return false;
    }
#ifdef _WIN32
    DWORD to = 200;
    setsockopt((SOCKET)s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof to);
#else
    timeval to{ 0, 200000 };
    setsockopt((int)s, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
#endif
    {
        std::lock_guard<std::mutex> l(m_);
        err_.clear();
        have_ = false;
    }
    sock_ = s;
    port_ = port;
    running_ = true;
    th_ = std::thread(&Receiver::run, this);
    return true;
}

void Receiver::stop() {
    running_ = false;
    if (th_.joinable()) th_.join();
    if (sock_ >= 0) { closeSock(sock_); sock_ = -1; }
    rate_ = 0;
}

bool Receiver::latest(Telemetry& t, double& age) const {
    std::lock_guard<std::mutex> l(m_);
    if (!have_) { age = 1e9; return false; }
    t = last_;
    age = nowSec() - lastTime_;
    return true;
}

void Receiver::run() {
    uint8_t buf[2048];
    double winStart = nowSec();
    int count = 0;
    while (running_) {
        sockaddr_in from{};
        socklen_t fl = sizeof from;
        int n = (int)recvfrom((decltype(socket(0, 0, 0)))sock_, (char*)buf, sizeof buf, 0, (sockaddr*)&from, &fl);
        double now = nowSec();
        Telemetry t;
        if (n > 0 && parseDataOut(buf, (size_t)n, t)) {
            std::lock_guard<std::mutex> l(m_);
            last_ = t;
            have_ = true;
            lastTime_ = now;
            count++;
        }
        if (now - winStart >= 1.0) {
            rate_ = (float)(count / (now - winStart));
            count = 0;
            winStart = now;
        }
    }
}

} // namespace fh6
