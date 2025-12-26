#include "scheduleBuilderAgent.hpp"
#include "keynodes/scheduling-keynodes.hpp"

#include <sc-memory/sc_memory_headers.hpp>
#include <sc-agents-common/utils/IteratorUtils.hpp>
#include <algorithm>
#include <numeric>

ScheduleBuilderAgent::ScheduleBuilderAgent()
{
  m_logger = utils::ScLogger(
      utils::ScLogger::ScLogType::File, "logs/ScheduleBuilderAgent.log", utils::ScLogLevel::Debug);
}

ScAddr ScheduleBuilderAgent::GetActionClass() const
{
  return SchedulingKeynodes::action_build_weekly_schedule;
}

std::vector<ScAddr> ScheduleBuilderAgent::GetWeekdays()
{
  return {
      SchedulingKeynodes::monday,    SchedulingKeynodes::tuesday,  SchedulingKeynodes::wednesday,
      SchedulingKeynodes::thursday,  SchedulingKeynodes::friday,   SchedulingKeynodes::saturday,
      SchedulingKeynodes::sunday
  };
}

std::vector<ScAddr> ScheduleBuilderAgent::GetShiftTypes()
{
  return {
      SchedulingKeynodes::concept_morning_shift,
      SchedulingKeynodes::concept_day_shift,
      SchedulingKeynodes::concept_night_shift
  };
}

std::string ScheduleBuilderAgent::GetEmployeeName(ScAddr const & employee)
{
  ScIterator5Ptr it = m_context.CreateIterator5(
      employee, ScType::ConstCommonArc, ScType::ConstNodeLink, ScType::ConstPermPosArc,
      ScKeynodes::nrel_main_idtf);
  if (it->Next())
  {
    std::string name;
    m_context.GetLinkContent(it->Get(2), name);
    return name;
  }
  return "Unknown";
}

int ScheduleBuilderAgent::GetIntFromLink(ScAddr const & link, int defaultValue)
{
  if (!link.IsValid())
    return defaultValue;

  std::string content;
  m_context.GetLinkContent(link, content);

  try
  {
    return std::stoi(content);
  }
  catch (...)
  {
    return defaultValue;
  }
}

// Вспомогательный метод для получения требуемого количества профессии
int ScheduleBuilderAgent::GetRequiredCount(ScAddr const & profession, int defaultValue)
{
  ScIterator5Ptr it = m_context.CreateIterator5(
      profession, ScType::ConstCommonArc, ScType::ConstNodeLink, ScType::ConstPermPosArc,
      SchedulingKeynodes::nrel_required_count);
  if (it->Next())
    return GetIntFromLink(it->Get(2), defaultValue);
  return defaultValue;
}


ShiftRequirements ScheduleBuilderAgent::GetShiftRequirements(ScAction & action)
{
  ShiftRequirements reqs;

  auto const & [requirementsAddr] = action.GetArguments<1>();

  if (!requirementsAddr.IsValid())
  {
    m_logger.Info("ScheduleBuilderAgent: No requirements provided, using defaults");
    return reqs;
  }

  m_logger.Info("ScheduleBuilderAgent: Parsing shift requirements from input");

  reqs.cooksPerShift = GetRequiredCount(SchedulingKeynodes::concept_cook, reqs.cooksPerShift);
  reqs.waitersPerShift = GetRequiredCount(SchedulingKeynodes::concept_waiter, reqs.waitersPerShift);
  reqs.cleanersPerShift = GetRequiredCount(SchedulingKeynodes::concept_cleaner, reqs.cleanersPerShift);
  reqs.adminsPerShift = GetRequiredCount(SchedulingKeynodes::concept_admin, reqs.adminsPerShift);

  ScIterator5Ptr itMax = m_context.CreateIterator5(
      requirementsAddr, ScType::ConstCommonArc, ScType::ConstNodeLink, ScType::ConstPermPosArc,
      SchedulingKeynodes::nrel_max_shifts_per_week);
  if (itMax->Next())
    reqs.maxShiftsPerWeek = GetIntFromLink(itMax->Get(2), reqs.maxShiftsPerWeek);

  return reqs;
}

