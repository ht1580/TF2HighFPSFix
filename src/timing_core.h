#pragma once
#include <algorithm>
#include <cmath>

namespace tf2hf {
inline constexpr float commandFloor = 0.005f;
inline constexpr float maxFrameTime = 0.1f;
struct Decision { bool extra; float commandTime, sampleTime; };
struct Scheduler {
    float pending = 0;
    void reset() { pending = 0; }
    Decision advance(float frame, float timeScale, bool paused, float pauseElapsed) {
        float elapsed = paused ? pauseElapsed : frame + pending;
        const float scale = paused ? 1.0f : timeScale;
        const float delta = elapsed / scale;
        if (delta > maxFrameTime) elapsed = scale * maxFrameTime;
        if (timeScale == 1.0f && delta < commandFloor) {
            const float sample = !paused ? std::max(elapsed-pending,0.0f) : 0.0f;
            pending = elapsed;
            return {true,elapsed,sample};
        }
        pending = 0;
        return {false,elapsed,0};
    }
};
struct InputAccumulator {
    float extraTime=0, forward=0, side=0;
    bool extra=false, creating=false, restoreTime=false;
    float fullCommandTime=0;
    void reset() { extraTime=forward=side=fullCommandTime=0; extra=creating=restoreTime=false; }
    void resetMovement() { forward=side=0; }
    void addMouseMovement(float oldForward,float oldSide,float newForward,float newSide) {
        forward += newForward-oldForward; side += newSide-oldSide;
    }
    void merge(float& forwardMove,float& sideMove) {
        forwardMove+=forward; sideMove+=side; resetMovement();
    }
};
inline bool consumeButtons(bool requested,bool extraSample) { return requested&&!extraSample; }
struct PreservedKeyStates {
    int* states[8]{};
    int saved[8]{};
    int count;
    explicit PreservedKeyStates(int* const* pointers,int number):count(number) {
        for(int i=0;i<count;++i) { states[i]=pointers[i];saved[i]=*states[i]; }
    }
    ~PreservedKeyStates() {for(int i=0;i<count;++i) *states[i]=saved[i];}
    PreservedKeyStates(const PreservedKeyStates&)=delete;
};
struct TimeRemainder {
    const void* client=nullptr;
    double remainder=0;
    float last=0;
    float update(const void* self,float current,float frame,float uptime,float argument,bool accumulating) {
        if (client!=self || current!=last) remainder=0;
        client=self;
        if (accumulating && std::isfinite(current) && std::isfinite(frame) && std::isfinite(uptime)) {
            const double time=std::min(double(current)+remainder+double(frame),double(uptime));
            argument=float(time); remainder=time-double(argument);
        } else remainder=0;
        last=argument;
        return argument;
    }
};
}
