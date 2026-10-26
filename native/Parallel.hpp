#pragma once
#include <cstddef>
#include <functional>
namespace pc {
// Runs task(0) .. task(count-1) across the processor's cores and returns when
// all are done; the first exception a task throws is rethrown here.
// std::execution::par where the standard library runs it in parallel (MSVC);
// a pool of worker threads otherwise (libstdc++ without TBB runs it serially).
void parallelFor(std::size_t count,const std::function<void(std::size_t)>& task);
}