// Вспомогательный метод для получения множества смен сотрудника
ScAddrUnorderedSet ScheduleBuilderAgent::GetEmployeeShifts(
    ScAddr const & employee, ScAddr const & relation)
{
  ScAddrUnorderedSet shifts;
  ScIterator5Ptr it = m_context.CreateIterator5(
      employee, ScType::ConstCommonArc, ScType::ConstNode, ScType::ConstPermPosArc, relation);
  while (it->Next())
    shifts.insert(it->Get(2));
  return shifts;
}

std::vector<Employee> ScheduleBuilderAgent::GetEmployeesByProfession(ScAddr const & profession)
{
  std::vector<Employee> employees;

  ScIterator3Ptr it = m_context.CreateIterator3(profession, ScType::ConstPermPosArc, ScType::ConstNode);

  while (it->Next())
  {
    ScAddr empAddr = it->Get(2);
    Employee emp;
    emp.addr = empAddr;
    emp.profession = profession;
    emp.name = GetEmployeeName(empAddr);
    emp.assignedCount = 0;
    emp.allowedShifts = GetEmployeeShifts(empAddr, SchedulingKeynodes::nrel_allowed_shift);
    emp.forbiddenShifts = GetEmployeeShifts(empAddr, SchedulingKeynodes::nrel_can_not_work);

    employees.push_back(emp);
  }

  return employees;
}

bool ScheduleBuilderAgent::CanWorkShift(Employee const & emp, ScAddr const & shiftType)
{
  if (!emp.allowedShifts.empty())
    return emp.allowedShifts.count(shiftType) > 0;

  if (!emp.forbiddenShifts.empty())
    return emp.forbiddenShifts.count(shiftType) == 0;

  return true;
}

// ===== Построение двудольного графа =====

std::vector<std::pair<ScAddr, int>> ScheduleBuilderAgent::GetProfessionRequirements(
    ShiftRequirements const & reqs)
{
  return {
      {SchedulingKeynodes::concept_cook, reqs.cooksPerShift},
      {SchedulingKeynodes::concept_waiter, reqs.waitersPerShift},
      {SchedulingKeynodes::concept_cleaner, reqs.cleanersPerShift},
      {SchedulingKeynodes::concept_admin, reqs.adminsPerShift}
  };
}

void ScheduleBuilderAgent::BuildEmployeesPart(
    BipartiteGraph & graph, std::vector<std::pair<ScAddr, int>> const & professionRequirements)
{
  int employeeIndex = 0;
  for (auto const & [profession, count] : professionRequirements)
  {
    if (count > 0)
    {
      auto employees = GetEmployeesByProfession(profession);
      for (auto & emp : employees)
      {
        emp.index = employeeIndex++;
        graph.employees.push_back(emp);
      }
    }
  }
  m_logger.Info("ScheduleBuilderAgent: Left part (employees): ", graph.employees.size());
}

void ScheduleBuilderAgent::BuildSlotsPart(
    BipartiteGraph & graph,
    std::vector<std::pair<ScAddr, int>> const & professionRequirements,
    std::vector<ScAddr> const & weekdays,
    std::vector<ScAddr> const & shiftTypes)
{
  int slotIndex = 0;
  for (auto const & day : weekdays)
  {
    for (auto const & shiftType : shiftTypes)
    {
      for (auto const & [profession, count] : professionRequirements)
      {
        for (int pos = 0; pos < count; ++pos)
        {
          ShiftSlot slot;
          slot.day = day;
          slot.shiftType = shiftType;
          slot.profession = profession;
          slot.position = pos;
          slot.index = slotIndex++;
          graph.slots.push_back(slot);
        }
      }
    }
  }
  m_logger.Info("ScheduleBuilderAgent: Right part (shift slots): ", graph.slots.size());
}

