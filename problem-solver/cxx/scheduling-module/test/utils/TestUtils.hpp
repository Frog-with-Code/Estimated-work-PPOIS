#pragma once

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent_context.hpp>
#include <string>
#include "../../keynodes/scheduling-keynodes.hpp"

namespace TestUtils
{

// Ищет сотрудника по имени
ScAddr FindEmployeeByName(ScAgentContext & ctx, std::string const & targetName);

// Подсчитывает количество сотрудников данной профессии
int CountEmployeesByProfession(ScAgentContext & ctx, ScAddr const & profession);

// Проверяет, есть ли у сотрудника разрешённая смена
bool HasAllowedShift(ScAgentContext & ctx, ScAddr const & employee, ScAddr const & shift);

// Подсчитывает количество разрешённых смен у сотрудника
int CountAllowedShifts(ScAgentContext & ctx, ScAddr const & employee);

// Получает загруженность сотрудника
int GetEmployeeWorkload(ScAgentContext & ctx, ScAddr const & employee);

// Получает загруженность сотрудника по имени
int GetEmployeeWorkloadByName(ScAgentContext & ctx, std::string const & name);

}  // namespace TestUtils
