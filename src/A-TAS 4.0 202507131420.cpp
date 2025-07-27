// A-TAS 4.0 by mint/残云/碧水/正弦
// 技术指导：Leonhard/铃仙/向量/零度
// 本辅助工具目前版本使用的键控注入框架应使用AvZ2 2.8.5 20250711版本，源码不保证对更旧版本AvZ的兼容性

#define UNICODE
#define A_TAS_VERSION 202507131420
#include "AsmFunc.h"
#include "Draw.h"
#include "dsl.h"
#include "game_controller.h"
#include "showme/sm.h"
#include "win32gui/main.h"

#include <filesystem>
#include <tlhelp32.h>

std::shared_ptr<A7zCompressor> compressor = nullptr;
bool isInitSuccess = false;

// 绘制进度条的painter
MyPainter barPainter;
// 绘制战场信息的painter
MyPainter fightInfoPainter;
// 绘制七段码的painter
MyPainter SegPainter;
// 绘制背景
MyPainter backgroundPainter;
// 绘制低栈植物栈位
MyPainter lowIndexPainter;
// 绘制下一个栈位
MyPainter nextIndexPainter;
// 绘制红眼出怪信息
MyPainter GigaNumPainter;

// 绘制ShowMe的tickRunner
SMShowMe tickShowMe;
// 只在战斗界面运行的tickRunner
ATickRunner tickFight;
// 用于绘制的tickRunner
ATickRunner tickPainter;
// 全局运行的tickRunner
ATickRunner tickGlobal;

constexpr auto KEYBINDINGS_FILENAME = "keybindings.ini";
constexpr auto GAME_DATA_PATH = "C:/ProgramData/PopCap Games/PlantsVsZombies/userdata/";

// 预设按键
static std::array<std::string, 33> keyDefaults = {"A", "1", "2", "C", "R", "T", "F5", "BACKSPACE", "Z", "X", "SHIFT", "V", "G", "B", "H", "Q", "W", "S", "D", "F", "E", "I", "J", "Y", "N", "U", "L", "P", "UP", "DOWN", "LEFT", "RIGHT", "O"};
static std::array<const char*, 33> btnLabels = {"一键辅助", "减速一档", "加速一档", "0.25倍速", "10倍速", "跳到某波", "退出重进", "回档几帧", "高级暂停", "下一帧", "智能用卡", "卡槽置顶", "显示信息", "显示栈位", "智能铲除", "Dance快", "Dance慢", "女仆召唤", "女仆停滞", "女仆前进", "女仆解除", "自动收集", "小丑拦截", "气球拦截", "风炮修复", "隐藏UI", "跳过动画", "拍照模式", "视角上移", "视角下移", "视角左移", "视角右移", "PvZ初始化"};
static std::array<std::string, 33> keyBindings;
static std::array<AEdit*, 33> keyEdits;

// 预设设置
struct {
    // General
    char SpeedGears[256];
    int WavelengthRecord = 3;
    int SkipTickWave = 0;
    int ReadOnly = -1;

    // Replay
    bool AutoRecordOnGameStart = true;
    bool ShowMouse = true;
    bool AutoStop = true;
    bool Interpolate = true;
    bool ShowReplayInfo = true;
    int recordTickInterval = 10;
    int tickRewindCount = 1;
    char savePath[256];

    // Display
    bool ShowMe = true;
    bool CobCD = true;
    bool PlantOffset = true;
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

    // Spawn
    bool Types[26] = {};
    bool ZombieList = false;
    bool AverageRowSpawn = false;
    bool RandomType = false;
} settings;

// 得到本工具的路径
const std::string& GetToolPath() {
    static std::string toolPath;
    if (!toolPath.empty()) {
        return toolPath;
    }
    HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (handle == INVALID_HANDLE_VALUE) {
        return toolPath;
    }
    // 枚举进程模块
    MODULEENTRY32W info;
    info.dwSize = sizeof(MODULEENTRY32W);
    Module32FirstW(handle, &info);
    while (Module32NextW(handle, &info)) {
        if (std::wstring(info.szModule).find(L"libavz") != std::wstring::npos) {
            toolPath = AWStrToStr(info.szExePath);
            for (int i = toolPath.size() - 1; i >= 0; --i) {
                if (toolPath[i] == '\\' || toolPath[i] == '/') {
                    toolPath.resize(i);
                    break;
                }
            }
            CloseHandle(handle);
            return toolPath;
        }
    }
    CloseHandle(handle);
    return toolPath;
}

std::mutex mtx;

void SaveSettings() {
    if (!isInitSuccess) {
        return;
    }
    // 这里可能会导致线程不安全
    std::lock_guard lk(mtx);
    std::ofstream outFile(GetToolPath() + "/settings.dat", std::ios::out | std::ios::binary);
    outFile.write((char*)&settings, sizeof(settings));
    outFile.close();
}

void LoadSettings() {
    std::ifstream inFile(GetToolPath() + "/settings.dat", std::ios::in | std::ios::binary);
    if (!inFile) {
        return;
    }
    inFile.read((char*)&settings, sizeof(settings));
    inFile.close();
}

class EnsureSaveSettings {
public:
    ~EnsureSaveSettings() { SaveSettings(); }
} __; // 全局对象的析构函数确保调用 SaveSettings

bool SaveKeybindings() {
    if (!isInitSuccess)
        return false;
    // 这里可能会导致线程不安全
    std::lock_guard lk(mtx);
    std::ofstream outFile(GetToolPath() + "/" + KEYBINDINGS_FILENAME, std::ios::out | std::ios::binary);
    if (outFile.is_open()) {
        outFile << "A_TAS_VERSION:" << A_TAS_VERSION << std::endl;
        for (int i = 0; i < btnLabels.size(); ++i)
            outFile << btnLabels[i] << ":" << keyEdits[i]->GetText() << std::endl;
        outFile.close();
        return true;
    }
    return false;
}

bool LoadKeybindings() {
    std::ifstream inFile(GetToolPath() + "/" + KEYBINDINGS_FILENAME, std::ios::in | std::ios::binary);
    if (!inFile.is_open())
        return false;
    std::string line;
    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::string key;
        std::string value;
        if (std::getline(iss, key, ':')) {
            auto it = std::ranges::find(btnLabels, key);
            if (it != btnLabels.end()) {
                value.clear();
                std::getline(iss, value);
                auto i = std::distance(btnLabels.begin(), it);
                keyBindings[i] = value;
            }
        }
    }
    inFile.close();
    return true;
}

// 最左冰道
int LeftmostIceTrail(int Row) {
    int LeftCol = 10;
    for (int Col = 9; Col > 0; --Col) {
        if (isIceTrailCover(Row, Col))
            LeftCol -= 1;
    }
    return LeftCol;
}

// 冰道倒计时
int IceTrailCoverCD(int Row) { return AMRef<int>(0x6A9EC0, 0x768, 0x620 + Row * 4); }

// 正弦/碧水原教旨智能铲除
std::vector<std::vector<std::array<APlant*, 5>>> PlantMap(6, std::vector<std::array<APlant*, 5>>(9, {nullptr, nullptr, nullptr, nullptr, nullptr}));
#define Grid(Row, Col) PlantMap[Row - 1][Col - 1]