void ScheduleBuilderAgent::BuildGraphEdges(BipartiteGraph & graph)
{
  graph.adjacency.resize(graph.employees.size());

  for (auto const & emp : graph.employees)
  {
    for (auto const & slot : graph.slots)
    {
      if (emp.profession == slot.profession && CanWorkShift(emp, slot.shiftType))
        graph.adjacency[emp.index].push_back(slot.index);
    }
  }

  int edgeCount = std::accumulate(
      graph.adjacency.begin(), graph.adjacency.end(), 0,
      [](int sum, std::vector<int> const & adj) { return sum + adj.size(); });
  m_logger.Info("ScheduleBuilderAgent: Graph edges: ", edgeCount);
}

BipartiteGraph ScheduleBuilderAgent::BuildBipartiteGraph(
    ShiftRequirements const & reqs,
    std::vector<ScAddr> const & weekdays,
    std::vector<ScAddr> const & shiftTypes)
{
  m_logger.Info("ScheduleBuilderAgent: Building bipartite graph");

  BipartiteGraph graph;
  auto professionRequirements = GetProfessionRequirements(reqs);

  BuildEmployeesPart(graph, professionRequirements);
  BuildSlotsPart(graph, professionRequirements, weekdays, shiftTypes);
  BuildGraphEdges(graph);

  return graph;
}

// ===== Сохранение графа в SC-memory =====

ScAddr ScheduleBuilderAgent::CreateGraphNode()
{
  ScAddr graphNode = m_context.GenerateNode(ScType::ConstNode);
  m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_bipartite_graph, graphNode);
  return graphNode;
}

ScAddr ScheduleBuilderAgent::CreateGraphPart(ScAddr const & graphNode, ScAddr const & relation)
{
  ScAddr part = m_context.GenerateNode(ScType::ConstNode);
  ScAddr arc = m_context.GenerateConnector(ScType::ConstCommonArc, graphNode, part);
  m_context.GenerateConnector(ScType::ConstPermPosArc, relation, arc);
  return part;
}

void ScheduleBuilderAgent::AddEmployeesToPart(ScAddr const & leftPart, std::vector<Employee> const & employees)
{
  for (auto const & emp : employees)
    m_context.GenerateConnector(ScType::ConstPermPosArc, leftPart, emp.addr);
}

ScAddr ScheduleBuilderAgent::CreateSlotNode(ScAddr const & rightPart, ShiftSlot const & slot)
{
  ScAddr slotNode = m_context.GenerateNode(ScType::ConstNode);
  m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_shift_slot, slotNode);
  m_context.GenerateConnector(ScType::ConstPermPosArc, rightPart, slotNode);

  // Связываем слот с атрибутами
  auto createSlotRelation = [this, slotNode](ScAddr const & value, ScAddr const & relation) {
    ScAddr arc = m_context.GenerateConnector(ScType::ConstCommonArc, slotNode, value);
    m_context.GenerateConnector(ScType::ConstPermPosArc, relation, arc);
  };

  createSlotRelation(slot.day, SchedulingKeynodes::nrel_shift_day);
  createSlotRelation(slot.shiftType, SchedulingKeynodes::nrel_shift_type);
  createSlotRelation(slot.profession, SchedulingKeynodes::nrel_slot_profession);

  return slotNode;
}

void ScheduleBuilderAgent::CreateGraphEdgesInMemory(
    std::vector<Employee> const & employees,
    std::vector<ShiftSlot> const & slots,
    std::vector<int> const & matching,
    std::vector<ScAddr> const & slotAddrs)
{
  // Создаём рёбра только для пар, входящих в максимальное паросочетание
  for (size_t slotIdx = 0; slotIdx < matching.size(); ++slotIdx)
  {
    int empIdx = matching[slotIdx];
    if (empIdx != -1)  // Только если слот назначен сотруднику
    {
      auto const & emp = employees[empIdx];
      ScAddr arcCanWork = m_context.GenerateConnector(ScType::ConstCommonArc, emp.addr, slotAddrs[slotIdx]);
      m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_can_work, arcCanWork);
    }
  }
}

