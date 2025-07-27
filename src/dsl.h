/*
 * @Description：AvZdsl库的一些函数，不直接使用avz库的dsl的目的是减少编译时间，否则编译一次要花很久
 */
#pragma once
#ifndef __ADSL_H__
#define __ADSL_H__
#include "avz.h"

inline void AAverageSpawn(const std::set<int>& types = {}) {
    std::vector<int> default_rows;
    for (int i = 0; i < aFieldInfo.nRows; ++i)
        if (aFieldInfo.rowType[i + 1] == ARowType::LAND)
            default_rows.push_back(i);

    const int TOTAL_WAVE = AGetMainObject()->TotalWave();
    for (int wave = 1; wave <= TOTAL_WAVE; ++wave) {
        AConnect(ATime(wave, 0), [=] {
            std::vector<int> rows[33];
            int cur[33];
            for (int type = 0; type < 33; ++type) {
                if (ARangeIn(type, {ADUCKY_TUBE_ZOMBIE, ASNORKEL_ZOMBIE, ADOLPHIN_RIDER_ZOMBIE}))
                    rows[type] = {2, 3};
                else if (aFieldInfo.nRows == 5 && type == ADANCING_ZOMBIE)
                    rows[type] = {1, 2, 3};
                else if (aFieldInfo.hasPool && type == ABALLOON_ZOMBIE && ANowWave() > 5)
                    rows[type] = {0, 1, 2, 3, 4, 5};
                else
                    rows[type] = default_rows;
                std::ranges::shuffle(rows[type], aRandom.GetEngine());
                cur[type] = 0;
            }
            for (auto& z : aAliveZombieFilter) {
                if (z.ExistTime() != 0)
                    continue;

                int type = z.Type();
                if (type == ABUNGEE_ZOMBIE)
                    continue;

                if (!types.empty() && !types.contains(type))
                    continue;

                if (ARangeIn(type, {AZOMBIE, ACONEHEAD_ZOMBIE, ABUCKETHEAD_ZOMBIE}) && aFieldInfo.rowType[z.Row() + 1] == ARowType::POOL)
                    type = ADUCKY_TUBE_ZOMBIE;

                int row = z.Row(), newRow = rows[type][cur[type]];
                z.Row() = newRow;
                z.Ordinate() += aFieldInfo.rowHeight * (newRow - row);
                z.MRef<int>(0x20) += 10000 * (newRow - row);
                cur[type] = (cur[type] + 1) % rows[type].size();
            }
        });
    }
}

inline APlant* Card(APlantType seed, int row, float col) {
    int seed_ = seed % AM_PEASHOOTER;
    if (AAsm::GetPlantRejectType(seed_, row - 1, int(col - 0.5)) == AAsm::NEEDS_POT)
        ACard(AFLOWER_POT, row, col);
    if (AAsm::GetPlantRejectType(seed_, row - 1, int(col - 0.5)) == AAsm::NOT_ON_WATER)
        ACard(ALILY_PAD, row, col);
    return ACard(seed, row, col);
}

#endif //!__DSL_H__