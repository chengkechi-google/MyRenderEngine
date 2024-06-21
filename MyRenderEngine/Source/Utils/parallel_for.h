#pragma once
#include "Core/Engine.h"
#include "enkiTS/TaskScheduler.h"

template <typename F>
inline void ParallelFor(uint32_t begin, uint32_t end, F fun)
{
    enki::TaskScheduler* pTS = Engine::GetInstance()->GetTaskScheduler();
    enki::TaskSet taskSet(end - begin + 1, [&](enki::TaskSetPartition range, uint32_t threadNum)
        {
            for (uint32_t i = range.start; i != range.end; ++i)
            {
                fun(i + begin);
            }
        });

    pTS->AddTaskSetToPipe(&taskSet);
    pTS->WaitforTask(&taskSet);
}

template <typename F>
inline void ParallelFor(uint32_t num, F fun)
{
    ParallelFor<F>(0, num - 1, fun);
}