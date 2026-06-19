#pragma once
#ifndef __ATAS_H__
#define __ATAS_H__

#ifndef A_TAS_VERSION
#define A_TAS_VERSION 202603120330
#endif

#include "AsmFunc.h"
#include "Draw.h"
#include "asm_insert_code/asm_insert_code.h"
#include "dsl/shorthand.h"
#include "game_controller.h"

#include <array>
#include <climits>
#include <cmath>
#include <deque>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ATas {

// Preset settings
struct Settings {
    long long Version = A_TAS_VERSION;

    // General
    char SpeedGears[256];
    int WavelengthRecord = 3;
    int SkipTickWave = 0;
    int ReadOnly = -1;

    // Replay
    bool AutoRecordOnGameStart = true;
    bool ShowMouse = true;
    bool Interpolate = true;
    bool ShowReplayInfo = true;
    int recordTickInterval = 10;
    int tickRewindCount = 1;
    char savePath[256];

    // Special
    bool EnterHousePause = true;

    // Display
    bool ShowMe = true;
    bool PlantOffset = true;
    bool ProduceCD = true;
    bool CobCD = true;
    bool CobGloomHP = true;
    bool LilyPotHP = true;
    bool PumpkinHP = true;
    bool NutSpikeHP = true;
    bool OtherPlantHP = true;
    bool Crater = true;
    bool Icetrail = true;
    bool HPStyle = false;
    bool GigaStat = true;
    bool GigaHP = true;
    bool GigaCount = true;
    bool GargHP = true;
    bool ZomboniCount = true;
    bool FootballHP = true;
    bool FootballCount = true;
    bool JackCountdown = true;
    bool JackExplosionRange = true;
    bool TotalHP = true;
    bool ShowSpeed = true;
    bool CobColPreview = true;
    bool ActivationTime = true;
    int MarkerDuration = 300;
    bool VBEStat = true;

    // Display Color
    uint32_t ProduceCDARGB = 0xFFFFFF00;
    uint32_t CobCDARGB = 0xFFFFFF00;
    uint32_t CobGloomHPARGB = 0xFF4CAF50;
    uint32_t LilyPotHPARGB = 0xFF4CAF50;
    uint32_t PumpkinHPARGB = 0xFFFFA500;
    uint32_t NutSpikeHPARGB = 0xFF4CAF50;
    uint32_t OtherPlantHPARGB = 0xFF4CAF50;
    uint32_t CraterARGB = 0xFF965821;
    uint32_t IcetrailARGB = 0xFF16F2EB;
    uint32_t GigaStatARGB1 = 0xFFFF0000;
    uint32_t GigaStatARGB2 = 0xFF9868BC;
    uint32_t GigaHPARGB = 0xFFFF0000;
    uint32_t GigaCountARGB = 0xFFFF0000;
    uint32_t GargHPARGB = 0xFF9868BC;
    uint32_t ZomboniCountARGB = 0xFF0040FF;
    uint32_t FootballHPARGB = 0xFF6D706C;
    uint32_t FootballCountARGB = 0xFF6D706C;
    uint32_t JackCountdownARGB = 0xFFFF69B4;
    uint32_t JackExplosionRangeARGB = 0x9AFF0000;
    uint32_t TotalHPARGB1 = 0xFF9868BC;
    uint32_t TotalHPARGB2 = 0xFF6D706C;
    uint32_t ShowSpeedARGB1 = 0xFFFF0000;
    uint32_t ShowSpeedARGB2 = 0xFF00FF00;
    uint32_t VBEStatARGB = 0xFFFFFFFF;

    uint32_t PMarkerARGB = 0xFFFFA000;
    uint32_t IMarkerARGB = 0xFF00A0FF;
    uint32_t NMarkerARGB = 0xFF353535;
    uint32_t AMarkerARGB = 0xFFC00000;
    uint32_t JMarkerARGB = 0xFFFF0040;
    uint32_t WMarkerARGB = 0xFFA0B060;
    uint32_t MMarkerARGB = 0xFFDAC060;

    // Spawn
    bool Types[26] = {};
    bool ZombieList = false;
    bool AverageRowSpawn = false;
    bool RandomType = false;

    // Other
    bool Row6Plant = false;
    bool Row6Spawn = false;
    bool SmallPool = false;
    bool NormalPool = false;
    bool Row34PoolSpawn = false;
    bool SnorkelDolphinSpawn = false;

    bool AllowPoolAmbush = false;
    bool BanPoolAmbush = false;
    bool AllowSkyAmbush = false;
    bool BanSkyAmbush = false;

    bool AllowZomboni = false;
    bool BanZomboni = false;
    bool AllowSnorkel = false;
    bool BanSnorkel = false;
    bool AllowDolphin = false;
    bool BanDolphin = false;
    bool AllowDancing = false;
    bool BanDancing = false;
    bool AllowDigger = false;
    bool BanDigger = false;

    bool AllowBobsled = false;

    bool AllowPeashooterZombie = false;
    bool AllowWallnutZombie = false;
    bool AllowJalapenoZombie = false;
    bool AllowGatlingPeaZombie = false;
    bool AllowSquashZombie = false;
    bool AllowTallnutZombie = false;

};

inline int LeftmostIceTrail(int Row) {
    int LeftCol = 10;
    for (int Col = 9; Col > 0; --Col) {
        if (isIceTrailCover(Row, Col))
            LeftCol -= 1;
    }
    return LeftCol;
}

// 冰道倒计时
inline int IceTrailCoverCD(int Row) { return AMRef<int>(0x6A9EC0, 0x768, 0x620 + Row * 4); }

// 正弦/碧水原教旨智能铲除

class SmartRemoveController {
public:
    bool enabled = false;
    std::vector<std::vector<std::array<APlant*, 5>>> plantMap = std::vector<std::vector<std::array<APlant*, 5>>>(6, std::vector<std::array<APlant*, 5>>(9, {nullptr, nullptr, nullptr, nullptr, nullptr}));

    std::array<APlant*, 5>& Grid(int Row, int Col) { return plantMap[Row - 1][Col - 1]; }
    void UpdatePlantMap();
    void Tick();
    void Toggle() { enabled = !enabled; CreateCaption(enabled ? "SmartRemove: On" : "SmartRemove: Off"); }
    void Reset() { enabled = false; }
};

inline std::pair<int, int> GetDefenseRange(APlantType type) {
    switch (type) {
    case ATALL_NUT:
        return {30, 70};
    case APUMPKIN:
        return {20, 80};
    case ACOB_CANNON:
        return {20, 120};
    default:
        return {30, 50}; // 普通
    }
}

// 得到僵尸攻击域
inline std::pair<int, int> GetAttackRange(AZombieType type) {
    switch (type) {
    case AGIGA_GARGANTUAR:
    case AGARGANTUAR:
        return {-30, 59};
    default:
        return {10, 143}; // 双车
    }
}

// 判断某僵尸攻击域和某植物防御域是否重叠
// 植物坐标+植物防御域右伸>=僵尸坐标+僵尸攻击域左伸，且植物坐标+植物防御域左伸<=僵尸坐标+僵尸攻击域右伸
inline bool isRangeOverlap(APlantType plant_type, AZombieType zombie_type, int plant_x, int zombie_x) {
    auto plant_range = GetDefenseRange(plant_type);
    auto zombie_range = GetAttackRange(zombie_type);
    return plant_x + plant_range.second >= zombie_x + zombie_range.first && plant_x + plant_range.first <= zombie_x + zombie_range.second;
}

// 检查指定帧后是否有冰菇生效。应填写1~100的整数。没变身的模仿者不统计。
inline bool isCertainTickIce(int delayTime = 1) {
    for (auto& Plant : aAlivePlantFilter) {
        if (Plant.Type() == AICE_SHROOM && Plant.ExplodeCountdown() == delayTime && Plant.Hp() >= 0) // 没被投篮砸死
            return true;
    }
    return false;
}

// 遍历巨人
struct GargInfo {
    int row;
    double x;
};
inline std::vector<GargInfo>& GetHammeringGargInfo(bool hammerDownSoon = false, int row = -1) {
    static std::vector<GargInfo> Info;
    Info = {};
    bool canIce4 = false; // 下一帧是否可能Ice4（即是否有冰菇生效）。默认玩家不会自铲
    bool isCanIce4Verified = false;
    for (auto& Zombie : aAliveZombieFilter) {
        if (ARangeIn(Zombie.Type(), {AGIGA_GARGANTUAR, AGARGANTUAR}) && (row == -1 || Zombie.Row() == row - 1) && Zombie.State() == 70) {
            if (!isCanIce4Verified) {
                canIce4 = isCertainTickIce(1);
                isCanIce4Verified = true; // 没巨人举锤时不查冰菇，有巨人举锤时只查一次
            }
            float cr = Zombie.AnimationPtr()->CirculationRate();
            float crLast = Zombie.AnimationPtr()->MRef<float>(0x94); // 上一帧动画循环率
            bool isFrozen = Zombie.FreezeCountdown() ? true : false; // 僵尸是否被冻结
            if (hammerDownSoon && !isFrozen && !canIce4 && ((0.641 < cr && cr < 0.643) || (0.643 < cr && cr < 0.645 && 0.639 < crLast && crLast < 0.641)))
                Info.push_back({Zombie.Row() + 1, Zombie.Abscissa()}); // 包含了原速转减速
            else if (!hammerDownSoon && (cr < 0.641 || (0.641 < cr && cr < 0.643 && !isFrozen) || (0.643 < cr && cr < 0.645 && 0.639 < crLast && crLast < 0.641)))
                Info.push_back({Zombie.Row() + 1, Zombie.Abscissa()}); // 包含了原速转减速
        }
    }
    return Info;
}

// 遍历冰车
struct ZomboniInfo {
    int row;
    double x;
};
inline std::vector<ZomboniInfo>& GetZomboniInfo(int row = -1) {
    static std::vector<ZomboniInfo> Info;
    Info = {};
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AZOMBONI && (row == -1 || Zombie.Row() == row - 1) && Zombie.State() == 0)
            Info.push_back({Zombie.Row() + 1, Zombie.Abscissa()});
    }
    return Info;
}

// 遍历投篮
struct CatapultInfo {
    int row;
    double x;
};
inline std::vector<CatapultInfo>& GetCatapultInfo(int row = -1) {
    static std::vector<CatapultInfo> Info;
    Info = {};
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == ACATAPULT_ZOMBIE && (row == -1 || Zombie.Row() == row - 1) && Zombie.State() == 0) {
            Info.push_back({Zombie.Row() + 1, Zombie.Abscissa()});
        }
    }
    return Info;
}

// 检测下一帧植物会不会自然消失
// 目前只考虑樱桃 辣椒 核武 三叶 窝瓜 墓碑
inline bool isPlantDisappearedImmediately(APlant* Plant) {
    if (ARangeIn(Plant->Type(), {ACHERRY_BOMB, AJALAPENO, ADOOM_SHROOM}) && Plant->ExplodeCountdown() == 1)
        return true;
    if (Plant->Type() == ABLOVER && Plant->State() == 2 && Plant->BloverCountdown() == 1)
        return true;
    if (Plant->Type() == ASQUASH && Plant->State() == 4 && Plant->StateCountdown() == 1)
        return true;
    if (Plant->Type() == AGRAVE_BUSTER && Plant->State() == 9 && Plant->StateCountdown() == 1)
        return true;
    return false;
}

// 智能铲除主逻辑，这段逻辑最后更改的时间为20250124
// 1. 考虑了栈位垫时部分植物自身消失
// 2. 考虑了栈位垫时红白使右侧植物消失
// 3. 修复了复合铲除的栈位垫
// 4. 修复了多种僵尸铲套时植物指针不存在的崩溃，加入continue防崩溃同时优化代码
// 5. 修复了冰车篮球判定存在1cs误差的问题

inline void SmartRemoveController::UpdatePlantMap() {
    for (auto& Row : plantMap)
        for (auto& Grid : Row)
            Grid.fill(nullptr);
    for (auto& Plant : aAlivePlantFilter) {
        switch (Plant.Type()) {
        case ALILY_PAD:
        case AFLOWER_POT:
            plantMap[Plant.Row()][Plant.Col()][0] = &Plant;
            break;
        case APUMPKIN:
            plantMap[Plant.Row()][Plant.Col()][1] = &Plant;
            break;
        case ACOFFEE_BEAN:
            plantMap[Plant.Row()][Plant.Col()][2] = &Plant;
            break;
        case ASQUASH:
            if (ARangeIn(Plant.State(), {5, 6})) // 5-上升 6-下落
                plantMap[Plant.Row()][Plant.Col()][4] = &Plant;
            else
                plantMap[Plant.Row()][Plant.Col()][3] = &Plant;
            break;
        default:
            plantMap[Plant.Row()][Plant.Col()][3] = &Plant;
            break;
        }
    }
}

