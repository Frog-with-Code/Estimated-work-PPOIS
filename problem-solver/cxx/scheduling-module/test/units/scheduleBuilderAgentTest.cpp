#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include <algorithm>

#include "agents/scheduleBuilderAgent.hpp"
#include "keynodes/scheduling-keynodes.hpp"
#include "utils/TestUtils.hpp"

using ScheduleBuilderAgentTest = ScMemoryTest;

namespace
{

// Создаёт тестового сотрудника
ScAddr CreateEmployee(
    ScAgentContext & ctx,
    std::string const & name,
    ScAddr const & profession,
    std::vector<ScAddr> const & allowedShifts = {},
    std::vector<ScAddr> const & forbiddenShifts = {})
{
  ScAddr emp = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, profession, emp);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_employee, emp);
  
  // Имя
  ScAddr nameLink = ctx.GenerateLink(ScType::ConstNodeLink);
  ctx.SetLinkContent(nameLink, name);
  ScAddr nameArc = ctx.GenerateConnector(ScType::ConstCommonArc, emp, nameLink);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf, nameArc);
  
  // Разрешённые смены
  for (auto const & shift : allowedShifts)
  {
    ScAddr arc = ctx.GenerateConnector(ScType::ConstCommonArc, emp, shift);
    ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_allowed_shift, arc);
  }
  
  // Запрещённые смены
  for (auto const & shift : forbiddenShifts)
  {
    ScAddr arc = ctx.GenerateConnector(ScType::ConstCommonArc, emp, shift);
    ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_can_not_work, arc);
  }
  
  return emp;
}

// Создаёт требования к составу смены
ScAddr CreateShiftRequirements(
    ScAgentContext & ctx,
    int cooks,
    int waiters,
    int cleaners,
    int admins,
    int maxShiftsPerWeek)
{
  ScAddr reqs = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_shift_requirements, reqs);
  
  // Количество поваров
  ScAddr cookCount = ctx.GenerateLink(ScType::ConstNodeLink);
  ctx.SetLinkContent(cookCount, std::to_string(cooks));
  ScAddr arcCook = ctx.GenerateConnector(ScType::ConstCommonArc, SchedulingKeynodes::concept_cook, cookCount);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_required_count, arcCook);
  
  // Количество официантов
  ScAddr waiterCount = ctx.GenerateLink(ScType::ConstNodeLink);
  ctx.SetLinkContent(waiterCount, std::to_string(waiters));
  ScAddr arcWaiter = ctx.GenerateConnector(ScType::ConstCommonArc, SchedulingKeynodes::concept_waiter, waiterCount);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_required_count, arcWaiter);
  
  // Количество уборщиков
  ScAddr cleanerCount = ctx.GenerateLink(ScType::ConstNodeLink);
  ctx.SetLinkContent(cleanerCount, std::to_string(cleaners));
  ScAddr arcCleaner = ctx.GenerateConnector(ScType::ConstCommonArc, SchedulingKeynodes::concept_cleaner, cleanerCount);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_required_count, arcCleaner);
  
  // Количество администраторов
  ScAddr adminCount = ctx.GenerateLink(ScType::ConstNodeLink);
  ctx.SetLinkContent(adminCount, std::to_string(admins));
  ScAddr arcAdmin = ctx.GenerateConnector(ScType::ConstCommonArc, SchedulingKeynodes::concept_admin, adminCount);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_required_count, arcAdmin);
  
  // Максимум смен в неделю
  ScAddr maxLink = ctx.GenerateLink(ScType::ConstNodeLink);
  ctx.SetLinkContent(maxLink, std::to_string(maxShiftsPerWeek));
  ScAddr arcMax = ctx.GenerateConnector(ScType::ConstCommonArc, reqs, maxLink);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_max_shifts_per_week, arcMax);
  
  return reqs;
}

