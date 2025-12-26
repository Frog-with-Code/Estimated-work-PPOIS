/*
 * This source file is part of an OSTIS project. For the latest info, see
 * http://ostis.net Distributed under the MIT License (See accompanying file
 * COPYING.MIT or copy at http://opensource.org/licenses/MIT)
 */

#pragma once

#include <sc-memory/sc_agent.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Структура для хранения информации о сотруднике
struct Employee
{
  ScAddr addr;
  std::string name;
  ScAddr profession;
  ScAddrUnorderedSet allowedShifts;
  ScAddrUnorderedSet forbiddenShifts;
  int assignedCount = 0;
  int index = -1;  // Индекс в левой доле графа
};

// Структура для хранения слота смены (день + тип смены + позиция)
struct ShiftSlot
{
  ScAddr day;
  ScAddr shiftType;
  ScAddr profession;  // Какая профессия нужна для этого слота
  int position;       // Позиция в смене (0, 1, ... для нескольких сотрудников одной профессии)
  int index = -1;     // Индекс в правой доле графа
  ScAddr scAddr;      // Адрес узла слота в SC-memory
};

// Структура для хранения назначения на смену
struct ShiftAssignment
{
  ScAddr day;
  ScAddr shiftType;
  ScAddr employee;
};

// Структура для хранения требований к составу смены
struct ShiftRequirements
{
  int cooksPerShift = 1;
  int waitersPerShift = 2;
  int cleanersPerShift = 1;
  int adminsPerShift = 1;
  int maxShiftsPerWeek = 5;
};

// Структура двудольного графа для паросочетания
struct BipartiteGraph
{
  std::vector<Employee> employees;                    // Левая доля (сотрудники)
  std::vector<ShiftSlot> slots;                       // Правая доля (слоты смен)
  std::vector<std::vector<int>> adjacency;            // Списки смежности: employee -> slots
  ScAddr graphAddr;                                   // Адрес структуры графа в SC-memory
};

class ScheduleBuilderAgent : public ScActionInitiatedAgent
{
public:
  ScheduleBuilderAgent();

  ScAddr GetActionClass() const override;

  ScResult DoProgram(ScActionInitiatedEvent const & event, ScAction & action) override;

private:
  // ===== Вспомогательные методы для работы с данными =====
  
  std::vector<ScAddr> GetWeekdays();
  std::vector<ScAddr> GetShiftTypes();
  std::string GetEmployeeName(ScAddr const & employee);
  int GetIntFromLink(ScAddr const & link, int defaultValue);
  int GetRequiredCount(ScAddr const & profession, int defaultValue);
  ScAddrUnorderedSet GetEmployeeShifts(ScAddr const & employee, ScAddr const & relation);
  
  // ===== Работа с требованиями =====
  
  ShiftRequirements GetShiftRequirements(ScAction & action);
  std::vector<std::pair<ScAddr, int>> GetProfessionRequirements(ShiftRequirements const & reqs);
  
  // ===== Работа с сотрудниками =====
  
  std::vector<Employee> GetEmployeesByProfession(ScAddr const & profession);
  bool CanWorkShift(Employee const & emp, ScAddr const & shiftType);
  
  // ===== Построение двудольного графа =====
  
  BipartiteGraph BuildBipartiteGraph(
      ShiftRequirements const & reqs,
      std::vector<ScAddr> const & weekdays,
      std::vector<ScAddr> const & shiftTypes);
  
  void BuildEmployeesPart(
      BipartiteGraph & graph,
      std::vector<std::pair<ScAddr, int>> const & professionRequirements);
  
  void BuildSlotsPart(
      BipartiteGraph & graph,
      std::vector<std::pair<ScAddr, int>> const & professionRequirements,
      std::vector<ScAddr> const & weekdays,
      std::vector<ScAddr> const & shiftTypes);
  
  void BuildGraphEdges(BipartiteGraph & graph);
  
  // ===== Сохранение графа в SC-memory =====
  
  ScAddr SaveBipartiteGraphToScMemory(
      BipartiteGraph const & graph,
      std::vector<int> const & matching);
  ScAddr CreateGraphNode();
  ScAddr CreateGraphPart(ScAddr const & graphNode, ScAddr const & relation);
  void AddEmployeesToPart(ScAddr const & leftPart, std::vector<Employee> const & employees);
  ScAddr CreateSlotNode(ScAddr const & rightPart, ShiftSlot const & slot);
  void CreateGraphEdgesInMemory(
      std::vector<Employee> const & employees,
      std::vector<ShiftSlot> const & slots,
      std::vector<int> const & matching,
      std::vector<ScAddr> const & slotAddrs);
  
  // ===== Алгоритм Куна для максимального паросочетания =====
  
  std::vector<int> FindMaximumMatching(BipartiteGraph & graph, int maxShiftsPerWeek);
  bool TryKuhn(
      int employeeIdx,
      BipartiteGraph const & graph,
      std::vector<int> & matching,
      std::vector<bool> & used,
      std::vector<int> & employeeAssignments,
      int maxShiftsPerWeek);
  ScAddrUnorderedSet GetBusyDays(
      int employeeIdx, BipartiteGraph const & graph, std::vector<int> const & matching);
  std::vector<int> GetEmployeeOrder(std::vector<int> const & employeeAssignments);
  
  // ===== Создание результата =====
  
  ScAddr CreateShiftAssignment(
      ScAddr const & employee,
      ScAddr const & day,
      ScAddr const & shiftType);
  
  ScStructure CreateScheduleResult(
      std::vector<ShiftAssignment> const & assignments,
      std::unordered_map<ScAddr, int, ScAddrHashFunc> const & workloads,
      ScAddr const & bipartiteGraphAddr);
  
  void AddWorkloadsToResult(
      ScStructure & result,
      std::unordered_map<ScAddr, int, ScAddrHashFunc> const & workloads);
  
  // ===== Вспомогательные методы для DoProgram =====
  
  void LogRequirements(ShiftRequirements const & reqs);
  void ConvertMatchingToAssignments(
      BipartiteGraph const & graph,
      std::vector<int> const & matching,
      std::vector<ShiftAssignment> & assignments,
      std::unordered_map<ScAddr, int, ScAddrHashFunc> & workloads);
  void LogWeeklySchedule(
      BipartiteGraph const & graph,
      std::vector<ShiftAssignment> const & assignments,
      std::unordered_map<ScAddr, int, ScAddrHashFunc> const & workloads,
      std::vector<ScAddr> const & weekdays);
  
  bool IsSetValidAndNotEmpty(ScAddr const & setAddr) const;
};
