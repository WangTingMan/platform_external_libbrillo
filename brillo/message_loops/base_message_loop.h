// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_MESSAGE_LOOPS_BASE_MESSAGE_LOOP_H_
#define LIBBRILLO_BRILLO_MESSAGE_LOOPS_BASE_MESSAGE_LOOP_H_

// BaseMessageLoop is a brillo::MessageLoop implementation based on
// base::MessageLoopForIO. This allows to mix new code using
// brillo::MessageLoop and legacy code using base::MessageLoopForIO in the
// same thread and share a single main loop. This disadvantage of using this
// class is a less efficient implementation of CancelTask() for delayed tasks
// since base::MessageLoopForIO doesn't provide a way to remove the event.

#include <map>
#include <memory>
#include <string>

#include <base/location.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_pump_for_io.h>
#include <base/time/time.h>
#if __has_include(<gtest/gtest_prod.h>)
#include <gtest/gtest_prod.h>
#endif

#include <brillo/brillo_export.h>
#include <brillo/message_loops/message_loop.h>

namespace brillo {

class BRILLO_EXPORT BaseMessageLoop : public MessageLoop {
 public:
  // Construct a base::MessageLoopForIO message loop instance and use it as
  // the default message loop for this thread.
  BaseMessageLoop();

  // Construct a brillo::BaseMessageLoop using the passed base::MessageLoopForIO
  // instance.
  explicit BaseMessageLoop(base::MessageLoopForIO* base_loop);
  ~BaseMessageLoop() override;

  // MessageLoop overrides.
  TaskId PostDelayedTask(const base::Location& from_here,
                         base::OnceClosure task,
                         base::TimeDelta delay) override;
  using MessageLoop::PostDelayedTask;
  bool CancelTask(TaskId task_id) override;
  bool RunOnce(bool may_block) override;
  void Run() override;
  void BreakLoop() override;

  // Returns a callback that will quit the current message loop. If the message
  // loop is not running, an empty (null) callback is returned.
  base::RepeatingClosure QuitClosure() const;

 private:
  FRIEND_TEST(BaseMessageLoopTest, ParseBinderMinor);

  static const int kInvalidMinor;
  static const int kUninitializedMinor;

  // Parses the contents of the file /proc/misc passed in |file_contents| and
  // returns the minor device number reported for binder. On error or if not
  // found, returns kInvalidMinor.
  static int ParseBinderMinor(const std::string& file_contents);

  // Called by base::MessageLoopForIO when is time to call the callback
  // scheduled with Post*Task() of id |task_id|, even if it was canceled.
  void OnRanPostedTask(MessageLoop::TaskId task_id);

  // Return a new unused task_id.
  TaskId NextTaskId();

  // Returns binder minor device number.
  unsigned int GetBinderMinor();

  struct DelayedTask {
    base::Location location;

    MessageLoop::TaskId task_id;
    base::OnceClosure closure;
  };

  // The base::MessageLoopForIO instance owned by this class, if any. This
  // is declared first in this class so it is destroyed last.
  std::unique_ptr<base::MessageLoopForIO> owned_base_loop_;

  // Tasks blocked on a timeout.
  std::map<MessageLoop::TaskId, DelayedTask> delayed_tasks_;

  // Flag to mark that we should run the message loop only one iteration.
  bool run_once_{false};

  // The last used TaskId. While base::MessageLoopForIO doesn't allow to cancel
  // delayed tasks, we handle that functionality by not running the callback
  // if it fires at a later point.
  MessageLoop::TaskId last_id_{kTaskIdNull};

  // The pointer to the libchrome base::MessageLoopForIO we are wrapping with
  // this interface. If the instance was created from this object, this will
  // point to that instance.
  base::MessageLoopForIO* base_loop_;

  // FileDescriptorWatcher for |base_loop_|. This is used in AlarmTimer.
#ifndef _MSC_VER
  std::unique_ptr<base::FileDescriptorWatcher> watcher_;
#endif

  // The RunLoop instance used to run the main loop from Run().
  base::RunLoop* base_run_loop_{nullptr};

  // The binder minor device number. Binder is a "misc" char device with a
  // dynamically allocated minor number. When uninitialized, this value will
  // be negative, otherwise, it will hold the minor part of the binder device
  // number. This is populated by GetBinderMinor().
  int binder_minor_{kUninitializedMinor};

  // We use a WeakPtrFactory to schedule tasks with the base::MessageLoopForIO
  // since we can't cancel the callbacks we have scheduled there once this
  // instance is destroyed.
  base::WeakPtrFactory<BaseMessageLoop> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(BaseMessageLoop);
};

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_MESSAGE_LOOPS_BASE_MESSAGE_LOOP_H_
