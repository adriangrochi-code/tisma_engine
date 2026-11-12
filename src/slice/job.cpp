// A slice's progress and its cancelling (job.hpp). Own design.

#include "slice/job.hpp"

namespace tisma {

namespace {
thread_local JobHook* current = nullptr;
}

JobScope::JobScope(JobHook& hook) : previous_(current) { current = &hook; }

JobScope::~JobScope() { current = previous_; }

void job_check() {
    if (current && current->cancel.load())
        throw JobCancelled();
}

void job_step(const char* stage, int percent) {
    if (!current)
        return;
    job_check();
    if (percent > current->percent.load())
        current->percent.store(percent);
    current->stage.store(stage);
}

} // namespace tisma