// 5. 修复了冰车篮球判定存在1cs误差的问题
inline void SmartRemoveController::Tick() {
    if (!enabled)
        return;
    UpdatePlantMap(); // 遍历一次全场植物，得到一份按格子存储的植物map
    auto Gargantuar = GetHammeringGargInfo(true);
    auto GargantuarSparkle = GetHammeringGargInfo(false);
    auto& Zomboni = GetZomboniInfo();
    auto& Catapult = GetCatapultInfo();
    for (auto& Plant : aAlivePlantFilter) {
        int row = Plant.Row() + 1;
        int col = Plant.Col() + 1;
        int gargantuar_amount = 0;               // 统计下一帧有多少巨人能砸到右格植物，仅在数量为1时考虑栈位垫
        bool remove_cancel = false;              // 初次筛选，若至少有一个巨人能在下一帧砸全本格全部植物，逆开关为真
        bool remove = false;                     // 初次筛选后，若下一帧至少有一个巨人可以被合法骗锤，正开关为真
        bool sparkle_cancel = false;             // 初次筛选，若脚本预估至少有一个巨人能砸全本格全部植物，逆开关为真
        bool sparkle = false;                    // 初次筛选后，若脚本预估至少有一个巨人可以被合法骗锤，正开关为真
        bool zomboni_remove = false;             // 冰车铲除
        int index = Plant.MRef<uint16_t>(0x148); // 本格植物栈位
        int right_index = -1;
        int right_plant_map_type = -1;
        if (col <= 8 && !(Grid(row, col + 1)[3] && Grid(row, col + 1)[3]->Type() == ADOOM_SHROOM && Grid(row, col + 1)[3]->ExplodeCountdown() == 1)) {
            // 特判右格不是下一帧生效的毁灭菇
            if (Grid(row, col + 1)[1]) {
                right_index = Grid(row, col + 1)[1]->MRef<uint16_t>(0x148);
                right_plant_map_type = 1;
            } else if (Grid(row, col + 1)[3] && !isPlantDisappearedImmediately(Grid(row, col + 1)[3])) { // 植物自然消失
                right_index = Grid(row, col + 1)[3]->MRef<uint16_t>(0x148);
                right_plant_map_type = 3;
            } else if (Grid(row, col + 1)[0]) {
                right_index = Grid(row, col + 1)[0]->MRef<uint16_t>(0x148);
                right_plant_map_type = 0;
            }
        } // 按照南瓜→常规→花盆的顺序依次判断得到右侧格可被巨人索敌的植物
        if (right_index != -1) {
            for (GargInfo each : Gargantuar) { // 判断右格植物下一帧会不会被巨人砸
                if (each.row == row && isRangeOverlap(APlantType(Grid(row, col + 1)[right_plant_map_type]->Type()), AGARGANTUAR, Grid(row, col + 1)[right_plant_map_type]->Abscissa(), int(each.x))) {
                    ++gargantuar_amount;
                    if (gargantuar_amount == 2) {
                        right_index = -1; // 当有至少两个巨人下一帧砸的到右格植物，判定为右格植物消失
                        break;
                    }
                }
            }
        }
        if (Plant.MRef<int>(0xB8) == 59) {
            Plant.MRef<int>(0xB8) = 0;
            UpdateReanimColor(index);
        } // 每帧先重置所有在上一帧高亮的植物的闪光
        if (Plant.Type() == APUMPKIN) {
            if (Grid(row, col)[0] && Grid(row, col)[3]) { // 本格南瓜且同格不仅有花盆还有常规
                for (GargInfo each : Gargantuar) {
                    if (each.row == row && isRangeOverlap(APUMPKIN, AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格南瓜在下一帧会被锤击
                        if (isRangeOverlap(AFLOWER_POT, AGARGANTUAR, Grid(row, col)[0]->Abscissa(), int(each.x)) && isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), AGARGANTUAR, Grid(row, col)[3]->Abscissa(), int(each.x))) {
                            remove_cancel = true;
                            break; // 本格的花盆和常规都无法在下一帧活下来，以上为判断是否为栈位垫之前的“初次筛选”
                        } else
                            remove = true;
                    }
                }
                if (!remove_cancel && remove) {
                    if (right_index == -1) { // 如果右侧不存在植物，必定GG，铲南瓜
                        AAsm::RemovePlant(&Plant);
                        continue;
                    } else if (index < right_index) { // 若右侧存在植物，则仅当其为高栈时才铲南瓜
                        AAsm::RemovePlant(&Plant);
                        continue;
                    }
                }
                for (GargInfo each : GargantuarSparkle) {
                    if (each.row == row && isRangeOverlap(APUMPKIN, AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格南瓜正在被举锤
                        if (isRangeOverlap(AFLOWER_POT, AGARGANTUAR, Grid(row, col)[0]->Abscissa(), int(each.x)) && isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), AGARGANTUAR, Grid(row, col)[3]->Abscissa(), int(each.x))) {
                            sparkle_cancel = true;
                            break; // 脚本预估本格的花盆和常规都无法在落锤后活下来，以上为判断是否为栈位垫之前的“初次筛选”
                        } else
                            sparkle = true;
                    }
                }
                if (!sparkle_cancel && sparkle) {
                    if (right_index == -1) { // 若右侧不存在植物，必定无法栈位垫，脚本预估南瓜是危险的
                        Plant.MRef<int>(0xB8) = 60;
                        UpdateReanimColor(index);
                    } else if (index < right_index) { // 若右侧存在植物，则仅当其为高栈时，脚本才预估南瓜是危险的
                        Plant.MRef<int>(0xB8) = 60;
                        UpdateReanimColor(index);
                    }
                } // 将僵尸啃食植物的闪光倒计时设为游戏不会自然产生的60。光效此时近似铲子预瞄植物，且覆盖僵尸啃食植物的闪光
                for (ZomboniInfo each : Zomboni) {
                    if (!remove_cancel && each.row == row && isRangeOverlap(APUMPKIN, AZOMBONI, Plant.Abscissa(), int(each.x)) && (!isRangeOverlap(AFLOWER_POT, AZOMBONI, Grid(row, col)[0]->Abscissa(), int(each.x)) || !isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), AZOMBONI, Grid(row, col)[3]->Abscissa(), int(each.x)))) {
                        zomboni_remove = true;
                        break; // 本格南瓜在下一帧会被碾压，且本格的花盆和常规至少有一个可以在下一帧活下来
                    }
                }
                if (zomboni_remove) {
                    AAsm::RemovePlant(&Plant);
                    continue;
                }
                for (CatapultInfo each : Catapult) {
                    if (!remove_cancel && each.row == row && isRangeOverlap(APUMPKIN, AZOMBONI, Plant.Abscissa(), int(each.x)) && (!isRangeOverlap(AFLOWER_POT, ACATAPULT_ZOMBIE, Grid(row, col)[0]->Abscissa(), int(each.x)) || !isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), ACATAPULT_ZOMBIE, Grid(row, col)[3]->Abscissa(), int(each.x)))) {
                        AAsm::RemovePlant(&Plant);
                        break; // 本格南瓜在下一帧会被碾压，且本格的花盆和常规至少有一个可以在下一帧活下来
                    }
                }
            } else if (Grid(row, col)[0] || Grid(row, col)[3]) {
                if (Grid(row, col)[0]) { // 本格南瓜且同格只有一个花盆
                    for (GargInfo each : Gargantuar) {
                        if (each.row == row && isRangeOverlap(APUMPKIN, AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格南瓜在下一帧会被锤击
                            if (isRangeOverlap(AFLOWER_POT, AGARGANTUAR, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                                remove_cancel = true;
                                break; // 本格的花盆无法在下一帧活下来，以上为判断是否为栈位垫之前的“初次筛选”
                            } else
                                remove = true;
                        }
                    }
                    if (!remove_cancel && remove) {
                        if (right_index == -1) { // 如果右侧不存在植物，必定GG，铲南瓜
                            AAsm::RemovePlant(&Plant);
                            continue;
                        } else if (index < right_index) { // 若右侧存在植物，则仅当其为高栈时才铲南瓜
                            AAsm::RemovePlant(&Plant);
                            continue;
                        }
                    }
                    for (GargInfo each : GargantuarSparkle) {
                        if (each.row == row && isRangeOverlap(APUMPKIN, AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格南瓜正在被举锤
                            if (isRangeOverlap(AFLOWER_POT, AGARGANTUAR, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                                sparkle_cancel = true;
                                break; // 脚本预估本格的花盆无法在落锤后活下来，以上为判断是否为栈位垫之前的“初次筛选”
                            } else
                                sparkle = true;
                        }
                    }
                    if (!sparkle_cancel && sparkle) {
                        if (right_index == -1) { // 若右侧不存在植物，必定无法栈位垫，脚本预估南瓜是危险的
                            Plant.MRef<int>(0xB8) = 60;
                            UpdateReanimColor(index);
                        } else if (index < right_index) { // 若右侧存在植物，则仅当其为高栈时，脚本才预估南瓜是危险的
                            Plant.MRef<int>(0xB8) = 60;
                            UpdateReanimColor(index);
                        }
                    } // 将僵尸啃食植物的闪光倒计时设为游戏不会自然产生的60。光效此时近似铲子预瞄植物，且覆盖僵尸啃食植物的闪光
                    for (ZomboniInfo each : Zomboni) {
                        if (!remove_cancel && each.row == row && isRangeOverlap(APUMPKIN, AZOMBONI, Plant.Abscissa(), int(each.x)) && !isRangeOverlap(AFLOWER_POT, AZOMBONI, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                            zomboni_remove = true;
                            break; // 本格南瓜在下一帧会被碾压，且本格的花盆可以在下一帧活下来
                        }
                    }
                    if (zomboni_remove) {
                        AAsm::RemovePlant(&Plant);
                        continue;
                    }
                    for (CatapultInfo each : Catapult) {
                        if (!remove_cancel && each.row == row && isRangeOverlap(APUMPKIN, ACATAPULT_ZOMBIE, Plant.Abscissa(), int(each.x)) && !isRangeOverlap(AFLOWER_POT, ACATAPULT_ZOMBIE, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                            AAsm::RemovePlant(&Plant);
                            break; // 本格南瓜在下一帧会被碾压，且本格的花盆可以在下一帧活下来
                        }
                    }
                } else { // 本格南瓜且同格只有一个常规
                    for (GargInfo each : Gargantuar) {
                        if (each.row == row && isRangeOverlap(APUMPKIN, AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格南瓜在下一帧会被锤击
                            if (isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), AGARGANTUAR, Grid(row, col)[3]->Abscissa(), int(each.x))) {
                                remove_cancel = true;
                                break; // 本格的常规无法在下一帧活下来，以上为判断是否为栈位垫之前的“初次筛选”
                            } else
                                remove = true;
                        }
                    }
                    if (!remove_cancel && remove) {
                        if (right_index == -1) { // 如果右侧不存在植物，必定GG，铲南瓜
                            AAsm::RemovePlant(&Plant);
                            continue;
                        } else if (index < right_index) { // 若右侧存在植物，则仅当其为高栈时才铲南瓜
                            AAsm::RemovePlant(&Plant);
                            continue;
                        }
                    }
                    for (GargInfo each : GargantuarSparkle) {
                        if (each.row == row && isRangeOverlap(APUMPKIN, AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格南瓜正在被举锤
                            if (isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), AGARGANTUAR, Grid(row, col)[3]->Abscissa(), int(each.x))) {
                                sparkle_cancel = true;
                                break; // 脚本预估本格的常规无法在落锤后活下来，以上为判断是否为栈位垫之前的“初次筛选”
                            } else
                                sparkle = true;
                        }
                    }
                    if (!sparkle_cancel && sparkle) {
                        if (right_index == -1) { // 若右侧不存在植物，必定无法栈位垫，脚本预估南瓜是危险的
                            Plant.MRef<int>(0xB8) = 60;
                            UpdateReanimColor(index);
                        } else if (index < right_index) { // 若右侧存在植物，则仅当其为高栈时，脚本才预估南瓜是危险的
                            Plant.MRef<int>(0xB8) = 60;
                            UpdateReanimColor(index);
                        }
                    } // 将僵尸啃食植物的闪光倒计时设为游戏不会自然产生的60。光效此时近似铲子预瞄植物，且覆盖僵尸啃食植物的闪光
                    for (ZomboniInfo each : Zomboni) {
                        if (!remove_cancel && each.row == row && isRangeOverlap(APUMPKIN, AZOMBONI, Plant.Abscissa(), int(each.x)) && !isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), AZOMBONI, Grid(row, col)[3]->Abscissa(), int(each.x))) {
                            zomboni_remove = true;
                            break; // 本格南瓜在下一帧会被碾压，且本格的常规可以在下一帧活下来
                        }
                    }
                    if (zomboni_remove) {
                        AAsm::RemovePlant(&Plant);
                        continue;
                    }
                    for (CatapultInfo each : Catapult) {
                        if (!remove_cancel && each.row == row && isRangeOverlap(APUMPKIN, ACATAPULT_ZOMBIE, Plant.Abscissa(), int(each.x)) && !isRangeOverlap(APlantType(Grid(row, col)[3]->Type()), ACATAPULT_ZOMBIE, Grid(row, col)[3]->Abscissa(), int(each.x))) {
                            AAsm::RemovePlant(&Plant);
                            break; // 本格南瓜在下一帧会被碾压，且本格的常规可以在下一帧活下来
                        }
                    }
                }
            }
        } else if ((Plant.Type() == ATALL_NUT || (Plant.Type() == APUFF_SHROOM && 1 <= Plant.Abscissa() % 10 && Plant.Abscissa() % 10 <= 4) || (Plant.Type() == ASUN_SHROOM && 1 <= Plant.Abscissa() % 10 && Plant.Abscissa() % 10 <= 4)) && Grid(row, col)[0]) {
            // 本格高坚果/偏右小喷菇/偏右阳光菇且同格有花盆
            int pumpkin_index = -1;
            if (Grid(row, col)[1])
                pumpkin_index = Grid(row, col)[1]->MRef<uint16_t>(0x148);
            // 若本格有南瓜，获取南瓜的栈位
            for (GargInfo each : Gargantuar) {
                if (each.row == row && isRangeOverlap(APlantType(Plant.Type()), AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格偏右植物在下一帧会被锤击
                    if (isRangeOverlap(AFLOWER_POT, AGARGANTUAR, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                        remove_cancel = true;
                        break; // 本格的花盆无法在下一帧活下来，以上为判断是否为栈位垫之前的“初次筛选”
                    } else
                        remove = true;
                }
            }
            if (!remove_cancel && remove) {
                if (right_index == -1) { // 如果右侧不存在植物，必定GG，铲偏右植物
                    AAsm::RemovePlant(&Plant);
                    continue;
                } else {                       // 若右侧存在植物，
                    if (pumpkin_index == -1) { // 本格无南瓜
                        if (index < right_index) {
                            AAsm::RemovePlant(&Plant);
                            continue;
                        } // 右格植物高栈，铲偏右植物
                    } else if (pumpkin_index < right_index && index < right_index) {
                        AAsm::RemovePlant(&Plant);
                        continue;
                    } // 若本格有南瓜且右格植物同时相对于南瓜和偏右植物高栈，铲偏右植物
                }
            }
            for (GargInfo each : GargantuarSparkle) {
                if (each.row == row && isRangeOverlap(APlantType(Plant.Type()), AGARGANTUAR, Plant.Abscissa(), int(each.x))) { // 本格偏右植物正在被举锤
                    if (isRangeOverlap(AFLOWER_POT, AGARGANTUAR, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                        sparkle_cancel = true;
                        break; // 脚本预估本格的花盆无法在落锤后活下来，以上为判断是否为栈位垫之前的“初次筛选”
                    } else
                        sparkle = true;
                }
            }
            if (!sparkle_cancel && sparkle) {
                if (right_index == -1) { // 如果右侧不存在植物，必定GG，脚本预估偏右植物是危险的
                    Plant.MRef<int>(0xB8) = 60;
                    UpdateReanimColor(index);
                } else {                       // 若右侧存在植物，
                    if (pumpkin_index == -1) { // 本格无南瓜
                        if (index < right_index) {
                            Plant.MRef<int>(0xB8) = 60;
                            UpdateReanimColor(index);
                        } // 右格植物高栈，脚本预估偏右植物是危险的
                    } else if (pumpkin_index < right_index && index < right_index) {
                        Plant.MRef<int>(0xB8) = 60;
                        UpdateReanimColor(index);
                    } // 若本格有南瓜且右格植物同时相对于南瓜和偏右植物高栈，脚本预估偏右植物是危险的
                }     // 将僵尸啃食植物的闪光倒计时设为游戏不会自然产生的60。光效此时近似铲子预瞄植物，且覆盖僵尸啃食植物的闪光
            }
            for (ZomboniInfo each : Zomboni) {
                if (!remove_cancel && each.row == row && isRangeOverlap(APlantType(Plant.Type()), AZOMBONI, Plant.Abscissa(), int(each.x)) && !isRangeOverlap(AFLOWER_POT, AZOMBONI, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                    zomboni_remove = true;
                    break; // 本格偏右植物在下一帧会被碾压，且本格的花盆可以在下一帧活下来
                }
            }
            if (zomboni_remove) {
                AAsm::RemovePlant(&Plant);
                continue;
            }
            for (CatapultInfo each : Catapult) {
                if (!remove_cancel && each.row == row && isRangeOverlap(APlantType(Plant.Type()), ACATAPULT_ZOMBIE, Plant.Abscissa(), int(each.x)) && !isRangeOverlap(AFLOWER_POT, ACATAPULT_ZOMBIE, Grid(row, col)[0]->Abscissa(), int(each.x))) {
                    AAsm::RemovePlant(&Plant);
                    break; // 本格偏右植物在下一帧会被碾压，且本格的花盆可以在下一帧活下来
                }
            }
        }
    }
}

inline bool isSeedUsableOrHolding(APlantType Type) {
    if (Type >= 49) {
        return AIsSeedUsable(Type) || AGetMainObject()->MouseAttribution()->MRef<int>(0x2C) == Type - 49;
    } else
        return AIsSeedUsable(Type) || AGetMainObject()->MouseAttribution()->MRef<int>(0x28) == Type;
}

// 获取Type类型植物对小丑爆炸的判定范围
inline std::pair<int, int> GetExplodeRange(APlantType Type) {
    switch (Type) {
    case ATALL_NUT:
        return {10, 90};
    case APUMPKIN:
        return {0, 100};
    case ACOB_CANNON:
        return {0, 140};
    default:
        return {10, 70}; // 普通
    }
}

// 将给定格子转化为坐标
// 支持非整型参数
inline std::pair<int, int> MyGridToCoordinate(double Row, double Col) {
    if (ARangeIn(AGetMainObject()->Scene(), {0, 1, 6, 7, 8, 9}))
        return {40 + (Col - 1) * 80, 80 + (Row - 1) * 100};
    else if (ARangeIn(AGetMainObject()->Scene(), {2, 3, 10, 11}))
        return {40 + (Col - 1) * 80, 80 + (Row - 1) * 85};
    return {40 + (Col - 1) * 80, 70 + (Row - 1) * 85 + (Col < 6 ? (6 - Col) * 20 : 0)}; // 天台
}

inline int MyColToX(double Col) { return 40 + (Col - 1) * 80; }
inline int MyRowToY(double Row, double Col) {
    if (ARangeIn(AGetMainObject()->Scene(), {0, 1, 6, 7, 8, 9}))
        return 80 + (Row - 1) * 100;
    else if (ARangeIn(AGetMainObject()->Scene(), {2, 3, 10, 11}))
        return 80 + (Row - 1) * 85;
    return 70 + (Row - 1) * 85 + (Col < 6 ? (6 - Col) * 20 : 0); // 天台
}

// 判断当前鼠标是否在场内
inline bool isMouseInField() {
    int X = AMRef<int>(0x6A9EC0, 0x768, 0x138, 0x8);
    int Y = AMRef<int>(0x6A9EC0, 0x768, 0x138, 0xC); // 鼠标坐标
    if (X < 0)
        return false;
    return 45 <= Y && Y <= 565; // 此处数据为手工测试得到
}

// 判断某小丑是否可炸到某植物
// 植僵双遍历的情境下使用
inline bool JudgeExplode(APlant* Plant, AZombie* Zombie) {
    int JackX = Zombie->Abscissa() + 60;
    int JackY = Zombie->Ordinate() + 60; // 小丑爆心偏移
    int PlantX = Plant->Abscissa();
    int PlantY = Plant->Ordinate();
    int YDistance = 0;
    if (JackY < PlantY)
        YDistance = PlantY - JackY;
    else if (JackY > PlantY + 80)
        YDistance = JackY - (PlantY + 80);
    if (YDistance > 90)
        return false;
    int XDistance = sqrt(90 * 90 - YDistance * YDistance);
    auto Range = GetExplodeRange(APlantType(Plant->Type()));
    return PlantX + Range.first - XDistance <= JackX && JackX <= PlantX + Range.second + XDistance;
}

// 不遍历植物，预测小丑是否可炸到(Row, Col)格Type类型植物
// 用于SafeCard
inline bool PredictExplode(AZombie* Zombie, int PlantRow, int PlantCol, APlantType PlantType) {
    int JackX = Zombie->Abscissa() + 60;
    int JackY = Zombie->Ordinate() + 60; // 小丑爆心偏移
    auto PlantCoordinate = MyGridToCoordinate(PlantRow, PlantCol);
    int PlantX = PlantCoordinate.first, PlantY = PlantCoordinate.second;
    int YDistance = 0;
    if (JackY < PlantY)
        YDistance = PlantY - JackY;
    else if (JackY > PlantY + 80)
        YDistance = JackY - (PlantY + 80);
    if (YDistance > 90)
        return false;
    int XDistance = sqrt(90 * 90 - YDistance * YDistance);
    auto Range = GetExplodeRange(PlantType);
    return PlantX + Range.first - XDistance <= JackX && JackX <= PlantX + Range.second + XDistance;
}

// 拖延至不会被小丑炸的情况下再用卡，需存活时间NeedTime应填写≥1的数，默认卡片需存活至99cs后
// 若检测到在天台车底自动补充花盆式放置植物，则在小丑倒计时不为1的时候直接种植，以此兼容车底炸
inline void SafeCard(APlantType PlantType, int Row, int Col, int NeedTime = 99) {
    if (!isSeedUsableOrHolding(PlantType))
        return; // 卡片需要带了且CD是好的
    bool RoofUnderCarsExplode = false;
    if (AAsm::GetPlantRejectType(ACHERRY_BOMB, Row - 1, Col - 1) == AAsm::NEEDS_POT) { // 如果是天台自动补充花盆的情境，先查冰车/投篮
        auto Coordinate = MyGridToCoordinate(Row, Col);
        int Abscissa = Coordinate.first;
        for (auto& Zombie : aAliveZombieFilter) {
            if (ARangeIn(Zombie.Type(), {AZOMBONI, ACATAPULT_ZOMBIE}) && Zombie.Row() + 1 == Row && isRangeOverlap(ACHERRY_BOMB, AZOMBONI, Abscissa, int(Zombie.Abscissa()))) {
                RoofUnderCarsExplode = true;
                break;
            }
        }
    }
    for (auto& Zombie : aAliveZombieFilter) { // 再查小丑
        if (Zombie.Type() == AJACK_IN_THE_BOX_ZOMBIE && Zombie.State() == 16 && PredictExplode(&Zombie, Row, Col, PlantType)) {
            if (RoofUnderCarsExplode) {
                if (Zombie.StateCountdown() == 1) {
                    AConnect(ANowDelayTime(1), [=] { SafeCard(PlantType, Row, Col, NeedTime); });
                    return; // 天台车底炸且小丑倒计时为1，则延迟到下一帧重新判断
                }
            } else if (Zombie.StateCountdown() <= NeedTime) {
                AConnect(ANowDelayTime(1), [=] { SafeCard(PlantType, Row, Col, NeedTime); });
                return; // 不是天台车底炸且小丑倒计时≤NeedTime，则延迟到下一帧重新判断
            }
        }
    }
    // 查完小丑后发现一切正常，则使用dsl的Card函数自动补充容器
    At(now) Card(PlantType, Row, Col);
}

// 智能用卡
// 捏着卡片时按Shift将以SafeCard的形式放出该植物，可在水路和屋顶调用，会自动补充容器
// 默认樱桃、辣椒、夜间黑核、夜间蓝冰需存活至99cs后，其他植物需存活至1cs后
// 可同时作为天台车底炸快捷键，满足天台车底炸且当帧小丑倒计时不为1时会认为灰烬是安全的
inline void SmartAsh() {
    int MousePlant = AGetMainObject()->MouseAttribution()->MRef<int>(0x28);
    int MouseMPlant = AGetMainObject()->MouseAttribution()->MRef<int>(0x2C);
    if (!isMouseInField())
        return; // 鼠标需在场内，避免鼠标行/鼠标列在场外调用时溢出
    int Row = int(AMouseRow() + 0.5);
    int Col = int(AMouseCol() + 0.5); // 鼠标所在格
    if (Col > 9)                      // 兼容宽屏拓展
        Col = 9;
    if (MousePlant == AIMITATOR) {
        if (!isSeedUsableOrHolding(ALILY_PAD) && AAsm::GetPlantRejectType(MouseMPlant, Row - 1, Col - 1) == AAsm::NOT_ON_WATER)
            return;
        if (!isSeedUsableOrHolding(AFLOWER_POT) && AAsm::GetPlantRejectType(MouseMPlant, Row - 1, Col - 1) == AAsm::NEEDS_POT)
            return;
        SafeCard(APlantType(MouseMPlant + 49), Row, Col, 1);
    } else {
        if (!isSeedUsableOrHolding(ALILY_PAD) && AAsm::GetPlantRejectType(MousePlant, Row - 1, Col - 1) == AAsm::NOT_ON_WATER)
            return;
        if (!isSeedUsableOrHolding(AFLOWER_POT) && AAsm::GetPlantRejectType(MousePlant, Row - 1, Col - 1) == AAsm::NEEDS_POT)
            return;
        if (ARangeIn(MousePlant, {ACHERRY_BOMB, AJALAPENO}) || (aFieldInfo.isNight && ARangeIn(MousePlant, {ADOOM_SHROOM, AICE_SHROOM})))
            SafeCard(APlantType(MousePlant), Row, Col, 99);
        else
            SafeCard(APlantType(MousePlant), Row, Col, 1);
    }
    AAsm::ReleaseMouse();
}

// 小丑暂停


class WarningController {
public:
    int JackWarning = -1;
    bool BalloonWarning = false;
    int BalloonPauseCd = 200;

    void ToggleJack() {
        JackWarning = JackWarning == 1 ? -1 : ++JackWarning;
        CreateCaption(!JackWarning ? "JackWarning: OnlyWhenHit"
                : JackWarning == 1 ? "JackWarning: All"
                                   : "JackWarning: Off");
    }
    void ToggleBalloon() {
        BalloonWarning = !BalloonWarning;
        CreateCaption(BalloonWarning ? "BalloonWarning: On" : "BalloonWarning: Off");
    }
    void JackPause();
    void BalloonCaption();
    void BalloonPause();
    void Reset() { JackWarning = -1; BalloonWarning = false; BalloonPauseCd = 200; }
};

inline float BalloonΔX(int Time, float Speed, int SlowCountdown = 0) {
    if (!SlowCountdown)
        return Speed * Time; // 原速 × 总时间
    if (SlowCountdown > Time)
        return 0.4 * Speed * Time; // 减速 × 总时间
    return 0.4 * Speed * (SlowCountdown - 1) + Speed * (Time - (SlowCountdown - 1));
    // 减速 × (减速倒计时 - 1) + 原速 × (总时间 - (减速倒计时 - 1))
}

inline void WarningController::JackPause() {
    if (JackWarning == -1)
        return;
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AJACK_IN_THE_BOX_ZOMBIE && Zombie.State() == 16 && Zombie.StateCountdown() == 110 && (isSeedUsableOrHolding(ACHERRY_BOMB) || isSeedUsableOrHolding(AJALAPENO) || (isSeedUsableOrHolding(ADOOM_SHROOM) && aFieldInfo.isNight))) {
            if (JackWarning) {
                Paused = true;
                ASetAdvancedPause(Paused, false, 0);
                PausedCd = 0;
                return; // 小丑开盒且此时樱桃/辣椒/夜间黑核可用就高级暂停一次
            }
            for (auto& Plant : aAlivePlantFilter) {
                if ((Plant.Type() == ASQUASH && ARangeIn(Plant.State(), {5, 6})) || ARangeIn(Plant.Type(), {ABLOVER, ACHERRY_BOMB, AJALAPENO, ACOFFEE_BEAN}) || (aFieldInfo.isNight && ARangeIn(Plant.Type(), {ADOOM_SHROOM, AICE_SHROOM})))
                    continue; // 不考虑飞行窝瓜、三叶、樱桃、辣椒、咖啡、夜间黑核、夜间蓝冰
                if (JudgeExplode(&Plant, &Zombie)) {
                    Paused = true;
                    ASetAdvancedPause(Paused, false, 0);
                    PausedCd = 0;
                    return; // 如果有小丑开盒瞬间能炸到植物且此时樱桃/辣椒/夜间黑核可用就高级暂停一次
                }
            }
        }
    }
}

inline void WarningController::BalloonCaption() {
    if (PausedCd < 480)
        PausedCd += AGetPvzBase()->TickMs();
    if (!BalloonWarning)
        return;
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == ABALLOON_ZOMBIE && isSeedUsableOrHolding(ABLOVER)) {
            if (int(Zombie.Abscissa()) <= -100) {
                return;
            } else if (int(Zombie.Abscissa() - BalloonΔX(49, Zombie.Speed(), Zombie.SlowCountdown())) <= -100) {
                CreateCaption("Bite Blover", {BOTTOMFAST, 2});
                return; // 若预计气球<50cs就进家，打印字幕啃吹
            } else if (int(Zombie.Abscissa() - BalloonΔX(50, Zombie.Speed(), Zombie.SlowCountdown())) <= -100) {
                CreateCaption("Balloon Warning", {BOTTOMFAST, 2});
                return; // 若预计气球=50cs后进家且三叶草可用，打印字幕警告
            }
        }
    }
}

