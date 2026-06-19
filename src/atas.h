#pragma once
#ifndef __ATAS_H__
#define __ATAS_H__

#include "AsmFunc.h"
#include "Draw.h"
#include "asm_insert_code/asm_insert_code.h"
#include "dsl/shorthand.h"
#include "game_controller.h"

// 最左冰道
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
inline std::vector<std::vector<std::array<APlant*, 5>>> PlantMap(6, std::vector<std::array<APlant*, 5>>(9, {nullptr, nullptr, nullptr, nullptr, nullptr}));
inline auto& Grid(int Row, int Col) { return PlantMap[Row - 1][Col - 1]; }

// 遍历一次全场植物，得到一份按格子存储的PlantMap
// 0-容器 1-南瓜 2-咖啡 3-常规 4-飞行窝瓜
inline void UpdatePlantMap() {
    for (auto& Row : PlantMap)
        for (auto& Grid : Row)
            Grid.fill(nullptr);
    for (auto& Plant : aAlivePlantFilter) {
        switch (Plant.Type()) {
        case ALILY_PAD:
        case AFLOWER_POT:
            PlantMap[Plant.Row()][Plant.Col()][0] = &Plant;
            break;
        case APUMPKIN:
            PlantMap[Plant.Row()][Plant.Col()][1] = &Plant;
            break;
        case ACOFFEE_BEAN:
            PlantMap[Plant.Row()][Plant.Col()][2] = &Plant;
            break;
        case ASQUASH:
            if (ARangeIn(Plant.State(), {5, 6})) // 5-上升 6-下落
                PlantMap[Plant.Row()][Plant.Col()][4] = &Plant;
            else
                PlantMap[Plant.Row()][Plant.Col()][3] = &Plant;
            break;
        default:
            PlantMap[Plant.Row()][Plant.Col()][3] = &Plant;
            break;
        }
    }
}

