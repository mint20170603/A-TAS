/*
 * @Author: TemporalShards
 * @Date: 2024-01-31 22:14:06
 * @Description：A-TAS 组合键的按键连接
 */
#pragma once
#ifndef __KEYCONNECTOR_H__
#define __KEYCONNECTOR_H__

#include "avz.h"

// 判断两个向量是否具有包含关系
template <typename T>
bool __VectorsContain(const std::vector<T>& vec1, const std::vector<T>& vec2) {
    auto check = [](const std::vector<T>& vec1, const std::vector<T>& vec2) -> bool {
        std::unordered_set<T> count(vec1.begin(), vec1.end());
        for (const auto& ele : vec2) {
            if (!count.contains(ele)) {
                return false;
            }
        }
        return true;
    };

    return check(vec1, vec2) || check(vec2, vec1);
}

class MyKeyManager : public AOrderedExitFightHook<-1> {
public:
    static __AKeyManager::KeyState ToValidKey(std::vector<AKey> keyVec) {
        if (keyVec.empty()) {
            MessageBoxW(NULL, AStrToWstr("您尚未输入任何按键!").c_str(), L"A-TAS KeyManager", MB_OK);
            return __AKeyManager::UNKNOWN;
        }
        auto iter = _keyMap.find(keyVec);
        if (iter == _keyMap.end() || iter->second.IsStopped()) {
            for (const auto& [key, handle] : _keyMap) {
                if (__VectorsContain(key, keyVec) && !handle.IsStopped()) {
                    MessageBoxW(NULL, AStrToWstr(std::format("按键组合{}与{}具有包含关系，脚本可能会出现预期外的行为，为确保按键连接正确的操作，建议更换按键组合！", KeyVecToStr(keyVec), KeyVecToStr(key))).c_str(), L"A-TAS KeyManager", MB_OK);
                    // return __AKeyManager::REPEAT;
                }
            }
            return __AKeyManager::VALID;
        } else {
            MessageBoxW(NULL, AStrToWstr(std::format("按键{}已被绑定", KeyVecToStr(keyVec))).c_str(), L"A-TAS KeyManager", MB_OK);
            return __AKeyManager::REPEAT;
        }
    }

    static void AddKey(std::vector<AKey> keyVec, AConnectHandle connectHandle) {
        _keyMap.emplace(keyVec, connectHandle);
    }

    static std::string KeyVecToStr(std::vector<AKey> keyVec) {
        std::string result;
        for (int i = 0; i < keyVec.size(); ++i) {
            result += __AKeyManager::ToName(keyVec[i]);
            if (i < keyVec.size() - 1)
                result += "+";
            else
                result += " ";
        }
        return result;
    }

protected:
    static std::map<std::vector<AKey>, AConnectHandle> _keyMap;

    virtual void _ExitFight() override {
        _keyMap.clear();
    }
};
MyKeyManager __mykm; // AStateHook

std::map<std::vector<AKey>, AConnectHandle> MyKeyManager::_keyMap;