ScAddr ScheduleBuilderAgent::SaveBipartiteGraphToScMemory(
    BipartiteGraph const & graph,
    std::vector<int> const & matching)
{
  ScAddr graphNode = CreateGraphNode();

  ScAddr leftPart = CreateGraphPart(graphNode, SchedulingKeynodes::nrel_left_part);
  AddEmployeesToPart(leftPart, graph.employees);

  ScAddr rightPart = CreateGraphPart(graphNode, SchedulingKeynodes::nrel_right_part);

  std::vector<ScAddr> slotAddrs(graph.slots.size());
  for (size_t i = 0; i < graph.slots.size(); ++i)
    slotAddrs[i] = CreateSlotNode(rightPart, graph.slots[i]);

  // Сохраняем только рёбра из максимального паросочетания
  CreateGraphEdgesInMemory(graph.employees, graph.slots, matching, slotAddrs);

  m_logger.Info("ScheduleBuilderAgent: Bipartite graph saved to SC-memory with maximum matching");
  return graphNode;
}

// ===== Алгоритм Куна для максимального паросочетания =====

ScAddrUnorderedSet ScheduleBuilderAgent::GetBusyDays(
    int employeeIdx, BipartiteGraph const & graph, std::vector<int> const & matching)
{
  ScAddrUnorderedSet busyDays;
  for (size_t slotIdx = 0; slotIdx < matching.size(); ++slotIdx)
  {
    if (matching[slotIdx] == employeeIdx)
      busyDays.insert(graph.slots[slotIdx].day);
  }
  return busyDays;
}

bool ScheduleBuilderAgent::TryKuhn(
    int employeeIdx,
    BipartiteGraph const & graph,
    std::vector<int> & matching,
    std::vector<bool> & used,
    std::vector<int> & employeeAssignments,
    int maxShiftsPerWeek)
{
  if (employeeAssignments[employeeIdx] >= maxShiftsPerWeek)
    return false;

  ScAddrUnorderedSet busyDays = GetBusyDays(employeeIdx, graph, matching);

  for (int slotIdx : graph.adjacency[employeeIdx])
  {
    if (used[slotIdx] || busyDays.count(graph.slots[slotIdx].day) > 0)
      continue;

    used[slotIdx] = true;

    if (matching[slotIdx] == -1 ||
        TryKuhn(matching[slotIdx], graph, matching, used, employeeAssignments, maxShiftsPerWeek))
    {
      if (matching[slotIdx] != -1)
        employeeAssignments[matching[slotIdx]]--;

      matching[slotIdx] = employeeIdx;
      employeeAssignments[employeeIdx]++;
      return true;
    }
  }

  return false;
}

std::vector<int> ScheduleBuilderAgent::GetEmployeeOrder(std::vector<int> const & employeeAssignments)
{
  std::vector<int> order(employeeAssignments.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&employeeAssignments](int a, int b) {
    return employeeAssignments[a] < employeeAssignments[b];
  });
  return order;
}