// 遍历一次全场植物，得到一份按格子存储的PlantMap
// 0-容器 1-南瓜 2-咖啡 3-常规 4-飞行窝瓜
void UpdatePlantMap() {
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
std::pair<int, int> GetDefenseRange(APlantType type) {
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
std::pair<int, int> GetAttackRange(AZombieType type) {
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
bool isRangeOverlap(APlantType plant_type, AZombieType zombie_type, int plant_x, int zombie_x) {
    auto plant_range = GetDefenseRange(plant_type);
    auto zombie_range = GetAttackRange(zombie_type);
    return plant_x + plant_range.second >= zombie_x + zombie_range.first && plant_x + plant_range.first <= zombie_x + zombie_range.second;
}

// 检查指定帧后是否有冰菇生效。应填写1~100的整数。没变身的模仿者不统计。
bool isCertainTickIce(int delayTime = 1) {
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
std::vector<GargInfo>& GetHammeringGargInfo(bool hammerDownSoon = false, int row = -1) {
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
std::vector<ZomboniInfo>& GetZomboniInfo(int row = -1) {
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
std::vector<CatapultInfo>& GetCatapultInfo(int row = -1) {
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
bool isPlantDisappearedImmediately(APlant* Plant) {
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
static bool SmartRemoveSwitch = false;
void SmartRemove() {
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
bool isSeedUsableOrHolding(APlantType Type) {
    if (Type >= 49) {
        return AIsSeedUsable(Type) || AGetMainObject()->MouseAttribution()->MRef<int>(0x2C) == Type - 49;
    } else
        return AIsSeedUsable(Type) || AGetMainObject()->MouseAttribution()->MRef<int>(0x28) == Type;
}

// 获取Type类型植物对小丑爆炸的判定范围
std::pair<int, int> GetExplodeRange(APlantType Type) {
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
std::pair<int, int> MyGridToCoordinate(double Row, double Col) {
    if (ARangeIn(AGetMainObject()->Scene(), {0, 1, 6, 7, 8, 9}))
        return {40 + (Col - 1) * 80, 80 + (Row - 1) * 100};
    else if (ARangeIn(AGetMainObject()->Scene(), {2, 3, 10, 11}))
        return {40 + (Col - 1) * 80, 80 + (Row - 1) * 85};
    return {40 + (Col - 1) * 80, 70 + (Row - 1) * 85 + (Col < 6 ? (6 - Col) * 20 : 0)}; // 天台
}

int MyColToX(double Col) { return 40 + (Col - 1) * 80; }
int MyRowToY(double Row, double Col) {
    if (ARangeIn(AGetMainObject()->Scene(), {0, 1, 6, 7, 8, 9}))
        return 80 + (Row - 1) * 100;
    else if (ARangeIn(AGetMainObject()->Scene(), {2, 3, 10, 11}))
        return 80 + (Row - 1) * 85;
    return 70 + (Row - 1) * 85 + (Col < 6 ? (6 - Col) * 20 : 0); // 天台
}

// 判断当前鼠标是否在场内
bool isMouseInField() {
    int X = AMRef<int>(0x6A9EC0, 0x768, 0x138, 0x8);
    int Y = AMRef<int>(0x6A9EC0, 0x768, 0x138, 0xC); // 鼠标坐标
    if (X < 0)
        return false;
    return 45 <= Y && Y <= 565; // 此处数据为手工测试得到
}

// 判断某小丑是否可炸到某植物
// 植僵双遍历的情境下使用
bool JudgeExplode(APlant* Plant, AZombie* Zombie) {
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
bool PredictExplode(AZombie* Zombie, int PlantRow, int PlantCol, APlantType PlantType) {
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
void SafeCard(APlantType PlantType, int Row, int Col, int NeedTime = 99) {
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
    Card(PlantType, Row, Col);
}

// 智能用卡
// 捏着卡片时按Shift将以SafeCard的形式放出该植物，可在水路和屋顶调用，会自动补充容器
// 默认樱桃、辣椒、夜间黑核、夜间蓝冰需存活至99cs后，其他植物需存活至1cs后
// 可同时作为天台车底炸快捷键，满足天台车底炸且当帧小丑倒计时不为1时会认为灰烬是安全的
void SmartAsh() {
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
static int JackWarning = -1;
void JackPause() {
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
float BalloonΔX(int Time, float Speed, int SlowCountdown = 0) {
    if (!SlowCountdown)
        return Speed * Time; // 原速 × 总时间
    if (SlowCountdown > Time)
        return 0.4 * Speed * Time; // 减速 × 总时间
    return 0.4 * Speed * (SlowCountdown - 1) + Speed * (Time - (SlowCountdown - 1));
    // 减速 × (减速倒计时 - 1) + 原速 × (总时间 - (减速倒计时 - 1))
}

// 气球字幕、暂停防呆计算
static bool BalloonWarning = false;
void BalloonCaption() {
    if (PausedCd < 480)
        PausedCd += AGetPvzBase()->TickMs();
    if (!BalloonWarning)
        return;
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == ABALLOON_ZOMBIE && isSeedUsableOrHolding(ABLOVER)) {
            if (int(Zombie.Abscissa() - BalloonΔX(49, Zombie.Speed(), Zombie.SlowCountdown())) <= -100) {
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
static int BalloonPauseCd = 200;
void BalloonPause() {
    if (!BalloonWarning)
        return;
    if (BalloonPauseCd < 200) // 两次气球警告至少间隔200cs
        ++BalloonPauseCd;
    for (auto& Zombie : aAliveZombieFilter) {
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
int RealCountdown() {
    if (AGetMainObject()->Wave() == 20)
        return AGetMainObject()->LevelEndCountdown();
    if (AGetMainObject()->RefreshCountdown() > 200)
        return 0;
    if (AGetMainObject()->Wave() == 9 || AGetMainObject()->Wave() == 19) {
        if (AGetMainObject()->RefreshCountdown() <= 5)
            return AGetMainObject()->HugeWaveCountdown();
        return AGetMainObject()->RefreshCountdown() + 745;
    }
    return AGetMainObject()->RefreshCountdown();
}

// 显示信息，-1 = 关闭，0 = 基础，1 = 进阶
// 这段逻辑最后更改的时间为20250313，确保血条覆盖顺序
static int ShowInfoState = -1;
void DrawInfo() {
    // if (AGetMainObject() == nullptr)
    //     return; // 防崩溃代码
    if (settings.ShowReplayInfo && aReplay.GetState() == AReplay::RECORDING) {
        barPainter.Draw(ABar(685, 3, 1, 0, {}, 1, ABar::RIGHT, 106, 24, 0xFFFFC000, 0xC0FFFFFF));
        fightInfoPainter.Draw(AText("Rec.", 689, 4), 0xFFFF0000);
        fightInfoPainter.Draw(AText(std::to_string(aReplay.GetRecordIdx()), 733, 4), 0xFFFF0000);
    }
    if (settings.ShowReplayInfo && aReplay.GetState() == AReplay::PLAYING) {
        barPainter.Draw(ABar(685, 3, 1, 0, {}, 1, ABar::RIGHT, 106, 24, 0xFFFFC000, 0xC0FFFFFF));
        fightInfoPainter.Draw(AText("Play", 689, 4), 0xFF0000FF);
        fightInfoPainter.Draw(AText(std::to_string(aReplay.GetPlayIdx() + 1), 733, 4), 0xFF0000FF);
    }
    if (ShowInfoState == -1)
        return;
    for (auto& Plant : aAlivePlantFilter) { // 显血
        if (Plant.Type() == ACOB_CANNON && AGetCobRecoverTime(Plant.Index()) && settings.CobCD)
            barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 17, 3475, 3475 - AGetCobRecoverTime(Plant.Index()), {350, 3350}, 1, ABar::RIGHT, 152, 7, 0xFFFFFF00, 0xA0FFFFFF));
        if (Plant.Hp() != Plant.HpMax()) {
            if (Plant.Type() == ACOB_CANNON) {
                if (settings.CobGloomHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 152, 11, 0xFF4CAF50, 0xA0FFFFFF));
            } else if (Plant.Type() == AGLOOM_SHROOM) {
                if (settings.CobGloomHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, 0xFF4CAF50, 0xA0FFFFFF));
            } else if (Plant.Type() == ACOFFEE_BEAN) {
                if (settings.OtherPlantHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 13, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, 0xFF965821, 0xA0FFFFFF));
            } else if (ARangeIn(Plant.Type(), {AWALL_NUT, ATALL_NUT, ASPIKEROCK})) {
                if (settings.NutSpikeHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {Plant.HpMax() / 3, Plant.HpMax() * 2 / 3}, 1, ABar::RIGHT, 72, 11, 0xFF4CAF50, 0xA0FFFFFF));
            } else if (Plant.Type() == APUMPKIN) {
                if (settings.PumpkinHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 45, Plant.HpMax(), Plant.Hp(), {Plant.HpMax() / 3, Plant.HpMax() * 2 / 3}, 1, ABar::RIGHT, 72, 11, 0xFFFFA500, 0xA0FFFFFF));
            } else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT})) {
                if (settings.LilyPotHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 57, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, 0xFF4CAF50, 0xA0FFFFFF));
            } else if (Plant.Type() == ASQUASH) {
                if (settings.OtherPlantHP)
                    barPainter.Draw(ABar(Plant.Xi() + 4, Plant.Yi() + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, 0xFF4CAF50, 0xA0FFFFFF));
            } else {
                if (settings.OtherPlantHP)
                    barPainter.Draw(ABar(MyColToX(Plant.Col() + 1) + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, Plant.HpMax(), Plant.Hp(), {}, 1, ABar::RIGHT, 72, 11, 0xFF4CAF50, 0xA0FFFFFF));
            }
        }
    }
    for (auto& Place : aAlivePlaceItemFilter) { // 核坑
        if (Place.Type() != 2 || !settings.Crater)
            continue;
        auto Coordinate = MyGridToCoordinate(Place.Row() + 1, Place.Col() + 1);
        int Abscissa = Coordinate.first, Ordinate = Coordinate.second;
        barPainter.Draw(ABar(Abscissa + 4, Ordinate + 62, 18000, Place.Value(), {}, 1, ABar::RIGHT, 72, 6, 0xFF965821, 0xA0FFFFFF, 0xFF000000, 0));
    }
    for (int Row = 1; Row <= 6; ++Row) { // 冰道
        if (LeftmostIceTrail(Row) > 9 || !settings.Icetrail)
            continue;
        auto Coordinate = MyGridToCoordinate(Row, LeftmostIceTrail(Row));
        int Abscissa = Coordinate.first, Ordinate = Coordinate.second;
        barPainter.Draw(ABar(Abscissa + 4, Ordinate + 50, 3000, IceTrailCoverCD(Row), {}, 1, ABar::RIGHT, 72, 6, 0xFF16F2EB, 0xA0FFFFFF, 0xFF000000, 0));
    }
    std::vector<int> FootballThisWave(6, 0);
    std::vector<int> ZomboniThisWave(6, 0);
    std::vector<int> FootballCount(6, 0);
    std::vector<int> ZomboniCount(6, 0);
    // 确保血条覆盖顺序，故多次遍历
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AFOOTBALL_ZOMBIE && Zombie.Hp() >= 90) { // 橄榄血条
            if (settings.FootballHP)
                barPainter.Draw(ABar(Zombie.Abscissa() + 81, Zombie.Ordinate() + 69, 1580, Zombie.OneHp() + Zombie.Hp() - 90, {180}, 1, ABar::UP, 36, 6, 0xFF6D706C, 0xA0FFFFFF));
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
                barPainter.Draw(ABar(Zombie.Abscissa() + 49, Zombie.Ordinate() + 59, 3000, Zombie.Hp(), settings.HPStyle ? std::initializer_list<int> {1200} : std::initializer_list<int> {1500, 1800}, 1, ABar::UP, 40, 8, 0xFF9868BC, 0xA0FFFFFF));
    }
    std::vector<int> GigaThisWave(6, 0);
    std::vector<int> GigaCount(6, 0);
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AGIGA_GARGANTUAR) { // 红眼血条
            if (settings.GigaHP)
                barPainter.Draw(ABar(Zombie.Abscissa() + 49, Zombie.Ordinate() + 79, 6000, Zombie.Hp(), settings.HPStyle ? std::initializer_list<int> {600, 2400, 4200} : std::initializer_list<int> {1800, 3000, 4800}, 1, ABar::UP, 80, 8, 0xFFFF0000, 0xA0FFFFFF));
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
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 25, 152, 11), 0x9AFF0000);
                else if (Plant.Type() == ACOFFEE_BEAN)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 13, 72, 11), 0x9AFF0000);
                else if (Plant.Type() == APUMPKIN)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 45, 72, 11), 0x9AFF0000);
                else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT}))
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 57, 72, 11), 0x9AFF0000);
                else if (Plant.Type() == ASQUASH)
                    backgroundPainter.Draw(ARect(Plant.Xi() + 4, Plant.Yi() + 25, 72, 11), 0x9AFF0000);
                else
                    backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, 72, 11), 0x9AFF0000);
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
            backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 53 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, 14, RectHeight), 0xFF4CAF50);
            lowIndexPainter.Draw(AText("R" + std::to_string(Plantoffset), MyColToX(Plant.Col() + 1) + 52 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 21), 0xFF000000);
        }
        if (Plantoffset < 0) {
            backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 5 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, 14, RectHeight), 0xFF4CAF50);
            lowIndexPainter.Draw(AText("L" + std::to_string(-Plantoffset), MyColToX(Plant.Col() + 1) + 4 + 4, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 21), 0xFF000000);
        }
    }
    // 有小丑开盒，绘制爆炸倒计时
    for (auto& Zombie : aAliveZombieFilter) {
        if (Zombie.Type() == AJACK_IN_THE_BOX_ZOMBIE && Zombie.State() == 16 && settings.JackCountdown)
            barPainter.Draw(ABar(Zombie.Abscissa() + 65, Zombie.Ordinate() + 87, 110, Zombie.StateCountdown(), {100}, 1, ABar::UP, 55, 10, 0xFFFF69B4, 0xA0FFFFFF));
    }
    for (int Row : {0, 1, 2, 3, 4, 5}) {
        if (AMRef<int>(0x6A9EC0, 0x768, 0x5D8 + Row * 0x4) != 1)
            continue;
        int Height = 16;
        auto Coordinate = MyGridToCoordinate(Row + 1, 0.5);
        int Ordinate = Coordinate.second;
        if (GigaCount[Row] && settings.GigaCount) { // 红眼实时统计绘制
            std::string Giga = std::to_string(GigaThisWave[Row]) + "/" + std::to_string(GigaCount[Row]);
            backgroundPainter.Draw(ARect(0, Ordinate + aFieldInfo.rowHeight / 2 + 3 - Height, Giga.size() * 9 + 2, 16), 0xFFFF0000);
            fightInfoPainter.Draw(AText(Giga, 0, Ordinate + aFieldInfo.rowHeight / 2 - Height), 0xFFFFFFFF);
        }
        if (ZomboniCount[Row] && settings.ZomboniCount) { // 冰车实时统计绘制
            std::string Zomboni = std::to_string(ZomboniThisWave[Row]) + "/" + std::to_string(ZomboniCount[Row]);
            backgroundPainter.Draw(ARect(0, Ordinate + aFieldInfo.rowHeight / 2 + 3, Zomboni.size() * 9 + 2, 16), 0xFF0040FF);
            fightInfoPainter.Draw(AText(Zomboni, 0, Ordinate + aFieldInfo.rowHeight / 2), 0xFFFFFFFF);
        }
        if (FootballCount[Row] && settings.FootballCount) { // 橄榄实时统计绘制
            std::string Football = std::to_string(FootballThisWave[Row]) + "/" + std::to_string(FootballCount[Row]);
            backgroundPainter.Draw(ARect(0, Ordinate + aFieldInfo.rowHeight / 2 + 3 + Height, Football.size() * 9 + 2, 16), 0xFF6D706C);
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
    uint32_t GigaARGB = 0xFFFFFFFF;
    uint32_t GigaBGARGB = 0xFFFF0000;
    if (GigaCumulativeDistribution[AGetMainObject()->Wave() - 1] >= 50) {
        GigaARGB = 0xFF0000FF;
        GigaBGARGB = 0xFF9868BC;
    }
    if (GigaCumulativeDistribution[20 - 1] && settings.GigaStat) { // 红眼出怪表统计绘制
        backgroundPainter.Draw(ARect(19, 7, 61, 53), GigaBGARGB);
        if (AGetMainObject()->Wave()) {
            std::string Wave = "Wave";
            std::string Sum = "Sum";
            if (GigaDistribution[AGetMainObject()->Wave() - 1] < 10)
                Wave = "Wave ";
            if (GigaCumulativeDistribution[AGetMainObject()->Wave() - 1] < 10)
                Sum = "Sum  ";
            else if (GigaCumulativeDistribution[AGetMainObject()->Wave() - 1] < 100)
                Sum = "Sum ";
            GigaNumPainter.Draw(AText(Wave + std::to_string(GigaDistribution[AGetMainObject()->Wave() - 1]), 22, ShowInfoState ? 6 : 11), GigaARGB);
            GigaNumPainter.Draw(AText(Sum + std::to_string(GigaCumulativeDistribution[AGetMainObject()->Wave() - 1]), 22, ShowInfoState ? 22 : 33), GigaARGB);
        } else {
            GigaNumPainter.Draw(AText("Wave 0", 22, ShowInfoState ? 6 : 11), GigaARGB);
            GigaNumPainter.Draw(AText("Sum  0", 22, ShowInfoState ? 22 : 33), GigaARGB);
        }
        if (ShowInfoState) {
            std::string All = "All";
            if (GigaCumulativeDistribution[20 - 1] < 10)
                All = "All  ";
            else if (GigaCumulativeDistribution[20 - 1] < 100)
                All = "All ";
            GigaNumPainter.Draw(AText(All + std::to_string(GigaCumulativeDistribution[20 - 1]), 22, 38), GigaARGB);
        }
    }
    // 本波总血条
    if (settings.TotalHP) {
        if (AGetMainObject()->Wave() < 1)
            barPainter.Draw(ABar(58, 574, 1, 0, {}, 1, ABar::RIGHT, 127, 24, 0xFF9868BC, 0xC0FFFFFF));
        else if (ARangeIn(AGetMainObject()->Wave(), {9, 19, 20}) || ShowInfoState)
            barPainter.Draw(ABar(58, 574, AGetMainObject()->MRef<int>(0x5598), AAsm::ZombieTotalHp(ANowWave() - 1), {AGetMainObject()->ZombieRefreshHp()}, 1, ABar::RIGHT, 127, 24, RealCountdown() ? 0xFF6D706C : 0xFF9868BC, 0xC0FFFFFF));
        else
            barPainter.Draw(ABar(58, 574, AGetMainObject()->MRef<int>(0x5598), AAsm::ZombieTotalHp(ANowWave() - 1), {AGetMainObject()->MRef<int>(0x5598) * 13 / 20, AGetMainObject()->MRef<int>(0x5598) / 2}, 1, ABar::RIGHT, 127, 24, 0xFF9868BC, 0xC0FFFFFF));
        // 波数时间
        fightInfoPainter.Draw(AText((AGetMainObject()->Wave() < 10 ? "0" : "") + std::to_string(AGetMainObject()->Wave() ?: 1) + ",", 59, 575), 0xFF0000FF);
        fightInfoPainter.Draw(AText(std::to_string(ANowTime(ANowWave()) == -2147483648 ? (AGetMainObject()->CompletedRounds() ? -600 : -1800) : ANowTime(ANowWave())), 82, 575), 0xFF0000FF);
        fightInfoPainter.Draw(AText(RealCountdown() && (ARangeIn(AGetMainObject()->Wave(), {9, 19, 20}) || ShowInfoState) ? std::to_string(-RealCountdown()) : "", 145, 575), 0xFFFF0000);
    }
    // 波长记录
    for (int i = 0; i < settings.WavelengthRecord; ++i) {
        if (AGetMainObject()->Wave() - i > 0 && ANowTime(ANowWave() - i) > 0 && RealCountdown() && (ARangeIn(AGetMainObject()->Wave(), {9, 19, 20}) || ShowInfoState)) {
            barPainter.Draw(ABar(191 + 71 * i, 574, 1, 0, {}, 1, ABar::RIGHT, 65, 24, 0xFFFFC000, 0xC0FFFFFF));
            fightInfoPainter.Draw(AText((AGetMainObject()->Wave() - i < 10 ? "0" : "") + std::to_string(AGetMainObject()->Wave() - i ?: 1) + ",", 193 + 71 * i, 575), 0xFF0000FF);
            fightInfoPainter.Draw(AText(std::to_string(i ? ANowTime(ANowWave() - i) - ANowTime(ANowWave() + 1 - i) : ANowTime(ANowWave()) + RealCountdown()), 216 + 71 * i, 575), 0xFF0000FF);
        } else if (AGetMainObject()->Wave() - i > 1 && ANowTime(ANowWave() - 1 - i) > 0) {
            barPainter.Draw(ABar(191 + 71 * i, 574, 1, 0, {}, 1, ABar::RIGHT, 65, 24, 0xFFFFC000, 0xC0FFFFFF));
            fightInfoPainter.Draw(AText((AGetMainObject()->Wave() - 1 - i < 10 ? "0" : "") + std::to_string(AGetMainObject()->Wave() - 1 - i ?: 1) + ",", 193 + 71 * i, 575), 0xFF0000FF);
            fightInfoPainter.Draw(AText(std::to_string(ANowTime(ANowWave() - 1 - i) - ANowTime(ANowWave() - i)), 216 + 71 * i, 575), 0xFF0000FF);
        }
    }
    // 显示倍速
    if (AGetPvzBase()->TickMs() != 10 && settings.ShowSpeed) {
        barPainter.Draw(ABar(6, 574, 1, 1, {}, 1, ABar::RIGHT, 46, 24, AGetPvzBase()->TickMs() > 10 ? 0xFFFF0000 : 0xFF00FF00));
        fightInfoPainter.Draw(AText(std::to_string(10 / AGetPvzBase()->TickMs()), 8, 575), 0xFF000000);
        if (AGetPvzBase()->TickMs() == 1) {
            fightInfoPainter.Draw(AText(".", 26, 575), 0xFF000000);
            fightInfoPainter.Draw(AText("0", 30, 575), 0xFF000000);
        } else {
            fightInfoPainter.Draw(AText(".", 17, 575), 0xFF000000);
            fightInfoPainter.Draw(AText(1000 / AGetPvzBase()->TickMs() % 100 ? std::to_string(1000 / AGetPvzBase()->TickMs() % 100) : "00", 21, 575), 0xFF000000);
        }
        fightInfoPainter.Draw(AText("x", 39, 575), 0xFF000000);
    }
}

// 显示栈位，-1 = 关闭，0 = 前场，1 = 全部
// 这段逻辑最后更改的时间为20241221，修正了玉米炮对显栈区左扩的影响
static int ShowIndexState = -1;
static std::vector<int> LeftmostVisibleArea(6, 10);
void DrawIndex() {
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
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 110 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFF4CAF50, settings.CobGloomHP ? RectHeight : 11);
            else if (Plant.Type() == AGLOOM_SHROOM)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFF4CAF50, settings.CobGloomHP ? RectHeight : 11);
            else if (Plant.Type() == ACOFFEE_BEAN)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 14 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFFFFFFFF, 0xFF965821, settings.OtherPlantHP ? RectHeight : 11);
            else if (ARangeIn(Plant.Type(), {AWALL_NUT, ATALL_NUT, ASPIKEROCK}))
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFF4CAF50, settings.NutSpikeHP ? RectHeight : 11);
            else if (Plant.Type() == APUMPKIN)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 54 + DigitOffset + SizeOffset, Plant.Yi() + 46 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFFFFA500, settings.PumpkinHP ? RectHeight : 11);
            else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT}))
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 6 + DigitOffset + SizeOffset, Plant.Yi() + 58 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFF4CAF50, settings.LilyPotHP ? RectHeight : 11);
            else if (Plant.Type() == ASQUASH)
                SegPainter.Draw(A7Seg(Plant.Index(), Plant.Xi() + 30 + DigitOffset + SizeOffset, Plant.Yi() + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFF4CAF50, settings.OtherPlantHP ? RectHeight : 11);
            else
                SegPainter.Draw(A7Seg(Plant.Index(), MyColToX(Plant.Col() + 1) + 30 + DigitOffset + SizeOffset, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 26 + SizeOffset, FontSize, 1, 2), SizeOffset + 1, 0xFF0040FF, 0xFF4CAF50, settings.OtherPlantHP ? RectHeight : 11);
        } else { // 可栈位垫，9像素黑色Arial
            if (Plant.Type() == ACOB_CANNON) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 109 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.CobGloomHP ? RectHeight : 11), 0xFF4CAF50);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 108 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else if (Plant.Type() == AGLOOM_SHROOM) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.CobGloomHP ? RectHeight : 11), 0xFF4CAF50);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else if (Plant.Type() == ACOFFEE_BEAN) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 13, RectWidth, settings.OtherPlantHP ? RectHeight : 11), 0xFF965821);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 9), 0xFFFFFFFF);
            } else if (ARangeIn(Plant.Type(), {AWALL_NUT, ATALL_NUT, ASPIKEROCK})) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.NutSpikeHP ? RectHeight : 11), 0xFF4CAF50);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else if (Plant.Type() == APUMPKIN) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 53 + DigitOffset, Plant.Yi() + 45, RectWidth, settings.PumpkinHP ? RectHeight : 11), 0xFFFFA500);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 52 + DigitOffset, Plant.Yi() + 41), 0xFF000000);
            } else if (ARangeIn(Plant.Type(), {ALILY_PAD, AFLOWER_POT})) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 5 + DigitOffset, Plant.Yi() + 57, RectWidth, settings.LilyPotHP ? RectHeight : 11), 0xFF4CAF50);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 4 + DigitOffset, Plant.Yi() + 53), 0xFF000000);
            } else if (Plant.Type() == ASQUASH) {
                backgroundPainter.Draw(ARect(Plant.Xi() + 29 + DigitOffset, Plant.Yi() + 25, RectWidth, settings.OtherPlantHP ? RectHeight : 11), 0xFF4CAF50);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), Plant.Xi() + 28 + DigitOffset, Plant.Yi() + 21), 0xFF000000);
            } else {
                backgroundPainter.Draw(ARect(MyColToX(Plant.Col() + 1) + 29 + DigitOffset, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 25, RectWidth, settings.OtherPlantHP ? RectHeight : 11), 0xFF4CAF50);
                lowIndexPainter.Draw(AText(std::to_string(Plant.Index()), MyColToX(Plant.Col() + 1) + 28 + DigitOffset, MyRowToY(Plant.Row() + 1, Plant.Col() + 1) + 21), 0xFF000000);
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
        nextIndexPainter.Draw(AText(std::to_string(AGetMainObject()->PlantNext()), 634, 2), 0xFF000000);
    } else if (AGetMainObject()->PlantNext() < 100) {
        backgroundPainter.Draw(ARect(626, 8, 35, 26), 0xFF4CAF50);
        nextIndexPainter.Draw(AText(std::to_string(AGetMainObject()->PlantNext()), 626, 2), 0xFF000000);
    } else {
        backgroundPainter.Draw(ARect(618, 8, 51, 26), 0xFF4CAF50);
        nextIndexPainter.Draw(AText(std::to_string(AGetMainObject()->PlantNext()), 618, 2), 0xFF000000);
    }
}

