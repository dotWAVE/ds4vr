#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <openvr_driver.h>

namespace ds4vr {

namespace {

Config g_config;

void Log(const char *msg)
{
    if (vr::VRDriverLog()) vr::VRDriverLog()->Log(msg);
}

std::string Trim(const std::string &s)
{
    auto lo = s.find_first_not_of(" \t\r\n");
    if (lo == std::string::npos) return {};
    auto hi = s.find_last_not_of(" \t\r\n");
    return s.substr(lo, hi - lo + 1);
}

std::string ToLower(std::string s)
{
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

using IniMap = std::unordered_map<std::string,
              std::unordered_map<std::string, std::string>>;

IniMap ParseIni(const std::string &path)
{
    IniMap m;
    std::ifstream f(path);
    if (!f.is_open()) return m;

    std::string section;
    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = ToLower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = ToLower(Trim(line.substr(0, eq)));
        auto val = Trim(line.substr(eq + 1));
        // strip inline comment
        auto sc = val.find(';');
        if (sc != std::string::npos) val = Trim(val.substr(0, sc));
        m[section][key] = val;
    }
    return m;
}

float GetFloat(const IniMap &m, const char *sec, const char *key, float def)
{
    auto si = m.find(sec);
    if (si == m.end()) return def;
    auto ki = si->second.find(key);
    if (ki == si->second.end()) return def;
    return std::strtof(ki->second.c_str(), nullptr);
}

bool GetBool(const IniMap &m, const char *sec, const char *key, bool def)
{
    auto si = m.find(sec);
    if (si == m.end()) return def;
    auto ki = si->second.find(key);
    if (ki == si->second.end()) return def;
    auto v = ToLower(ki->second);
    if (v == "true" || v == "1" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "no") return false;
    return def;
}

std::string GetStr(const IniMap &m, const char *sec, const char *key, const char *def)
{
    auto si = m.find(sec);
    if (si == m.end()) return def;
    auto ki = si->second.find(key);
    if (ki == si->second.end()) return def;
    return ki->second;
}

void ParseVec3(const IniMap &m, const char *sec, const char *key, float out[3])
{
    auto si = m.find(sec);
    if (si == m.end()) return;
    auto ki = si->second.find(key);
    if (ki == si->second.end()) return;
    float a, b, c;
    if (std::sscanf(ki->second.c_str(), "%f,%f,%f", &a, &b, &c) == 3) {
        out[0] = a; out[1] = b; out[2] = c;
    }
}

ButtonTarget ParseTarget(const std::string &val)
{
    auto v = ToLower(val);
    if (v == "x" || v == "a" || v == "face_primary"  || v == "face_lower") return ButtonTarget::FaceLower;
    if (v == "y" || v == "b" || v == "face_secondary" || v == "face_upper") return ButtonTarget::FaceUpper;
    if (v == "grip")   return ButtonTarget::Grip;
    if (v == "system") return ButtonTarget::System;
    return ButtonTarget::Unbound;
}

ButtonTarget GetTarget(const IniMap &m, const char *sec, const char *key, ButtonTarget def)
{
    auto s = GetStr(m, sec, key, "");
    if (s.empty()) return def;
    return ParseTarget(s);
}

} // namespace

const Config &Cfg() { return g_config; }

bool LoadConfig(const std::string &path)
{
    auto m = ParseIni(path);
    if (m.empty()) {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "ds4vr: config file not found or empty: %s (using defaults)\n", path.c_str());
        Log(buf);
        return false;
    }

    Config &c = g_config;

    c.stick_deadzone           = GetFloat(m, "sticks",   "deadzone",         c.stick_deadzone);
    c.response_curve           = GetFloat(m, "sticks",   "response_curve",   c.response_curve);
    c.trigger_click_threshold  = GetFloat(m, "trigger",  "click_threshold",  c.trigger_click_threshold);
    c.imu_beta                 = GetFloat(m, "imu",      "beta",             c.imu_beta);
    c.t_dtap_ms                = GetFloat(m, "engage",   "t_dtap_ms",        c.t_dtap_ms);

