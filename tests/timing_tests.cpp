#include "timing_core.h"
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace tf2hf;
void check(bool good,const char* text) { if(!good) throw std::runtime_error(text); }
int main() {
    try {
        for(int fps:{144,180,190,195,200,205,210,240,300}) {
            Scheduler clock;
            const float dt=1.0f/fps;
            int commands=0,extras=0;
            double commandTime=0;
            for(int i=0;i<fps*6;++i) {
                const auto result=clock.advance(dt,1,false,0);
                if(result.extra) {++extras;check(result.sampleTime>0,"extra sample lost");}
                else {++commands;commandTime+=result.commandTime;check(result.commandTime>=commandFloor,"command below Vanilla floor");}
            }
            check(std::abs(commandTime+clock.pending-6.0)<0.001,"elapsed time not conserved");
            check(commands==(fps<=200 ? fps*6:fps*3),"unexpected command cadence");
            std::cout<<fps<<" FPS: simulated cmd/s="<<commands/6.0<<", extra/s="<<extras/6.0<<'\n';
        }
        Scheduler clock;
        auto first=clock.advance(1.f/240,1,false,0);
        auto second=clock.advance(1.f/240,1,false,0);
        check(first.extra && !second.extra,"240 scheduling");
        check(std::abs(second.commandTime-2.f/240)<1e-6,"full command time");
        // A movement delta generated on the skipped frame must be merged exactly once.
        InputAccumulator input;
        input.extraTime=first.sampleTime;
        input.addMouseMovement(5,7,8,5);
        float forward=10,side=20;
        input.merge(forward,side);
        check(forward==13 && side==18,"mouse movement not accumulated");
        input.merge(forward,side);
        check(forward==13 && side==18,"movement applied twice");
        check(std::abs(second.commandTime-input.extraTime-1.f/240)<1e-6,"view double integrated");
        // A press+release entirely on an extra frame carries both edge bits to the real command.
        int shortTap=6,held=3,nonPreserved=6;
        int* pointers[]={&shortTap,&held};
        {
            PreservedKeyStates keep(pointers,2);
            shortTap&=1;held&=1;nonPreserved&=1;
        }
        check(shortTap==6&&held==3&&nonPreserved==0,"movement edge restoration damaged input states");
        std::uint32_t latchedButtons=2;
        auto readButtons=[&](bool reset) {auto result=latchedButtons;if(reset) latchedButtons=0;return result;};
        check(readButtons(consumeButtons(true,true))==2&&latchedButtons==2,"extra sample consumed tap");
        check(readButtons(consumeButtons(true,false))==2&&latchedButtons==0,"real command did not consume tap exactly once");
        check(readButtons(consumeButtons(true,false))==0,"tap repeated on next command");
        clock.pending=0.003f;
        clock.reset();
        check(clock.pending==0,"reconnect retained command time");
        input.reset();
        check(input.extraTime==0 && input.forward==0,"input reset");
        Scheduler hitch;
        check(hitch.advance(0.5f,1,false,0).commandTime==0.1f,"hitch not capped");
        check(!hitch.advance(0.003f,2,false,0).extra,"timescale behavior differs from Ion");
        TimeRemainder precision;
        int client1=0,client2=0;
        float time=100000.f;
        const double begin=time;
        for(int i=0;i<24000;++i) time=precision.update(&client1,time,1.f/240,200000.f,time,true);
        check(std::abs(double(time)-begin-100)<0.01,"large-uptime precision drift");
        check(precision.update(&client1,time,1,1,time,true)==1,"server-time cap ignored");
        precision.update(&client1,1,0.01f,3,1.5f,false);
        check(precision.remainder==0,"non-accumulating call retained remainder");
        precision.update(&client2,2,0.01f,3,2,true);
        check(precision.client==&client2,"client switch not detected");
        std::cout<<"PASS: offline timing/input/precision tests; these are not game-runtime measurements\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return EXIT_FAILURE;}
}