// 虚拟键码表, 与AvZ库__AKeyManager::_keyVec中的按键保持一致
static const std::unordered_map<std::string, AKey> VirtualKeyMap = {
    {"LBUTTON", VK_LBUTTON},
    {"RBUTTON", VK_RBUTTON},
    {"CANCEL", VK_CANCEL},
    {"MBUTTON", VK_MBUTTON},
    {"XBUTTON1", VK_XBUTTON1},
    {"XBUTTON2", VK_XBUTTON2},
    {"BACK", VK_BACK},
    {"TAB", VK_TAB},
    {"CLEAR", VK_CLEAR},
    {"RETURN", VK_RETURN},
    {"SHIFT", VK_SHIFT},
    {"CONTROL", VK_CONTROL},
    {"MENU", VK_MENU},
    {"PAUSE", VK_PAUSE},
    {"CAPITAL", VK_CAPITAL},
    {"KANA", VK_KANA},
    {"HANGEUL", VK_HANGEUL},
    {"HANGUL", VK_HANGUL},
    {"JUNJA", VK_JUNJA},
    {"FINAL", VK_FINAL},
    {"HANJA", VK_HANJA},
    {"KANJI", VK_KANJI},
    {"ESCAPE", VK_ESCAPE},
    {"CONVERT", VK_CONVERT},
    {"NONCONVERT", VK_NONCONVERT},
    {"ACCEPT", VK_ACCEPT},
    {"MODECHANGE", VK_MODECHANGE},
    {"SPACE", VK_SPACE},
    {"PRIOR", VK_PRIOR},
    {"NEXT", VK_NEXT},
    {"END", VK_END},
    {"HOME", VK_HOME},
    {"LEFT", VK_LEFT},
    {"UP", VK_UP},
    {"RIGHT", VK_RIGHT},
    {"DOWN", VK_DOWN},
    {"SELECT", VK_SELECT},
    {"PRINT", VK_PRINT},
    {"EXECUTE", VK_EXECUTE},
    {"SNAPSHOT", VK_SNAPSHOT},
    {"INSERT", VK_INSERT},
    {"DELETE", VK_DELETE},
    {"HELP", VK_HELP},
    {"0", '0'},
    {"1", '1'},
    {"2", '2'},
    {"3", '3'},
    {"4", '4'},
    {"5", '5'},
    {"6", '6'},
    {"7", '7'},
    {"8", '8'},
    {"9", '9'},
    {"A", 'A'},
    {"B", 'B'},
    {"C", 'C'},
    {"D", 'D'},
    {"E", 'E'},
    {"F", 'F'},
    {"G", 'G'},
    {"H", 'H'},
    {"I", 'I'},
    {"J", 'J'},
    {"K", 'K'},
    {"L", 'L'},
    {"M", 'M'},
    {"N", 'N'},
    {"O", 'O'},
    {"P", 'P'},
    {"Q", 'Q'},
    {"R", 'R'},
    {"S", 'S'},
    {"T", 'T'},
    {"U", 'U'},
    {"V", 'V'},
    {"W", 'W'},
    {"X", 'X'},
    {"Y", 'Y'},
    {"Z", 'Z'},
    {"LWIN", VK_LWIN},
    {"RWIN", VK_RWIN},
    {"APPS", VK_APPS},
    {"SLEEP", VK_SLEEP},
    {"NUMPAD0", VK_NUMPAD0},
    {"NUMPAD1", VK_NUMPAD1},
    {"NUMPAD2", VK_NUMPAD2},
    {"NUMPAD3", VK_NUMPAD3},
    {"NUMPAD4", VK_NUMPAD4},
    {"NUMPAD5", VK_NUMPAD5},
    {"NUMPAD6", VK_NUMPAD6},
    {"NUMPAD7", VK_NUMPAD7},
    {"NUMPAD8", VK_NUMPAD8},
    {"NUMPAD9", VK_NUMPAD9},
    {"MULTIPLY", VK_MULTIPLY},
    {"ADD", VK_ADD},
    {"SEPARATOR", VK_SEPARATOR},
    {"SUBTRACT", VK_SUBTRACT},
    {"DECIMAL", VK_DECIMAL},
    {"DIVIDE", VK_DIVIDE},
    {"F1", VK_F1},
    {"F2", VK_F2},
    {"F3", VK_F3},
    {"F4", VK_F4},
    {"F5", VK_F5},
    {"F6", VK_F6},
    {"F7", VK_F7},
    {"F8", VK_F8},
    {"F9", VK_F9},
    {"F10", VK_F10},
    {"F11", VK_F11},
    {"F12", VK_F12},
    {"F13", VK_F13},
    {"F14", VK_F14},
    {"F15", VK_F15},
    {"F16", VK_F16},
    {"F17", VK_F17},
    {"F18", VK_F18},
    {"F19", VK_F19},
    {"F20", VK_F20},
    {"F21", VK_F21},
    {"F22", VK_F22},
    {"F23", VK_F23},
    {"F24", VK_F24},
    {"NUMLOCK", VK_NUMLOCK},
    {"SCROLL", VK_SCROLL},
    {"LSHIFT", VK_LSHIFT},
    {"RSHIFT", VK_RSHIFT},
    {"LCONTROL", VK_LCONTROL},
    {"RCONTROL", VK_RCONTROL},
    {"LMENU", VK_LMENU},
    {"RMENU", VK_RMENU},
    {"BROWSER_BACK", VK_BROWSER_BACK},
    {"BROWSER_FORWARD", VK_BROWSER_FORWARD},
    {"BROWSER_REFRESH", VK_BROWSER_REFRESH},
    {"BROWSER_STOP", VK_BROWSER_STOP},
    {"BROWSER_SEARCH", VK_BROWSER_SEARCH},
    {"BROWSER_FAVORITES", VK_BROWSER_FAVORITES},
    {"BROWSER_HOME", VK_BROWSER_HOME},
    {"VOLUME_MUTE", VK_VOLUME_MUTE},
    {"VOLUME_DOWN", VK_VOLUME_DOWN},
    {"VOLUME_UP", VK_VOLUME_UP},
    {"MEDIA_NEXT_TRACK", VK_MEDIA_NEXT_TRACK},
    {"MEDIA_PREV_TRACK", VK_MEDIA_PREV_TRACK},
    {"MEDIA_STOP", VK_MEDIA_STOP},
    {"MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE},
    {"LAUNCH_MAIL", VK_LAUNCH_MAIL},
    {"LAUNCH_MEDIA_SELECT", VK_LAUNCH_MEDIA_SELECT},
    {"LAUNCH_APP1", VK_LAUNCH_APP1},
    {"LAUNCH_APP2", VK_LAUNCH_APP2},
    {"OEM_1", VK_OEM_1},
    {"OEM_2", VK_OEM_2},
    {"OEM_3", VK_OEM_3},
    {"OEM_4", VK_OEM_4},
    {"OEM_5", VK_OEM_5},
    {"OEM_6", VK_OEM_6},
    {"OEM_7", VK_OEM_7},
    {"OEM_8", VK_OEM_8},
    {"PROCESSKEY", VK_PROCESSKEY},
    {"PACKET", VK_PACKET},
    {"ATTN", VK_ATTN},
    {"CRSEL", VK_CRSEL},
    {"EXSEL", VK_EXSEL},
    {"EREOF", VK_EREOF},
    {"PLAY", VK_PLAY},
    {"ZOOM", VK_ZOOM},
    {"NONAME", VK_NONAME},
    {"PA1", VK_PA1},
    {"OEM_CLEAR", VK_OEM_CLEAR},

    // 兼容旧版的部分按键
    {"BACKSPACE", VK_BACK},
    {"CTRL", VK_CONTROL},
};

// 将std::string中的所有英文大写
inline std::string ToUpper(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return result;
}

// 将std::string对应的键转为键值
inline AKey StrKeyToValue(const std::string& keystr) {
    auto str = ToUpper(keystr);
    if (!VirtualKeyMap.contains(str)) {
        MessageBoxW(NULL, AStrToWstr(keystr + "不存在").c_str(), L"A-TAS KeyManager", MB_OK);
        return -1;
    } else
        return VirtualKeyMap.at(str);
}

// 以"+"为分隔符，将std::string分为多个std::string，并将它们的键值传入一个std::vector中作为返回值
inline std::vector<AKey> StrToKeyValue(const std::string& str) {
    std::vector<AKey> result;
    std::istringstream iss(str);
    std::string temp;
    AKey val;
    while (std::getline(iss, temp, '+')) {
        val = StrKeyToValue(temp);
        if (val >= 0)
            result.push_back(val);
    }
    return result;
}
inline std::chrono::milliseconds GetInitialDelayFromSystem() {
    UINT keyboardDelay = 0;
    if (SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &keyboardDelay, 0)) {
        // The system returns a value from 0 to 3.
        // The actual delay is (keyboardDelay + 1) * 250 ms.
        return std::chrono::milliseconds((keyboardDelay + 1) * 250);
    }
    // Fallback default
    return std::chrono::milliseconds(500);
}