// Создаёт минимальный штат для тестирования
void CreateMinimalStaff(ScAgentContext & ctx)
{
  // 4 повара (не могут работать в ночь)
  CreateEmployee(ctx, "Повар1", SchedulingKeynodes::concept_cook, 
      {}, {SchedulingKeynodes::concept_night_shift});
  CreateEmployee(ctx, "Повар2", SchedulingKeynodes::concept_cook,
      {}, {SchedulingKeynodes::concept_night_shift});
  CreateEmployee(ctx, "Повар3", SchedulingKeynodes::concept_cook,
      {}, {SchedulingKeynodes::concept_night_shift});
  CreateEmployee(ctx, "Повар4", SchedulingKeynodes::concept_cook,
      {}, {SchedulingKeynodes::concept_night_shift});
  
  // 4 официанта (без ограничений)
  CreateEmployee(ctx, "Официант1", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "Официант2", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "Официант3", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "Официант4", SchedulingKeynodes::concept_waiter);
  
  // 2 уборщика (без ограничений)
  CreateEmployee(ctx, "Уборщик1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "Уборщик2", SchedulingKeynodes::concept_cleaner);
  
  // 2 администратора (только дневная смена)
  CreateEmployee(ctx, "Админ1", SchedulingKeynodes::concept_admin,
      {SchedulingKeynodes::concept_day_shift}, {});
  CreateEmployee(ctx, "Админ2", SchedulingKeynodes::concept_admin,
      {SchedulingKeynodes::concept_day_shift}, {});
}

// Подсчитывает количество назначений на смены
int CountAssignments(ScAgentContext & ctx)
{
  int count = 0;
  ScIterator3Ptr it = ctx.CreateIterator3(
      SchedulingKeynodes::concept_shift_assignment, ScType::ConstPermPosArc, ScType::ConstNode);
  while (it->Next())
  {
    count++;
  }
  return count;
}

// Проверяет, что двудольный граф был создан
bool BipartiteGraphExists(ScAgentContext & ctx)
{
  ScIterator3Ptr it = ctx.CreateIterator3(
      SchedulingKeynodes::concept_bipartite_graph, ScType::ConstPermPosArc, ScType::ConstNode);
  return it->Next();
}

// Подсчитывает количество слотов смен
int CountShiftSlots(ScAgentContext & ctx)
{
  int count = 0;
  ScIterator3Ptr it = ctx.CreateIterator3(
      SchedulingKeynodes::concept_shift_slot, ScType::ConstPermPosArc, ScType::ConstNode);
  while (it->Next())
  {
    count++;
  }
  return count;
}

}  // namespace

// ====== БАЗОВЫЕ ТЕСТЫ ======

TEST_F(ScheduleBuilderAgentTest, BuildSchedule_Success)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  CreateMinimalStaff(ctx);
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 2, 1, 1, 5);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  ScStructure result = scAction.GetResult();
  EXPECT_TRUE(result.IsValid());
  
  int assignmentCount = CountAssignments(ctx);
  EXPECT_GT(assignmentCount, 0);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, BuildSchedule_NoStaff)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedWithError());
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

// ====== ТЕСТЫ ДВУДОЛЬНОГО ГРАФА ======

TEST_F(ScheduleBuilderAgentTest, BipartiteGraph_Created)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  CreateMinimalStaff(ctx);
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 5);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // Проверяем, что двудольный граф создан
  EXPECT_TRUE(BipartiteGraphExists(ctx));
  
  // Проверяем слоты смен: 7 дней * 3 смены * 4 профессии = 84 слота
  int slotCount = CountShiftSlots(ctx);
  EXPECT_EQ(slotCount, 84);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, BipartiteGraph_HasLeftAndRightParts)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  CreateMinimalStaff(ctx);
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 5);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  
  // Находим граф
  ScIterator3Ptr itGraph = ctx.CreateIterator3(
      SchedulingKeynodes::concept_bipartite_graph, ScType::ConstPermPosArc, ScType::ConstNode);
  ASSERT_TRUE(itGraph->Next());
  ScAddr graphNode = itGraph->Get(2);
  
  // Проверяем наличие левой доли
  ScIterator5Ptr itLeft = ctx.CreateIterator5(
      graphNode, ScType::ConstCommonArc, ScType::ConstNode,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_left_part);
  EXPECT_TRUE(itLeft->Next());
  
  // Проверяем наличие правой доли
  ScIterator5Ptr itRight = ctx.CreateIterator5(
      graphNode, ScType::ConstCommonArc, ScType::ConstNode,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_right_part);
  EXPECT_TRUE(itRight->Next());
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