// 得到植物防御域
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
inline bool SmartRemoveSwitch = false;
inline void SmartRemove() {
    if (!SmartRemoveSwitch)
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

// 检查卡片是否能用或是被拿着
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
inline int JackWarning = -1;
inline void JackPause() {
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

// 气球位移
inline float BalloonΔX(int Time, float Speed, int SlowCountdown = 0) {
    if (!SlowCountdown)
        return Speed * Time; // 原速 × 总时间
    if (SlowCountdown > Time)
        return 0.4 * Speed * Time; // 减速 × 总时间
    return 0.4 * Speed * (SlowCountdown - 1) + Speed * (Time - (SlowCountdown - 1));
    // 减速 × (减速倒计时 - 1) + 原速 × (总时间 - (减速倒计时 - 1))
}

// 气球字幕、暂停防呆计算
inline bool BalloonWarning = false;
inline void BalloonCaption() {
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

// 气球暂停
inline int BalloonPauseCd = 200;
inline void BalloonPause() {
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

// 真实倒计时
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

// 自制时钟
inline std::vector<int> WaveClock(40, 0);
inline ATime Now;
inline void WaveClockUpdate() {
    if (AGetMainObject() == nullptr)
        return;
    if (RealCountdown())
        WaveClock[AGetMainObject()->Wave()] = AGetMainObject()->GameClock() + RealCountdown();

    Now.wave = AGetMainObject()->Wave() ?: 1;
    if (AGetMainObject()->Wave() == 0)
        Now.time = -AGetMainObject()->RefreshCountdown();
    else if (WaveClock[AGetMainObject()->Wave() - 1] == 0)
        Now.time = ANowTime(ANowWave());
    else
        Now.time = AGetMainObject()->GameClock() - WaveClock[AGetMainObject()->Wave() - 1];
}

class ActivationMarker : public ATickRunnerWithNoStart, public AOrderedEnterFightHook<-1> {
public:
    bool enabled = true;
    int markerDuration = 300;
    uint32_t PMarkerARGB = 0xFFFFA000;
    uint32_t IMarkerARGB = 0xFF00A0FF;
    uint32_t NMarkerARGB = 0xFF353535;
    uint32_t AMarkerARGB = 0xFFC00000;
    uint32_t JMarkerARGB = 0xFFFF0040;
    uint32_t WMarkerARGB = 0xFFA0B060;
    uint32_t MMarkerARGB = 0xFFDAC060;
    MyPainter painter;

protected:
    struct Info {
        ATime begin;
        AGrid grid;
        int stackIndex;
        uint32_t argb;
        std::string mainText;
        std::string colText;
    };

    inline static ActivationMarker* instance = nullptr;
    std::deque<Info> infos;

    int Clock(const ATime& time) {
        if (time.wave <= 1)
            return time.time;
        int idx = time.wave - 1;
        return 0 <= idx && idx < int(WaveClock.size()) && WaveClock[idx] ? WaveClock[idx] + time.time : INT_MIN;
    }

    void Prune(const ATime& now) {
        if (markerDuration <= 0) {
            infos.clear();
            return;
        }
        auto elapsed = [this, now](const Info& info) {
            int beginClock = Clock(info.begin), nowClock = Clock(now);
            if (info.begin.wave == now.wave && (beginClock == INT_MIN || nowClock == INT_MIN))
                beginClock = info.begin.time, nowClock = now.time;
            return beginClock != INT_MIN && nowClock != INT_MIN ? nowClock - beginClock : (now.wave < info.begin.wave ? -1 : markerDuration);
        };
        while (!infos.empty() && elapsed(infos.back()) < 0)
            infos.pop_back();
        while (!infos.empty() && elapsed(infos.front()) >= markerDuration)
            infos.pop_front();
    }

    void Add(const AGrid& grid, uint32_t argb, std::string mainText, std::string colText) {
        if (AGetMainObject() == nullptr)
            return;
        Prune(Now);
        if (markerDuration <= 0)
            return;
        bool used[4] = {};
        int sameGridCount = 0;
        for (const auto& info : infos) {
            if (info.grid == grid) {
                used[info.stackIndex % 4] = true;
                ++sameGridCount;
            }
        }
        int stackIndex = sameGridCount;
        for (int i = 0; i < 4; ++i)
            if (!used[i]) {
                stackIndex = i;
                break;
            }
        infos.push_back({Now, grid, stackIndex, argb, std::move(mainText), std::move(colText)});
    }

    virtual void _EnterFight() override {infos.clear();}

    // void Plant::DoSpecial()
    static void __stdcall AsmCallBack0x4666A0(AAsmCodeContext* context) {
        if (!instance || !instance->enabled)
            return;
        APlant* plant = *(APlant**)(context->esp + 4);
        if (!plant)
            return;
        std::map<APlantType, uint32_t> plant_colors = {
            {AICE_SHROOM, instance->IMarkerARGB},
            {ADOOM_SHROOM, instance->NMarkerARGB},
            {ACHERRY_BOMB, instance->AMarkerARGB},
            {AJALAPENO, instance->JMarkerARGB},
            {APOTATO_MINE, instance->MMarkerARGB},
            // {ABLOVER, 0xFF00A000},
            // {ACOFFEE_BEAN, 0x00000000},
            // {AUMBRELLA_LEAF, 0x00000000},
        };
        auto colors = plant_colors.find(static_cast<APlantType>(plant->Type()));
        if (colors == plant_colors.end())
            return;
        instance->Add(
            AGrid(plant->Row() + 1, plant->Col() + 1), colors->second, 
            std::format("{:<4}  00", Now.time + 1), 
            std::format("{}.", plant->Col() + 1)
        );
    }

    // void Plant::DoSquashDamage()
    static void __stdcall AsmCallBack0x4606F0(AAsmCodeContext* context) {
        if (!instance || !instance->enabled)
            return;
        APlant* plant = *(APlant**)(context->esp + 4);
        if (!plant)
            return;
        int abscissa = plant->Abscissa() + 40, col = abscissa / 80;
        instance->Add(
            AGrid(plant->Row() + 1, plant->Col() + 1), instance->WMarkerARGB, 
            std::format("{:<4}  {:02}", Now.time + 1, abscissa % 80 * 125 / 100), 
            col < 10 ? std::format("{}.", col) : "X."
        );
    }

    // void Projectile::UpdateLobMotion()
    // mBoard->KillAllZombiesInRadius(mRow, mPosX + 80, mPosY + 40, 115, 1, true, mDamageRangeFlags);
    static void __stdcall AsmCallBack0x46D85B(AAsmCodeContext* context) {
        if (!instance || !instance->enabled)
            return;
        AProjectile* p = (AProjectile*)(context->ebp);
        if (!p)
            return;
        int targetAbscissa = p->CobTargetAbscissa(), col = targetAbscissa / 80;
        instance->Add(
            AGrid(p->CobTargetRow() + 1, int(targetAbscissa / 80.0f - 0.5) + 1), instance->PMarkerARGB, 
            std::format("{:<4}  {:02}", Now.time + 1, targetAbscissa % 80 * 125 / 100), 
            col < 10 ? std::format("{}.", col) : "X."
        );
    }

public:
    void Draw() {
        if (AGetMainObject() == nullptr)
            return;

        Prune(Now);
        if (!enabled)
            return;

        for (const auto& info : infos) {
            int x = MyColToX(info.grid.col);
            int y = MyRowToY(info.grid.row, info.grid.col);
            int offset = info.stackIndex % 4 * 15;
            painter.Draw(ARect(x + 4, y + 9 + offset, 72, 14), info.argb);
            painter.Draw(AText(info.mainText, x + 3, y + 5 + offset), 0xFFFFFFFF, 0x0);
            painter.Draw(AText(info.colText, x + 44, y + 5 + offset), 0xFFFFFFFF, 0x0);
        }
    }

    void Start() {
        instance = this;
        AInsertUniqueAsmCode(0x4666A0, AsmCallBack0x4666A0);
        AInsertUniqueAsmCode(0x4606F0, AsmCallBack0x4606F0);
        AInsertUniqueAsmCode(0x46D85B, AsmCallBack0x46D85B);
    }
};

// 六路种植相关代码
// 鼠标座标转换成格子
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

#endif // __ATAS_H__
