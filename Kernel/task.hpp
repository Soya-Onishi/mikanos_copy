#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <deque>
#include <optional>

#include "message.hpp"
#include "error.hpp"

using TaskFunc = void (uint64_t, int64_t);

class TaskManager;

struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1;
  uint64_t cs, ss, fs, gs;
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
  std::array<uint8_t, 512> fxsave_area;
} __attribute__((packed));

class Task {
  public:
    static const int kDefaultLevel = 1;
    static const size_t kDefaultStackBytes = 4096;

    Task(uint64_t id);
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();
    uint64_t ID() const;
    bool Running() const;
    unsigned int Level() const;
    Task& Sleep();
    Task& Wakeup();
    void SendMessage(const Message& message);
    std::optional<Message> ReceiveMessage();

  private:
    uint64_t id_;
    std::vector<uint64_t> stack_;
    alignas(16) TaskContext context_;
    std::deque<Message> messages_{};
    unsigned int level_{kDefaultLevel};
    bool running_{false};

    Task& SetLevel(int level) { level_ = level; return *this; };
    Task& SetRunning(bool running) { running_ = running; return *this; };

    friend TaskManager;
};

class TaskManager {
  public:
    static const int kMaxLevel = 3;

    TaskManager();
    Task& NewTask();
    Task* RotateRunQueue(bool current_sleep);
    void SwitchTask(const TaskContext& context);
    Task& CurrentTask();
    Error SendMessage(uint64_t id, const Message& message);    

    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    void Wakeup(Task* task, int level = -1);
    Error Wakeup(uint64_t id, int level = -1);

  private:
    std::vector<std::unique_ptr<Task>> tasks_{};
    uint64_t latest_id_{0};
    std::array<std::deque<Task*>, kMaxLevel + 1> running_{};
    int current_level_{kMaxLevel};
    bool level_changed_{false};

    void ChangeLevelRunning(Task* task, int level);
};

inline TaskManager* task_manager = nullptr;

void InitializeTask();