std::vector<int> ScheduleBuilderAgent::FindMaximumMatching(BipartiteGraph & graph, int maxShiftsPerWeek)
{
  int n = graph.employees.size();
  int m = graph.slots.size();

  std::vector<int> matching(m, -1);
  std::vector<int> employeeAssignments(n, 0);

  m_logger.Info("ScheduleBuilderAgent: Starting Kuhn's algorithm");
  m_logger.Info("ScheduleBuilderAgent: Employees: ", n, ", Slots: ", m, ", Max shifts/week: ", maxShiftsPerWeek);

  bool improved = true;
  int iteration = 0;

  while (improved)
  {
    improved = false;
    iteration++;

    auto order = GetEmployeeOrder(employeeAssignments);

    for (int empIdx : order)
    {
      if (employeeAssignments[empIdx] < maxShiftsPerWeek)
      {
        std::vector<bool> used(m, false);
        if (TryKuhn(empIdx, graph, matching, used, employeeAssignments, maxShiftsPerWeek))
          improved = true;
      }
    }
  }

  int matchedCount = std::count_if(matching.begin(), matching.end(), [](int m) { return m != -1; });

  m_logger.Info("ScheduleBuilderAgent: Matching completed in ", iteration, " iterations");
  m_logger.Info("ScheduleBuilderAgent: Matched ", matchedCount, " of ", m, " slots");

  return matching;
}

// ===== Создание результата =====

ScAddr ScheduleBuilderAgent::CreateShiftAssignment(
    ScAddr const & employee,
    ScAddr const & day,
    ScAddr const & shiftType)
{
  ScAddr assignment = m_context.GenerateNode(ScType::ConstNode);
  m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_shift_assignment, assignment);

  auto createAssignmentRelation = [this, assignment](ScAddr const & value, ScAddr const & relation) {
    ScAddr arc = m_context.GenerateConnector(ScType::ConstCommonArc, assignment, value);
    m_context.GenerateConnector(ScType::ConstPermPosArc, relation, arc);
  };

  createAssignmentRelation(employee, SchedulingKeynodes::nrel_assigned_to_shift);
  createAssignmentRelation(day, SchedulingKeynodes::nrel_shift_day);
  createAssignmentRelation(shiftType, SchedulingKeynodes::nrel_shift_type);

  return assignment;
}

void ScheduleBuilderAgent::AddWorkloadsToResult(
    ScStructure & result, std::unordered_map<ScAddr, int, ScAddrHashFunc> const & workloads)
{
  for (auto const & [empAddr, count] : workloads)
  {
    ScAddr countLink = m_context.GenerateLink(ScType::ConstNodeLink);
    m_context.SetLinkContent(countLink, count);
    ScAddr arcWorkload = m_context.GenerateConnector(ScType::ConstCommonArc, empAddr, countLink);
    m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_workload, arcWorkload);
    result << countLink << arcWorkload;
  }
}

ScStructure ScheduleBuilderAgent::CreateScheduleResult(
    std::vector<ShiftAssignment> const & assignments,
    std::unordered_map<ScAddr, int, ScAddrHashFunc> const & workloads,
    ScAddr const & bipartiteGraphAddr)
{
  ScStructure result = m_context.GenerateStructure();
  m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_schedule, result);

  if (bipartiteGraphAddr.IsValid())
    result << bipartiteGraphAddr;

  for (auto const & assignment : assignments)
  {
    ScAddr assignmentNode = CreateShiftAssignment(assignment.employee, assignment.day, assignment.shiftType);
    result << assignmentNode;
  }

  AddWorkloadsToResult(result, workloads);
  return result;
}

bool ScheduleBuilderAgent::IsSetValidAndNotEmpty(ScAddr const & setAddr) const
{
  if (!setAddr.IsValid())
    return false;
  ScIterator3Ptr it = m_context.CreateIterator3(setAddr, ScType::ConstPermPosArc, ScType::Unknown);
  return it->Next();
}

// ===== Основная логика агента =====

void ScheduleBuilderAgent::LogRequirements(ShiftRequirements const & reqs)
{
  m_logger.Info("ScheduleBuilderAgent: Requirements - cooks: ", reqs.cooksPerShift, ", waiters: ",
                reqs.waitersPerShift, ", cleaners: ", reqs.cleanersPerShift, ", admins: ",
                reqs.adminsPerShift, ", max shifts/week: ", reqs.maxShiftsPerWeek);
}

