#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/importStaffAgent.hpp"
#include "agents/scheduleBuilderAgent.hpp"
#include "keynodes/scheduling-keynodes.hpp"
#include "utils/TestUtils.hpp"

using ImportStaffAgentTest = ScMemoryTest;

namespace
{

std::string const TEST_CSV_DATA = 
    "profession,name,allowed_shifts\n"
    "повар,Захаренков,M;D\n"
    "официант,Лойко,M;D;N\n"
    "администратор,Шумилов,D\n";

std::string const TEST_CSV_UNKNOWN_PROFESSION = 
    "profession,name,allowed_shifts\n"
    "инженер,Зотов,M;D\n";

std::string const TEST_CSV_MULTIPLE_EMPLOYEES = 
    "profession,name,allowed_shifts\n"
    "повар,Повар1,M;D\n"
    "повар,Повар2,D;N\n"
    "повар,Повар3,M;D;N\n"
    "официант,Официант1,M\n"
    "официант,Официант2,D\n"
    "официант,Официант3,N\n"
    "уборщик,Уборщик1,M;D;N\n"
    "администратор,Админ1,D\n";

std::string const TEST_CSV_NO_ALLOWED_SHIFTS = 
    "profession,name,allowed_shifts\n"
    "повар,СвободныйПовар,\n"
    "официант,СвободныйОфициант,\n";

std::string const TEST_CSV_WITH_WHITESPACE = 
    "profession,name,allowed_shifts\n"
    "  повар  ,  Пробельный  ,  M ; D  \n";

}  // namespace

// ====== БАЗОВЫЕ ТЕСТЫ ======

TEST_F(ImportStaffAgentTest, ImportStaffFromCsv_Success)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, TEST_CSV_DATA);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Проверяем Антонова (повар)
  ScAddr antonovAddr = TestUtils::FindEmployeeByName(*m_ctx, "Захаренков");
  EXPECT_TRUE(antonovAddr.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_cook, antonovAddr, ScType::ConstPermPosArc));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, antonovAddr, SchedulingKeynodes::concept_morning_shift));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, antonovAddr, SchedulingKeynodes::concept_day_shift));

  // Проверяем Дмитриева (официант, все смены)
  ScAddr dmitrievAddr = TestUtils::FindEmployeeByName(*m_ctx, "Лойко");
  EXPECT_TRUE(dmitrievAddr.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_waiter, dmitrievAddr, ScType::ConstPermPosArc));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, dmitrievAddr, SchedulingKeynodes::concept_morning_shift));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, dmitrievAddr, SchedulingKeynodes::concept_day_shift));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, dmitrievAddr, SchedulingKeynodes::concept_night_shift));

  // Проверяем Львова (администратор, только день)
  ScAddr lvovAddr = TestUtils::FindEmployeeByName(*m_ctx, "Шумилов");
  EXPECT_TRUE(lvovAddr.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_admin, lvovAddr, ScType::ConstPermPosArc));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, lvovAddr, SchedulingKeynodes::concept_day_shift));
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, lvovAddr), 1);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

TEST_F(ImportStaffAgentTest, ImportStaffFromCsv_UnknownProfession)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, TEST_CSV_UNKNOWN_PROFESSION);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Петров не должен быть создан
  ScAddr petrovAddr = TestUtils::FindEmployeeByName(*m_ctx, "Зотов");
  EXPECT_FALSE(petrovAddr.IsValid());

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

TEST_F(ImportStaffAgentTest, ImportStaffFromCsv_EmptyData)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, "");
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedWithError());

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

// ====== ТЕСТЫ МНОЖЕСТВЕННОГО ИМПОРТА ======

TEST_F(ImportStaffAgentTest, ImportMultipleEmployees)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, TEST_CSV_MULTIPLE_EMPLOYEES);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Проверяем количество сотрудников по профессиям
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cook), 3);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_waiter), 3);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cleaner), 1);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_admin), 1);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

TEST_F(ImportStaffAgentTest, ImportEmployees_DifferentShiftRestrictions)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, TEST_CSV_MULTIPLE_EMPLOYEES);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Повар1 - M;D (2 смены)
  ScAddr cook1 = TestUtils::FindEmployeeByName(*m_ctx, "Повар1");
  EXPECT_TRUE(cook1.IsValid());
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, cook1), 2);
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, cook1, SchedulingKeynodes::concept_morning_shift));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, cook1, SchedulingKeynodes::concept_day_shift));

  // Повар2 - D;N (2 смены)
  ScAddr cook2 = TestUtils::FindEmployeeByName(*m_ctx, "Повар2");
  EXPECT_TRUE(cook2.IsValid());
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, cook2), 2);
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, cook2, SchedulingKeynodes::concept_day_shift));
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, cook2, SchedulingKeynodes::concept_night_shift));

  // Повар3 - M;D;N (3 смены)
  ScAddr cook3 = TestUtils::FindEmployeeByName(*m_ctx, "Повар3");
  EXPECT_TRUE(cook3.IsValid());
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, cook3), 3);

  // Официант1 - только утро (1 смена)
  ScAddr waiter1 = TestUtils::FindEmployeeByName(*m_ctx, "Официант1");
  EXPECT_TRUE(waiter1.IsValid());
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, waiter1), 1);
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, waiter1, SchedulingKeynodes::concept_morning_shift));

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

// ====== ТЕСТЫ БЕЗ ОГРАНИЧЕНИЙ ======