inline void WarningController::BalloonPause() {
    if (!BalloonWarning)
        return;
    if (BalloonPauseCd < 200) // 两次气球警告至少间隔200cs
        ++BalloonPauseCd;
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == ABALLOON_ZOMBIE && int(Zombie.Abscissa()) <= -100)
            return;
        if (BalloonPauseCd == 200 && Zombie.Type() == ABALLOON_ZOMBIE && int(Zombie.Abscissa() - BalloonΔX(50, Zombie.Speed(), Zombie.SlowCountdown())) <= -100 && isSeedUsableOrHolding(ABLOVER)) {
            Paused = true;
            ASetAdvancedPause(Paused, false, 0);
            PausedCd = 0;
            BalloonPauseCd = 0;
            return;
        } // 如果预计气球在50cs后进家且三叶草可用就高级暂停一次，用于判断极限吹气球时机
    }
}

inline int RealCountdown() {
    if (AGetMainObject()->Wave() == AGetMainObject()->TotalWave())
        return AGetMainObject()->LevelEndCountdown();
    if (AGetMainObject()->RefreshCountdown() > 200)
        return 0;
    if (ARangeIn(AGetMainObject()->Wave(), {9, 19, 29, 39})) {
        if (AGetMainObject()->RefreshCountdown() <= 5)
            return AGetMainObject()->HugeWaveCountdown();
        return AGetMainObject()->RefreshCountdown() + 745;
    }
    return AGetMainObject()->RefreshCountdown();
}

