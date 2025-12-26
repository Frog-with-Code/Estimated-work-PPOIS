#include "importStaffAgent.hpp"
#include "keynodes/scheduling-keynodes.hpp"
#include "utils/stringFormatter.hpp"
#include <algorithm>
#include <sc-memory/sc_memory.hpp>

ScAddr ImportStaffAgent::GetActionClass() const
{
  return SchedulingKeynodes::action_import_staff_from_csv;
}

std::map<std::string, ScAddr> ImportStaffAgent::GetProfessionMap()
{
  return {
      {"повар", SchedulingKeynodes::concept_cook},
      {"официант", SchedulingKeynodes::concept_waiter},
      {"уборщик", SchedulingKeynodes::concept_cleaner},
      {"администратор", SchedulingKeynodes::concept_admin}
  };
}

std::map<std::string, ScAddr> ImportStaffAgent::GetShiftMap()
{
  return {
      {"M", SchedulingKeynodes::concept_morning_shift},
      {"D", SchedulingKeynodes::concept_day_shift},
      {"N", SchedulingKeynodes::concept_night_shift}
  };
}

ScAddr ImportStaffAgent::GetShiftConcept(const std::string& code)
{
  auto shiftMap = GetShiftMap();
  auto it = shiftMap.find(code);
  if (it != shiftMap.end())
    return it->second;
  return ScAddr();
}

void ImportStaffAgent::AddShiftRestriction(
    ScAddr const & employee, ScAddr const & shift, ScAddr const & relation)
{
  ScAddr arc = m_context.GenerateConnector(ScType::ConstCommonArc, employee, shift);
  m_context.GenerateConnector(ScType::ConstPermPosArc, relation, arc);
}

void ImportStaffAgent::CreateEmployee(
    const std::string& name,
    ScAddr const & profession,
    const std::vector<std::string>& allowedShifts,
    const std::vector<std::string>& forbiddenShifts)
{
  ScAddr emp = m_context.GenerateNode(ScType::ConstNode);

  // Имя сотрудника
  ScAddr nameLink = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(nameLink, name);
  ScAddr nameArc = m_context.GenerateConnector(ScType::ConstCommonArc, emp, nameLink);
  m_context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf, nameArc);

  // Профессия и класс
  m_context.GenerateConnector(ScType::ConstPermPosArc, profession, emp);
  m_context.GenerateConnector(ScType::ConstPermPosArc, SchedulingKeynodes::concept_employee, emp);

  // Разрешённые смены
  for (const auto& shiftCode : allowedShifts)
  {
    if (shiftCode.empty())
      continue;
    ScAddr shift = GetShiftConcept(shiftCode);
    if (shift.IsValid())
      AddShiftRestriction(emp, shift, SchedulingKeynodes::nrel_allowed_shift);
  }

  // Запрещённые смены
  for (const auto& shiftCode : forbiddenShifts)
  {
    if (shiftCode.empty())
      continue;
    ScAddr shift = GetShiftConcept(shiftCode);
    if (shift.IsValid())
      AddShiftRestriction(emp, shift, SchedulingKeynodes::nrel_can_not_work);
  }
}

std::string ImportStaffAgent::GetCsvData(ScAction & action)
{
  ScIterator5Ptr it5 = m_context.CreateIterator5(
      action, ScType::ConstCommonArc, ScType::ConstNodeLink, ScType::ConstPermPosArc,
      SchedulingKeynodes::nrel_file_content);
  if (!it5->Next())
  {
    m_logger.Error("CSV data not provided");
    return "";
  }

  std::string csvData;
  m_context.GetLinkContent(it5->Get(2), csvData);
  return csvData;
}

CsvFormat ImportStaffAgent::ParseCsvHeader(const std::string& header)
{
  CsvFormat format;
  format.hasForbiddenColumn = (header.find("запрещённые") != std::string::npos || 
                                header.find("forbidden") != std::string::npos);
  format.hasAllowedColumn = (header.find("разрешённые") != std::string::npos || 
                             header.find("allowed") != std::string::npos);
  format.columnCount = 1 + std::count(header.begin(), header.end(), ',');
  return format;
}

void ImportStaffAgent::ParseCsvRow(
    const std::string& line,
    CsvFormat const & format,
    std::string& profession,
    std::string& name,
    std::vector<std::string>& allowedShifts,
    std::vector<std::string>& forbiddenShifts)
{
  std::stringstream ss(line);
  std::string col3, col4;

  std::getline(ss, profession, ',');
  std::getline(ss, name, ',');
  std::getline(ss, col3, ',');
  if (format.columnCount >= 4)
    std::getline(ss, col4, ',');

  profession = StringFormatter::Trim(profession);
  name = StringFormatter::Trim(name);
  col3 = StringFormatter::Trim(col3);
  col4 = StringFormatter::Trim(col4);

  if (format.columnCount >= 4)
  {
    allowedShifts = StringFormatter::ParseShifts(col3);
    forbiddenShifts = StringFormatter::ParseShifts(col4);
  }
  else if (format.columnCount == 3)
  {
    if (format.hasForbiddenColumn && !format.hasAllowedColumn)
      forbiddenShifts = StringFormatter::ParseShifts(col3);
    else
      allowedShifts = StringFormatter::ParseShifts(col3);
  }
}

ScResult ImportStaffAgent::DoProgram(ScAction & action)
{
  std::string csvData = GetCsvData(action);
  if (csvData.empty())
  {
    m_logger.Error("CSV data is empty");
    return action.FinishWithError();
  }

  std::istringstream dataStream(csvData);
  std::string line;

  // Парсим заголовок
  std::getline(dataStream, line);
  CsvFormat format = ParseCsvHeader(StringFormatter::Trim(line));

  auto profMap = GetProfessionMap();
  int importedCount = 0;

  while (std::getline(dataStream, line))
  {
    line = StringFormatter::Trim(line);
    if (line.empty())
      continue;

    std::string profession, name;
    std::vector<std::string> allowedShifts, forbiddenShifts;

    ParseCsvRow(line, format, profession, name, allowedShifts, forbiddenShifts);

    if (profMap.count(profession) == 0)
    {
      m_logger.Warning("Unknown profession: ", profession);
      continue;
    }

    CreateEmployee(name, profMap[profession], allowedShifts, forbiddenShifts);
    importedCount++;
  }

  m_logger.Info("Staff imported successfully: ", importedCount, " employees");
  return action.FinishSuccessfully();
}
