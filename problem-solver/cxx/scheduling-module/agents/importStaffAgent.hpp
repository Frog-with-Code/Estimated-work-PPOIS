#pragma once
#include <sc-memory/sc_agent.hpp>
#include <map>
#include <vector>
#include <string>

struct CsvFormat
{
  bool hasForbiddenColumn = false;
  bool hasAllowedColumn = false;
  int columnCount = 0;
};

class ImportStaffAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  std::map<std::string, ScAddr> GetProfessionMap();
  std::map<std::string, ScAddr> GetShiftMap();
  ScAddr GetShiftConcept(const std::string& shiftCode);
  
  void CreateEmployee(
      const std::string& name,
      ScAddr const & profession,
      const std::vector<std::string>& allowedShifts,
      const std::vector<std::string>& forbiddenShifts);
  
  void AddShiftRestriction(
      ScAddr const & employee,
      ScAddr const & shift,
      ScAddr const & relation);
  
  std::string GetCsvData(ScAction & action);
  CsvFormat ParseCsvHeader(const std::string& header);
  void ParseCsvRow(
      const std::string& line,
      CsvFormat const & format,
      std::string& profession,
      std::string& name,
      std::vector<std::string>& allowedShifts,
      std::vector<std::string>& forbiddenShifts);
};
