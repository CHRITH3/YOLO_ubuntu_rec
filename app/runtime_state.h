#ifndef RUNTIME_STATE_H_
#define RUNTIME_STATE_H_

#include <chrono>
#include <string>

// Runtime flags for controlling recording behavior
struct RuntimeFlags {
  bool record_enabled = false;  // 录制开关，默认关闭
};

// Data session for recording
struct DataSession {
  std::string session_id;           // 会话ID (时间戳格式)
  std::string output_dir;           // 输出目录路径
  std::chrono::system_clock::time_point start_time;  // 会话开始时间
  bool active = false;              // 会话是否激活
};

extern RuntimeFlags g_runtime_flags;
extern DataSession g_current_session;

bool CreateNewSession();

#endif  // RUNTIME_STATE_H_