inline std::chrono::milliseconds GetRepeatIntervalFromSystem() {
    UINT keyboardSpeed = 0;
    if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &keyboardSpeed, 0)) {
        // keyboardSpeed is between 0 (slow) and 31 (fast).
        // For example, 0 yields ~2.5 repeats per second (≈400 ms interval)
        // and 31 yields ~30 repeats per second (≈33 ms interval).
        const int slowInterval = 400; // ms for keyboardSpeed 0
        const int fastInterval = 33;  // ms for keyboardSpeed 31
        int repeatIntervalMs = slowInterval - ((slowInterval - fastInterval) * keyboardSpeed / 31);
        return std::chrono::milliseconds(repeatIntervalMs);
    }
    // Fallback default
    return std::chrono::milliseconds(100);
}

// 支持组合键的AConnect
// AConnect 第一个参数传入一个 std::vector<AKey>, 包含虚拟按键对应的键值
// 注意，为了确保按键连接到正确的操作，传入的std::vector<AKey>不能与之前的按键组合具有包含关系
// 示例:
// AConnect(std::vector<AKey>{VK_CONTROL, VK_SHIFT, 'P'}, [=] { AMsgBox::Show("VK_CONTROL+VK_SHIFT+'P'"); });
// 当pvz窗口为顶层窗口时，依次按下ctrl，shift，p，会弹出一个对话框
// 如果再添加一个链接:
// AConnect(std::vector<AKey>{VK_CONTROL, VK_SHIFT, 'P', 'Z'}, [=] { AMsgBox::Show("VK_CONTROL+VK_SHIFT+'P'+'Z'"); });
// 就会报错，是因为先按下ctrl，shift，p时会执行第一个AConnect，再此基础上再按下Z才会执行第二个AConnect，可能导致操作的冲突
template <typename Op>
    requires __AIsCoOpOrOp<Op>
