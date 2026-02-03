#include "runtime_state.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

RuntimeFlags g_runtime_flags;
DataSession g_current_session;

// Create a new data session
bool CreateNewSession() {
  auto now = std::chrono::system_clock::now();
  auto now_time = std::chrono::system_clock::to_time_t(now);

  std::ostringstream session_id;
  session_id << std::put_time(std::localtime(&now_time), "%Y%m%d_%H%M%S");

  g_current_session.session_id = session_id.str();
  g_current_session.output_dir = "runs/" + session_id.str();
  g_current_session.start_time = now;
  g_current_session.active = true;

  // Create directory
  std::string mkdir_cmd = "mkdir -p " + g_current_session.output_dir;
  int ret = system(mkdir_cmd.c_str());

  if (ret == 0) {
    std::cout << "[Session] 新会话已创建: " << g_current_session.output_dir << std::endl;
    return true;
  } else {
    std::cerr << "[Session] 创建目录失败: " << g_current_session.output_dir << std::endl;
    g_current_session.active = false;
    return false;
  }
}