struct ClockState {
    std::vector<int> waveClock = std::vector<int>(40, 0);
    ATime now;

    void Update();
    void Reset() { waveClock.assign(40, 0); now = ATime(); }
    int Clock(const ATime& time) const {
        if (time.wave <= 1)
            return time.time;
        int idx = time.wave - 1;
        return 0 <= idx && idx < int(waveClock.size()) && waveClock[idx] ? waveClock[idx] + time.time : INT_MIN;
    }
};

inline void ClockState::Update() {
    if (AGetMainObject() == nullptr)
        return;
    if (RealCountdown())
        waveClock[AGetMainObject()->Wave()] = AGetMainObject()->GameClock() + RealCountdown();

    now.wave = AGetMainObject()->Wave() ?: 1;
    if (AGetMainObject()->Wave() == 0)
        now.time = -AGetMainObject()->RefreshCountdown();
    else if (waveClock[AGetMainObject()->Wave() - 1] == 0)
        now.time = ANowTime(ANowWave());
    else
        now.time = AGetMainObject()->GameClock() - waveClock[AGetMainObject()->Wave() - 1];
}

class FightInfoDrawer {
public:
    MyPainter barPainter;
    MyPainter fightInfoPainter;
    MyPainter SegPainter;
    MyPainter backgroundPainter;
    MyPainter lowIndexPainter;
    MyPainter nextIndexPainter;
    MyPainter GigaNumPainter;
    int ShowInfoState = -1;
    int ShowIndexState = -1;
    std::vector<int> LeftmostVisibleArea = std::vector<int>(6, 10);

    void InitPainterStyle() {
        fightInfoPainter.SetFontSize(17);
        lowIndexPainter.SetFont("Arial");
        lowIndexPainter.SetFontSize(12);
        nextIndexPainter.SetFont("Arial");
        nextIndexPainter.SetFontSize(30);
        GigaNumPainter.SetFont("");
        GigaNumPainter.SetFontSize(17);
    }
    void ToggleInfo() {
        ShowInfoState = ShowInfoState == 1 ? -1 : ++ShowInfoState;
        CreateCaption(!ShowInfoState ? "ShowInfo: Basic"
                : ShowInfoState == 1 ? "ShowInfo: Advanced"
                                     : "ShowInfo: Off");
    }
    void ToggleIndex() {
        ShowIndexState = ShowIndexState == 1 ? -1 : ++ShowIndexState;
        ResetIndexArea();
    }
    void SetInfoState(int state) { ShowInfoState = state; }
    void SetIndexState(int state) { ShowIndexState = state; ResetIndexArea(); }
    void ResetIndexArea() { LeftmostVisibleArea.assign(6, 10); }
    void Reset() { ShowInfoState = -1; ShowIndexState = -1; ResetIndexArea(); ProduceCDMax.clear(); }
    void DrawInfo(const Settings& settings, const ClockState& clock);
    void DrawIndex(const Settings& settings);

private:
    struct TimerData { int max = 2500, last = INT_MAX; };
    std::map<uint32_t, TimerData> ProduceCDMax;
};