void ScheduleBuilderAgent::ConvertMatchingToAssignments(
    BipartiteGraph const & graph,
    std::vector<int> const & matching,
    std::vector<ShiftAssignment> & assignments,
    std::unordered_map<ScAddr, int, ScAddrHashFunc> & workloads)
{
  for (auto const & emp : graph.employees)
    workloads[emp.addr] = 0;

  for (size_t slotIdx = 0; slotIdx < matching.size(); ++slotIdx)
  {
    int empIdx = matching[slotIdx];
    if (empIdx != -1)
    {
      auto const & emp = graph.employees[empIdx];
      auto const & slot = graph.slots[slotIdx];
      assignments.push_back({slot.day, slot.shiftType, emp.addr});
      workloads[emp.addr]++;
    }
  }
}

void ScheduleBuilderAgent::LogWeeklySchedule(
    BipartiteGraph const & graph,
    std::vector<ShiftAssignment> const & assignments,
    std::unordered_map<ScAddr, int, ScAddrHashFunc> const & workloads,
    std::vector<ScAddr> const & weekdays)
{
  m_logger.Info("ScheduleBuilderAgent: === Weekly schedule per employee ===");
  for (auto const & emp : graph.employees)
  {
    std::string schedule = emp.name + ": ";
    for (auto const & day : weekdays)
    {
      bool hasShift = std::any_of(assignments.begin(), assignments.end(),
                                  [&emp, &day](ShiftAssignment const & a) {
                                    return a.employee == emp.addr && a.day == day;
                                  });
      schedule += hasShift ? "W " : "- ";
    }
    schedule += "| Total: " + std::to_string(workloads.at(emp.addr)) + " shifts";
    m_logger.Info(schedule);
  }
}

ScResult ScheduleBuilderAgent::DoProgram(ScActionInitiatedEvent const & event, ScAction & action)
{
  m_logger.Info("ScheduleBuilderAgent: Starting schedule building with bipartite matching");

  ShiftRequirements reqs = GetShiftRequirements(action);
  LogRequirements(reqs);

  auto weekdays = GetWeekdays();
  auto shiftTypes = GetShiftTypes();

  BipartiteGraph graph = BuildBipartiteGraph(reqs, weekdays, shiftTypes);

  if (graph.employees.empty())
  {
    m_logger.Error("ScheduleBuilderAgent: No employees found");
    return action.FinishWithError();
  }

  if (graph.slots.empty())
  {
    m_logger.Error("ScheduleBuilderAgent: No shift slots to fill");
    return action.FinishWithError();
  }

  // Сначала находим максимальное паросочетание
  std::vector<int> matching = FindMaximumMatching(graph, reqs.maxShiftsPerWeek);
  
  // Затем сохраняем граф с учётом паросочетания (только рёбра из matching)
  ScAddr graphAddr = SaveBipartiteGraphToScMemory(graph, matching);

  std::vector<ShiftAssignment> assignments;
  std::unordered_map<ScAddr, int, ScAddrHashFunc> workloads;
  ConvertMatchingToAssignments(graph, matching, assignments, workloads);

  int totalSlots = graph.slots.size();
  int filledSlots = assignments.size();
  bool scheduleComplete = (filledSlots == totalSlots);

  m_logger.Info("ScheduleBuilderAgent: Filled ", filledSlots, " of ", totalSlots, " slots");
  m_logger.Info("ScheduleBuilderAgent: Schedule complete: ",
                scheduleComplete ? "YES" : "NO (some shifts unfilled)");

  LogWeeklySchedule(graph, assignments, workloads, weekdays);

  ScStructure result = CreateScheduleResult(assignments, workloads, graphAddr);
  action.SetResult(result);

  m_logger.Info("ScheduleBuilderAgent: Created ", assignments.size(), " shift assignments");

  return action.FinishSuccessfully();
}
