/*
 * @Description：A-TAS 游戏倍速设置
 */
#pragma once
#ifndef __GAME_CONTROLLER_H__
#define __GAME_CONTROLLER_H__
#include "keyconnector.h"

// 和暂停相关的代码移到这里，主要是与恢复原速相关
static bool SnapshotModeSwitch = false;
static bool PausedSlowed = false;

static bool Paused = false;
static int PausedCd = 480;

static std::vector<double> SpeedGearsVec = {0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
static std::string SpeedGearsDefault = "0.1 0.25 0.5 1.0 2.5 5.0 10.0";

inline void Decelerate() {
    int game_speed_gear = SpeedGearsVec.size();
    int ms = AGetPvzBase()->TickMs();
    for (int i = 0; i < SpeedGearsVec.size(); ++i) {
        int set = int(10 / SpeedGearsVec[i] + 0.5);
        if (ms >= set) {
            game_speed_gear = i;
            break;
        }
    }
    if (game_speed_gear > 0) {
        auto speed = SpeedGearsVec[game_speed_gear - 1];
        ASetGameSpeed(speed);
    }
}

inline void Accelerate() {
    int game_speed_gear = 0;
    int ms = AGetPvzBase()->TickMs();
    for (int i = SpeedGearsVec.size() - 1; i >= 0; --i) {
        int set = int(10 / SpeedGearsVec[i] + 0.5);
        if (ms <= set) {
            game_speed_gear = i;
            break;
        }
    }
    if (game_speed_gear < SpeedGearsVec.size() - 1) {
        auto speed = SpeedGearsVec[game_speed_gear + 1];
        ASetGameSpeed(speed);
    }
}

// 0.25倍速
inline void ResetSpeed() {
    if (SnapshotModeSwitch)
        return;
    if (PausedCd < 480)
        return;
    if (Paused) {
        Paused = false;
        PausedSlowed = true;
        ASetAdvancedPause(Paused, false, 0);
        AGetPvzBase()->TickMs() = 40;
    } else if (PausedSlowed) {
        Paused = true;
        PausedSlowed = false;
        ASetAdvancedPause(Paused, false, 0);
        AGetPvzBase()->TickMs() = 10;
    } else {
        AGetPvzBase()->TickMs() = AGetPvzBase()->TickMs() == 10 ? 40 : 10;
    }
}

// 设置速度档位
inline void SetGameSpeedGears(const std::string& str) {
    std::set<double> result;
    std::istringstream iss(str);
    std::string temp;
    while (std::getline(iss, temp, ' ')) {
        auto val = std::stof(temp);
        if (val >= 0.05 && val <= 10)
            result.insert(val);
        else
            aLogger->Error("倍速设置的范围为 [0.05, 10],您填入的{}倍速不在此范围内，已自动忽略", val);
    }
    SpeedGearsVec = std::vector<double>(result.begin(), result.end());
}

#endif //!__GAME_CONTROLLER_H__