// 主体函数
static uint32_t MaidPhase = AMaidCheats::MC_STOP;
static bool UISwitch = true;
static bool ATASSwitch = false;

// 10倍速
void Speed10x() { AGetPvzBase()->TickMs() = AGetPvzBase()->TickMs() == 1 ? 10 : 1; }
// 跳到某波
void SkiptoWave() { ASkipTick(settings.SkipTickWave, 0); }
// 退出重进
void Restart() {
    ABackToMain();
    AEnterGame(AMRef<int>(0x6A9EC0, 0x7F8));
}
// 回档几帧
void func8() {
    if (SnapshotModeSwitch)
        return;
    Paused = true;
    PausedSlowed = false;
    ASetAdvancedPause(Paused, false, 0);
    aReplay.ShowOneTick(aReplay.GetEndIdx() - settings.tickRewindCount >= 0 ? aReplay.GetRecordIdx() - settings.tickRewindCount : aReplay.GetStartIdx());
}
// 高级暂停
void func9() {
    if (SnapshotModeSwitch)
        return;
    if (PausedCd < 480)
        return;
    Paused = !Paused;
    PausedSlowed = false;
    ASetAdvancedPause(Paused, false, 0);
}
// 下一帧
void func10() {
    if (SnapshotModeSwitch)
        return;
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

// 卡槽置顶
void func13() { AMRef<int>(0x416DBE) = AMRef<int>(0x416DBE) == 699999 ? 100001 : 699999; }
// 显示信息
void func14() {
    ShowInfoState = ShowInfoState == 1 ? -1 : ++ShowInfoState;
    CreateCaption(!ShowInfoState ? "ShowInfo: Basic"
            : ShowInfoState == 1 ? "ShowInfo: Advanced"
                                 : "ShowInfo: Off");
}
// 显示栈位
void func15() {
    ShowIndexState = ShowIndexState == 1 ? -1 : ++ShowIndexState;
    LeftmostVisibleArea.assign(6, 10);
}
// 智能铲除
void func16() {
    SmartRemoveSwitch = !SmartRemoveSwitch;
    CreateCaption(SmartRemoveSwitch ? "SmartRemove: On" : "SmartRemove: Off");
}
// Dance秘籍
void func17() {
    DCState = DCState == 0 ? -1 : 0;
    SetDance(false);
    CreateCaption(DCState == 0 ? "DanceCheat: Fast" : "DanceCheat: Off");
}
void func18() {
    DCState = DCState == 1 ? -1 : 1;
    SetDance(false);
    CreateCaption(DCState == 1 ? "DanceCheat: Slow" : "DanceCheat: Off");
}
// 女仆秘籍
void func19() {
    MaidPhase = AMaidCheats::MC_CALL_PARTNER;
    AMaidCheats::Phase() = MaidPhase;
    CreateCaption("Maid: Summon");
}
void func20() {
    MaidPhase = AMaidCheats::MC_DANCING;
    AMaidCheats::Phase() = MaidPhase;
    CreateCaption("Maid: Dance");
}
void func21() {
    MaidPhase = AMaidCheats::MC_MOVE;
    AMaidCheats::Phase() = MaidPhase;
    CreateCaption("Maid: Forward");
}
void func22() {
    MaidPhase = AMaidCheats::MC_STOP;
    AMaidCheats::Phase() = MaidPhase;
    CreateCaption("Maid: End");
}
// 自动收集
void func23() {
    *(uint8_t*)0x0043158F = *(uint8_t*)0x0043158F == 0xEB ? 0x75 : 0xEB;
}
// 小丑拦截
void func24() {
    JackWarning = JackWarning == 1 ? -1 : ++JackWarning;
    CreateCaption(!JackWarning ? "JackWarning: OnlyWhenHit"
            : JackWarning == 1 ? "JackWarning: All"
                               : "JackWarning: Off");
}
// 气球拦截
void func25() {
    BalloonWarning = !BalloonWarning;
    CreateCaption(BalloonWarning ? "BalloonWarning: On" : "BalloonWarning: Off");
}
// 风炮修复
void func26() {
    *(uint8_t*)0x46DCE3 == 0x83 ? *(std::array<uint8_t, 10>*)0x46DCE3 = {0x75, 0x08, 0xD9, 0x46, 0x34, 0xD8, 0xC1, 0xD9, 0x5E, 0x34} : *(std::array<uint8_t, 10>*)0x46dce3 = {0x83, 0x7E, 0x5C, 0x0B, 0x75, 0x04, 0xDD, 0xD8, 0xEB, 0x1B};
    CreateCaption(*(uint8_t*)0x46DCE3 == 0x83 ? "FixWind: On" : "FixWind: Off");
}
// 隐藏UI
void func27() {
    UISwitch = !UISwitch;
    AMRef<bool>(0x6A9EC0, 0x768, 0x144, 0x18) = AMRef<bool>(0x6A9EC0, 0x768, 0x55F1) = UISwitch;
    AMRef<int>(0x6A9EC0, 0x768, 0x55F4) = AMRef<bool>(0x6A9EC0, 0x768, 0x148, 0xF9) = !UISwitch;
}
// 点击跳过动画、快速随机选卡
void func28() { AMRef<bool>(0x6A9EC0, 0x7F5) = AMRef<bool>(0x6A9EC0, 0x7F5) ? false : true; }

// 拍照模式
void func30() {
    SnapshotModeSwitch = !SnapshotModeSwitch;
    CreateCaption(SnapshotModeSwitch ? "SnapshotMode: On" : "SnapshotMode: Off");
    ASetAdvancedPause(SnapshotModeSwitch, false, 0);
    AMRef<int>(0x6A9EC0, 0x768, 0x30) = 0;
    AMRef<int>(0x6A9EC0, 0x768, 0x34) = 0;
    if (Paused)
        ASetAdvancedPause(Paused, false, 0);
}
// 调整视角
void func31() { AMRef<int>(0x6A9EC0, 0x768, 0x34) -= 5; }
void func32() { AMRef<int>(0x6A9EC0, 0x768, 0x34) += 5; }
void func33() { AMRef<int>(0x6A9EC0, 0x768, 0x30) -= 5; }
void func34() { AMRef<int>(0x6A9EC0, 0x768, 0x30) += 5; }
// 辅助开关
void OneKeySwitch() {
    ATASSwitch = !ATASSwitch;
    if (ATASSwitch) {
        CreateCaption("A-TAS: On", {BOTTOMFAST});
        *(uint8_t*)0x0043158F = 0xEB;
        ShowInfoState = 0;
        ShowIndexState = 0;
        LeftmostVisibleArea.assign(6, 10);
        SmartRemoveSwitch = true;
        BalloonWarning = true;
    } else {
        CreateCaption("A-TAS: Off");
        *(uint8_t*)0x0043158F = 0x75;
        ShowInfoState = -1;
        ShowIndexState = -1;
        LeftmostVisibleArea.assign(6, 10);
        SmartRemoveSwitch = false;
        BalloonWarning = false;
    }
}

// 初始化
void ResetGame() {
    CreateCaption("Reset");
    ASetAdvancedPause(false, false, 0);
    Paused = false;
    PausedSlowed = false;
    SnapshotModeSwitch = false;
    AMRef<int>(0x6A9EC0, 0x768, 0x30) = 0;
    AMRef<int>(0x6A9EC0, 0x768, 0x34) = 0;
    AGetPvzBase()->TickMs() = 10;
    AMRef<bool>(0x6A9EAB) = false;
    DCState = -1;
    SetDance(false);
    MaidPhase = AMaidCheats::MC_STOP;
    AMaidCheats::Phase() = MaidPhase;
    *(std::array<uint8_t, 10>*)0x46DCE3 = {0x75, 0x08, 0xD9, 0x46, 0x34, 0xD8, 0xC1, 0xD9, 0x5E, 0x34};
    SetMusic(AGetMainObject()->Scene() + 1);
    AMRef<int>(0x416DBE) = 100001;
    UISwitch = true;
    AMRef<bool>(0x6A9EC0, 0x768, 0x144, 0x18) = AMRef<bool>(0x6A9EC0, 0x768, 0x55F1) = true;
    AMRef<int>(0x6A9EC0, 0x768, 0x55F4) = AMRef<bool>(0x6A9EC0, 0x768, 0x148, 0xF9) = false;
    ATASSwitch = false;
    *(uint8_t*)0x0043158F = 0x75;
    ShowInfoState = -1;
    ShowIndexState = -1;
    LeftmostVisibleArea.assign(6, 10);
    SmartRemoveSwitch = false;
    JackWarning = -1;
    BalloonWarning = false;
}

// UI
std::array<AConnectHandle, 33> keyHandles;
std::array<AOperation, 33> funcs = {OneKeySwitch, Decelerate, Accelerate, ResetSpeed, Speed10x, SkiptoWave, Restart, func8, func9, func10, SmartAsh, func13, func14, func15, func16, func17, func18, func19, func20, func21, func22, func23, func24, func25, func26, func27, func28, func30, func31, func32, func33, func34, ResetGame};
std::array<APushButton*, 33> keyButtons;

ALabel* infoLabel = nullptr;
void Info(const std::string& tip) {
    if (infoLabel != nullptr) {
        infoLabel->SetText(tip);
    }
}

void Warning(const std::string& tip) {
    Info(tip);
    MessageBeep(MB_ICONWARNING);
}

#define FightUiCheck()                            \
    if (AGetPvzBase()->GameUi() != 3) {           \
        Warning("只有在战斗界面才能使用此功能!"); \
        return;                                   \
    }
#define FightOrCardUiCheck()                                            \
    if (AGetPvzBase()->GameUi() != 3 && AGetPvzBase()->GameUi() != 2) { \
        Warning("只有在战斗界面或者选卡界面才能使用此功能!");           \
        return;                                                         \
    }

#define __CheckASCII(path, info, ret) \
    for (auto c : path) {             \
        if (uint8_t(c) > 127) {       \
            info;                     \
            return ret;               \
        }                             \
    }

// 只读功能
void Lock(int userId, int gameID, int state) {
    if (state == -1)
        return;
    auto Dataname = "game" + std::to_string(userId) + "_" + std::to_string(gameID) + ".dat";
    auto filePath = std::filesystem::path(GAME_DATA_PATH + Dataname);
    if (!std::filesystem::exists(filePath)) {
        Info("文件" + filePath.string() + "不存在");
        return;
    }
    try {
        auto perms = std::filesystem::status(filePath).permissions();
        if (state) {
            perms &= ~std::filesystem::perms::owner_write;
            perms &= ~std::filesystem::perms::group_write;
            perms &= ~std::filesystem::perms::others_write;
        } else {
            perms |= ~std::filesystem::perms::owner_write;
            perms |= ~std::filesystem::perms::group_write;
            perms |= ~std::filesystem::perms::others_write;
        }
        std::filesystem::permissions(filePath, perms);
        Info("已将" + filePath.string() + "的状态设置为" + (state ? "只读" : "不只读"));
    } catch (const std::filesystem::filesystem_error& e) {
        MessageBoxW(NULL, AStrToWstr("错误：" + std::string(e.what())).c_str(), L"A-TAS 设置文件属性错误", MB_OK);
    }
}

std::string GetCurTimeStr() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d-%H-%M-%S");
    return ss.str();
}