AConnectHandle AConnect(std::vector<AKey> keyVec, Op&& op, int priority = 0, int runMode = ATickRunner::GLOBAL) {
    if (MyKeyManager::ToValidKey(keyVec) != __AKeyManager::VALID)
        return AConnectHandle();

    auto nextTime = std::chrono::steady_clock::time_point::max();
    bool isRepeat = false;
    auto pvzHandle = AGetPvzBase()->Hwnd();
    auto keyFunc = [keyVec, nextTime, isRepeat, pvzHandle] mutable -> bool {
        if (GetForegroundWindow() != pvzHandle)
            return false;

        for (const auto& key : keyVec) {
            if ((GetAsyncKeyState(key) & 0x8000) == 0) {
                isRepeat = false;
                nextTime = std::chrono::steady_clock::time_point::max();
                return false;
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (!isRepeat) {
            nextTime = now + GetInitialDelayFromSystem();
            isRepeat = true;
            return true;
        } else if (now >= nextTime) {
            nextTime = now + GetRepeatIntervalFromSystem();
            return true;
        }
        return false;
    };
    auto handle = ::AConnect(std::move(keyFunc), std::forward<Op>(op), runMode, priority);
    MyKeyManager::AddKey(keyVec, handle);
    return handle;
}

// 支持组合键的AConnect
// AConnect 第一个参数传入一个 std::string, 包含按键名称或组合键，不区分大小写
// 注意，为了确保按键连接到正确的操作，传入的std::string不能与之前的按键组合具有包含关系
// 示例:
// AConnect("ctrl+shift+p", [=] { AMsgBox::Show("ctrl+shift+p"); });
// 当pvz窗口为顶层窗口时，依次按下ctrl，shift，p，会弹出一个对话框
// 如果再添加一个链接:
// AConnect("ctrl+shift+p+Z", [=] { AMsgBox::Show("VK_CONTROL+VK_SHIFT+'P'+'Z'"); });
// 就会报错，是因为先按下ctrl，shift，p时会执行第一个AConnect，再此基础上再按下Z才会执行第二个AConnect，可能导致操作的冲突
template <typename Op>
    requires __AIsCoOpOrOp<Op>
AConnectHandle AConnect(std::string keyStr, Op&& op, int priority = 0, int runMode = ATickRunner::GLOBAL) {
    if (keyStr.empty())
        return AConnectHandle();

    std::vector<AKey> keyVec = StrToKeyValue(keyStr);
    if (keyVec.empty())
        return AConnectHandle();

    return AConnect(keyVec, std::forward<Op>(op), priority, runMode);
}

#endif //!__KEYCONNECTOR_H__