TEST_F(ImportStaffAgentTest, ImportEmployees_NoAllowedShifts)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, TEST_CSV_NO_ALLOWED_SHIFTS);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Сотрудники без указанных allowed_shifts могут работать на всех сменах (нет ограничений)
  ScAddr freeCook = TestUtils::FindEmployeeByName(*m_ctx, "СвободныйПовар");
  EXPECT_TRUE(freeCook.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_cook, freeCook, ScType::ConstPermPosArc));
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, freeCook), 0);

  ScAddr freeWaiter = TestUtils::FindEmployeeByName(*m_ctx, "СвободныйОфициант");
  EXPECT_TRUE(freeWaiter.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_waiter, freeWaiter, ScType::ConstPermPosArc));
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, freeWaiter), 0);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

// ====== ТЕСТЫ ГРАНИЧНЫХ СЛУЧАЕВ ======

TEST_F(ImportStaffAgentTest, ImportStaff_OnlyHeader)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  // CSV только с заголовком
  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, "profession,name,allowed_shifts\n");
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Никаких сотрудников не должно быть создано
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cook), 0);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_waiter), 0);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

TEST_F(ImportStaffAgentTest, ImportStaff_AllProfessions)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  std::string csv = 
      "profession,name,allowed_shifts\n"
      "повар,TestCook,M;D;N\n"
      "официант,TestWaiter,M;D;N\n"
      "уборщик,TestCleaner,M;D;N\n"
      "администратор,TestAdmin,D\n";

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, csv);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // По одному сотруднику каждой профессии
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cook), 1);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_waiter), 1);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cleaner), 1);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_admin), 1);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

TEST_F(ImportStaffAgentTest, ImportStaff_CyrillicNames)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  std::string csv = 
      "profession,name,allowed_shifts\n"
      "повар,Александр Иванов,U;D\n"
      "официант,Мария Петрова,D;N\n";

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, csv);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Проверяем имена с пробелами
  ScAddr ivanov = TestUtils::FindEmployeeByName(*m_ctx, "Александр Иванов");
  EXPECT_TRUE(ivanov.IsValid());

  ScAddr petrova = TestUtils::FindEmployeeByName(*m_ctx, "Мария Петрова");
  EXPECT_TRUE(petrova.IsValid());

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

// ====== ТЕСТЫ ИНТЕГРАЦИИ ======

TEST_F(ImportStaffAgentTest, ImportStaff_ForbiddenShifts)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  // Формат с запрещёнными сменами: profession,name,allowed_shifts,forbidden_shifts
  std::string csv = 
      "profession,name,allowed_shifts,forbidden_shifts\n"
      "повар,ПоварБезНочи,,N\n"
      "администратор,АдминТолькоДень,D,\n";

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, csv);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Проверяем повара с запрещённой ночной сменой
  ScAddr cook = TestUtils::FindEmployeeByName(*m_ctx, "ПоварБезНочи");
  EXPECT_TRUE(cook.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_cook, cook, ScType::ConstPermPosArc));
  
  // Проверяем nrel_can_not_work -> concept_night_shift
  ScIterator5Ptr itForbidden = m_ctx->CreateIterator5(
      cook, ScType::ConstCommonArc, SchedulingKeynodes::concept_night_shift,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_can_not_work);
  EXPECT_TRUE(itForbidden->Next());
  
  // У повара не должно быть разрешённых смен (пустая колонка)
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, cook), 0);

  // Проверяем админа с разрешённой дневной сменой
  ScAddr admin = TestUtils::FindEmployeeByName(*m_ctx, "АдминТолькоДень");
  EXPECT_TRUE(admin.IsValid());
  EXPECT_TRUE(m_ctx->CheckConnector(SchedulingKeynodes::concept_admin, admin, ScType::ConstPermPosArc));
  
  // Проверяем nrel_allowed_shift -> concept_day_shift
  EXPECT_TRUE(TestUtils::HasAllowedShift(*m_ctx, admin, SchedulingKeynodes::concept_day_shift));
  EXPECT_EQ(TestUtils::CountAllowedShifts(*m_ctx, admin), 1);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}

TEST_F(ImportStaffAgentTest, ImportStaff_FullTaskExample)
{
  m_ctx->SubscribeAgent<ImportStaffAgent>();

  // Полный пример из задания (Вариант 7)
  std::string csv = 
      "profession,name,allowed_shifts,forbidden_shifts\n"
      "повар,Антонов,,N\n"
      "повар,Белов,,N\n"
      "повар,Васильев,,N\n"
      "повар,Гришин,,N\n"
      "официант,Дмитриев,,\n"
      "официант,Егоров,,\n"
      "официант,Жуков,,\n"
      "официант,Зайцев,,\n"
      "уборщик,Иванов,,\n"
      "уборщик,Козлов,,\n"
      "администратор,Львов,D,\n"
      "администратор,Михайлов,D,\n";

  ScAddr action = m_ctx->GenerateNode(ScType::ConstNode);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::action_import_staff_from_csv, action);

  ScAddr link = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(link, csv);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, link);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::nrel_file_content, arc);

  ScAction scAction = m_ctx->ConvertToAction(action);
  EXPECT_TRUE(scAction.InitiateAndWait(5000));
  EXPECT_TRUE(scAction.IsFinishedSuccessfully());

  // Проверяем количество сотрудников по профессиям
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cook), 4);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_waiter), 4);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_cleaner), 2);
  EXPECT_EQ(TestUtils::CountEmployeesByProfession(*m_ctx, SchedulingKeynodes::concept_admin), 2);

  m_ctx->UnsubscribeAgent<ImportStaffAgent>();
}
