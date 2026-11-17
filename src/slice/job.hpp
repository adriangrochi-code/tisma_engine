#pragma once
// A slice's progress and its cancelling, for a caller that runs the slice on
// a thread of its own and asks how it goes (the C API's jobs,
// include/tisma/tisma_engine.h). Own design.
//
// The thread that slices sets a JobHook with JobScope; the slice calls
// job_step at the end of each stage on that same thread. Without a hook,
// job_step does nothing. Cancelling is checked there: a cancelled job throws
// JobCancelled at its next step, so it stops within one stage.

#include <atomic>

namespace tisma {

struct JobHook {
    std::atomic<bool> cancel{false};
    std::atomic<int> percent{0};
    std::atomic<const char*> stage{"idle"}; // a static string
};

// Not a std::exception: the commands' catch-alls for the engine's own
// errors (and the GPU's fall back to the CPU) let it through to the job.
struct JobCancelled {};

// The hook of the thread that runs the job, for as long as it lives.
class JobScope {
public:
    explicit JobScope(JobHook& hook);
    ~JobScope();
    JobScope(const JobScope&) = delete;
    JobScope& operator=(const JobScope&) = delete;

private:
    JobHook* previous_;
};

// A stage reached: the percent done (it never goes back) and the next stage's
// name. Throws JobCancelled when the job was cancelled.
void job_step(const char* stage, int percent);

// Throws JobCancelled when this thread's job was cancelled.
void job_check();

} // namespace tisma