std::string OpenFileDialog(const std::string& initPath) {
    // 动态加载 comdlg32.dll
    HMODULE hComDlg = LoadLibraryW(L"comdlg32.dll");
    if (!hComDlg)
        return "";

    // 定义 GetOpenFileNameW 的函数指针类型并获取函数地址
    typedef BOOL(WINAPI * PFN_GetOpenFileNameW)(LPOPENFILENAMEW);
    PFN_GetOpenFileNameW pGetOpenFileNameW = (PFN_GetOpenFileNameW)GetProcAddress(hComDlg, "GetOpenFileNameW");
    if (!pGetOpenFileNameW) {
        FreeLibrary(hComDlg);
        return "";
    }

    wchar_t szFileName[MAX_PATH] = {};
    auto initDir = AStrToWstr(initPath);

    OPENFILENAMEW openFileName = {};
    openFileName.lStructSize = sizeof(OPENFILENAMEW);
    openFileName.nMaxFile = MAX_PATH; // 这个必须设置，否则不会显示打开文件对话框
    openFileName.lpstrFilter = L"回放文件 (*.7z)\0\0";
    openFileName.lpstrFile = szFileName;
    openFileName.nFilterIndex = 1;
    openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    openFileName.lpstrInitialDir = initDir.c_str();

    std::string result;
    if (pGetOpenFileNameW(&openFileName)) {
        result = AWStrToStr(openFileName.lpstrFile);
    }

    // 卸载 DLL
    FreeLibrary(hComDlg);
    return result;
}