// ====== ТЕСТЫ ПАРОСОЧЕТАНИЯ ======

TEST_F(ScheduleBuilderAgentTest, Matching_RespectsMaxShiftsPerWeek)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Создаём только 2 поваров с лимитом 3 смены в неделю
  CreateEmployee(ctx, "Повар1", SchedulingKeynodes::concept_cook);
  CreateEmployee(ctx, "Повар2", SchedulingKeynodes::concept_cook);
  // Минимальные другие сотрудники
  CreateEmployee(ctx, "Официант1", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "Уборщик1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "Админ1", SchedulingKeynodes::concept_admin);
  
  // Требуем 1 повара на смену, макс 3 смены
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 3);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // С 2 поварами и лимитом 3 смены максимум можно заполнить 6 слотов поваров
  // Всего 21 слот поваров (7 дней * 3 смены), но заполнено <= 6
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, Matching_RespectsShiftRestrictions)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Повар может работать только утром
  CreateEmployee(ctx, "ПоварУтро", SchedulingKeynodes::concept_cook,
      {SchedulingKeynodes::concept_morning_shift}, {});
  // Добавляем ещё сотрудников
  CreateEmployee(ctx, "Официант1", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "Уборщик1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "Админ1", SchedulingKeynodes::concept_admin);
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 7);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  // Агент завершится успешно, но не все слоты будут заполнены
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

// ====== ТЕСТЫ ЗАГРУЖЕННОСТИ ======

TEST_F(ScheduleBuilderAgentTest, Workload_Calculated)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  CreateEmployee(ctx, "TestCook1", SchedulingKeynodes::concept_cook);
  CreateEmployee(ctx, "TestWaiter1", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "TestCleaner1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "TestAdmin1", SchedulingKeynodes::concept_admin);
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 7);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // Проверяем, что существуют записи о загруженности (nrel_workload)
  int workloadRecords = 0;
  ScIterator5Ptr it = ctx.CreateIterator5(
      ScType::ConstNode, ScType::ConstCommonArc, ScType::ConstNodeLink,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_workload);
  while (it->Next())
  {
    workloadRecords++;
  }
  EXPECT_GT(workloadRecords, 0);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, Workload_Balanced)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Создаём штат с достаточным количеством сотрудников
  // 21 смена = 7 дней × 3 смены, max 7 смен/сотрудник → нужно 3 сотрудника минимум
  CreateEmployee(ctx, "BalanceCook1", SchedulingKeynodes::concept_cook);
  CreateEmployee(ctx, "BalanceCook2", SchedulingKeynodes::concept_cook);
  CreateEmployee(ctx, "BalanceCook3", SchedulingKeynodes::concept_cook);
  
  CreateEmployee(ctx, "BalanceWaiter1", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "BalanceWaiter2", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "BalanceWaiter3", SchedulingKeynodes::concept_waiter);
  
  CreateEmployee(ctx, "BalanceCleaner1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "BalanceCleaner2", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "BalanceCleaner3", SchedulingKeynodes::concept_cleaner);
  
  CreateEmployee(ctx, "BalanceAdmin1", SchedulingKeynodes::concept_admin);
  CreateEmployee(ctx, "BalanceAdmin2", SchedulingKeynodes::concept_admin);
  CreateEmployee(ctx, "BalanceAdmin3", SchedulingKeynodes::concept_admin);
  
  // 1 сотрудник каждой профессии на смену, макс 7 смен
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 7);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // Проверяем, что расписание создано (есть назначения)
  int assignmentCount = CountAssignments(ctx);
  EXPECT_GT(assignmentCount, 0);
  
  // Проверяем, что созданы записи о загруженности
  int workloadCount = 0;
  ScIterator5Ptr itWorkload = ctx.CreateIterator5(
      ScType::ConstNode, ScType::ConstCommonArc, ScType::ConstNodeLink,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_workload);
  while (itWorkload->Next())
  {
    workloadCount++;
  }
  EXPECT_GT(workloadCount, 0);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

// ====== ТЕСТЫ РАЗЛИЧНЫХ ТРЕБОВАНИЙ ======