inline void FightInfoDrawer::DrawInfo(const Settings& settings, const ClockState& clock) {
    if (AGetMainObject() == nullptr)
        return; // 防崩溃代码
    if (settings.ShowReplayInfo && aReplay.GetState() == AReplay::RECORDING) {
        barPainter.Draw(ABar(685, 3, 1, 0, {}, 1, ABar::RIGHT, 106, 24, 0xFFFFC000, 0xC0FFFFFF));
        fightInfoPainter.Draw(AText("Rec.", 689, 4), 0xFFFF0000);
        fightInfoPainter.Draw(AText(std::format("{}", aReplay.GetRecordIdx()), 733, 4), 0xFFFF0000);
    }
    if (settings.ShowReplayInfo && aReplay.GetState() == AReplay::PLAYING) {
        barPainter.Draw(ABar(685, 3, 1, 0, {}, 1, ABar::RIGHT, 106, 24, 0xFFFFC000, 0xC0FFFFFF));
        fightInfoPainter.Draw(AText("Play", 689, 4), 0xFF0000FF);
        fightInfoPainter.Draw(AText(std::format("{}", aReplay.GetPlayIdx()), 733, 4), 0xFF0000FF);
    }
    if (ShowInfoState == -1)
        return;
    for (auto& Plant : aAlivePlantFilter) { // 显血
        if (ARangeIn(Plant.Type(), {ASUNFLOWER, ASUN_SHROOM, AMARIGOLD, ATWIN_SUNFLOWER}) && !Plant.IsSleeping() && settings.ProduceCD) {
            auto& data = ProduceCDMax[Plant.Id()];
            int cur = Plant.MRef<int>(0x58);
            if (cur > data.last)
                data.max = cur;
            data.last = cur;
            barPainter.Draw(ABar(Plant.Xi() + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 17, 2500, data.max - cur, {2350}, 1, ABar::RIGHT, 72, 7, settings.ProduceCDARGB, 0xA0FFFFFF));
        }
        if (Plant.Type() == ACOB_CANNON && AGetCobRecoverTime(Plant.Index()) && settings.CobCD)
            barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 17, 3475, 3475 - AGetCobRecoverTime(Plant.Index()), {350, 3350}, 1, ABar::RIGHT, 152, 7, settings.CobCDARGB, 0xA0FFFFFF));
        if (Plant.Hp() != Plant.HpMax()) {
            if (Plant.Type() == ACOB_CANNON) {
                if (settings.CobGloomHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 152, 11, settings.CobGloomHPARGB, 0xA0FFFFFF));
            } else if (Plant.Type() == AGLOOM_SHROOM) {
                if (settings.CobGloomHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, settings.CobGloomHPARGB, 0xA0FFFFFF));
            } else if (Plant.Type() == ACOFFEE_BEAN) {
                if (settings.OtherPlantHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 13, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, settings.OtherPlantHPARGB, 0xA0FFFFFF));
            } else if (ARangeIn(Plant.Type(), {AWALL_NUT, ATALL_NUT, ASPIKEROCK})) {
                if (settings.NutSpikeHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {Plant.HpMax() / 3, Plant.HpMax() * 2 / 3}, 1, ABar::RIGHT, 72, 11, settings.NutSpikeHPARGB, 0xA0FFFFFF));
            } else if (Plant.Type() == APUMPKIN) {
                if (settings.PumpkinHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 45, Plant.HpMax(), Plant.Hp(), {Plant.HpMax() / 3, Plant.HpMax() * 2 / 3}, 1, ABar::RIGHT, 72, 11, settings.PumpkinHPARGB, 0xA0FFFFFF));
            } else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT})) {
                if (settings.LilyPotHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 57, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, settings.LilyPotHPARGB, 0xA0FFFFFF));
            } else if (Plant.Type() == ASQUASH) {
                if (settings.OtherPlantHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, settings.OtherPlantHPARGB, 0xA0FFFFFF));
            } else {
                if (settings.OtherPlantHP)
                    barPainter.Draw(ABar(MyColToX(Plant.Col() + 1) + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, settings.OtherPlantHPARGB, 0xA0FFFFFF));
            }
        }
    }
    for (auto& Place : aAlivePlaceItemFilter) { // 核坑
        if (Place.Type() != 2 || !settings.Crater)
            continue;
        auto Coordinate = MyGridToCoordinate(Place.Row() + 1, Place.Col() + 1);
        int Abscissa = Coordinate.first, Ordinate = Coordinate.second;
        barPainter.Draw(ABar(Abscissa + 4, Ordinate + 62, 18000, Place.Value(), {}, 1, ABar::RIGHT, 72, 6, settings.CraterARGB, 0xA0FFFFFF, 0xFF000000, 0));
    }
    for (int Row = 1; Row <= 6; ++Row) { // 冰道
        if (LeftmostIceTrail(Row) > 9 || !settings.Icetrail)
            continue;
        auto Coordinate = MyGridToCoordinate(Row, LeftmostIceTrail(Row));
        int Abscissa = Coordinate.first, Ordinate = Coordinate.second;
        barPainter.Draw(ABar(Abscissa + 4, Ordinate + 50, 3000, IceTrailCoverCD(Row), {}, 1, ABar::RIGHT, 72, 6, settings.IcetrailARGB, 0xA0FFFFFF, 0xFF000000, 0));
    }
    std::vector<int> FootballThisWave(6, 0);
    std::vector<int> ZomboniThisWave(6, 0);
    std::vector<int> FootballCount(6, 0);
    std::vector<int> ZomboniCount(6, 0);
    // 确保血条覆盖顺序，故多次遍历
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AFOOTBALL_ZOMBIE && Zombie.Hp() >= 90) { // 橄榄血条
            if (settings.FootballHP)
                barPainter.Draw(ABar(Zombie.Abscissa() + 81, Zombie.Ordinate() + 69, 1580, Zombie.OneHp() + Zombie.Hp() - 90, {180}, 1, ABar::UP, 36, 6, settings.FootballHPARGB, 0xA0FFFFFF));
            ++FootballCount[Zombie.Row()]; // 橄榄实时统计
            if (Zombie.AtWave() == AGetMainObject()->Wave() - 1)
                ++FootballThisWave[Zombie.Row()];
        }
        if (Zombie.Type() == AZOMBONI) {
            ++ZomboniCount[Zombie.Row()]; // 冰车实时统计
            if (Zombie.AtWave() == AGetMainObject()->Wave() - 1)
                ++ZomboniThisWave[Zombie.Row()];
        }
    }
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AGARGANTUAR) // 白眼血条
            if (settings.GargHP)
                barPainter.Draw(ABar(Zombie.Abscissa() + 49, Zombie.Ordinate() + 59, 3000, Zombie.Hp(), settings.HPStyle ? std::initializer_list<int> {1200} : std::initializer_list<int> {1500, 1800}, 1, ABar::UP, 40, 8, settings.GargHPARGB, 0xA0FFFFFF));
    }
    std::vector<int> GigaThisWave(6, 0);
    std::vector<int> GigaCount(6, 0);
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AGIGA_GARGANTUAR) { // 红眼血条
            if (settings.GigaHP)
                barPainter.Draw(ABar(Zombie.Abscissa() + 49, Zombie.Ordinate() + 79, 6000, Zombie.Hp(), settings.HPStyle ? std::initializer_list<int> {600, 2400, 4200} : std::initializer_list<int> {1800, 3000, 4800}, 1, ABar::UP, 80, 8, settings.GigaHPARGB, 0xA0FFFFFF));
            ++GigaCount[Zombie.Row()]; // 红眼实时统计
            if (Zombie.AtWave() == AGetMainObject()->Wave() - 1)
                ++GigaThisWave[Zombie.Row()];
        }
    }
    for (auto& Plant : aAlivePlantFilter) {
        // 受炸提示，打印一个与植物血条重合的半透明红色矩形，包括飞行窝瓜和咖啡豆
        for (auto& Zombie : aAliveZombieFilter) {
            if (Zombie.Type() == AJACK_IN_THE_BOX_ZOMBIE && Zombie.State() == 16 && JudgeExplode(&Plant, &Zombie) && settings.JackExplosionRange) {
                if (Plant.Type() == ACOB_CANNON)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 25, 152, 11), settings.JackExplosionRangeARGB);
                else if (Plant.Type() == ACOFFEE_BEAN)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 13, 72, 11), settings.JackExplosionRangeARGB);
                else if (Plant.Type() == APUMPKIN)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 45, 72, 11), settings.JackExplosionRangeARGB);
                else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT}))
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 57, 72, 11), settings.JackExplosionRangeARGB);
                else if (Plant.Type() == ASQUASH)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 25, 72, 11), settings.JackExplosionRangeARGB);
                else
                    backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, 72, 11), settings.JackExplosionRangeARGB);
                break;
            }
        }
        // 小喷菇阳光菇海蘑菇偏移
        if (!settings.PlantOffset)
            continue;
        int RectHeight = 11;
        if (Plant.Hp() < Plant.HpMax() || !settings.OtherPlantHP)
            RectHeight = 0;
        int Plantoffset = Plant.Xi() - MyColToX(Plant.Col() + 1);
        if (Plantoffset > 0) {
            backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 53 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, 14, RectHeight), settings.OtherPlantHPARGB);
            lowIndexPainter.Draw(AText(std::format("R{}", Plantoffset), MyColToX(Plant.Col() + 1) + 52 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 21), 0xFF000000);
        }
        if (Plantoffset < 0) {
            backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 5 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, 14, RectHeight), settings.OtherPlantHPARGB);
            lowIndexPainter.Draw(AText(std::format("L{}", -Plantoffset), MyColToX(Plant.Col() + 1) + 4 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 21), 0xFF000000);
        }
    }
    // 有小丑开盒，绘制爆炸倒计时
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AJACK_IN_THE_BOX_ZOMBIE && Zombie.State() == 16 && settings.JackCountdown)
            barPainter.Draw(ABar(Zombie.Abscissa() + 65, Zombie.Ordinate() + 87, 110, Zombie.StateCountdown(), {100}, 1, ABar::UP, 55, 10, settings.JackCountdownARGB, 0xA0FFFFFF));
    }
    for (int Row : {0, 1, 2, 3, 4, 5}) {
        if (AMRef<int>(0x6A9EC0, 0x768, 0x5D8 + Row * 0x4) != 1)
            continue;
        int Height = 16;
        auto Coordinate = MyGridToCoordinate(Row + 1, 0.5);
        int Ordinate = Coordinate.second;
        if (GigaCount[Row] && settings.GigaCount) { // 红眼实时统计绘制
            std::string Giga = std::format("{}/{}", GigaThisWave[Row], GigaCount[Row]);
            backgroundPainter.Draw(ARect(0, Ordinate + aFieldInfo.rowHeight / 2 + 3 - Height, Giga.size() * 9 + 2, 16), settings.GigaCountARGB);
            fightInfoPainter.Draw(AText(Giga, 0, Ordinate + aFieldInfo.rowHeight / 2 - Height), 0xFFFFFFFF);
        }
        if (ZomboniCount[Row] && settings.ZomboniCount) { // 冰车实时统计绘制
            std::string Zomboni = std::format("{}/{}", ZomboniThisWave[Row], ZomboniCount[Row]);
            backgroundPainter.Draw(ARect(0, Ordinate + aFieldInfo.rowHeight / 2 + 3, Zomboni.size() * 9 + 2, 16), settings.ZomboniCountARGB);
            fightInfoPainter.Draw(AText(Zomboni, 0, Ordinate + aFieldInfo.rowHeight / 2), 0xFFFFFFFF);
        }
        if (FootballCount[Row] && settings.FootballCount) { // 橄榄实时统计绘制
            std::string Football = std::format("{}/{}", FootballThisWave[Row], FootballCount[Row]);
            backgroundPainter.Draw(ARect(0, Ordinate + aFieldInfo.rowHeight / 2 + 3 + Height, Football.size() * 9 + 2, 16), settings.FootballCountARGB);
            fightInfoPainter.Draw(AText(Football, 0, Ordinate + aFieldInfo.rowHeight / 2 + Height), 0xFFFFFFFF);
        }
    }
    std::vector<int> GigaDistribution(20, 0);
    std::vector<int> GigaCumulativeDistribution(20, 0);
    for (int Wave = 0; Wave < 20; ++Wave) { // 红眼出怪表统计
        for (int i = 0; i < 50; ++i)
            if (*(AGetMainObject()->ZombieList() + 50 * Wave + i) == AGIGA_GARGANTUAR)
                ++GigaDistribution[Wave];
        GigaCumulativeDistribution[Wave] = Wave ? GigaCumulativeDistribution[Wave - 1] + GigaDistribution[Wave] : GigaDistribution[Wave];
    }
    if (GigaCumulativeDistribution[20 - 1] && settings.GigaStat) { // 红眼出怪表统计绘制
        backgroundPainter.Draw(ARect(19, 7, 61, 53), GigaCumulativeDistribution[AGetMainObject()->Wave() - 1] < 50 ? settings.GigaStatARGB1 : settings.GigaStatARGB2);
        if (AGetMainObject()->Wave()) {
            GigaNumPainter.Draw(AText(std::format("Wave{:2}", GigaDistribution[ANowWave(false) - 1]), 22, ShowInfoState ? 6 : 11), 0xFFFFFFFF);
            GigaNumPainter.Draw(AText(std::format("Sum{:3}", GigaCumulativeDistribution[ANowWave(false) - 1]), 22, ShowInfoState ? 22 : 33), 0xFFFFFFFF);
        } else {
            GigaNumPainter.Draw(AText("Wave 0", 22, ShowInfoState ? 6 : 11), 0xFFFFFFFF);
            GigaNumPainter.Draw(AText("Sum  0", 22, ShowInfoState ? 22 : 33), 0xFFFFFFFF);
        }
        if (ShowInfoState)
            GigaNumPainter.Draw(AText(std::format("All{:3}", GigaCumulativeDistribution[20 - 1]), 22, 38), 0xFFFFFFFF);
    }

    // 本波总血条
    if (settings.TotalHP) {
        if (AGetMainObject()->Wave() == 0)
            barPainter.Draw(ABar(58, 574, 1, 0, {}, 1, ABar::RIGHT, 127, 24, settings.TotalHPARGB1, 0xC0FFFFFF));
        else if (ARangeIn(AGetMainObject()->Wave(), {9, 19, 29, 39, AGetMainObject()->TotalWave()}) || ShowInfoState)
            barPainter.Draw(ABar(58, 574, AGetMainObject()->MRef<int>(0x5598), AAsm::ZombieTotalHp(ANowWave() - 1), {AGetMainObject()->ZombieRefreshHp()}, 1, ABar::RIGHT, 127, 24, RealCountdown() ? settings.TotalHPARGB2 : settings.TotalHPARGB1, 0xC0FFFFFF));
        else
            barPainter.Draw(ABar(58, 574, AGetMainObject()->MRef<int>(0x5598), AAsm::ZombieTotalHp(ANowWave() - 1), {AGetMainObject()->MRef<int>(0x5598) * 13 / 20, AGetMainObject()->MRef<int>(0x5598) / 2}, 1, ABar::RIGHT, 127, 24, settings.TotalHPARGB1, 0xC0FFFFFF));
        // 波数时间
        fightInfoPainter.Draw(AText(std::format("{:02},", AGetMainObject()->Wave() ?: 1), 59, 575), 0xFF0000FF);
        if (AGetMainObject()->Wave() == 0)
            fightInfoPainter.Draw(AText(std::format("{}", -AGetMainObject()->RefreshCountdown()), 82, 575), 0xFF0000FF);
        else if (clock.waveClock[AGetMainObject()->Wave() - 1] == 0)
            fightInfoPainter.Draw(AText(std::format("{}", ANowTime(ANowWave())), 82, 575), 0xFF0000FF);
        else
            fightInfoPainter.Draw(AText(std::format("{}", AGetMainObject()->GameClock() - clock.waveClock[AGetMainObject()->Wave() - 1]), 82, 575), 0xFF0000FF);
        fightInfoPainter.Draw(AText(RealCountdown() && (ARangeIn(AGetMainObject()->Wave(), {9, 19, 20}) || ShowInfoState) ? std::format("{}", -RealCountdown()) : "", 145, 575), 0xFFFF0000);
    }

    // 波长记录
    for (int i = 0; i < settings.WavelengthRecord; ++i) {
        if (AGetMainObject()->Wave() - i > 0 && clock.waveClock[AGetMainObject()->Wave() - i] > 0 && RealCountdown() && (ARangeIn(AGetMainObject()->Wave(), {9, 19, 29, 39, AGetMainObject()->TotalWave()}) || ShowInfoState)) {
            barPainter.Draw(ABar(191 + 71 * i, 574, 1, 0, {}, 1, ABar::RIGHT, 65, 24, 0xFFFFC000, 0xC0FFFFFF));
            fightInfoPainter.Draw(AText(std::format("{:02},", AGetMainObject()->Wave() - i ?: 1), 193 + 71 * i, 575), 0xFF0000FF);
            fightInfoPainter.Draw(AText(std::format("{}", clock.waveClock[AGetMainObject()->Wave() - i] - clock.waveClock[AGetMainObject()->Wave() - 1 - i]), 216 + 71 * i, 575), 0xFF0000FF);
        } else if (AGetMainObject()->Wave() - 1 - i > 0 && clock.waveClock[AGetMainObject()->Wave() - 1 - i] > 0) {
            barPainter.Draw(ABar(191 + 71 * i, 574, 1, 0, {}, 1, ABar::RIGHT, 65, 24, 0xFFFFC000, 0xC0FFFFFF));
            fightInfoPainter.Draw(AText(std::format("{:02},", AGetMainObject()->Wave() - i - 1 ?: 1), 193 + 71 * i, 575), 0xFF0000FF);
            if (clock.waveClock[AGetMainObject()->Wave() - 2 - i] > 0)
                fightInfoPainter.Draw(AText(std::format("{}", clock.waveClock[AGetMainObject()->Wave() - 1 - i] - clock.waveClock[AGetMainObject()->Wave() - 2 - i]), 216 + 71 * i, 575), 0xFF0000FF);
            else
                fightInfoPainter.Draw(AText(std::format("{}", ANowTime(ANowWave() - 1 - i) - ANowTime(ANowWave() - i)), 216 + 71 * i, 575), 0xFF0000FF);
        }
    }

    // 显示倍速
    if (AGetPvzBase()->TickMs() != 10 && settings.ShowSpeed) {
        barPainter.Draw(ABar(6, 574, 1, 1, {}, 1, ABar::RIGHT, 46, 24, AGetPvzBase()->TickMs() > 10 ? settings.ShowSpeedARGB1 : settings.ShowSpeedARGB2));
        fightInfoPainter.Draw(AText(std::format("{}", 10 / AGetPvzBase()->TickMs()), 8, 575), 0xFF000000);
        if (AGetPvzBase()->TickMs() == 1) {
            fightInfoPainter.Draw(AText(".", 26, 575), 0xFF000000);
            fightInfoPainter.Draw(AText("0", 30, 575), 0xFF000000);
        } else {
            fightInfoPainter.Draw(AText(".", 17, 575), 0xFF000000);
            fightInfoPainter.Draw(AText(1000 / AGetPvzBase()->TickMs() % 100 ? std::format("{}", 1000 / AGetPvzBase()->TickMs() % 100) : "00", 21, 575), 0xFF000000);
        }
        fightInfoPainter.Draw(AText("x", 39, 575), 0xFF000000);
    }

    // 落点预览
    if (settings.CobColPreview && AGetMainObject()->MouseAttribution()->Type() == 8) {
        backgroundPainter.Draw(ARect(AGetMainObject()->MouseAttribution()->MRef<int>(0x8) + 1, AGetMainObject()->MouseAttribution()->MRef<int>(0xC) + 22, 49, 14), 0xC0FFFFFF);
        fightInfoPainter.Draw(AText(std::format("{}.", ((AGetMainObject()->MouseAttribution()->MRef<int>(0x8) + 25) / 80) < 10 ? std::format("{}", (AGetMainObject()->MouseAttribution()->MRef<int>(0x8) + 25) / 80) : "X"), AGetMainObject()->MouseAttribution()->MRef<int>(0x8), AGetMainObject()->MouseAttribution()->MRef<int>(0xC) + 18), 0xFF000000);
        fightInfoPainter.Draw(AText(std::format("{:04}", ((AGetMainObject()->MouseAttribution()->MRef<int>(0x8) + 25) % 80 * 125)), AGetMainObject()->MouseAttribution()->MRef<int>(0x8) + 13, AGetMainObject()->MouseAttribution()->MRef<int>(0xC) + 18), 0xFF000000);
    }

    // 罐子统计
    if (settings.VBEStat && AMRef<int>(0x6A9EC0, 0x7F8) == AAsm::SCARY_POTTER_ENDLESS) {
        std::vector<int> VBStat(13, 0);
        for (auto& Item : aAlivePlaceItemFilter) {
            if (Item.Type() != 7)
                continue;
            if (Item.MRef<int>(0x44) == 1) {
                if (Item.MRef<int>(0x40) == 0)
                    ++VBStat[1];
                if (Item.MRef<int>(0x40) == 52)
                    ++VBStat[2];
                if (Item.MRef<int>(0x40) == 18)
                    ++VBStat[3];
                if (Item.MRef<int>(0x40) == 5)
                    ++VBStat[4];
                if (Item.MRef<int>(0x40) == 17)
                    ++VBStat[5];
                if (Item.MRef<int>(0x40) == 4)
                    ++VBStat[6];
                if (Item.MRef<int>(0x40) == 3)
                    ++VBStat[7];
                if (Item.MRef<int>(0x40) == 25)
                    ++VBStat[8];
            } else if (Item.MRef<int>(0x44) == 3) {
                ++VBStat[0];
            } else if (Item.MRef<int>(0x44) == 2) {
                if (Item.MRef<int>(0x3C) == 0)
                    ++VBStat[9];
                if (Item.MRef<int>(0x3C) == 4)
                    ++VBStat[10];
                if (Item.MRef<int>(0x3C) == 15)
                    ++VBStat[11];
                if (Item.MRef<int>(0x3C) == 23)
                    ++VBStat[12];
            }
        }
        fightInfoPainter.Draw(AText(std::format("{}阳\n\n{}单\n\n{}双\n\n{}三\n\n{}冰\n\n{}窝\n\n{}雷\n\n{}坚\n\n{}灯\n\n{}普\n\n{}桶\n\n{}丑\n\n{}巨", VBStat[0], VBStat[1], VBStat[2], VBStat[3], VBStat[4], VBStat[5], VBStat[6], VBStat[7], VBStat[8], VBStat[9], VBStat[10], VBStat[11], VBStat[12]), 770, 120), settings.VBEStatARGB);
    }
}

