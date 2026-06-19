// A-TAS 4.0 by mint/残云/碧水/正弦
// 技术指导：Leonhard/铃仙/向量/零度
// 本辅助工具目前版本使用的键控注入框架应使用AvZ2 2.9.0 20260224版本，源码不保证对更旧版本AvZ的兼容性

#define UNICODE
#define A_TAS_VERSION 202603120330
#include "atas.h"
#include "showme/sm.h"
#include "win32gui/main.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <tlhelp32.h>

using namespace ATas;

std::shared_ptr<A7zCompressor> compressor = nullptr;
bool isInitSuccess = false;

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
static std::array<std::string, 33> keyDefaults = {"A", "1", "2", "C", "R", "T", "F5", "BACKSPACE", "Z", "X", "SHIFT", "V", "G", "B", "H", "Q", "W", "S", "D", "F", "E", "I", "J", "Y", "N", "U", "L", "ALT", "UP", "DOWN", "LEFT", "RIGHT", "O"};
static std::array<const char*, 33> btnLabels = {"一键辅助", "减速一档", "加速一档", "0.25倍速", "10倍速", "跳到某波", "退出重进", "回档几帧", "高级暂停", "下一帧", "智能用卡", "卡槽置顶", "显示信息", "显示栈位", "智能铲除", "Dance快", "Dance慢", "女仆召唤", "女仆停滞", "女仆前进", "女仆解除", "自动收集", "小丑拦截", "气球拦截", "风炮修正", "隐藏UI", "跳过动画", "六路种植", "视角上移", "视角下移", "视角左移", "视角右移", "PvZ初始化"};
static std::array<std::string, 33> keyBindings;
static std::array<AEdit*, 33> keyEdits;

// 预设设置
ATas::Settings settings;
ATas::Runtime runtime;

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
    std::ifstream inFile(GetToolPath() + "/settings.dat", std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return;
    if (inFile.tellg() != sizeof(settings)) {
        inFile.close();
        return;
    }
    inFile.seekg(0, std::ios::beg);
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

// 10倍速
void Speed10x() { runtime.action.Speed10x(); }
// 跳到某波
void SkiptoWave() { runtime.action.SkipToWave(settings); }
// 退出重进
void Restart() { runtime.action.Restart(); }
// 回档几帧
void func8() { runtime.action.ReplayRewind(settings); }
// 高级暂停
void func9() { runtime.action.ToggleAdvancedPause(); }
// 下一帧
void func10() { runtime.action.NextTick(); }

// 卡槽置顶
void func13() { runtime.action.ToggleSeedChooserTop(); }
// 显示信息
void func14() { runtime.drawer.ToggleInfo(); }
// 显示栈位
void func15() { runtime.drawer.ToggleIndex(); }
// 智能铲除
void func16() { runtime.smartRemove.Toggle(); }
// Dance秘籍
void func17() { runtime.action.ToggleDanceFast(); }
void func18() { runtime.action.ToggleDanceSlow(); }
// 女仆秘籍
void func19() { runtime.maid.Summon(); }
void func20() { runtime.maid.Dance(); }
void func21() { runtime.maid.Move(); }
void func22() { runtime.maid.Stop(); }
// 自动收集
void func23() { runtime.action.ToggleAutoCollect(); }
// 小丑拦截
void func24() { runtime.warning.ToggleJack(); }
// 气球拦截
void func25() { runtime.warning.ToggleBalloon(); }
// 风炮修正
void func26() { runtime.action.ToggleWindFix(); }
// 隐藏UI
void func27() { runtime.action.ToggleGameUi(); }
// 点击跳过动画、快速随机选卡
void func28() { runtime.action.ToggleAnimationSkip(); }

// 拍照模式
void func30() { runtime.action.ToggleSnapshotMode(); }
// 调整视角
void func31() { runtime.action.MoveViewUp(); }
void func32() { runtime.action.MoveViewDown(); }
void func33() { runtime.action.MoveViewLeft(); }
void func34() { runtime.action.MoveViewRight(); }

// 辅助开关
void OneKeySwitch() { runtime.action.OneKeySwitch(runtime.drawer, runtime.smartRemove, runtime.warning); }

// 初始化
void ResetGame() { runtime.action.ResetGame(runtime.maid, runtime.drawer, runtime.smartRemove, runtime.warning); }

// UI
std::array<AConnectHandle, 33> keyHandles;
std::array<AOperation, 33> funcs = {OneKeySwitch, Decelerate, Accelerate, ResetSpeed, Speed10x, SkiptoWave, Restart, func8, func9, func10, SmartAsh, func13, func14, func15, func16, func17, func18, func19, func20, func21, func22, func23, func24, func25, func26, func27, func28, PlantShovelFireForbiddenGrid, func31, func32, func33, func34, ResetGame};
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
    auto Dataname = std::format("game{}_{}.dat", userId, gameID);
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
    HMODULE hComDlg = LoadLibraryW(L"comdlg32.dll");
    if (!hComDlg)
        return "";

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
    openFileName.nMaxFile = MAX_PATH;
    openFileName.lpstrFilter = L"回放文件 (*.7z)\0\0";
    openFileName.lpstrFile = szFileName;
    openFileName.nFilterIndex = 1;
    openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    openFileName.lpstrInitialDir = initDir.c_str();

    std::string result;
    if (pGetOpenFileNameW(&openFileName)) {
        result = AWStrToStr(openFileName.lpstrFile);
    }

    FreeLibrary(hComDlg);
    return result;
}

constexpr static int MAIN_WIDTH = 640;
constexpr static int MAIN_HEIGHT = 480;

AMainWindow mainWindow(std::format("A-TAS 4.0 {}", A_TAS_VERSION), MAIN_WIDTH, MAIN_HEIGHT);

// // 回放函数
// struct {
//     AOperation startPlayOp = [] {
//     };
//     AOperation startRecordOp = [] {
//     };
//     AOperation pauseOp = [] {
//     };
//     AOperation stopOp = [] {
//         aReplay.Stop();
//     };
//     AOperation nextTickOp = [] {
//         FightUiCheck();
//         if (aReplay.GetState() == aReplay.RECORDING) {
//             ASetAdvancedPause(true);
//         }
//         aReplay.Pause();
//         if (aReplay.GetState() == AReplay::PLAYING) {
//             aReplay.ShowOneTick(aReplay.GetPlayIdx() + 1);
//         }
//     };
//     AOperation preTickOp = [] {
//         FightUiCheck();
//         if (aReplay.GetState() == aReplay.RECORDING) {
//             ASetAdvancedPause(true);
//         }
//         aReplay.Pause();
//         if (aReplay.GetState() == AReplay::RECORDING) {
//             aReplay.ShowOneTick(aReplay.GetRecordIdx() - 1);
//         } else {
//             aReplay.ShowOneTick(aReplay.GetPlayIdx() - 1);
//         }
//     };
// } replayOp;