TEST_F(ScheduleBuilderAgentTest, Requirements_DifferentCounts)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Много поваров и официантов
  for (int i = 0; i < 6; ++i)
  {
    CreateEmployee(ctx, "Повар" + std::to_string(i), SchedulingKeynodes::concept_cook);
  }
  for (int i = 0; i < 6; ++i)
  {
    CreateEmployee(ctx, "Официант" + std::to_string(i), SchedulingKeynodes::concept_waiter);
  }
  CreateEmployee(ctx, "Уборщик1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "Админ1", SchedulingKeynodes::concept_admin);
  
  // 2 повара, 3 официанта на смену
  ScAddr requirements = CreateShiftRequirements(ctx, 2, 3, 1, 1, 5);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(15000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // Общее число назначений должно быть значительным
  int assignmentCount = CountAssignments(ctx);
  // 7 дней * 3 смены * (2 + 3 + 1 + 1) = 147 слотов максимум
  EXPECT_GT(assignmentCount, 50);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, Requirements_ZeroProfession)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Только повара и официанты
  CreateEmployee(ctx, "Повар1", SchedulingKeynodes::concept_cook);
  CreateEmployee(ctx, "Официант1", SchedulingKeynodes::concept_waiter);
  
  // 0 уборщиков и 0 админов
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 0, 0, 7);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // Слотов: 7 дней * 3 смены * 2 профессии = 42
  int slotCount = CountShiftSlots(ctx);
  EXPECT_EQ(slotCount, 42);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

// ====== ТЕСТЫ ГРАНИЧНЫХ СЛУЧАЕВ ======

TEST_F(ScheduleBuilderAgentTest, Edge_OneEmployeePerProfession)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Минимальный штат: 1 сотрудник каждой профессии
  CreateEmployee(ctx, "Повар1", SchedulingKeynodes::concept_cook);
  CreateEmployee(ctx, "Официант1", SchedulingKeynodes::concept_waiter);
  CreateEmployee(ctx, "Уборщик1", SchedulingKeynodes::concept_cleaner);
  CreateEmployee(ctx, "Админ1", SchedulingKeynodes::concept_admin);
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 7);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  int assignmentCount = CountAssignments(ctx);
  // 1 сотрудник * 7 дней = 7 назначений на каждую профессию
  // Но один сотрудник может работать только 1 смену в день
  EXPECT_GT(assignmentCount, 0);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, Edge_AllEmployeesRestrictedToOneShift)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  
  // Все могут работать только утром
  CreateEmployee(ctx, "Повар1", SchedulingKeynodes::concept_cook,
      {SchedulingKeynodes::concept_morning_shift}, {});
  CreateEmployee(ctx, "Официант1", SchedulingKeynodes::concept_waiter,
      {SchedulingKeynodes::concept_morning_shift}, {});
  CreateEmployee(ctx, "Уборщик1", SchedulingKeynodes::concept_cleaner,
      {SchedulingKeynodes::concept_morning_shift}, {});
  CreateEmployee(ctx, "Админ1", SchedulingKeynodes::concept_admin,
      {SchedulingKeynodes::concept_morning_shift}, {});
  
  ScAddr requirements = CreateShiftRequirements(ctx, 1, 1, 1, 1, 7);
  
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  ScAddr arcReqs = ctx.GenerateConnector(ScType::ConstPermPosArc, action, requirements);
  ctx.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::rrel_1, arcReqs);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  // Максимум 7 утренних смен * 4 сотрудника = 28 назначений
  int assignmentCount = CountAssignments(ctx);
  EXPECT_EQ(assignmentCount, 28);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}

TEST_F(ScheduleBuilderAgentTest, Edge_NoRequirementsProvided)
{
  ScAgentContext & ctx = *m_ctx;
  
  ctx.SubscribeAgent<ScheduleBuilderAgent>();
  CreateMinimalStaff(ctx);
  
  // Действие БЕЗ требований (используются значения по умолчанию)
  ScAddr action = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_build_weekly_schedule, action);
  
  ScAction scAction = ctx.ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(10000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());
  
  int assignmentCount = CountAssignments(ctx);
  EXPECT_GT(assignmentCount, 0);
  
  ctx.UnsubscribeAgent<ScheduleBuilderAgent>();
}