// 显示栈位，-1 = 关闭，0 = 前场，1 = 全部
// 这段逻辑最后更改的时间为20260305

inline void FightInfoDrawer::DrawIndex(const Settings& settings) {
    if (ShowIndexState == -1)
        return;
    std::vector<int> RightmostPlantCol(6, -1);
    if (ShowIndexState)
        LeftmostVisibleArea.assign(6, -1);
    for (auto& Plant : aAlivePlantFilter) {
        int PlantCol = Plant.Col();
        if (Plant.Type() == ACOB_CANNON) // 炮判定前轮
            PlantCol = Plant.Col() + 1;
        if (RightmostPlantCol[Plant.Row()] < PlantCol) // 检查最右植物
            RightmostPlantCol[Plant.Row()] = PlantCol;
        if (PlantCol < LeftmostVisibleArea[Plant.Row()])
            continue;
        int RectHeight = 11;
        if (ShowInfoState >= 0 && Plant.Hp() < Plant.HpMax())
            RectHeight = 0;
        int RectWidth = 21;
        int DigitOffset = 0;
        if (Plant.Index() < 100) {
            RectWidth = 14;
            DigitOffset = 4;
        }
        if (Plant.Index() < 10) {
            RectWidth = 7;
            DigitOffset = 7;
        }
        if (Plant.Index() < AGetMainObject()->PlantNext()) { // 不可栈位垫，7像素蓝色七段码
            int SizeOffset = 1;
            int FontSize = 2;
            if (Plant.Type() == ACOB_CANNON)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 110 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.CobGloomHPARGB, settings.CobGloomHP ? RectHeight : 11);
            else if (Plant.Type() == AGLOOM_SHROOM)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.CobGloomHPARGB, settings.CobGloomHP ? RectHeight : 11);
            else if (Plant.Type() == ACOFFEE_BEAN)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 14 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.OtherPlantHPARGB, settings.OtherPlantHP ? RectHeight : 11);
            else if (ARangeIn(Plant.Type(), {AWALL_NUT, ATALL_NUT, ASPIKEROCK}))
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.NutSpikeHPARGB, settings.NutSpikeHP ? RectHeight : 11);
            else if (Plant.Type() == APUMPKIN)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 54 + DigitOffset + SizeOffset, Plant.Yi() + 46 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.PumpkinHPARGB, settings.PumpkinHP ? RectHeight : 11);
            else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT}))
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 6 + DigitOffset + SizeOffset, Plant.Yi() + 58 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.LilyPotHPARGB, settings.LilyPotHP ? RectHeight : 11);
            else if (Plant.Type() == ASQUASH)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.OtherPlantHPARGB, settings.OtherPlantHP ? RectHeight : 11);
            else
                SegPainter.Draw(A7Seg(Plant.Index(), MyColToX(Plant.Col() + 1) + 30 + DigitOffset + SizeOffset, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, settings.OtherPlantHPARGB, settings.OtherPlantHP ? RectHeight : 11);
        } else { // 可栈位垫，9像素黑色Arial
            if (Plant.Type() == ACOB_CANNON) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 109 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.CobGloomHP ? RectHeight : 11), settings.CobGloomHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 108 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else if (Plant.Type() == AGLOOM_SHROOM) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.CobGloomHP ? RectHeight : 11), settings.CobGloomHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else if (Plant.Type() == ACOFFEE_BEAN) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 13, RectWidth, settings.OtherPlantHP ? RectHeight : 11), settings.OtherPlantHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 9), 0xFF000000);
            } else if (ARangeIn(Plant.Type(), {AWALL_NUT, ATALL_NUT, ASPIKEROCK})) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.NutSpikeHP ? RectHeight : 11), settings.NutSpikeHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else if (Plant.Type() == APUMPKIN) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 53 + DigitOffset, Plant.Yi() + 45, RectWidth, settings.PumpkinHP ? RectHeight : 11), settings.PumpkinHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 52 + DigitOffset, Plant.Yi() + 41), 0xFF000000);
            } else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT})) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 5 + DigitOffset, Plant.Yi() + 57, RectWidth, settings.LilyPotHP ? RectHeight : 11), settings.LilyPotHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 4 + DigitOffset, Plant.Yi() + 53), 0xFF000000);
            } else if (Plant.Type() == ASQUASH) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.OtherPlantHP ? RectHeight : 11), settings.OtherPlantHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else {
                backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 29 + DigitOffset, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, RectWidth, settings.OtherPlantHP ? RectHeight : 11), settings.OtherPlantHPARGB);
                lowIndexPainter.Draw(AText(std::format("{}", Plant.Index()), MyColToX(Plant.Col() + 1) + 28 + DigitOffset, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 21), 0xFF000000);
            }
        }
    }
    // 前场左扩
    for (int Row : {0, 1, 2, 3, 4, 5}) {
        if (LeftmostVisibleArea[Row] > RightmostPlantCol[Row])
            LeftmostVisibleArea[Row] = RightmostPlantCol[Row];
    }
    // 铲子栈位
    if (AGetMainObject()->PlantNext() < 10) {
        backgroundPainter.Draw(ARect(634, 8, 19, 26), 0xFF4CAF50);
        nextIndexPainter.Draw(AText(std::format("{}", AGetMainObject()->PlantNext()), 634, 2), 0xFF000000);
    } else if (AGetMainObject()->PlantNext() < 100) {
        backgroundPainter.Draw(ARect(626, 8, 35, 26), 0xFF4CAF50);
        nextIndexPainter.Draw(AText(std::format("{}", AGetMainObject()->PlantNext()), 626, 2), 0xFF000000);
    } else {
        backgroundPainter.Draw(ARect(618, 8, 51, 26), 0xFF4CAF50);
        nextIndexPainter.Draw(AText(std::format("{}", AGetMainObject()->PlantNext()), 618, 2), 0xFF000000);
    }
}



