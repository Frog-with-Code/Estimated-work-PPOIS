#include "schedulingModule.hpp"

#include "agents/scheduleBuilderAgent.hpp"
#include "agents/importStaffAgent.hpp"

SC_MODULE_REGISTER(SchedulingModule)
    ->Agent<ScheduleBuilderAgent>()
    ->Agent<ImportStaffAgent>();
