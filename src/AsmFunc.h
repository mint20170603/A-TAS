/*
 * @Description：A-TAS汇编函数
 */
#pragma once
#ifndef __ASM_FUNC_H__
#define __ASM_FUNC_H__
#include "avz.h"

// 设定音乐
inline void SetMusic(int musicid) {
    asm volatile("movl %[musicid], %%edi;"
                 "movl 0x6A9EC0, %%eax;"
                 "movl 0x83C(%%eax), %%eax;"
                 "movl $0x45B750, %%edx;"
                 "calll *%%edx;"
        :
        : [musicid] "rm"(musicid)
        : "eax", "edx", "edi");
}

// 在当帧更新植物动画的颜色
inline void UpdateReanimColor(int plant_index) {
    asm volatile("movl 0x6A9EC0, %%eax;"
                 "movl 0x768(%%eax), %%eax;"
                 "movl 0xAC(%%eax), %%eax;"
                 "movl $0x14C, %%ecx;"
                 "imull %[plant_index], %%ecx;"
                 "addl %%ecx, %%eax;"
                 "pushl %%eax;"
                 "movl $0x4635C0, %%edx;"
                 "calll *%%edx;"
        :
        : [plant_index] "m"(plant_index)
        : "eax", "ecx", "edx");
}

// 设定Dance
inline void SetDance(bool state) {
    asm volatile("movl 0x6A9EC0, %%eax;"
                 "movl 0x768(%%eax), %%ecx;"
                 "pushl %%ecx;"
                 "movl %[state], %%ebx;"
                 "movl $0x41AFD0, %%eax;"
                 "calll *%%eax;"
        :
        : [state] "rm"(unsigned(state))
        : "eax", "ebx", "ecx");
}

// -1 = 正常，0 = 加速，1 = 减速
static int DCState = -1;
inline void DanceCheat() {
    if (DCState != -1)
        SetDance(DCState);
}

// 生成字幕
enum CaptionStyle {
    LOWERMIDDLE = 1, // y 400 ~ 510
    LOWERMIDDLESTAY,
    LOWERPART, // y 480 ~ 580 约5s
    LATER,
    LATERSTAY,
    BOTTOM,     // y 530 ~ 585 约15s
    BOTTOMFAST, // y 530 ~ 585 约5s
    STAY,
    TALLFAST,
    TALL10SEC,
    TALL8SEC,
    CENTER,
    CENTERFAST,
    BOTTOMWHITE,
    CENTERRED,
    TOPYELLOW,
    ZENGARDEN,
};

struct CaptionSet {
    CaptionStyle style = BOTTOMFAST;
    int duration = 100;
};

template <typename... CaptionArgs>
void CreateCaption(const std::string& content, CaptionSet captionSet = {}, CaptionArgs... args) {
    // if (AGetMainObject() == nullptr)
    //     return; // 防崩溃代码
    std::string _content = content;
    std::initializer_list<int> {(__StringConvert(_content, args), 0)...};
    const char* caption = _content.c_str();
    uint32_t str[7] {};
    str[1] = (uint32_t)caption;
    str[5] = (uint32_t)strlen(caption);
    str[6] = 16 + str[5];
    void* _str = str;
    asm volatile("movl 0x6A9EC0, %%esi;"
                 "movl 0x768(%%esi), %%esi;"
                 "movl 0x140(%%esi), %%esi;"
                 "movl %[style], %%ecx;"
                 "movl %[_str], %%edx;"
                 "movl $0x459010, %%eax;"
                 "calll *%%eax;"
        :
        : [_str] "m"(_str), [style] "m"(captionSet.style)
        : "eax", "ecx", "edx", "esi");
    AGetMainObject()->Words()->DisappearCountdown() = captionSet.duration;
}

// 冰道覆盖
inline bool isIceTrailCover(int Row, int Col) {
    bool IceCovered;
    asm volatile("movl %[Row], %%eax;"
                 "pushl %[Col];"
                 "movl 0x6A9EC0, %%esi;"
                 "movl 0x768(%%esi), %%esi;"
                 "movl $0x40DFC0, %%ecx;"
                 "calll *%%ecx;"
                 "mov %%eax, %[IceCovered];"
        : [IceCovered] "=rm"(IceCovered)
        : [Row] "rm"(Row - 1), [Col] "rm"(Col - 1)
        : "eax", "ecx", "esi");
    return IceCovered;
}

// 设定关卡的基础初始数据，包括出怪类型数组、出怪列表和第一波僵尸的倒计时等
inline void InitZombieWaves() {
    asm volatile(
        "movl 0x6A9EC0, %%eax;"     // 将种子地址加载到 eax
        "movl 0x768(%%eax), %%eax;" // 通过偏移 0x768 更新 eax
        "movl $0x40ABB0, %%edx;"    // 将目标函数地址加载到 edx
        "calll *%%edx;"             // 通过 edx 指针调用目标函数
        :
        :
        : "eax", "edx");
}

// 生成关卡可能出现的僵尸的预览
inline void PlaceStreetZombies() {
    *(uint8_t*)0x0043A153 = 0x80;
    asm volatile(
        "movl 0x6A9EC0, %%eax;"
        "movl 0x768(%%eax), %%eax;"
        "movl 0x15C(%%eax), %%eax;" // eax = CutScene* this
        "pushl %%eax;"              // 將 this 指針壓棧作為參數
        "movl $0x43A140, %%edx;"    // 函數地址
        "calll *%%edx;"             // 調用
        :
        :
        : "eax", "edx");
    *(uint8_t*)0x0043A153 = 0x85;
}

#endif //!__ASM_FUNC_H__