void CreateReplayGroup(AWindow* window, int LeftEdge, int TopEdge) {
    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;

    int y = TopEdge;
    int x = LeftEdge;
    window->AddLabel("", x, y, 4 * (SPACE + WIDTH) + SPACE, (SPACE + HEIGHT) * 7);

    x += SPACE;
    window->AddLabel("回放:    必须录制才能回档", x, y, WIDTH * 2 + SPACE, HEIGHT);

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    auto showMouseBox = window->AddCheckBox("显示鼠标", x, y, WIDTH, HEIGHT);
    showMouseBox->SetCheck(settings.ShowMouse);
    x += SPACE + WIDTH;
    auto autoRecordBox = window->AddCheckBox("开始时录制", x, y, WIDTH, HEIGHT);
    autoRecordBox->SetCheck(settings.AutoRecordOnGameStart);
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
    autoRecordBox->Connect([=] {
        settings.AutoRecordOnGameStart = autoRecordBox->GetCheck();
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
    auto startPlayBtn = window->AddPushButton("📂选择回放", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto startRecordBtn = window->AddPushButton("⏺️开始录制", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto pauseBtn = window->AddPushButton("⏯️播放/暂停", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto stopBtn = window->AddPushButton("⏹️停止", x, y, WIDTH, HEIGHT);

    startPlayBtn->Connect([=] {
        if (AGetPvzBase()->GameUi() != AAsm::PLAYING)
            EnterGame(AAsm::CHALLENGE_ICE);
        FightUiCheck();
        if (aReplay.GetState() != aReplay.REST)
            aReplay.Stop();
        auto fileName = OpenFileDialog(settings.savePath);
        if (fileName.empty())
            return;
        compressor->SetFilePath(fileName);
        aReplay.StartPlay();
        Info("Replay : 开始播放");
    });
    startRecordBtn->Connect([=] {
        FightUiCheck();
        if (aReplay.GetState() != aReplay.REST)
            aReplay.Stop();
        compressor->SetFilePath(settings.savePath + std::string("/") + GetCurTimeStr() + ".7z");
        aReplay.StartRecord(std::round(settings.recordTickInterval));
        Info("Replay : 开始录制");
    });
    pauseBtn->Connect([=] {
        FightUiCheck();
        func9();
    });
    stopBtn->Connect([=] {
        aReplay.Stop();
    });

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    auto firstTickBtn = window->AddPushButton("⏮️首帧", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto preTickBtn = window->AddPushButton("⏪上一帧", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto nextTickBtn = window->AddPushButton("⏩下一帧", x, y, WIDTH, HEIGHT);
    x += SPACE + WIDTH;
    auto lastTickBtn = window->AddPushButton("⏭️末帧", x, y, WIDTH, HEIGHT);

    firstTickBtn->Connect([=] {
        FightUiCheck();
        if (aReplay.GetState() == AReplay::PLAYING) {
            aReplay.Pause();
            aReplay.ShowOneTick(aReplay.GetStartIdx());
        } else {
            Paused = true;
            PausedSlowed = false;
            ASetAdvancedPause(Paused, false, 0);
            aReplay.ShowOneTick(aReplay.GetStartIdx());
        }
    });
    preTickBtn->Connect([=] {
        FightUiCheck();
        if (aReplay.GetState() == aReplay.PLAYING) {
            aReplay.Pause();
            aReplay.ShowOneTick(aReplay.GetPlayIdx() - 1);
        } else {
            Paused = true;
            PausedSlowed = false;
            ASetAdvancedPause(Paused, false, 0);
            aReplay.ShowOneTick(aReplay.GetRecordIdx() - 1);
        }
    });
    nextTickBtn->Connect([=] {
        FightUiCheck();
        func10();
    });
    lastTickBtn->Connect([=] {
        FightUiCheck();
        if (aReplay.GetState() == AReplay::PLAYING) {
            aReplay.Pause();
            aReplay.ShowOneTick(aReplay.GetEndIdx() - 1);
        } else {
            Paused = true;
            PausedSlowed = false;
            ASetAdvancedPause(Paused, false, 0);
        }
    });

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;

    x += SPACE;
    window->AddLabel("存档精度(每隔多少cs存一次档):", x, y, WIDTH * 2 + SPACE, HEIGHT);
    x += (SPACE + WIDTH) * 2;
    auto tickIntervalEdit = window->AddEdit(std::format("{}", settings.recordTickInterval), x, y, WIDTH, HEIGHT, ES_NUMBER | ES_CENTER);
    x += SPACE + WIDTH;
    auto tickIntervalBtn = window->AddPushButton("设置", x, y, WIDTH, HEIGHT);
    tickIntervalBtn->Connect([=] {
        if (aReplay.GetState() != aReplay.REST) {
            Warning("回放正在工作，无法设置精度");
            return;
        };
        auto tickInterval = std::stoi(tickIntervalEdit->GetText());
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
    auto tickRewindCountEdit = window->AddEdit(std::format("{}", settings.tickRewindCount), x, y, WIDTH, HEIGHT, ES_NUMBER | ES_CENTER);
    x += SPACE + WIDTH;
    auto tickRewindCountBtn = window->AddPushButton("设置", x, y, WIDTH, HEIGHT);
    tickRewindCountBtn->Connect([=] {
        auto tickRewindCount = std::stoi(tickRewindCountEdit->GetText());
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

void CreateSpecialGroup(AWindow* window, int LeftEdge, int TopEdge) {
    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;

    int y = TopEdge;
    int x = LeftEdge;
    window->AddLabel("", x, y, 4 * (SPACE + WIDTH) + SPACE, (SPACE + HEIGHT));

    x += SPACE;
    window->AddLabel("特殊功能", x, y, WIDTH * 2 + SPACE, HEIGHT);

    x += SPACE + WIDTH;
    auto EnterHousePauseBox = window->AddCheckBox("进家时暂停", x, y, WIDTH, HEIGHT);
    EnterHousePauseBox->SetCheck(settings.EnterHousePause);
    EnterHousePauseBox->Connect([=] {
        settings.EnterHousePause = EnterHousePauseBox->GetCheck();
    });
    x += SPACE + WIDTH;

    // 下一行
    y += SPACE + HEIGHT;
    x = LeftEdge;
}

static uint32_t HexToUL(const std::string& text, uint32_t originalVal) {
    if (text.empty())
        return originalVal;
    try {
        return std::stoul(text, nullptr, 16);
    } catch (...) {
        return originalVal;
    }
}

void CreateShowInfoGroup(AWindow* window, int LeftEdge, int TopEdge) {
    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;
    constexpr static int BTNWIDTH = 90;
    constexpr static int EDITWIDTH = 100;

    int y = TopEdge;
    int x = LeftEdge;
    window->AddLabel("", x, y, 624, 362);

    x += SPACE;
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

    auto ProduceCDBox = window->AddCheckBox("生产冷却", x, y, BTNWIDTH, HEIGHT);
    ProduceCDBox->SetCheck(settings.ProduceCD);
    ProduceCDBox->Connect([=] { settings.ProduceCD = ProduceCDBox->GetCheck(); });
    auto ProduceCDARGBEdit = window->AddEdit(std::format("{:08X}", settings.ProduceCDARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;

    auto CobCDBox = window->AddCheckBox("炮冷却条", x, y, BTNWIDTH, HEIGHT);
    CobCDBox->SetCheck(settings.CobCD);
    CobCDBox->Connect([=] { settings.CobCD = CobCDBox->GetCheck(); });
    auto CobCDARGBEdit = window->AddEdit(std::format("{:08X}", settings.CobCDARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto CobGloomHPBox = window->AddCheckBox("炮曾血条", x, y, BTNWIDTH, HEIGHT);
    CobGloomHPBox->SetCheck(settings.CobGloomHP);
    CobGloomHPBox->Connect([=] { settings.CobGloomHP = CobGloomHPBox->GetCheck(); });
    auto CobGloomHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.CobGloomHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto PumpkinHPBox = window->AddCheckBox("南瓜血条", x, y, BTNWIDTH, HEIGHT);
    PumpkinHPBox->SetCheck(settings.PumpkinHP);
    PumpkinHPBox->Connect([=] { settings.PumpkinHP = PumpkinHPBox->GetCheck(); });
    auto PumpkinHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.PumpkinHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto NutSpikeHPBox = window->AddCheckBox("坚刺血条", x, y, BTNWIDTH, HEIGHT);
    NutSpikeHPBox->SetCheck(settings.NutSpikeHP);
    NutSpikeHPBox->Connect([=] { settings.NutSpikeHP = NutSpikeHPBox->GetCheck(); });
    auto NutSpikeHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.NutSpikeHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto LilyPotHPBox = window->AddCheckBox("荷盆血条", x, y, BTNWIDTH, HEIGHT);
    LilyPotHPBox->SetCheck(settings.LilyPotHP);
    LilyPotHPBox->Connect([=] { settings.LilyPotHP = LilyPotHPBox->GetCheck(); });
    auto LilyPotHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.LilyPotHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto OtherPlantHPBox = window->AddCheckBox("其他血条", x, y, BTNWIDTH, HEIGHT);
    OtherPlantHPBox->SetCheck(settings.OtherPlantHP);
    OtherPlantHPBox->Connect([=] { settings.OtherPlantHP = OtherPlantHPBox->GetCheck(); });
    auto OtherPlantHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.OtherPlantHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto JackExplosionRangeBox = window->AddCheckBox("受炸提示", x, y, BTNWIDTH, HEIGHT);
    JackExplosionRangeBox->SetCheck(settings.JackExplosionRange);
    JackExplosionRangeBox->Connect([=] { settings.JackExplosionRange = JackExplosionRangeBox->GetCheck(); });
    auto JackExplosionRangeARGBEdit = window->AddEdit(std::format("{:08X}", settings.JackExplosionRangeARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto IcetrailBox = window->AddCheckBox("冰道冷却", x, y, BTNWIDTH, HEIGHT);
    IcetrailBox->SetCheck(settings.Icetrail);
    IcetrailBox->Connect([=] { settings.Icetrail = IcetrailBox->GetCheck(); });
    auto IcetrailARGBEdit = window->AddEdit(std::format("{:08X}", settings.IcetrailARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto CraterBox = window->AddCheckBox("核坑冷却", x, y, BTNWIDTH, HEIGHT);
    CraterBox->SetCheck(settings.Crater);
    CraterBox->Connect([=] { settings.Crater = CraterBox->GetCheck(); });
    auto CraterARGBEdit = window->AddEdit(std::format("{:08X}", settings.CraterARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto VBEStatBox = window->AddCheckBox("罐子统计", x, y, BTNWIDTH, HEIGHT);
    VBEStatBox->SetCheck(settings.VBEStat);
    VBEStatBox->Connect([=] { settings.VBEStat = VBEStatBox->GetCheck(); });
    auto VBEStatARGBEdit = window->AddEdit(std::format("{:08X}", settings.VBEStatARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;

    // 下一列
    x += WIDTH + SPACE;
    y = TopEdge;

    auto PlantOffsetBox = window->AddCheckBox("小喷偏移", x, y, BTNWIDTH, HEIGHT);
    PlantOffsetBox->SetCheck(settings.PlantOffset);
    PlantOffsetBox->Connect([=] { settings.PlantOffset = PlantOffsetBox->GetCheck(); });

    // 下一列
    x += WIDTH + SPACE;
    y = TopEdge;

    auto HPStyleBox = window->AddCheckBox("炮阵样式", x, y, BTNWIDTH, HEIGHT);
    HPStyleBox->SetCheck(settings.HPStyle);
    HPStyleBox->Connect([=] { settings.HPStyle = HPStyleBox->GetCheck(); });

    y += SPACE + HEIGHT;
    auto GigaHPBox = window->AddCheckBox("红眼血条", x, y, BTNWIDTH, HEIGHT);
    GigaHPBox->SetCheck(settings.GigaHP);
    GigaHPBox->Connect([=] { settings.GigaHP = GigaHPBox->GetCheck(); });
    auto GigaHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.GigaHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto GargHPBox = window->AddCheckBox("白眼血条", x, y, BTNWIDTH, HEIGHT);
    GargHPBox->SetCheck(settings.GargHP);
    GargHPBox->Connect([=] { settings.GargHP = GargHPBox->GetCheck(); });
    auto GargHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.GargHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto FootballHPBox = window->AddCheckBox("橄榄血条", x, y, BTNWIDTH, HEIGHT);
    FootballHPBox->SetCheck(settings.FootballHP);
    FootballHPBox->Connect([=] { settings.FootballHP = FootballHPBox->GetCheck(); });
    auto FootballHPARGBEdit = window->AddEdit(std::format("{:08X}", settings.FootballHPARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto GigaCountBox = window->AddCheckBox("红眼数量", x, y, BTNWIDTH, HEIGHT);
    GigaCountBox->SetCheck(settings.GigaCount);
    GigaCountBox->Connect([=] { settings.GigaCount = GigaCountBox->GetCheck(); });
    auto GigaCountARGBEdit = window->AddEdit(std::format("{:08X}", settings.GigaCountARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto ZomboniCountBox = window->AddCheckBox("冰车数量", x, y, BTNWIDTH, HEIGHT);
    ZomboniCountBox->SetCheck(settings.ZomboniCount);
    ZomboniCountBox->Connect([=] { settings.ZomboniCount = ZomboniCountBox->GetCheck(); });
    auto ZomboniCountARGBEdit = window->AddEdit(std::format("{:08X}", settings.ZomboniCountARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto FootballCountBox = window->AddCheckBox("橄榄数量", x, y, BTNWIDTH, HEIGHT);
    FootballCountBox->SetCheck(settings.FootballCount);
    FootballCountBox->Connect([=] { settings.FootballCount = FootballCountBox->GetCheck(); });
    auto FootballCountARGBEdit = window->AddEdit(std::format("{:08X}", settings.FootballCountARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto JackCountdownBox = window->AddCheckBox("小丑炸条", x, y, BTNWIDTH, HEIGHT);
    JackCountdownBox->SetCheck(settings.JackCountdown);
    JackCountdownBox->Connect([=] { settings.JackCountdown = JackCountdownBox->GetCheck(); });
    auto JackCountdownARGBEdit = window->AddEdit(std::format("{:08X}", settings.JackCountdownARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto GigaStatBox = window->AddCheckBox("红眼统计", x, y, BTNWIDTH, HEIGHT);
    GigaStatBox->SetCheck(settings.GigaStat);
    GigaStatBox->Connect([=] { settings.GigaStat = GigaStatBox->GetCheck(); });
    auto GigaStatARGB1Edit = window->AddEdit(std::format("{:08X}", settings.GigaStatARGB1), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    auto GigaStatARGB2Edit = window->AddEdit(std::format("{:08X}", settings.GigaStatARGB2), x + BTNWIDTH + SPACE + EDITWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto TotalHPBox = window->AddCheckBox("本波血条", x, y, 100, HEIGHT);
    TotalHPBox->SetCheck(settings.TotalHP);
    TotalHPBox->Connect([=] { settings.TotalHP = TotalHPBox->GetCheck(); });
    auto TotalHPARGB1Edit = window->AddEdit(std::format("{:08X}", settings.TotalHPARGB1), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    auto TotalHPARGB2Edit = window->AddEdit(std::format("{:08X}", settings.TotalHPARGB2), x + BTNWIDTH + SPACE + EDITWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    y += SPACE + HEIGHT;
    auto ShowSpeedBox = window->AddCheckBox("显示倍速", x, y, BTNWIDTH, HEIGHT);
    ShowSpeedBox->SetCheck(settings.ShowSpeed);
    ShowSpeedBox->Connect([=] { settings.ShowSpeed = ShowSpeedBox->GetCheck(); });
    auto ShowSpeedARGB1Edit = window->AddEdit(std::format("{:08X}", settings.ShowSpeedARGB1), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    auto ShowSpeedARGB2Edit = window->AddEdit(std::format("{:08X}", settings.ShowSpeedARGB2), x + BTNWIDTH + SPACE + EDITWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    // 下一列
    x += WIDTH + SPACE;
    y = TopEdge;

    auto CobColPreviewBox = window->AddCheckBox("落点预览", x, y, BTNWIDTH, HEIGHT);
    CobColPreviewBox->SetCheck(settings.CobColPreview);
    CobColPreviewBox->Connect([=] { settings.CobColPreview = CobColPreviewBox->GetCheck(); });

    // 下一列
    x += WIDTH + SPACE;
    y = TopEdge;

    auto ActivationTimeBox = window->AddCheckBox(std::format("生效时机显示{}s", settings.MarkerDuration / 100.0f), x, y, BTNWIDTH + EDITWIDTH - HEIGHT - HEIGHT, HEIGHT);
    ActivationTimeBox->SetCheck(settings.ActivationTime);
    ActivationTimeBox->Connect([=] { settings.ActivationTime = ActivationTimeBox->GetCheck(); });

    auto DecreaseMarkerDurationBtn = window->AddPushButton("-", x + BTNWIDTH + EDITWIDTH - HEIGHT - HEIGHT, y, HEIGHT, HEIGHT);
    DecreaseMarkerDurationBtn->Connect([=] {
        settings.MarkerDuration = settings.MarkerDuration - 10 >= 0 ? settings.MarkerDuration - 10 : 0;
        ActivationTimeBox->SetText(std::format("生效时机显示{}s", settings.MarkerDuration / 100.0f));
    });
    auto IncreaseMarkerDurationBtn = window->AddPushButton("+", x + BTNWIDTH + EDITWIDTH - HEIGHT, y, HEIGHT, HEIGHT);
    IncreaseMarkerDurationBtn->Connect([=] {
        settings.MarkerDuration += 10;
        ActivationTimeBox->SetText(std::format("生效时机显示{}s", settings.MarkerDuration / 100.0f));
    });

    y += SPACE + HEIGHT;
    window->AddLabel("炮生效背景", x, y, BTNWIDTH, HEIGHT);
    auto PMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.PMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    y += SPACE + HEIGHT;
    window->AddLabel("冰生效背景", x, y, BTNWIDTH, HEIGHT);
    auto IMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.IMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    y += SPACE + HEIGHT;
    window->AddLabel("核生效背景", x, y, BTNWIDTH, HEIGHT);
    auto NMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.NMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    y += SPACE + HEIGHT;
    window->AddLabel("樱生效背景", x, y, BTNWIDTH, HEIGHT);
    auto AMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.AMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    y += SPACE + HEIGHT;
    window->AddLabel("辣生效背景", x, y, BTNWIDTH, HEIGHT);
    auto JMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.JMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    y += SPACE + HEIGHT;
    window->AddLabel("窝生效背景", x, y, BTNWIDTH, HEIGHT);
    auto WMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.WMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);
    y += SPACE + HEIGHT;
    window->AddLabel("雷生效背景", x, y, BTNWIDTH, HEIGHT);
    auto MMarkerARGBEdit = window->AddEdit(std::format("{:08X}", settings.MMarkerARGB), x + BTNWIDTH, y, EDITWIDTH, HEIGHT, ES_CENTER);

    // x = LeftEdge + SPACE + 5 * (WIDTH + SPACE);

    x += BTNWIDTH;
    y = TopEdge + 10 * (SPACE + HEIGHT);

    auto ApplyAllBtn = window->AddPushButton("一键改色", x, y, EDITWIDTH, HEIGHT);
    ApplyAllBtn->Connect([=] {
        settings.ProduceCDARGB = HexToUL(ProduceCDARGBEdit->GetText(), settings.ProduceCDARGB);
        settings.CobCDARGB = HexToUL(CobCDARGBEdit->GetText(), settings.CobCDARGB);
        settings.CobGloomHPARGB = HexToUL(CobGloomHPARGBEdit->GetText(), settings.CobGloomHPARGB);
        settings.PumpkinHPARGB = HexToUL(PumpkinHPARGBEdit->GetText(), settings.PumpkinHPARGB);
        settings.NutSpikeHPARGB = HexToUL(NutSpikeHPARGBEdit->GetText(), settings.NutSpikeHPARGB);
        settings.LilyPotHPARGB = HexToUL(LilyPotHPARGBEdit->GetText(), settings.LilyPotHPARGB);
        settings.OtherPlantHPARGB = HexToUL(OtherPlantHPARGBEdit->GetText(), settings.OtherPlantHPARGB);
        settings.JackExplosionRangeARGB = HexToUL(JackExplosionRangeARGBEdit->GetText(), settings.JackExplosionRangeARGB);
        settings.IcetrailARGB = HexToUL(IcetrailARGBEdit->GetText(), settings.IcetrailARGB);
        settings.CraterARGB = HexToUL(CraterARGBEdit->GetText(), settings.CraterARGB);
        settings.VBEStatARGB = HexToUL(VBEStatARGBEdit->GetText(), settings.VBEStatARGB);
        settings.GigaHPARGB = HexToUL(GigaHPARGBEdit->GetText(), settings.GigaHPARGB);
        settings.GargHPARGB = HexToUL(GargHPARGBEdit->GetText(), settings.GargHPARGB);
        settings.FootballHPARGB = HexToUL(FootballHPARGBEdit->GetText(), settings.FootballHPARGB);
        settings.GigaCountARGB = HexToUL(GigaCountARGBEdit->GetText(), settings.GigaCountARGB);
        settings.ZomboniCountARGB = HexToUL(ZomboniCountARGBEdit->GetText(), settings.ZomboniCountARGB);
        settings.FootballCountARGB = HexToUL(FootballCountARGBEdit->GetText(), settings.FootballCountARGB);
        settings.JackCountdownARGB = HexToUL(JackCountdownARGBEdit->GetText(), settings.JackCountdownARGB);
        settings.GigaStatARGB1 = HexToUL(GigaStatARGB1Edit->GetText(), settings.GigaStatARGB1);
        settings.GigaStatARGB2 = HexToUL(GigaStatARGB2Edit->GetText(), settings.GigaStatARGB2);
        settings.TotalHPARGB1 = HexToUL(TotalHPARGB1Edit->GetText(), settings.TotalHPARGB1);
        settings.TotalHPARGB2 = HexToUL(TotalHPARGB2Edit->GetText(), settings.TotalHPARGB2);
        settings.ShowSpeedARGB1 = HexToUL(ShowSpeedARGB1Edit->GetText(), settings.ShowSpeedARGB1);
        settings.ShowSpeedARGB2 = HexToUL(ShowSpeedARGB2Edit->GetText(), settings.ShowSpeedARGB2);

        settings.PMarkerARGB = HexToUL(PMarkerARGBEdit->GetText(), settings.PMarkerARGB);
        settings.IMarkerARGB = HexToUL(IMarkerARGBEdit->GetText(), settings.IMarkerARGB);
        settings.NMarkerARGB = HexToUL(NMarkerARGBEdit->GetText(), settings.NMarkerARGB);
        settings.AMarkerARGB = HexToUL(AMarkerARGBEdit->GetText(), settings.AMarkerARGB);
        settings.JMarkerARGB = HexToUL(JMarkerARGBEdit->GetText(), settings.JMarkerARGB);
        settings.WMarkerARGB = HexToUL(WMarkerARGBEdit->GetText(), settings.WMarkerARGB);
        settings.MMarkerARGB = HexToUL(MMarkerARGBEdit->GetText(), settings.MMarkerARGB);
        Info("所有颜色设置已保存");
    });
}

AWindow* BasicPageWindow(int pageX, int pageY) {
    auto window = mainWindow.AddWindow(pageX, pageY);

    constexpr static int SPACE = 5;
    // constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;
    constexpr static int BTNWIDTH = 75;

    int x = SPACE;
    int y = 0;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("速度档位", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto SpeedGearsEdit = window->AddEdit(settings.SpeedGears, x, y, 345, HEIGHT, ES_AUTOHSCROLL);
    x += SpeedGearsEdit->GetWidth() + SPACE;

    x = SPACE;
    y += SPACE + HEIGHT;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("切换音乐", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto MusicComboBox = window->AddComboBox(x, y, 265, 500);
    MusicComboBox->AddString("-", "1. Grasswalk", "2. Moongrains", "3. Watery Graves", "4. Rigor Mormist", "5. Graze the Roof", "6. Choose Your Seeds", "7. Crazy Dave", "8. Zen Garden", "9. Cerebrawl", "10. Loonboon", "11. Ultimite Battle", "12. Brainiac Maniac");

    x += MusicComboBox->GetWidth() + SPACE;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    auto LockBox = window->AddCheckBox("只读", x + 5, y, BTNWIDTH - 5, HEIGHT);

    x = SPACE;
    y += SPACE + HEIGHT;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("波长记录", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto WavelengthRecordComboBox = window->AddComboBox(x, y, 50, 500);
    WavelengthRecordComboBox->AddString("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20");
    WavelengthRecordComboBox->SetText(std::format("{}", settings.WavelengthRecord));

    x += WavelengthRecordComboBox->GetWidth() + SPACE;

    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    window->AddLabel("跳帧波次", x + SPACE, y, BTNWIDTH - SPACE, HEIGHT);
    x += BTNWIDTH + SPACE;
    auto SkipTickWaveComboBox = window->AddComboBox(x, y, 50, 500);
    SkipTickWaveComboBox->AddString("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20");
    SkipTickWaveComboBox->SetText(std::format("{}", settings.SkipTickWave));

    x += SkipTickWaveComboBox->GetWidth() + SPACE;

    auto ApplyAllBtn = window->AddPushButton("一键设置", x, y, BTNWIDTH, HEIGHT);
    ApplyAllBtn->Connect([=] {
        std::strcpy(settings.SpeedGears, SpeedGearsEdit->GetText().c_str());
        SetGameSpeedGears(SpeedGearsEdit->GetText());
        if (MusicComboBox->GetText() != "-")
            SetMusic(std::stoi(MusicComboBox->GetText()));
        settings.WavelengthRecord = std::stoi(WavelengthRecordComboBox->GetText());
        settings.SkipTickWave = std::stoi(SkipTickWaveComboBox->GetText());
        Info("一键设置成功");
    });

    x += ApplyAllBtn->GetWidth() + SPACE;
    window->AddLabel("", x, y, BTNWIDTH, HEIGHT);
    auto UnlockBox = window->AddCheckBox("不只读", x + 5, y, BTNWIDTH - 5, HEIGHT);
    LockBox->SetCheck(settings.ReadOnly == 1 ? true : false);
    UnlockBox->SetCheck(settings.ReadOnly == 0 ? true : false);

    LockBox->Connect([=] {
        if (LockBox->GetCheck()) {
            settings.ReadOnly = 1;
            UnlockBox->SetCheck(false);
        } else if (UnlockBox->GetCheck()) {
            settings.ReadOnly = 0;
        } else {
            settings.ReadOnly = -1;
        }
        Lock(AGetPvzBase()->MPtr(0x82C)->MRef<int>(0x20), AGetPvzBase()->LevelId(), settings.ReadOnly);
    });
    UnlockBox->Connect([=] {
        if (UnlockBox->GetCheck()) {
            settings.ReadOnly = 0;
            LockBox->SetCheck(false);
        } else if (LockBox->GetCheck()) {
            settings.ReadOnly = 1;
        } else {
            settings.ReadOnly = -1;
        }
        Lock(AGetPvzBase()->MPtr(0x82C)->MRef<int>(0x20), AGetPvzBase()->LevelId(), settings.ReadOnly);
    });

    x = SPACE;
    y += SPACE + HEIGHT;
    CreateReplayGroup(window, x, y);

    y += 7 * (SPACE + HEIGHT);
    y += SPACE;

    CreateSpecialGroup(window, x, y);

    return window;
}

AWindow* DisplayPageWindow(int pageX, int pageY) {
    auto window = mainWindow.AddWindow(pageX, pageY);

    constexpr static int SPACE = 5;
    int x = SPACE;
    int y = 0;

    CreateShowInfoGroup(window, x, y);

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
            if (tmp == 0xFFFFFFFF)
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
        info_w[i] = std::format("{:2}\n", i + 1);
    std::string sum = "sum\n";
    for (int i = 0; i <= 32; i++) {
        if (i == 1)
            continue;
        if (zombie_sum[i] == 0)
            continue;
        name += name_list[i];
        name += +"\n";
        sum += std::format("{}\n", zombie_sum[i]);
        for (int w = 0; w < 20; w++)
            info_w[w] += std::format("{:2}\n", zombie_list[i][w]);
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

void ApplySpawningRulesModify() {
    // *(std::array<uint8_t, 6>*)0x412DCE = {0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC3}; // 原始函数
    *(std::array<uint8_t, 6>*)0x412DCE = {0xE9, 0x51, 0xFF, 0xFF, 0xFF, 0x90};

    if (settings.AllowPoolAmbush) {
        *(std::array<uint8_t, 1>*)0x412DBD = {0xEB};
    } else if (settings.BanPoolAmbush) {
        *(std::array<uint8_t, 1>*)0x412DBD = {0x74};
        *(std::array<uint8_t, 1>*)0x412DBC = {0xFF};
        *(std::array<uint8_t, 1>*)0x412DC1 = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x412DBD = {0x74};
        *(std::array<uint8_t, 1>*)0x412DBC = {0x02};
        *(std::array<uint8_t, 1>*)0x412DC1 = {0x03};
    }

    if (settings.AllowSkyAmbush) {
        *(std::array<uint8_t, 1>*)0x412D12 = {0xEB};
    } else if (settings.BanSkyAmbush) {
        *(std::array<uint8_t, 1>*)0x412D12 = {0x74};
        *(std::array<uint8_t, 1>*)0x412D11 = {0xFF};
        *(std::array<uint8_t, 1>*)0x412D16 = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x412D12 = {0x74};
        *(std::array<uint8_t, 1>*)0x412D11 = {0x04};
        *(std::array<uint8_t, 1>*)0x412D16 = {0x05};
    }

    if (settings.AllowZomboni) {
        *(std::array<uint8_t, 1>*)0x42576E = {0xFF};
    } else if (settings.BanZomboni) {
        *(std::array<uint8_t, 1>*)0x42576E = {0x0C};
        *(std::array<uint8_t, 2>*)0x42576A = {0x90, 0x90};
    } else {
        *(std::array<uint8_t, 1>*)0x42576E = {0x0C};
        *(std::array<uint8_t, 2>*)0x42576A = {0x74, 0x09};
    }

    if (settings.AllowSnorkel) {
        *(std::array<uint8_t, 1>*)0x425723 = {0xFF};
    } else if (settings.BanSnorkel) {
        *(std::array<uint8_t, 1>*)0x425723 = {0x0B};
        *(std::array<uint8_t, 2>*)0x425724 = {0x74, 0x2C};
    } else {
        *(std::array<uint8_t, 1>*)0x425723 = {0x0B};
        *(std::array<uint8_t, 2>*)0x425724 = {0x74, 0x05};
    }

    if (settings.AllowDolphin) {
        *(std::array<uint8_t, 1>*)0x425728 = {0xFF};
    } else if (settings.BanDolphin) {
        *(std::array<uint8_t, 1>*)0x425728 = {0x0E};
        *(std::array<uint8_t, 2>*)0x42572B = {0xEB, 0x25};
    } else {
        *(std::array<uint8_t, 1>*)0x425728 = {0x0E};
        *(std::array<uint8_t, 2>*)0x42572B = {0x8B, 0x86};
    }

    if (settings.AllowDancing) {
        *(std::array<uint8_t, 1>*)0x42575A = {0xFF};
    } else if (settings.BanDancing) {
        *(std::array<uint8_t, 1>*)0x42575A = {0x08};
        *(std::array<uint8_t, 2>*)0x42574D = {0x90, 0x90};
    } else {
        *(std::array<uint8_t, 1>*)0x42575A = {0x08};
        *(std::array<uint8_t, 2>*)0x42574D = {0x75, 0x12};
    }

    if (settings.AllowDigger) {
        *(std::array<uint8_t, 1>*)0x425751 = {0xFF};
    } else if (settings.BanDigger) {
        *(std::array<uint8_t, 1>*)0x425751 = {0x11};
        *(std::array<uint8_t, 2>*)0x42574D = {0x90, 0x90};
    } else {
        *(std::array<uint8_t, 1>*)0x425751 = {0x11};
        *(std::array<uint8_t, 2>*)0x42574D = {0x75, 0x12};
    }

    if (settings.AllowBobsled) {
        *(std::array<uint8_t, 1>*)0x4257E1 = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x4257E1 = {0x0D};
    }

    if (settings.AllowPeashooterZombie) {
        *(std::array<uint8_t, 1>*)0x4257F5 = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x4257F5 = {0x1A};
    }
    if (settings.AllowWallnutZombie) {
        *(std::array<uint8_t, 1>*)0x4257FA = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x4257FA = {0x1B};
    }
    if (settings.AllowJalapenoZombie) {
        *(std::array<uint8_t, 1>*)0x425804 = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x425804 = {0x1C};
    }
    if (settings.AllowGatlingPeaZombie) {
        *(std::array<uint8_t, 1>*)0x425809 = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x425809 = {0x1D};
    }
    if (settings.AllowSquashZombie) {
        *(std::array<uint8_t, 1>*)0x42580E = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x42580E = {0x1E};
    }
    if (settings.AllowTallnutZombie) {
        *(std::array<uint8_t, 1>*)0x4257FF = {0xFF};
    } else {
        *(std::array<uint8_t, 1>*)0x4257FF = {0x1F};
    }
}

ACheckBox* AllowPoolAmbushBox;
ACheckBox* BanPoolAmbushBox;
ACheckBox* AllowSkyAmbushBox;
ACheckBox* BanSkyAmbushBox;
ACheckBox* AllowZomboniBox;
ACheckBox* BanZomboniBox;
ACheckBox* AllowSnorkelBox;
ACheckBox* BanSnorkelBox;
ACheckBox* AllowDolphinBox;
ACheckBox* BanDolphinBox;
ACheckBox* AllowDancingBox;
ACheckBox* BanDancingBox;
ACheckBox* AllowDiggerBox;
ACheckBox* BanDiggerBox;
ACheckBox* AllowBobsledBox;
ACheckBox* AllowPeashooterZombieBox;
ACheckBox* AllowWallnutZombieBox;
ACheckBox* AllowJalapenoZombieBox;
ACheckBox* AllowGatlingPeaZombieBox;
ACheckBox* AllowSquashZombieBox;
ACheckBox* AllowTallnutZombieBox;

void CreateStageModifyGroup(AWindow* window, int LeftEdge, int TopEdge) {
    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;
    constexpr static int BTNWIDTH = 75;

    int y = TopEdge;
    int x = LeftEdge;
    window->AddLabel("", x, y, 194, (SPACE + HEIGHT) * 12);

    x += SPACE;
    window->AddLabel("场地特性", x, y, WIDTH, HEIGHT);

    y += SPACE + HEIGHT;
    AllowPoolAmbushBox = window->AddCheckBox("允许珊瑚", x, y, BTNWIDTH, HEIGHT);
    AllowPoolAmbushBox->SetCheck(settings.AllowPoolAmbush);
    BanPoolAmbushBox = window->AddCheckBox("禁止珊瑚", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanPoolAmbushBox->SetCheck(settings.BanPoolAmbush);

    AllowPoolAmbushBox->Connect([=] {
        settings.AllowPoolAmbush = AllowPoolAmbushBox->GetCheck();
        if (AllowPoolAmbushBox->GetCheck() && BanPoolAmbushBox->GetCheck()) {
            settings.BanPoolAmbush = false;
            BanPoolAmbushBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanPoolAmbushBox->Connect([=] {
        settings.BanPoolAmbush = BanPoolAmbushBox->GetCheck();
        if (BanPoolAmbushBox->GetCheck() && AllowPoolAmbushBox->GetCheck()) {
            settings.AllowPoolAmbush = false;
            AllowPoolAmbushBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowSkyAmbushBox = window->AddCheckBox("允许空降", x, y, BTNWIDTH, HEIGHT);
    AllowSkyAmbushBox->SetCheck(settings.AllowSkyAmbush);
    BanSkyAmbushBox = window->AddCheckBox("禁止空降", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanSkyAmbushBox->SetCheck(settings.BanSkyAmbush);

    AllowSkyAmbushBox->Connect([=] {
        settings.AllowSkyAmbush = AllowSkyAmbushBox->GetCheck();
        if (AllowSkyAmbushBox->GetCheck() && BanSkyAmbushBox->GetCheck()) {
            settings.BanSkyAmbush = false;
            BanSkyAmbushBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanSkyAmbushBox->Connect([=] {
        settings.BanSkyAmbush = BanSkyAmbushBox->GetCheck();
        if (BanSkyAmbushBox->GetCheck() && AllowSkyAmbushBox->GetCheck()) {
            settings.AllowSkyAmbush = false;
            AllowSkyAmbushBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowZomboniBox = window->AddCheckBox("允许冰车", x, y, BTNWIDTH, HEIGHT);
    AllowZomboniBox->SetCheck(settings.AllowZomboni);
    BanZomboniBox = window->AddCheckBox("禁止冰车", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanZomboniBox->SetCheck(settings.BanZomboni);

    AllowZomboniBox->Connect([=] {
        settings.AllowZomboni = AllowZomboniBox->GetCheck();
        if (AllowZomboniBox->GetCheck() && BanZomboniBox->GetCheck()) {
            settings.BanZomboni = false;
            BanZomboniBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanZomboniBox->Connect([=] {
        settings.BanZomboni = BanZomboniBox->GetCheck();
        if (BanZomboniBox->GetCheck() && AllowZomboniBox->GetCheck()) {
            settings.AllowZomboni = false;
            AllowZomboniBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowSnorkelBox = window->AddCheckBox("允许潜水", x, y, BTNWIDTH, HEIGHT);
    AllowSnorkelBox->SetCheck(settings.AllowSnorkel);
    BanSnorkelBox = window->AddCheckBox("禁止潜水", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanSnorkelBox->SetCheck(settings.BanSnorkel);

    AllowSnorkelBox->Connect([=] {
        settings.AllowSnorkel = AllowSnorkelBox->GetCheck();
        if (AllowSnorkelBox->GetCheck() && BanSnorkelBox->GetCheck()) {
            settings.BanSnorkel = false;
            BanSnorkelBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanSnorkelBox->Connect([=] {
        settings.BanSnorkel = BanSnorkelBox->GetCheck();
        if (BanSnorkelBox->GetCheck() && AllowSnorkelBox->GetCheck()) {
            settings.AllowSnorkel = false;
            AllowSnorkelBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowDolphinBox = window->AddCheckBox("允许海豚", x, y, BTNWIDTH, HEIGHT);
    AllowDolphinBox->SetCheck(settings.AllowDolphin);
    BanDolphinBox = window->AddCheckBox("禁止海豚", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanDolphinBox->SetCheck(settings.BanDolphin);

    AllowDolphinBox->Connect([=] {
        settings.AllowDolphin = AllowDolphinBox->GetCheck();
        if (AllowDolphinBox->GetCheck() && BanDolphinBox->GetCheck()) {
            settings.BanDolphin = false;
            BanDolphinBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanDolphinBox->Connect([=] {
        settings.BanDolphin = BanDolphinBox->GetCheck();
        if (BanDolphinBox->GetCheck() && AllowDolphinBox->GetCheck()) {
            settings.AllowDolphin = false;
            AllowDolphinBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowDancingBox = window->AddCheckBox("允许舞王", x, y, BTNWIDTH, HEIGHT);
    AllowDancingBox->SetCheck(settings.AllowDancing);
    BanDancingBox = window->AddCheckBox("禁止舞王", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanDancingBox->SetCheck(settings.BanDancing);

    AllowDancingBox->Connect([=] {
        settings.AllowDancing = AllowDancingBox->GetCheck();
        if (AllowDancingBox->GetCheck() && BanDancingBox->GetCheck()) {
            settings.BanDancing = false;
            BanDancingBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanDancingBox->Connect([=] {
        settings.BanDancing = BanDancingBox->GetCheck();
        if (BanDancingBox->GetCheck() && AllowDancingBox->GetCheck()) {
            settings.AllowDancing = false;
            AllowDancingBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowDiggerBox = window->AddCheckBox("允许矿工", x, y, BTNWIDTH, HEIGHT);
    AllowDiggerBox->SetCheck(settings.AllowDigger);
    BanDiggerBox = window->AddCheckBox("禁止矿工", x + WIDTH, y, BTNWIDTH, HEIGHT);
    BanDiggerBox->SetCheck(settings.BanDigger);

    AllowDiggerBox->Connect([=] {
        settings.AllowDigger = AllowDiggerBox->GetCheck();
        if (AllowDiggerBox->GetCheck() && BanDiggerBox->GetCheck()) {
            settings.BanDigger = false;
            BanDiggerBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    BanDiggerBox->Connect([=] {
        settings.BanDigger = BanDiggerBox->GetCheck();
        if (BanDiggerBox->GetCheck() && AllowDiggerBox->GetCheck()) {
            settings.AllowDigger = false;
            AllowDiggerBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });

    y += SPACE + HEIGHT;
    AllowBobsledBox = window->AddCheckBox("允许雪橇", x, y, BTNWIDTH, HEIGHT);
    AllowBobsledBox->SetCheck(settings.AllowBobsled);
    AllowBobsledBox->Connect([=] { settings.AllowBobsled = AllowBobsledBox->GetCheck(); ApplySpawningRulesModify(); });

    int TempY = y;

    y += SPACE + HEIGHT;
    AllowPeashooterZombieBox = window->AddCheckBox("允许豌豆", x, y, BTNWIDTH, HEIGHT);
    AllowPeashooterZombieBox->SetCheck(settings.AllowPeashooterZombie);
    AllowPeashooterZombieBox->Connect([=] { settings.AllowPeashooterZombie = AllowPeashooterZombieBox->GetCheck(); ApplySpawningRulesModify(); });

    y += SPACE + HEIGHT;
    AllowWallnutZombieBox = window->AddCheckBox("允许坚果", x, y, BTNWIDTH, HEIGHT);
    AllowWallnutZombieBox->SetCheck(settings.AllowWallnutZombie);
    AllowWallnutZombieBox->Connect([=] { settings.AllowWallnutZombie = AllowWallnutZombieBox->GetCheck(); ApplySpawningRulesModify(); });

    y += SPACE + HEIGHT;
    AllowJalapenoZombieBox = window->AddCheckBox("允许辣椒", x, y, BTNWIDTH, HEIGHT);
    AllowJalapenoZombieBox->SetCheck(settings.AllowJalapenoZombie);
    AllowJalapenoZombieBox->Connect([=] { settings.AllowJalapenoZombie = AllowJalapenoZombieBox->GetCheck(); ApplySpawningRulesModify(); });

    y = TempY;
    x += WIDTH;

    y += SPACE + HEIGHT;
    AllowGatlingPeaZombieBox = window->AddCheckBox("允许机枪", x, y, BTNWIDTH, HEIGHT);
    AllowGatlingPeaZombieBox->SetCheck(settings.AllowGatlingPeaZombie);
    AllowGatlingPeaZombieBox->Connect([=] { settings.AllowGatlingPeaZombie = AllowGatlingPeaZombieBox->GetCheck(); ApplySpawningRulesModify(); });

    y += SPACE + HEIGHT;
    AllowSquashZombieBox = window->AddCheckBox("允许倭瓜", x, y, BTNWIDTH, HEIGHT);
    AllowSquashZombieBox->SetCheck(settings.AllowSquashZombie);
    AllowSquashZombieBox->Connect([=] { settings.AllowSquashZombie = AllowSquashZombieBox->GetCheck(); ApplySpawningRulesModify(); });

    y += SPACE + HEIGHT;
    AllowTallnutZombieBox = window->AddCheckBox("允许高坚", x, y, BTNWIDTH, HEIGHT);
    AllowTallnutZombieBox->SetCheck(settings.AllowTallnutZombie);
    AllowTallnutZombieBox->Connect([=] { settings.AllowTallnutZombie = AllowTallnutZombieBox->GetCheck(); ApplySpawningRulesModify(); });
}

std::array<AEdit*, 54> GridTypeEdits;
std::array<AEdit*, 6> RowTypeEdits;

void ApplyStageModify() {
    for (size_t i = 0; i < GridTypeEdits.size(); ++i)
        AGetMainObject()->MRef<uint32_t>(0x168 + 0x4 * i) = std::stoi(GridTypeEdits[i]->GetText());
    for (size_t i = 0; i < RowTypeEdits.size(); ++i)
        AGetMainObject()->MRef<uint32_t>(0x5D8 + 0x4 * i) = std::stoi(RowTypeEdits[i]->GetText());
}

AWindow* StagePageWindow(int pageX, int pageY) {
    auto window = mainWindow.AddWindow(pageX, pageY);

    constexpr static int SPACE = 5;
    constexpr static int WIDTH = 100;
    constexpr static int HEIGHT = 25;
    constexpr static int EDITWIDTH = 25;

    int x = SPACE;
    int y = 0;
    CreateStageModifyGroup(window, x + WIDTH * 4 + SPACE * 6, y);

    x = SPACE;
    APushButton* RowColBtn = window->AddPushButton("0", x, y, EDITWIDTH, HEIGHT);
    std::array<APushButton*, 6> ThisRowBtn;
    std::array<APushButton*, 9> ThisColBtn;

    y += SPACE + HEIGHT;
    for (size_t i = 0; i < ThisRowBtn.size(); ++i) {
        ThisRowBtn[i] = window->AddPushButton(std::format("{}", i + 1), x, y, EDITWIDTH, HEIGHT);
        y += SPACE + HEIGHT;
    }
    y = 0;
    x = SPACE + EDITWIDTH + SPACE;
    for (size_t i = 0; i < ThisColBtn.size(); ++i) {
        ThisColBtn[i] = window->AddPushButton(std::format("{}", i + 1), x, y, EDITWIDTH, HEIGHT);
        x += SPACE + EDITWIDTH;
    }
    APushButton* RowTypeBtn = window->AddPushButton("10", x, y, EDITWIDTH * 2, HEIGHT);

    x = SPACE + EDITWIDTH + SPACE;
    y = SPACE + HEIGHT;
    for (size_t i = 0; i < GridTypeEdits.size(); ++i) {
        GridTypeEdits[i] = window->AddEdit("?", x, y, EDITWIDTH, HEIGHT, ES_CENTER);
        y += SPACE + HEIGHT;
        if (i % 6 == 5) {
            x += SPACE + EDITWIDTH;
            y = SPACE + HEIGHT;
        }
    }
    for (size_t i = 0; i < RowTypeEdits.size(); ++i) {
        RowTypeEdits[i] = window->AddEdit("?", x, y, EDITWIDTH * 2, HEIGHT, ES_CENTER);
        y += SPACE + HEIGHT;
    }

    RowColBtn->Connect([=] {
        FightOrCardUiCheck();
        if (RowColBtn->GetText() == "🌳") {
            RowColBtn->SetText("💧");
            RowTypeBtn->SetText("💧");
            for (size_t i = 0; i < ThisRowBtn.size(); ++i)
                ThisRowBtn[i]->SetText("💧");
            for (size_t i = 0; i < ThisColBtn.size(); ++i)
                ThisColBtn[i]->SetText("💧");
            for (size_t i = 0; i < GridTypeEdits.size(); ++i)
                GridTypeEdits[i]->SetText("3");
            for (size_t i = 0; i < RowTypeEdits.size(); ++i)
                RowTypeEdits[i]->SetText("2");
        } else if (RowColBtn->GetText() == "💧") {
            RowColBtn->SetText("🚫");
            RowTypeBtn->SetText("🚫");
            for (size_t i = 0; i < ThisRowBtn.size(); ++i)
                ThisRowBtn[i]->SetText("🚫");
            for (size_t i = 0; i < ThisColBtn.size(); ++i)
                ThisColBtn[i]->SetText("🚫");
            for (size_t i = 0; i < GridTypeEdits.size(); ++i)
                GridTypeEdits[i]->SetText("2");
            for (size_t i = 0; i < RowTypeEdits.size(); ++i)
                RowTypeEdits[i]->SetText("0");
        } else {
            RowColBtn->SetText("🌳");
            RowTypeBtn->SetText("🌳");
            for (size_t i = 0; i < ThisRowBtn.size(); ++i)
                ThisRowBtn[i]->SetText("🌳");
            for (size_t i = 0; i < ThisColBtn.size(); ++i)
                ThisColBtn[i]->SetText("🌳");
            for (size_t i = 0; i < GridTypeEdits.size(); ++i)
                GridTypeEdits[i]->SetText("1");
            for (size_t i = 0; i < RowTypeEdits.size(); ++i)
                RowTypeEdits[i]->SetText("1");
        }
        ApplyStageModify();
    });
    RowTypeBtn->Connect([=] {
        FightOrCardUiCheck();
        if (RowTypeBtn->GetText() == "🌳") {
            RowTypeBtn->SetText("💧");
            for (size_t i = 0; i < RowTypeEdits.size(); ++i)
                RowTypeEdits[i]->SetText("2");
        } else if (RowTypeBtn->GetText() == "💧") {
            RowTypeBtn->SetText("🚫");
            for (size_t i = 0; i < RowTypeEdits.size(); ++i)
                RowTypeEdits[i]->SetText("0");
        } else {
            RowTypeBtn->SetText("🌳");
            for (size_t i = 0; i < RowTypeEdits.size(); ++i)
                RowTypeEdits[i]->SetText("1");
        }
        ApplyStageModify();
    });
    for (size_t i = 0; i < ThisRowBtn.size(); ++i) {
        ThisRowBtn[i]->Connect([=] {
            FightOrCardUiCheck();
            if (ThisRowBtn[i]->GetText() == "🌳") {
                ThisRowBtn[i]->SetText("💧");
                for (size_t j = 0; j < GridTypeEdits.size(); ++j)
                    if (j % 6 == i)
                        GridTypeEdits[j]->SetText("3");
                RowTypeEdits[i]->SetText("2");
            } else if (ThisRowBtn[i]->GetText() == "💧") {
                ThisRowBtn[i]->SetText("🚫");
                for (size_t j = 0; j < GridTypeEdits.size(); ++j)
                    if (j % 6 == i)
                        GridTypeEdits[j]->SetText("2");
                RowTypeEdits[i]->SetText("0");
            } else {
                ThisRowBtn[i]->SetText("🌳");
                for (size_t j = 0; j < GridTypeEdits.size(); ++j)
                    if (j % 6 == i)
                        GridTypeEdits[j]->SetText("1");
                RowTypeEdits[i]->SetText("1");
            }
            ApplyStageModify();
        });
    }
    for (size_t i = 0; i < ThisColBtn.size(); ++i) {
        ThisColBtn[i]->Connect([=] {
            FightOrCardUiCheck();
            if (ThisColBtn[i]->GetText() == "🌳") {
                ThisColBtn[i]->SetText("💧");
                for (size_t j = 0; j < GridTypeEdits.size(); ++j)
                    if (j / 6 == i)
                        GridTypeEdits[j]->SetText("3");
            } else if (ThisColBtn[i]->GetText() == "💧") {
                ThisColBtn[i]->SetText("🚫");
                for (size_t j = 0; j < GridTypeEdits.size(); ++j)
                    if (j / 6 == i)
                        GridTypeEdits[j]->SetText("2");
            } else {
                ThisColBtn[i]->SetText("🌳");
                for (size_t j = 0; j < GridTypeEdits.size(); ++j)
                    if (j / 6 == i)
                        GridTypeEdits[j]->SetText("1");
            }
            ApplyStageModify();
        });
    }

    y = SPACE + HEIGHT;
    y += 6 * (SPACE + HEIGHT);
    x = SPACE;
    auto StageModifyBtn = window->AddPushButton("一键设置", x, y, WIDTH, HEIGHT);
    StageModifyBtn->Connect([=] {
        FightOrCardUiCheck();
        ApplyStageModify();
    });
    x += 100;
    auto Row6Btn = window->AddPushButton("R6E模式", x, y, WIDTH, HEIGHT);
    Row6Btn->Connect([=] {
        FightOrCardUiCheck();
        for (size_t i = 0; i < GridTypeEdits.size(); ++i)
            GridTypeEdits[i]->SetText("1");
        for (size_t i = 0; i < RowTypeEdits.size(); ++i)
            RowTypeEdits[i]->SetText("1");
        ApplyStageModify();
        AllowSkyAmbushBox->SetCheck(true);
        settings.AllowSkyAmbush = AllowSkyAmbushBox->GetCheck();
        if (AllowSkyAmbushBox->GetCheck() && BanSkyAmbushBox->GetCheck()) {
            settings.BanSkyAmbush = false;
            BanSkyAmbushBox->SetCheck(false);
        }
        AllowPoolAmbushBox->SetCheck(false);
        settings.AllowPoolAmbush = AllowPoolAmbushBox->GetCheck();
        if (AllowPoolAmbushBox->GetCheck() && BanPoolAmbushBox->GetCheck()) {
            settings.BanPoolAmbush = false;
            BanPoolAmbushBox->SetCheck(false);
        }
        AllowSnorkelBox->SetCheck(false);
        settings.AllowSnorkel = AllowSnorkelBox->GetCheck();
        if (AllowSnorkelBox->GetCheck() && BanSnorkelBox->GetCheck()) {
            settings.BanSnorkel = false;
            BanSnorkelBox->SetCheck(false);
        }
        AllowDolphinBox->SetCheck(false);
        settings.AllowDolphin = AllowDolphinBox->GetCheck();
        if (AllowDolphinBox->GetCheck() && BanDolphinBox->GetCheck()) {
            settings.BanDolphin = false;
            BanDolphinBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });
    x += 100;
    auto RoofPoolBtn = window->AddPushButton("RPE模式", x, y, WIDTH, HEIGHT);
    RoofPoolBtn->Connect([=] {
        FightOrCardUiCheck();
        for (size_t i = 0; i < GridTypeEdits.size(); ++i) {
            if ((i % 6 == 2 || i % 6 == 3) && i >= 30)
                GridTypeEdits[i]->SetText("3");
            else
                GridTypeEdits[i]->SetText("1");
        }
        for (size_t i = 0; i < RowTypeEdits.size(); ++i) {
            if (i % 6 == 2 || i % 6 == 3)
                RowTypeEdits[i]->SetText("2");
            else
                RowTypeEdits[i]->SetText("1");
        }
        ApplyStageModify();
        AllowSkyAmbushBox->SetCheck(true);
        settings.AllowSkyAmbush = AllowSkyAmbushBox->GetCheck();
        if (AllowSkyAmbushBox->GetCheck() && BanSkyAmbushBox->GetCheck()) {
            settings.BanSkyAmbush = false;
            BanSkyAmbushBox->SetCheck(false);
        }
        AllowPoolAmbushBox->SetCheck(true);
        settings.AllowPoolAmbush = AllowPoolAmbushBox->GetCheck();
        if (AllowPoolAmbushBox->GetCheck() && BanPoolAmbushBox->GetCheck()) {
            settings.BanPoolAmbush = false;
            BanPoolAmbushBox->SetCheck(false);
        }
        AllowSnorkelBox->SetCheck(true);
        settings.AllowSnorkel = AllowSnorkelBox->GetCheck();
        if (AllowSnorkelBox->GetCheck() && BanSnorkelBox->GetCheck()) {
            settings.BanSnorkel = false;
            BanSnorkelBox->SetCheck(false);
        }
        AllowDolphinBox->SetCheck(true);
        settings.AllowDolphin = AllowDolphinBox->GetCheck();
        if (AllowDolphinBox->GetCheck() && BanDolphinBox->GetCheck()) {
            settings.BanDolphin = false;
            BanDolphinBox->SetCheck(false);
        }
        ApplySpawningRulesModify();
    });
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
    static constexpr int TOPWIDTH = 75;
    static constexpr int TOPHEIGHT = 25;
    int x = 0;

    auto basicPageBtn = mainWindow.AddPushButton("常规设置", x, 0, TOPWIDTH, TOPHEIGHT);
    x += TOPWIDTH;
    auto displayPageBtn = mainWindow.AddPushButton("显示设置", x, 0, TOPWIDTH, TOPHEIGHT);
    x += TOPWIDTH;
    auto keyPageBtn = mainWindow.AddPushButton("按键设置", x, 0, TOPWIDTH, TOPHEIGHT);
    x += TOPWIDTH;
    auto spawnPageBtn = mainWindow.AddPushButton("出怪设置", x, 0, TOPWIDTH, TOPHEIGHT);
    x += TOPWIDTH;
    auto stagePageBtn = mainWindow.AddPushButton("场地设置", x, 0, TOPWIDTH, TOPHEIGHT);
    mainWindow.AddPushButton("", 0, TOPHEIGHT, 635, 5);

    auto basicPage = BasicPageWindow(0, TOPHEIGHT + SPACE);
    auto displayPage = DisplayPageWindow(0, TOPHEIGHT + SPACE);
    auto keyPage = KeyPageWindow(0, TOPHEIGHT + SPACE);
    auto spawnPage = SpawnPageWindow(0, TOPHEIGHT + SPACE);
    auto stagePage = StagePageWindow(0, TOPHEIGHT + SPACE);

    mainWindow.AddLabel("", 5, MAIN_HEIGHT - 78, 624, 44);
    mainWindow.AddLabel("信息:", 10, MAIN_HEIGHT - 78, 40, 44);
    infoLabel = mainWindow.AddLabel("", 50, MAIN_HEIGHT - 78, 579, 44);

    keyPage->Hide();
    displayPage->Hide();
    spawnPage->Hide();
    stagePage->Hide();
    basicPage->Show();

    AConnect(basicPageBtn, [=] {
        keyPage->Hide();
        displayPage->Hide();
        spawnPage->Hide();
        stagePage->Hide();
        basicPage->Show();
    });
    AConnect(displayPageBtn, [=] {
        basicPage->Hide();
        keyPage->Hide();
        spawnPage->Hide();
        stagePage->Hide();
        displayPage->Show();
    });
    AConnect(keyPageBtn, [=] {
        basicPage->Hide();
        displayPage->Hide();
        spawnPage->Hide();
        stagePage->Hide();
        keyPage->Show();
    });
    AConnect(spawnPageBtn, [=] {
        basicPage->Hide();
        displayPage->Hide();
        keyPage->Hide();
        stagePage->Hide();
        spawnPage->Show();
    });
    AConnect(stagePageBtn, [=] {
        basicPage->Hide();
        displayPage->Hide();
        keyPage->Hide();
        spawnPage->Hide();
        stagePage->Show();
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
    aItemCollector.Stop();
    runtime.drawer.ResetIndexArea();
    if (settings.AverageRowSpawn)
        AAverageSpawn();
    aReplay.SetMaxSaveCnt(INT_MAX);
    aReplay.SetShowInfo(false);
    // ApplySpawningRulesModify();
    zombieListInfo_update();
    if (settings.AutoRecordOnGameStart && AMRef<int>(0x6A9EC0, 0x7F8) != AAsm::CHALLENGE_ICE) {
        compressor->SetFilePath(settings.savePath + std::string("/") + GetCurTimeStr() + ".7z");
        aReplay.StartRecord(std::round(settings.recordTickInterval));
        Info("Replay : 开始录制");
    }
    if (settings.ShowMe) {
        tickShowMe.Start();
    } else {
        tickShowMe.Stop();
    }
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i].Stop();
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i] = AConnect(keyBindings[i], funcs[i]);
});

AOnExitFight({
    runtime.drawer.ResetIndexArea();
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i].Stop();
    if (spawnseedEdit)
        spawnseedEdit->SetText("No Seed");
    for (size_t i = 0; i < GridTypeEdits.size(); ++i)
        if (GridTypeEdits[i])
            GridTypeEdits[i]->SetText("?");
    for (size_t i = 0; i < RowTypeEdits.size(); ++i)
        if (RowTypeEdits[i])
            RowTypeEdits[i]->SetText("?");
});

// ALogger<AConsole> ConsoleLogger;

// 不进家
void __stdcall AsmCallBack0x413400(AAsmCodeContext* context) {
    if (!settings.EnterHousePause)
        return;
    context->eip = 0x4138C9;
    Paused = true;
    PausedSlowed = false;
    ASetAdvancedPause(Paused, false, 0);
    CreateCaption("The Zombies Ate Your Brains!", {BOTTOMFAST, 1000});
}


// 主体
void AScript() {
    ASetReloadMode(AReloadMode::MAIN_UI_OR_FIGHT_UI);
    Lock(AGetPvzBase()->MPtr(0x82C)->MRef<int>(0x20), AGetPvzBase()->LevelId(), settings.ReadOnly);
    AGetInternalLogger()->SetLevel({});
    // ASetInternalLogger(ConsoleLogger);
    runtime.maid.Apply();

    // 不进家
    AInsertUniqueAsmCode(0x413400, AsmCallBack0x413400);

    runtime.drawer.InitPainterStyle();
    runtime.activationMarker.painter.SetFontSize(17);
    runtime.activationMarker.ApplySettings(settings);

    tickFight.Start([=] { DanceCheat(); runtime.warning.JackPause(); runtime.warning.BalloonPause(); });
    tickPainter.Start([=] { runtime.activationMarker.ApplySettings(settings); runtime.drawer.DrawInfo(settings, runtime.clock); runtime.drawer.DrawIndex(settings); runtime.activationMarker.Draw(runtime.clock); }, ATickRunner::PAINT);
    tickGlobal.Start([=] { runtime.clock.Update(); runtime.smartRemove.Tick(); runtime.warning.BalloonCaption(); }, ATickRunner::GLOBAL);

    runtime.activationMarker.Start(runtime.clock);

    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i].Stop();
    for (size_t i = 0; i < keyHandles.size(); ++i)
        keyHandles[i] = AConnect(keyBindings[i], funcs[i]);

    if (spawnseedEdit)
        spawnseedEdit->SetText(std::format("{:08X}", AGetMainObject()->MRef<uint32_t>(0x561C)));
    for (size_t i = 0; i < GridTypeEdits.size(); ++i)
        if (GridTypeEdits[i])
            GridTypeEdits[i]->SetText(std::format("{}", AGetMainObject()->MRef<uint32_t>(0x168 + 0x4 * i)));
    for (size_t i = 0; i < RowTypeEdits.size(); ++i)
        if (RowTypeEdits[i])
            RowTypeEdits[i]->SetText(std::format("{}", AGetMainObject()->MRef<uint32_t>(0x5D8 + 0x4 * i)));
}