constexpr static int MAIN_WIDTH = 640;
constexpr static int MAIN_HEIGHT = 480;

AMainWindow mainWindow("A-TAS 4.0 " + std::to_string(A_TAS_VERSION), MAIN_WIDTH, MAIN_HEIGHT);

// 回放函数
struct {
    AOperation startPlayOp = [] {
        FightUiCheck();
        if (aReplay.GetState() == AReplay::PLAYING) {
            Warning("正在播放");
            return;
        }
        if (settings.AutoStop)
            aReplay.Stop();
        auto fileName = OpenFileDialog(settings.savePath);
        if (fileName.empty())
            return;
        compressor->SetFilePath(fileName);
        aReplay.StartPlay();
        Info("Replay : 开始播放");
    };
    AOperation startRecordOp = [] {
        FightUiCheck();
        if (aReplay.GetState() == AReplay::RECORDING) {
            Warning("正在录制");
            return;
        }
        if (settings.AutoStop)
            aReplay.Stop();
        compressor->SetFilePath(settings.savePath + std::string("/") + GetCurTimeStr() + ".7z");
        aReplay.StartRecord(std::round(settings.recordTickInterval));
        Info("Replay : 开始录制");
    };
    AOperation pauseOp = [] {
        FightUiCheck();
        if (aReplay.IsPaused())
            aReplay.GoOn();
        else
            aReplay.Pause();
    };
    AOperation stopOp = [] {
        aReplay.Stop();
    };
} replayOp;