    ParseVec3(m, "armmodel", "shoulder_offset_left",  c.shoulder_left);
    ParseVec3(m, "armmodel", "shoulder_offset_right", c.shoulder_right);
    c.forearm_nominal = GetFloat(m, "armmodel", "forearm_nominal", c.forearm_nominal);
    c.elbow_lift_max  = GetFloat(m, "armmodel", "elbow_lift_max",  c.elbow_lift_max);
    c.pitch_lo_deg    = GetFloat(m, "armmodel", "pitch_lo_deg",    c.pitch_lo_deg);
    c.pitch_hi_deg    = GetFloat(m, "armmodel", "pitch_hi_deg",    c.pitch_hi_deg);
    c.pose_blend_ms         = GetFloat(m, "armmodel", "pose_blend_ms",         c.pose_blend_ms);
    c.hmd_pitch_influence   = GetFloat(m, "armmodel", "hmd_pitch_influence",   c.hmd_pitch_influence);

    c.reach_rest       = GetFloat(m, "reach", "rest",        c.reach_rest);
    c.rest_outward_deg = GetFloat(m, "reach", "outward_deg", c.rest_outward_deg);
    c.reach_default  = GetFloat(m, "reach", "default",  c.reach_default);
    c.reach_min      = GetFloat(m, "reach", "d_min",    c.reach_min);
    c.reach_max      = GetFloat(m, "reach", "d_max",    c.reach_max);
    c.reach_rate     = GetFloat(m, "reach", "rate_mps", c.reach_rate);
    c.reach_deadzone = GetFloat(m, "reach", "deadzone", c.reach_deadzone);
    c.reach_invert   = GetBool (m, "reach", "invert",   c.reach_invert);

    c.grip_toggle = GetBool(m, "mapping", "grip_toggle", c.grip_toggle);

    c.left_dpad_up    = GetTarget(m, "mapping.left", "dpad_up",    c.left_dpad_up);
    c.left_dpad_left  = GetTarget(m, "mapping.left", "dpad_left",  c.left_dpad_left);
    c.left_dpad_down  = GetTarget(m, "mapping.left", "dpad_down",  c.left_dpad_down);
    c.left_dpad_right = GetTarget(m, "mapping.left", "dpad_right", c.left_dpad_right);

    c.right_cross    = GetTarget(m, "mapping.right", "cross",    c.right_cross);
    c.right_circle   = GetTarget(m, "mapping.right", "circle",   c.right_circle);
    c.right_square   = GetTarget(m, "mapping.right", "square",   c.right_square);
    c.right_triangle = GetTarget(m, "mapping.right", "triangle", c.right_triangle);

    auto lm = ToLower(GetStr(m, "haptics", "left_motor",  "large"));
    auto rm = ToLower(GetStr(m, "haptics", "right_motor", "small"));
    c.haptic_left_large  = (lm == "large");
    c.haptic_right_large = (rm == "large");

    char buf[256];
    std::snprintf(buf, sizeof(buf), "ds4vr: config loaded from %s\n", path.c_str());
    Log(buf);
    return true;
}

std::string GetIniPath()
{
    // DLL lives at <driver_root>/bin/win64/driver_ds4vr.dll.
    // INI is at <driver_root>/ds4vr.ini.
    WCHAR wpath[MAX_PATH]{};
    HMODULE hMod = nullptr;

    // Grab our own DLL's module handle by address.
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetIniPath),
        &hMod);

    if (!hMod || GetModuleFileNameW(hMod, wpath, MAX_PATH) == 0) {
        return "ds4vr.ini";
    }

    // Convert wide → narrow (ASCII paths only; fine for a local dev machine).
    char narrow[MAX_PATH]{};
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, narrow, MAX_PATH, nullptr, nullptr);

    std::string p(narrow);
    for (int i = 0; i < 3; ++i) {
        auto pos = p.find_last_of("\\/");
        if (pos == std::string::npos) break;
        p = p.substr(0, pos);
    }
    return p + "\\ds4vr.ini";
}

} // namespace ds4vr