class MaidController {
public:
    uint32_t phase = AMaidCheats::MC_STOP;

    void Apply() { AMaidCheats::Phase() = phase; }
    void Summon() { phase = AMaidCheats::MC_CALL_PARTNER; Apply(); CreateCaption("Maid: Summon"); }
    void Dance() { phase = AMaidCheats::MC_DANCING; Apply(); CreateCaption("Maid: Dance"); }
    void Move() { phase = AMaidCheats::MC_MOVE; Apply(); CreateCaption("Maid: Forward"); }
    void Stop() { phase = AMaidCheats::MC_STOP; Apply(); CreateCaption("Maid: End"); }
    void Reset() { phase = AMaidCheats::MC_STOP; Apply(); }
};

class ActivationMarker : public ATickRunnerWithNoStart, public AOrderedEnterFightHook<-1> {
protected:
    struct Info { ATime begin; AGrid grid; int stackIndex; uint32_t argb; std::string mainText; std::string colText; };
    inline static ActivationMarker* instance = nullptr;
    std::deque<Info> infos;
    const ClockState* clock = nullptr;
    int Clock(const ATime& time) { return clock ? clock->Clock(time) : INT_MIN; }
    void Prune(const ATime& now) {
        if (markerDuration <= 0) { infos.clear(); return; }
        auto elapsed = [this, now](const Info& info) {
            int beginClock = Clock(info.begin), nowClock = Clock(now);
            if (info.begin.wave == now.wave && (beginClock == INT_MIN || nowClock == INT_MIN))
                beginClock = info.begin.time, nowClock = now.time;
            return beginClock != INT_MIN && nowClock != INT_MIN ? nowClock - beginClock : (now.wave < info.begin.wave ? -1 : markerDuration);
        };
        while (!infos.empty() && elapsed(infos.back()) < 0) infos.pop_back();
        while (!infos.empty() && elapsed(infos.front()) >= markerDuration) infos.pop_front();
    }
    void Add(const AGrid& grid, uint32_t argb, std::string mainText, std::string colText) {
        if (AGetMainObject() == nullptr || !clock) return;
        Prune(clock->now);
        if (markerDuration <= 0) return;
        bool used[4] = {};
        int sameGridCount = 0;
        for (const auto& info : infos) if (info.grid == grid) { used[info.stackIndex % 4] = true; ++sameGridCount; }
        int stackIndex = sameGridCount;
        for (int i = 0; i < 4; ++i) if (!used[i]) { stackIndex = i; break; }
        infos.push_back({clock->now, grid, stackIndex, argb, std::move(mainText), std::move(colText)});
    }
    virtual void _EnterFight() override { Reset(); }
    static void __stdcall AsmCallBack0x4666A0(AAsmCodeContext* context) {
        if (!instance || !instance->enabled || !instance->clock) return;
        APlant* plant = *(APlant**)(context->esp + 4);
        if (!plant) return;
        std::map<APlantType, uint32_t> plantColors = {
            {AICE_SHROOM, instance->IMarkerARGB}, {ADOOM_SHROOM, instance->NMarkerARGB}, {ACHERRY_BOMB, instance->AMarkerARGB}, {AJALAPENO, instance->JMarkerARGB}, {APOTATO_MINE, instance->MMarkerARGB},
        };
        auto colors = plantColors.find(static_cast<APlantType>(plant->Type()));
        if (colors == plantColors.end()) return;
        instance->Add(AGrid(plant->Row() + 1, plant->Col() + 1), colors->second, std::format("{:<4}  00", instance->clock->now.time + 1), std::format("{}.", plant->Col() + 1));
    }
    static void __stdcall AsmCallBack0x4606F0(AAsmCodeContext* context) {
        if (!instance || !instance->enabled || !instance->clock) return;
        APlant* plant = *(APlant**)(context->esp + 4);
        if (!plant) return;
        int abscissa = plant->Abscissa() + 40, col = abscissa / 80;
        instance->Add(AGrid(plant->Row() + 1, plant->Col() + 1), instance->WMarkerARGB, std::format("{:<4}  {:02}", instance->clock->now.time + 1, abscissa % 80 * 125 / 100), col < 10 ? std::format("{}.", col) : "X.");
    }
    static void __stdcall AsmCallBack0x46D85B(AAsmCodeContext* context) {
        if (!instance || !instance->enabled || !instance->clock) return;
        AProjectile* p = (AProjectile*)(context->ebp);
        if (!p) return;
        int targetAbscissa = p->CobTargetAbscissa(), col = targetAbscissa / 80;
        instance->Add(AGrid(p->CobTargetRow() + 1, int(targetAbscissa / 80.0f - 0.5) + 1), instance->PMarkerARGB, std::format("{:<4}  {:02}", instance->clock->now.time + 1, targetAbscissa % 80 * 125 / 100), col < 10 ? std::format("{}.", col) : "X.");
    }
public:
    bool enabled = true;
    int markerDuration = 300;
    uint32_t PMarkerARGB = 0xFFFFA000, IMarkerARGB = 0xFF00A0FF, NMarkerARGB = 0xFF353535, AMarkerARGB = 0xFFC00000, JMarkerARGB = 0xFFFF0040, WMarkerARGB = 0xFFA0B060, MMarkerARGB = 0xFFDAC060;
    MyPainter painter;
    ActivationMarker() = default;
    void ApplySettings(const Settings& settings) {
        enabled = settings.ActivationTime; markerDuration = settings.MarkerDuration; PMarkerARGB = settings.PMarkerARGB; IMarkerARGB = settings.IMarkerARGB; NMarkerARGB = settings.NMarkerARGB; AMarkerARGB = settings.AMarkerARGB; JMarkerARGB = settings.JMarkerARGB; WMarkerARGB = settings.WMarkerARGB; MMarkerARGB = settings.MMarkerARGB;
    }
    void Reset() { infos.clear(); }
    void Draw(const ClockState& clockState) {
        clock = &clockState;
        if (AGetMainObject() == nullptr) return;
        Prune(clockState.now);
        if (!enabled) return;
        for (const auto& info : infos) {
            int x = MyColToX(info.grid.col), y = MyRowToY(info.grid.row, info.grid.col), offset = info.stackIndex % 4 * 15;
            painter.Draw(ARect(x + 4, y + 9 + offset, 72, 14), info.argb);
            painter.Draw(AText(info.mainText, x + 3, y + 5 + offset), 0xFFFFFFFF, 0x0);
            painter.Draw(AText(info.colText, x + 44, y + 5 + offset), 0xFFFFFFFF, 0x0);
        }
    }
    void Start(const ClockState& clockState) {
        clock = &clockState; instance = this;
        AInsertUniqueAsmCode(0x4666A0, AsmCallBack0x4666A0);
        AInsertUniqueAsmCode(0x4606F0, AsmCallBack0x4606F0);
        AInsertUniqueAsmCode(0x46D85B, AsmCallBack0x46D85B);
    }
};