void CreateReplayGroup(AWindow* window, int LeftEdge, int TopEdge) {
    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;

    int y = TopEdge;
    int x = LeftEdge;
    window->AddLabel("", x, y, 4 * (SPACE + WIDTH) + SPACE, (SPACE + HEIGHT) * 6);

    x += SPACE;
    window->AddLabel("回放:    必须录制才能回档", x, y, WIDTH * 2 + SPACE, HEIGHT);
    x += (SPACE + WIDTH) * 2;
    auto autoRecordBox = window->AddCheckBox("游戏开始时自动录制", x, y, WIDTH * 2, HEIGHT);
    autoRecordBox->SetCheck(settings.AutoRecordOnGameStart);

    autoRecordBox->Connect([=] { settings.AutoRecordOnGameStart = autoRecordBox->GetCheck(); });

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    auto startPlayBtn = window->AddPushButton("📂选择回放", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto startRecordBtn = window->AddPushButton("⏺️开始录制", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto pauseBtn = window->AddPushButton("⏯️暂停/继续", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto stopBtn = window->AddPushButton("⏹️停止", x, y, WIDTH, HEIGHT);

    startPlayBtn->Connect(replayOp.startPlayOp);
    startRecordBtn->Connect(replayOp.startRecordOp);
    pauseBtn->Connect(replayOp.pauseOp);
    stopBtn->Connect(replayOp.stopOp);

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    auto showMouseBox = window->AddCheckBox("显示鼠标", x, y, WIDTH, HEIGHT);
    showMouseBox->SetCheck(settings.ShowMouse);
    x += SPACE + WIDTH;
    auto autoStopBox = window->AddCheckBox("切换时停止", x, y, WIDTH, HEIGHT);
    autoStopBox->SetCheck(settings.AutoStop);
    x += SPACE + WIDTH;
    auto interpolateBox = window->AddCheckBox("播放时补帧", x, y, WIDTH, HEIGHT);
    interpolateBox->SetCheck(settings.Interpolate);
    x += SPACE + WIDTH;
    auto tipBox = window->AddCheckBox("回放信息栏", x, y, WIDTH, HEIGHT);
    tipBox->SetCheck(settings.ShowReplayInfo);

    showMouseBox->Connect([=] {
        settings.ShowMouse = showMouseBox->GetCheck();
        aReplay.SetMouseVisible(settings.ShowMouse);
    });
    autoStopBox->Connect([=] {
        settings.AutoStop = autoStopBox->GetCheck();
    });
    interpolateBox->Connect([=] {
        settings.Interpolate = interpolateBox->GetCheck();
        aReplay.SetInterpolate(settings.Interpolate);
    });
    tipBox->Connect([=] {
        settings.ShowReplayInfo = tipBox->GetCheck();
    });

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    window->AddLabel("存档精度(每隔多少cs存一次档):", x, y, WIDTH * 2 + SPACE, HEIGHT);
    x += (SPACE + WIDTH) * 2;
    auto tickIntervalEdit = window->AddEdit(std::to_string(settings.recordTickInterval), x, y, WIDTH, HEIGHT, ES_NUMBER | ES_CENTER);
    x += SPACE + WIDTH;
    auto tickIntervalBtn = window->AddPushButton("设置", x, y, WIDTH, HEIGHT);
    tickIntervalBtn->Connect([=] {
        auto tickInterval = stoi(tickIntervalEdit->GetText());
        if (aReplay.GetState() != aReplay.REST) {
            Warning("回放正在工作，无法设置精度");
            return;
        };
        if (tickInterval < 1) {
            Warning("精度最小为 1 帧");
            tickInterval = 1;
        }
        settings.recordTickInterval = tickInterval;
        Info("设置存档精度成功");
    });

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    window->AddLabel("回档个数(每按一次要回几个档):", x, y, WIDTH * 2 + SPACE, HEIGHT);
    x += (SPACE + WIDTH) * 2;
    auto tickRewindCountEdit = window->AddEdit(std::to_string(settings.tickRewindCount), x, y, WIDTH, HEIGHT, ES_NUMBER | ES_CENTER);
    x += SPACE + WIDTH;
    auto tickRewindCountBtn = window->AddPushButton("设置", x, y, WIDTH, HEIGHT);
    tickRewindCountBtn->Connect([=] {
        auto tickRewindCount = stoi(tickRewindCountEdit->GetText());
        if (tickRewindCount < 1) {
            Warning("个数最小为 1 个");
            tickRewindCount = 1;
        }
        settings.tickRewindCount = tickRewindCount;
        Info("设置回档个数成功");
    });

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    window->AddLabel("保存路径:", x, y, WIDTH * 2 + SPACE, HEIGHT);
    x += SPACE + WIDTH;
    auto savePathEdit = window->AddEdit(settings.savePath, x, y, WIDTH * 2 + SPACE, HEIGHT, ES_AUTOHSCROLL);
    savePathEdit->SetText(settings.savePath);
    x += (SPACE + WIDTH) * 2;
    auto savePathBtn = window->AddPushButton("设置", x, y, WIDTH, HEIGHT);
    savePathBtn->Connect([=] {
        auto path = savePathEdit->GetText();
        __CheckASCII(path, Warning("您设置的保存路径: [" + path + "] 中含有非 ASCII 字符, 请将其设置为纯英文路径再次尝试");
                     savePathEdit->SetText(settings.savePath), );
        if (!std::filesystem::exists(path)) {
            Warning("设置的路径: [" + path + "] 不存在");
            savePathEdit->SetText(settings.savePath);
        } else {
            std::strcpy(settings.savePath, path.c_str());
            Info("设置路径: [" + path + "] 成功");
        }
    });
}

void CreateShowInfoGroup(AWindow* window, int LeftEdge, int TopEdge) {
    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;
    constexpr static int BTNWIDTH = 75;

    int y = TopEdge;
    int x = LeftEdge;
    window->AddLabel("", x, y, 194, (SPACE + HEIGHT) * 12);

    x += SPACE;
    window->AddLabel("显示信息", x, y, BTNWIDTH, HEIGHT);

    y += SPACE + HEIGHT;

    auto CobCDBox = window->AddCheckBox("炮冷却条", x, y, BTNWIDTH, HEIGHT);
    CobCDBox->SetCheck(settings.CobCD);
    CobCDBox->Connect([=] { settings.CobCD = CobCDBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto CobGloomHPBox = window->AddCheckBox("炮曾血条", x, y, BTNWIDTH, HEIGHT);
    CobGloomHPBox->SetCheck(settings.CobGloomHP);
    CobGloomHPBox->Connect([=] { settings.CobGloomHP = CobGloomHPBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto PumpkinHPBox = window->AddCheckBox("南瓜血条", x, y, BTNWIDTH, HEIGHT);
    PumpkinHPBox->SetCheck(settings.PumpkinHP);
    PumpkinHPBox->Connect([=] { settings.PumpkinHP = PumpkinHPBox->GetCheck(); });

    y += SPACE + HEIGHT;

    auto OtherPlantHPBox = window->AddCheckBox("其他植物血条", x, y, 110, HEIGHT);
    OtherPlantHPBox->SetCheck(settings.OtherPlantHP);
    OtherPlantHPBox->Connect([=] { settings.OtherPlantHP = OtherPlantHPBox->GetCheck(); });

    y += SPACE + HEIGHT;

    auto CraterBox = window->AddCheckBox("核坑冷却", x, y, BTNWIDTH, HEIGHT);
    CraterBox->SetCheck(settings.Crater);
    CraterBox->Connect([=] { settings.Crater = CraterBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto HPStyleBox = window->AddCheckBox("炮阵样式", x, y, BTNWIDTH, HEIGHT);
    HPStyleBox->SetCheck(settings.HPStyle);
    HPStyleBox->Connect([=] { settings.HPStyle = HPStyleBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto GigaHPBox = window->AddCheckBox("红眼血条", x, y, BTNWIDTH, HEIGHT);
    GigaHPBox->SetCheck(settings.GigaHP);
    GigaHPBox->Connect([=] { settings.GigaHP = GigaHPBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto GargHPBox = window->AddCheckBox("白眼血条", x, y, BTNWIDTH, HEIGHT);
    GargHPBox->SetCheck(settings.GargHP);
    GargHPBox->Connect([=] { settings.GargHP = GargHPBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto FootballHPBox = window->AddCheckBox("橄榄血条", x, y, BTNWIDTH, HEIGHT);
    FootballHPBox->SetCheck(settings.FootballHP);
    FootballHPBox->Connect([=] { settings.FootballHP = FootballHPBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto JackCountdownBox = window->AddCheckBox("小丑炸条", x, y, BTNWIDTH, HEIGHT);
    JackCountdownBox->SetCheck(settings.JackCountdown);
    JackCountdownBox->Connect([=] { settings.JackCountdown = JackCountdownBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto TotalHPBox = window->AddCheckBox("本波总血条", x, y, 100, HEIGHT);
    TotalHPBox->SetCheck(settings.TotalHP);
    TotalHPBox->Connect([=] { settings.TotalHP = TotalHPBox->GetCheck(); });

    // 下一列
    x = LeftEdge + SPACE * 2 + WIDTH;
    y = TopEdge;

    auto ShowMeBox = window->AddCheckBox("悬停显示", x, y, BTNWIDTH, HEIGHT);
    ShowMeBox->SetCheck(settings.ShowMe);
    ShowMeBox->Connect([=] {
        settings.ShowMe = ShowMeBox->GetCheck();
        if (settings.ShowMe)
            tickShowMe.Start();
        else
            tickShowMe.Stop();
    });

    y += SPACE + HEIGHT;
    auto PlantOffsetBox = window->AddCheckBox("小喷偏移", x, y, BTNWIDTH, HEIGHT);
    PlantOffsetBox->SetCheck(settings.PlantOffset);
    PlantOffsetBox->Connect([=] { settings.PlantOffset = PlantOffsetBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto LilyPotHPBox = window->AddCheckBox("荷盆血条", x, y, BTNWIDTH, HEIGHT);
    LilyPotHPBox->SetCheck(settings.LilyPotHP);
    LilyPotHPBox->Connect([=] { settings.LilyPotHP = LilyPotHPBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto NutSpikeHPBox = window->AddCheckBox("坚刺血条", x, y, BTNWIDTH, HEIGHT);
    NutSpikeHPBox->SetCheck(settings.NutSpikeHP);
    NutSpikeHPBox->Connect([=] { settings.NutSpikeHP = NutSpikeHPBox->GetCheck(); });

    y += SPACE + HEIGHT;

    y += SPACE + HEIGHT;
    auto IcetrailBox = window->AddCheckBox("冰道冷却", x, y, BTNWIDTH, HEIGHT);
    IcetrailBox->SetCheck(settings.Icetrail);
    IcetrailBox->Connect([=] { settings.Icetrail = IcetrailBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto GigaStatBox = window->AddCheckBox("红眼统计", x, y, BTNWIDTH, HEIGHT);
    GigaStatBox->SetCheck(settings.GigaStat);
    GigaStatBox->Connect([=] { settings.GigaStat = GigaStatBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto GigaCountBox = window->AddCheckBox("红眼数量", x, y, BTNWIDTH, HEIGHT);
    GigaCountBox->SetCheck(settings.GigaCount);
    GigaCountBox->Connect([=] { settings.GigaCount = GigaCountBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto ZomboniCountBox = window->AddCheckBox("冰车数量", x, y, BTNWIDTH, HEIGHT);
    ZomboniCountBox->SetCheck(settings.ZomboniCount);
    ZomboniCountBox->Connect([=] { settings.ZomboniCount = ZomboniCountBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto FootballCountBox = window->AddCheckBox("橄榄数量", x, y, BTNWIDTH, HEIGHT);
    FootballCountBox->SetCheck(settings.FootballCount);
    FootballCountBox->Connect([=] { settings.FootballCount = FootballCountBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto JackExplosionRangeBox = window->AddCheckBox("受炸提示", x, y, BTNWIDTH, HEIGHT);
    JackExplosionRangeBox->SetCheck(settings.JackExplosionRange);
    JackExplosionRangeBox->Connect([=] { settings.JackExplosionRange = JackExplosionRangeBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto ShowSpeedBox = window->AddCheckBox("显示倍速", x, y, BTNWIDTH, HEIGHT);
    ShowSpeedBox->SetCheck(settings.ShowSpeed);
    ShowSpeedBox->Connect([=] { settings.ShowSpeed = ShowSpeedBox->GetCheck(); });
}

AWindow* BasicPageWindow(int pageX, int pageY) {
    auto window = mainWindow.AddWindow(pageX, pageY);

    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;
    constexpr static int BTNWIDTH = 75;

    int x = SPACE;
    int y = 0;

    CreateShowInfoGroup(window, x + WIDTH * 4 + SPACE * 6, y);

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("速度档位", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto SpeedGearsEdit = window->AddEdit(settings.SpeedGears, x, y, 345, HEIGHT, ES_AUTOHSCROLL);
    x += SpeedGearsEdit->GetWidth() + SPACE;

    x = SPACE;
    y += SPACE + HEIGHT;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("波长记录", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto WavelengthRecordComboBox = window->AddComboBox(x, y, 50, 500);
    WavelengthRecordComboBox->AddString("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20");
    WavelengthRecordComboBox->SetText(std::to_string(settings.WavelengthRecord));

    x += WavelengthRecordComboBox->GetWidth() + SPACE;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("跳帧波次", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto SkipTickWaveComboBox = window->AddComboBox(x, y, 50, 500);
    SkipTickWaveComboBox->AddString("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20");
    SkipTickWaveComboBox->SetText(std::to_string(settings.SkipTickWave));

    x += SkipTickWaveComboBox->GetWidth() + SPACE;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("只读状态", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto LockStateComboBox = window->AddComboBox(x, y, 76, 500);
    LockStateComboBox->AddString("-", "No", "Yes");
    if (settings.ReadOnly == -1)
        LockStateComboBox->SetText("-");
    if (settings.ReadOnly == 0)
        LockStateComboBox->SetText("No");
    if (settings.ReadOnly == 1)
        LockStateComboBox->SetText("Yes");

    x += LockStateComboBox->GetWidth() + SPACE;

    x = SPACE;
    y += SPACE + HEIGHT;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("切换音乐", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto MusicComboBox = window->AddComboBox(x, y, 265, 500);
    MusicComboBox->AddString("-", "1. Grasswalk", "2. Moongrains", "3. Watery Graves", "4. Rigor Mormist", "5. Graze the Roof", "6. Choose Your Seeds", "7. Crazy Dave", "8. Zen Garden", "9. Cerebrawl", "10. Loonboon", "11. Ultimite Battle", "12. Brainiac Maniac");

    x += MusicComboBox->GetWidth() + SPACE;

    auto ApplyAllBtn = window->AddPushButton("一键设置", x, y, BTNWIDTH, HEIGHT);
    ApplyAllBtn->Connect([=] {
        std::strcpy(settings.SpeedGears, SpeedGearsEdit->GetText().c_str());
        SetGameSpeedGears(SpeedGearsEdit->GetText());
        settings.WavelengthRecord = std::stoi(WavelengthRecordComboBox->GetText());
        settings.SkipTickWave = std::stoi(SkipTickWaveComboBox->GetText());
        if (LockStateComboBox->GetText() == "-")
            settings.ReadOnly = -1;
        if (LockStateComboBox->GetText() == "No")
            settings.ReadOnly = 0;
        if (LockStateComboBox->GetText() == "Yes")
            settings.ReadOnly = 1;
        if (MusicComboBox->GetText() != "-")
            SetMusic(std::stoi(MusicComboBox->GetText()));
        Info("一键设置成功");
    });

    x = SPACE;
    y += SPACE + HEIGHT;
    CreateReplayGroup(window, x, y);

    return window;
}

AWindow* KeyPageWindow(int pageX, int pageY) {
    auto window = mainWindow.AddWindow(pageX, pageY);

    constexpr static int SPACE = 5;
    constexpr static int HEIGHT = 25;
    constexpr static int BTNWIDTH = 75;
    constexpr static int BOXWIDTH = 125;

    int x = 0;
    int y = 0;

    x += SPACE;

    for (size_t i = 0; i < btnLabels.size(); ++i) {
        keyButtons[i] = window->AddPushButton(btnLabels[i], x, y, BTNWIDTH, HEIGHT);
        keyButtons[i]->Connect([=] { FightOrCardUiCheck(); funcs[i](); });
        y += SPACE + HEIGHT;
        if (i % 11 == 10) {
            x += 2 * SPACE + BTNWIDTH + BOXWIDTH;
            y = 0;
        }
    }

    x = 2 * SPACE + BTNWIDTH;
    y = 0;

    for (size_t i = 0; i < keyBindings.size(); ++i) {
        keyEdits[i] = window->AddEdit(keyBindings[i], x, y, BOXWIDTH, HEIGHT, ES_AUTOHSCROLL | ES_UPPERCASE);
        y += SPACE + HEIGHT;
        if (i % 11 == 10 && i != 32) {
            x += 2 * SPACE + BTNWIDTH + BOXWIDTH;
            y = 0;
        }
    }

    x = SPACE;

    // 全部绑定
    auto keybindBtn = window->AddPushButton("全部绑定", x, y, 100, HEIGHT);
    keybindBtn->Connect([=]() mutable {
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyBindings[i] = keyEdits[i]->GetText();
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyHandles[i].Stop();
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyHandles[i] = AConnect(keyBindings[i], funcs[i]);
        Info("已绑定所有按键");
    });

    x += keybindBtn->GetWidth() + SPACE;

    // 清除绑定
    auto clearBtn = window->AddPushButton("清除绑定", x, y, 100, HEIGHT);
    clearBtn->Connect([=]() mutable {
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyBindings[i] = "";
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyHandles[i].Stop();
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyHandles[i] = AConnect(keyBindings[i], funcs[i]);
        for (size_t i = 0; i < keyEdits.size(); ++i)
            keyEdits[i]->SetText("");
        Info("已将所有按键解除绑定");
    });

    x += clearBtn->GetWidth() + SPACE;

    // 按键初始化
    auto resetBtn = window->AddPushButton("按键初始化", x, y, 100, HEIGHT);
    resetBtn->Connect([=]() mutable {
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyBindings[i] = keyDefaults[i];
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyHandles[i].Stop();
        for (size_t i = 0; i < keyHandles.size(); ++i)
            keyHandles[i] = AConnect(keyBindings[i], funcs[i]);
        for (size_t i = 0; i < keyEdits.size(); ++i)
            keyEdits[i]->SetText(keyDefaults[i]);
        Info("已将所有按键初始化");
    });

    x += resetBtn->GetWidth() + SPACE;

    // 导入配置文件
    auto loadSettingsBtn = window->AddPushButton("导入按键配置", x, y, 100, HEIGHT);
    loadSettingsBtn->Connect([=] {
        if (!LoadKeybindings()) {
            ::Info("未找到按键配置文件，导入失败！");
            return;
        }
        for (size_t i = 0; i < keyEdits.size(); ++i)
            keyEdits[i]->SetText(keyBindings[i]);
        ::Info("按键配置文件导入成功！");
    });

    x += loadSettingsBtn->GetWidth() + SPACE;

    // 导出配置文件
    auto saveSettingsBtn = window->AddPushButton("导出按键配置", x, y, 100, HEIGHT);
    saveSettingsBtn->Connect([=] {
        if (SaveKeybindings())
            ::Info("按键配置文件导出成功！保存在A-TAS的根目录下，文件名为keybindings.ini");
        else
            ::Info("按键配置文件导出失败！");
    });

    return window;
}

// 出怪列表label
ALabel* zombieListName_label = nullptr;
ALabel* zombieListInfo_label[20] = {};
ALabel* zombieListSum_label = nullptr;
AEdit* spawnseedEdit = nullptr;

void zombieListInfo_update() {
    const char* name_list[33] = {"普僵", "旗帜", "路障", "撑杆", "铁桶", "读报", "铁门", "橄榄", "舞王", "伴舞", "鸭子", "潜水", "冰车", "雪橇", "海豚", "小丑", "气球", "矿工", "跳跳", "雪人", "蹦极", "扶梯", "投篮", "白眼", "小鬼", "僵博", "豌豆", "坚果", "辣椒", "机枪", "倭瓜", "高坚", "红眼"};
    int zombie_list[33][20] = {};
    int zombie_sum[33] = {};
    auto list = AGetMainObject()->ZombieList();
    for (int w = 1; w <= 20; w++)
        for (int i = 50 * (w - 1); i < 50 * w; i++) {
            auto tmp = *(list + i);
            if (tmp == 0xffffffff)
                break;
            else if (tmp >= 0 and tmp <= 32)
                zombie_list[tmp][w - 1]++;
        }
    for (int i = 0; i <= 32; i++) {
        if (i == 1 || i == 19)
            continue;
        for (int w = 0; w < 20; w++)
            zombie_sum[i] += zombie_list[i][w];
    }

    std::string name = "wave\n";
    std::string info_w[20];
    for (int i = 0; i < 20; i++)
        info_w[i] = (i + 1 >= 10 ? "" : " ") + std::to_string(i + 1) + "\n";
    std::string sum = "sum\n";
    for (int i = 0; i <= 32; i++) {
        if (i == 1)
            continue;
        if (zombie_sum[i] == 0)
            continue;
        name += name_list[i];
        name += +"\n";
        sum += std::to_string(zombie_sum[i]) + "\n";
        for (int w = 0; w < 20; w++)
            info_w[w] += (zombie_list[i][w] >= 10 ? "" : " ") + std::to_string(zombie_list[i][w]) + "\n";
    }

    if (settings.ZombieList) {
        if (zombieListName_label != nullptr)
            zombieListName_label->SetText(name);
        for (int w = 0; w < 20; w++)
            if (zombieListInfo_label[w] != nullptr)
                zombieListInfo_label[w]->SetText(info_w[w]);
        if (zombieListSum_label != nullptr)
            zombieListSum_label->SetText(sum);
    } else {
        if (zombieListName_label != nullptr)
            zombieListName_label->SetText("");
        for (int w = 0; w < 20; w++)
            if (zombieListInfo_label[w] != nullptr)
                zombieListInfo_label[w]->SetText("");
        if (zombieListSum_label != nullptr)
            zombieListSum_label->SetText("");
    }
}

AWindow* SpawnPageWindow(int pageX, int pageY) {
    auto window = mainWindow.AddWindow(pageX, pageY);

    struct Info {
        const char* name;
        int type;
        ACheckBox* box;
    };
    std::vector<Info> infos = {
        {"路障", ACONEHEAD_ZOMBIE},
        {"撑杆", APOLE_VAULTING_ZOMBIE},
        {"铁桶", ABUCKETHEAD_ZOMBIE},
        {"读报", ANEWSPAPER_ZOMBIE},
        {"铁门", ASCREEN_DOOR_ZOMBIE},
        {"橄榄", AFOOTBALL_ZOMBIE},
        {"舞王", ADANCING_ZOMBIE},
        {"潜水", ASNORKEL_ZOMBIE},
        {"冰车", AZOMBONI},
        {"雪橇", AZOMBIE_BOBSLED_TEAM},
        {"海豚", ADOLPHIN_RIDER_ZOMBIE},
        {"小丑", AJACK_IN_THE_BOX_ZOMBIE},
        {"气球", ABALLOON_ZOMBIE},
        {"矿工", ADIGGER_ZOMBIE},
        {"跳跳", APOGO_ZOMBIE},
        {"蹦极", ABUNGEE_ZOMBIE},
        {"扶梯", ALADDER_ZOMBIE},
        {"投篮", ACATAPULT_ZOMBIE},
        {"白眼", AGARGANTUAR},
        {"红眼", AGIGA_GARGANTUAR},
        {"豌豆", APEASHOOTER_ZOMBIE},
        {"坚果", AWALL_NUT_ZOMBIE},
        {"辣椒", AJALAPENO_ZOMBIE},
        {"机枪", AGATLING_PEA_ZOMBIE},
        {"倭瓜", ASQUASH_ZOMBIE},
        {"高坚", ATALL_NUT_ZOMBIE},
    };

    constexpr int SPACE = 10;
    constexpr int WIDTH = 50;
    constexpr int HEIGHT = 20;
    constexpr int ROW_CNT = 3;
    constexpr int COL_CNT = 10;

    window->AddLabel("", 5, 0, 624, 362);

    for (int row = 0; row < ROW_CNT; ++row) {
        for (int col = 0; col < COL_CNT; ++col) {
            int idx = row * COL_CNT + col;
            if (idx > 25)
                continue;
            int x = col * (WIDTH + SPACE + 3) + 10;
            int y = row * (HEIGHT + SPACE);

            infos[idx].box = window->AddCheckBox(infos[idx].name, x, y, 50, 25);
            infos[idx].box->SetCheck(settings.Types[idx]);
            infos[idx].box->Connect([=] { settings.Types[idx] = infos[idx].box->GetCheck(); });
        }
    }

    int x = SPACE + 6 * (WIDTH + SPACE + 3);
    int y = (HEIGHT + SPACE) * 4 + SPACE * 1 - 70;
    spawnseedEdit = window->AddEdit("No Seed", x, y, 110, 25, ES_CENTER);
    x += 2 * (WIDTH + SPACE + 3);
    auto spawnseedBtn = window->AddPushButton("⬅️依此种子刷怪", x, y, 110, 25);
    spawnseedBtn->Connect([=] {
        FightOrCardUiCheck();
        AGetMainObject()->MRef<uint32_t>(0x561C) = std::stoul(spawnseedEdit->GetText(), nullptr, 16);
        InitZombieWaves();
        zombieListInfo_update();
        if (AGetPvzBase()->GameUi() != 2)
            return;
        AAsm::KillZombiesPreview();
        PlaceStreetZombies();
    });

    x = SPACE;
    y += HEIGHT + SPACE;

    auto showZombieListBox = window->AddCheckBox("查看出怪列表", x, y, 125, 25);
    showZombieListBox->SetCheck(settings.ZombieList);
    x += 2 * (WIDTH + SPACE + 3);
    auto averageSpawnBox = window->AddCheckBox("每行平均分配", x, y, 125, 25);
    averageSpawnBox->SetCheck(settings.AverageRowSpawn);
    x += 2 * (WIDTH + SPACE + 3);
    auto randomTypeBox = window->AddCheckBox("随机添加种类", x, y, 125, 25);
    randomTypeBox->SetCheck(settings.RandomType);
    x += 2 * (WIDTH + SPACE + 3);
    auto averageBtn = window->AddPushButton("平均出怪", x, y, 110, 25);
    x += 2 * (WIDTH + SPACE + 3);
    auto internalBtn = window->AddPushButton("自然出怪", x, y, 110, 25);

    auto spawnFunc = [infos = std::move(infos)](ASetZombieMode mode) {
        std::vector<int> types;
        types.push_back(AZOMBIE);
        for (int i = 0; i < infos.size(); ++i) {
            settings.Types[i] = infos[i].box->GetCheck();
            if (settings.Types[i]) {
                types.push_back(infos[i].type);
            }
        }
        FightOrCardUiCheck();
        if (settings.RandomType) {
            try {
                types = ACreateRandomTypeList(types);
            } catch (AException& exce) {
                Warning(std::string("捕获到 AException:") + exce.what());
                return;
            }
        }
        ASetZombies(types, mode);
        ::Info("出怪设置成功");
        zombieListInfo_update();
    };
    showZombieListBox->Connect([=] { settings.ZombieList = showZombieListBox->GetCheck(); FightOrCardUiCheck(); zombieListInfo_update(); });
    averageSpawnBox->Connect([=] { settings.AverageRowSpawn = averageSpawnBox->GetCheck(); FightOrCardUiCheck(); if (settings.AverageRowSpawn) {AAverageSpawn();} });
    randomTypeBox->Connect([=] { settings.RandomType = randomTypeBox->GetCheck(); });
    averageBtn->Connect([=] { spawnFunc(ASetZombieMode::AVERAGE); });
    internalBtn->Connect([spawnFunc = std::move(spawnFunc)] { spawnFunc(ASetZombieMode::INTERNAL); });

    y += HEIGHT + SPACE;
    x = SPACE + 5;

    int tmp_height = (SPACE + HEIGHT) * 8;
    zombieListName_label = window->AddLabel("", x, y, 40, tmp_height);
    x += 51;
    for (int i = 0; i < 20; i++) {
        zombieListInfo_label[i] = window->AddLabel("", x, y, 20, tmp_height);
        x += 26;
    }
    x += 5;
    zombieListSum_label = window->AddLabel("", x, y, 30, tmp_height);

    return window;
}

AOnAfterInject({
    __CheckASCII(GetToolPath(),
                 AMsgBox::Show("本工具只能在纯英文路径下才能正常运行, 你放置的路径: [" + GetToolPath() + "] 中含有非 ASCII 字符, 请将本工具的所有文件放置在纯英文路径下再次尝试运行");
                 ATerminate(), );
    isInitSuccess = true;
    strcpy(settings.SpeedGears, SpeedGearsDefault.c_str());
    strcpy(settings.savePath, GetToolPath().c_str());
    LoadSettings();

    compressor = std::make_shared<A7zCompressor>(GetToolPath() + "/7z.exe");

    SetGameSpeedGears(settings.SpeedGears);

    aReplay.SetCompressor(*compressor);
    aReplay.SetMouseVisible(settings.ShowMouse);
    aReplay.SetSaveDirPath(settings.savePath);

    // 若找不到keybindings.ini，使用预设
    if (!LoadKeybindings())
        keyBindings = keyDefaults;

    // 点击选卡僵尸不进图鉴
    *(uint8_t*)0x486B0A = 0xEB;
    *(std::array<uint8_t, 3>*)0x42DF5D = {0x38, 0x59, 0x54};
    *(std::array<uint8_t, 3>*)0x471DCF = {0xEB, 0x24, 0x90};

    static constexpr int SPACE = 10;
    static constexpr int TOPWIDTH = 100;
    static constexpr int TOPHEIGHT = 25;
    int x = 0;

    auto basicPageBtn = mainWindow.AddPushButton("常规设置", x, 0, TOPWIDTH, TOPHEIGHT);
    x += TOPWIDTH;
    auto keyPageBtn = mainWindow.AddPushButton("按键设置", x, 0, TOPWIDTH, TOPHEIGHT);
    x += TOPWIDTH;
    auto spawnPageBtn = mainWindow.AddPushButton("出怪设置", x, 0, TOPWIDTH, TOPHEIGHT);
    mainWindow.AddPushButton("", 0, TOPHEIGHT, 635, 5);

    auto basicPage = BasicPageWindow(0, TOPHEIGHT + SPACE);
    auto keyPage = KeyPageWindow(0, TOPHEIGHT + SPACE);
    auto spawnPage = SpawnPageWindow(0, TOPHEIGHT + SPACE);

    mainWindow.AddLabel("", 5, MAIN_HEIGHT - 78, 624, 44);
    mainWindow.AddLabel("信息:", 10, MAIN_HEIGHT - 78, 40, 44);
    infoLabel = mainWindow.AddLabel("", 50, MAIN_HEIGHT - 78, 579, 44);

    keyPage->Hide();
    spawnPage->Hide();
    basicPage->Show();

    AConnect(basicPageBtn, [=] {
        keyPage->Hide();
        spawnPage->Hide();
        basicPage->Show();
    });
    AConnect(keyPageBtn, [=] {
        basicPage->Hide();
        spawnPage->Hide();
        keyPage->Show();
    });
    AConnect(spawnPageBtn, [=] {
        basicPage->Hide();
        keyPage->Hide();
        spawnPage->Show();
    });
});

AOnBeforeExit({
    SaveSettings();
});

AOnBeforeScript({
    if (AGetPvzBase()->GameUi() == 2)
        zombieListInfo_update();
});

// 进入战斗即执行
AOnEnterFight({
    Paused = false;
    PausedSlowed = false;
    AMaidCheats::Phase() = MaidPhase;
    aItemCollector.Stop();
    LeftmostVisibleArea.assign(6, 10);
    if (settings.AverageRowSpawn)
        AAverageSpawn();
    aReplay.SetMaxSaveCnt(INT_MAX);
    aReplay.SetShowInfo(false);
    zombieListInfo_update();
    if (settings.AutoRecordOnGameStart)
        replayOp.startRecordOp();
    if (settings.ShowMe)
        tickShowMe.Start();
    else
        tickShowMe.Stop();
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i].Stop();
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i] = AConnect(keyBindings[i], funcs[i]);
});

AOnExitFight({
    LeftmostVisibleArea.assign(6, 10);
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i].Stop();
    if (spawnseedEdit)
        spawnseedEdit->SetText("No Seed");
});

// ALogger<AConsole> ConsoleLogger;

// 主体
void AScript() {
    ASetReloadMode(AReloadMode::MAIN_UI_OR_FIGHT_UI);
    Lock(AGetPvzBase()->MPtr(0x82C)->MRef<int>(0x20), AGetPvzBase()->LevelId(), settings.ReadOnly);
    AGetInternalLogger()->SetLevel({});
    // ASetInternalLogger(ConsoleLogger);
    fightInfoPainter.SetFontSize(17); // 波数时间，僵尸计数
    lowIndexPainter.SetFont("Arial");
    lowIndexPainter.SetFontSize(12); // 低栈植物栈位样式
    nextIndexPainter.SetFont("Arial");
    nextIndexPainter.SetFontSize(30); // 下一个栈位样式
    GigaNumPainter.SetFont("");
    GigaNumPainter.SetFontSize(17); // 红眼数样式

    tickFight.Start([=] { DanceCheat(); JackPause(); BalloonPause(); });
    tickPainter.Start([=] { DrawInfo(); DrawIndex(); }, ATickRunner::PAINT);
    tickGlobal.Start([=] { SmartRemove(); BalloonCaption(); }, ATickRunner::GLOBAL);

    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i].Stop();
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i] = AConnect(keyBindings[i], funcs[i]);

    if (spawnseedEdit)
        spawnseedEdit->SetText(std::format("{:08X}", AGetMainObject()->MRef<uint32_t>(0x561C)));
}