inline int MouseXToCol(int X) { return (X + 65) / 80; }
inline int MouseXYToRow(int X, int Y) {
    int Col = (X + 65) / 80;
    if (aFieldInfo.rowHeight == 100)
        return (Y + 55) / 100;
    else if (aFieldInfo.isRoof)
        return (Y + 40 - (Col < 5 ? (5 - Col) * 20 : 0)) / 85; // 天台
    return (Y + 40) / 85;
}
// 土炮点击扣阳光种植回冷却函数
inline void ClickSunPlantCd(int Type, int Row, int Col) {
    AAsm::MouseClick(0, 0, 1);
    AGetMainObject()->Sun() -= AAsm::GetSeedSunVal(Type >= 49 ? 48 : Type, Type - 49);
    AAsm::PutPlant(Row - 1, Col - 1, static_cast<APlantType>(Type));
    for (auto&& Seed : ABasicFilter<ASeed>()) {
        if ((Type >= 49 ? Seed.ImitatorType() : Seed.Type()) == (Type >= 49 ? Type - 49 : Type)) {
            Seed.InitialCd() = AMRef<int>(0x69F2B0 + 0x14 + 0x24 * (Type >= 49 ? Type - 49 : Type));
            Seed.IsUsable() = false;
            Seed.MRef<bool>(0x49 + 0x28) = true;
            ++Seed.MRef<int>(0x4C + 0x28);
        }
    }
}
// 对六路或屋顶水路的格子进行操作
inline void PlantShovelFireForbiddenGrid() {
    int Row = MouseXYToRow(AGetMainObject()->MouseAttribution()->MRef<int>(0x8), AGetMainObject()->MouseAttribution()->MRef<int>(0xC));
    int Col = MouseXToCol(AGetMainObject()->MouseAttribution()->MRef<int>(0x8));
    int MousePlant = AGetMainObject()->MouseAttribution()->MRef<int>(0x28);
    int MouseMPlant = AGetMainObject()->MouseAttribution()->MRef<int>(0x2C);
    // 屋顶水路种植
    if (MousePlant != -1 && (AGetPlantPtr(Row, Col, ALILY_PAD) || AGetPlantPtr(Row, Col, ACATTAIL))) {
        switch (MousePlant) {
        case AGATLING_PEA:
            if (!AGetPlantPtr(Row, Col, AREPEATER))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AREPEATER));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case ATWIN_SUNFLOWER:
            if (!AGetPlantPtr(Row, Col, ASUNFLOWER))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, ASUNFLOWER));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case AGLOOM_SHROOM:
            if (!AGetPlantPtr(Row, Col, AFUME_SHROOM))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AFUME_SHROOM));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case ACATTAIL:
            if (!AGetPlantPtr(Row, Col, ALILY_PAD))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, ALILY_PAD));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case AWINTER_MELON:
            if (!AGetPlantPtr(Row, Col, AMELON_PULT))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AMELON_PULT));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case AGOLD_MAGNET:
            if (!AGetPlantPtr(Row, Col, AMAGNET_SHROOM))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AMAGNET_SHROOM));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case ASPIKEROCK:
            if (!AGetPlantPtr(Row, Col, ASPIKEWEED))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, ASPIKEWEED));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case ACOB_CANNON:
            if (!AGetPlantPtr(Row, Col, AKERNEL_PULT) || !AGetPlantPtr(Row, Col + 1, AKERNEL_PULT))
                break;
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AKERNEL_PULT));
            AAsm::RemovePlant(AGetPlantPtr(Row, Col + 1, AKERNEL_PULT));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        case AIMITATOR:
            if (MouseMPlant != APUMPKIN && AGetPlantPtr(Row, Col))
                break;
            if (MouseMPlant == AWALL_NUT && AGetPlantPtr(Row, Col, AWALL_NUT))
                AAsm::RemovePlant(AGetPlantPtr(Row, Col, AWALL_NUT));
            if (MouseMPlant == ATALL_NUT && AGetPlantPtr(Row, Col, ATALL_NUT))
                AAsm::RemovePlant(AGetPlantPtr(Row, Col, ATALL_NUT));
            if (MouseMPlant == APUMPKIN && AGetPlantPtr(Row, Col, APUMPKIN))
                AAsm::RemovePlant(AGetPlantPtr(Row, Col, APUMPKIN));
            ClickSunPlantCd(MouseMPlant + 49, Row, Col);
            break;
        default:
            if (MousePlant != APUMPKIN && AGetPlantPtr(Row, Col))
                break;
            if (MousePlant == AWALL_NUT && AGetPlantPtr(Row, Col, AWALL_NUT))
                AAsm::RemovePlant(AGetPlantPtr(Row, Col, AWALL_NUT));
            if (MousePlant == ATALL_NUT && AGetPlantPtr(Row, Col, ATALL_NUT))
                AAsm::RemovePlant(AGetPlantPtr(Row, Col, ATALL_NUT));
            if (MousePlant == APUMPKIN && AGetPlantPtr(Row, Col, APUMPKIN))
                AAsm::RemovePlant(AGetPlantPtr(Row, Col, APUMPKIN));
            ClickSunPlantCd(MousePlant, Row, Col);
            break;
        }
        return;
    }
    // 六路铲除点炮种植
    if (Row != 6)
        return;
    // 铲除
    if (AGetMainObject()->MouseAttribution()->Type() == 6) {
        if (AGetPlantPtr(Row, Col))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col));
        else if (AGetPlantPtr(Row, Col, APUMPKIN))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, APUMPKIN));
        else if (AGetPlantPtr(Row, Col, ALILY_PAD))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, ALILY_PAD));
        else if (AGetPlantPtr(Row, Col, AFLOWER_POT))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AFLOWER_POT));
        return;
    }
    // 点炮
    if (AGetCobRecoverTime(AGetPlantIndex(Row, Col, ACOB_CANNON)) == 0) {
        AGetMainObject()->MouseAttribution()->Type() = 8;
        AGetMainObject()->MouseAttribution()->CannonAddress() = AGetPlantPtr(Row, Col, ACOB_CANNON)->Id();
        return;
    }
    if (AGetCobRecoverTime(AGetPlantIndex(Row, Col - 1, ACOB_CANNON)) == 0) {
        AGetMainObject()->MouseAttribution()->Type() = 8;
        AGetMainObject()->MouseAttribution()->CannonAddress() = AGetPlantPtr(Row, Col - 1, ACOB_CANNON)->Id();
        return;
    }
    // 种植
    if (MousePlant == -1)
        return;
    if (MousePlant != AIMITATOR && AAsm::GetPlantRejectType(MousePlant, Row - 1, Col - 1) != AAsm::NIL)
        return;
    if (MousePlant == AIMITATOR && AAsm::GetPlantRejectType(MouseMPlant, Row - 1, Col - 1) != AAsm::NIL)
        return;
    switch (MousePlant) {
    case AGATLING_PEA:
        if (!AGetPlantPtr(Row, Col, AREPEATER))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, AREPEATER));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case ATWIN_SUNFLOWER:
        if (!AGetPlantPtr(Row, Col, ASUNFLOWER))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, ASUNFLOWER));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case AGLOOM_SHROOM:
        if (!AGetPlantPtr(Row, Col, AFUME_SHROOM))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, AFUME_SHROOM));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case ACATTAIL:
        if (!AGetPlantPtr(Row, Col, ALILY_PAD))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, ALILY_PAD));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case AWINTER_MELON:
        if (!AGetPlantPtr(Row, Col, AMELON_PULT))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, AMELON_PULT));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case AGOLD_MAGNET:
        if (!AGetPlantPtr(Row, Col, AMAGNET_SHROOM))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, AMAGNET_SHROOM));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case ASPIKEROCK:
        if (!AGetPlantPtr(Row, Col, ASPIKEWEED))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, ASPIKEWEED));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case ACOB_CANNON:
        if (!AGetPlantPtr(Row, Col, AKERNEL_PULT) || !AGetPlantPtr(Row, Col + 1, AKERNEL_PULT))
            break;
        AAsm::RemovePlant(AGetPlantPtr(Row, Col, AKERNEL_PULT));
        AAsm::RemovePlant(AGetPlantPtr(Row, Col + 1, AKERNEL_PULT));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    case AIMITATOR:
        if (MouseMPlant == AWALL_NUT && AGetPlantPtr(Row, Col, AWALL_NUT))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AWALL_NUT));
        if (MouseMPlant == ATALL_NUT && AGetPlantPtr(Row, Col, ATALL_NUT))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, ATALL_NUT));
        if (MouseMPlant == APUMPKIN && AGetPlantPtr(Row, Col, APUMPKIN))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, APUMPKIN));
        ClickSunPlantCd(MouseMPlant + 49, Row, Col);
        break;
    default:
        if (MousePlant == AWALL_NUT && AGetPlantPtr(Row, Col, AWALL_NUT))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, AWALL_NUT));
        if (MousePlant == ATALL_NUT && AGetPlantPtr(Row, Col, ATALL_NUT))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, ATALL_NUT));
        if (MousePlant == APUMPKIN && AGetPlantPtr(Row, Col, APUMPKIN))
            AAsm::RemovePlant(AGetPlantPtr(Row, Col, APUMPKIN));
        ClickSunPlantCd(MousePlant, Row, Col);
        break;
    }
}

class ActionController {
public:
    bool uiVisible = true;
    bool enabled = false;

    void Speed10x() { AGetPvzBase()->TickMs() = AGetPvzBase()->TickMs() == 1 ? 10 : 1; }
    void SkipToWave(const Settings& settings) { ASkipTick(settings.SkipTickWave, 0); }
    void Restart() {
        ABackToMain();
        AEnterGame(AMRef<int>(0x6A9EC0, 0x7F8));
    }
    void ReplayRewind(const Settings& settings) {
        if (aReplay.GetState() == aReplay.PLAYING) {
            aReplay.Pause();
            aReplay.ShowOneTick(aReplay.GetPlayIdx() - settings.tickRewindCount);
        } else {
            Paused = true;
            PausedSlowed = false;
            ASetAdvancedPause(Paused, false, 0);
            aReplay.ShowOneTick(aReplay.GetRecordIdx() - settings.tickRewindCount);
        }
    }
    void ToggleAdvancedPause() {
        if (aReplay.GetState() == aReplay.PLAYING) {
            aReplay.IsPaused() ? aReplay.GoOn() : aReplay.Pause();
        } else {
            if (PausedCd < 480)
                return;
            Paused = !Paused;
            PausedSlowed = false;
            ASetAdvancedPause(Paused, false, 0);
        }
    }
    void NextTick() {
        if (aReplay.GetState() == aReplay.PLAYING) {
            aReplay.Pause();
            aReplay.ShowOneTick(aReplay.GetPlayIdx() + 1);
        } else {
            if (PausedCd < 480)
                return;
            Paused = false;
            PausedSlowed = false;
            ASetAdvancedPause(Paused, false, 0);
            AConnect(ANowDelayTime(1), [] {
                Paused = !Paused;
                ASetAdvancedPause(Paused, false, 0);
            });
        }
    }
    void ToggleSeedChooserTop() { AMRef<int>(0x416DBE) = AMRef<int>(0x416DBE) == 699999 ? 100001 : 699999; }
    void ToggleDanceFast() {
        DCState = DCState == 0 ? -1 : 0;
        SetDance(false);
        CreateCaption(DCState == 0 ? "DanceCheat: Fast" : "DanceCheat: Off");
    }
    void ToggleDanceSlow() {
        DCState = DCState == 1 ? -1 : 1;
        SetDance(false);
        CreateCaption(DCState == 1 ? "DanceCheat: Slow" : "DanceCheat: Off");
    }
    void ToggleAutoCollect() { *(uint8_t*)0x0043158F = *(uint8_t*)0x0043158F == 0xEB ? 0x75 : 0xEB; }
    void ToggleWindFix() {
        *(uint8_t*)0x46DCE3 == 0x83 ? *(std::array<uint8_t, 10>*)0x46DCE3 = {0x75, 0x08, 0xD9, 0x46, 0x34, 0xD8, 0xC1, 0xD9, 0x5E, 0x34} : *(std::array<uint8_t, 10>*)0x46DCE3 = {0x83, 0x7E, 0x5C, 0x0B, 0x75, 0x04, 0xDD, 0xD8, 0xEB, 0x1B};
        CreateCaption(*(uint8_t*)0x46DCE3 == 0x83 ? "FixWind: On" : "FixWind: Off");
    }
    void ToggleGameUi() {
        uiVisible = !uiVisible;
        AMRef<bool>(0x6A9EC0, 0x768, 0x144, 0x18) = AMRef<bool>(0x6A9EC0, 0x768, 0x55F1) = uiVisible;
        AMRef<int>(0x6A9EC0, 0x768, 0x55F4) = AMRef<bool>(0x6A9EC0, 0x768, 0x148, 0xF9) = !uiVisible;
    }
    void ToggleAnimationSkip() { AMRef<bool>(0x6A9EC0, 0x7F5) = AMRef<bool>(0x6A9EC0, 0x7F5) ? false : true; }
    void ToggleSnapshotMode() {
        SnapshotModeSwitch = !SnapshotModeSwitch;
        CreateCaption(SnapshotModeSwitch ? "SnapshotMode: On" : "SnapshotMode: Off");
        ASetAdvancedPause(SnapshotModeSwitch, false, 0);
        AMRef<int>(0x6A9EC0, 0x768, 0x30) = 0;
        AMRef<int>(0x6A9EC0, 0x768, 0x34) = 0;
        if (Paused)
            ASetAdvancedPause(Paused, false, 0);
    }
    void MoveViewUp() { AMRef<int>(0x6A9EC0, 0x768, 0x34) -= 5; }
    void MoveViewDown() { AMRef<int>(0x6A9EC0, 0x768, 0x34) += 5; }
    void MoveViewLeft() { AMRef<int>(0x6A9EC0, 0x768, 0x30) -= 5; }
    void MoveViewRight() { AMRef<int>(0x6A9EC0, 0x768, 0x30) += 5; }
    void OneKeySwitch(FightInfoDrawer& drawer, SmartRemoveController& smartRemove, WarningController& warning) {
        enabled = !enabled;
        if (enabled) {
            CreateCaption("A-TAS: On", {BOTTOMFAST});
            *(uint8_t*)0x0043158F = 0xEB;
            drawer.SetInfoState(0);
            drawer.SetIndexState(0);
            smartRemove.enabled = true;
            warning.BalloonWarning = true;
        } else {
            CreateCaption("A-TAS: Off");
            *(uint8_t*)0x0043158F = 0x75;
            drawer.Reset();
            smartRemove.Reset();
            warning.BalloonWarning = false;
        }
    }
    void ResetGame(MaidController& maid, FightInfoDrawer& drawer, SmartRemoveController& smartRemove, WarningController& warning) {
        CreateCaption("Reset");
        ASetAdvancedPause(false, false, 0);
        Paused = false;
        PausedSlowed = false;
        AMRef<int>(0x6A9EC0, 0x768, 0x30) = 0;
        AMRef<int>(0x6A9EC0, 0x768, 0x34) = 0;
        AGetPvzBase()->TickMs() = 10;
        AMRef<bool>(0x6A9EAB) = false;
        DCState = -1;
        SetDance(false);
        maid.Reset();
        *(std::array<uint8_t, 10>*)0x46DCE3 = {0x75, 0x08, 0xD9, 0x46, 0x34, 0xD8, 0xC1, 0xD9, 0x5E, 0x34};
        SetMusic(AGetMainObject()->Scene() + 1);
        AMRef<int>(0x416DBE) = 100001;
        uiVisible = true;
        AMRef<bool>(0x6A9EC0, 0x768, 0x144, 0x18) = AMRef<bool>(0x6A9EC0, 0x768, 0x55F1) = true;
        AMRef<int>(0x6A9EC0, 0x768, 0x55F4) = AMRef<bool>(0x6A9EC0, 0x768, 0x148, 0xF9) = false;
        enabled = false;
        *(uint8_t*)0x0043158F = 0x75;
        drawer.Reset();
        smartRemove.Reset();
        warning.Reset();
    }
};

// 主体函数


struct Runtime {
    ClockState clock;
    FightInfoDrawer drawer;
    SmartRemoveController smartRemove;
    WarningController warning;
    MaidController maid;
    ActionController action;
    ActivationMarker activationMarker;
};

} // namespace ATas

#endif // __ATAS_H__
