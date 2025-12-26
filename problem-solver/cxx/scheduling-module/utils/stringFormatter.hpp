#pragma once

#include <string>
#include <vector>

class StringFormatter
{
public:
  static void Ltrim(std::string & str);
  static void Rtrim(std::string & str);
  static std::string Trim(std::string str);
  static std::vector<std::string> Split(std::string const & str, char const & separator = ';');
  static std::vector<std::string> ParseShifts(std::string const & shiftsStr);
};

class StringOperationInSystemIdentifier
{
public:
  static int GetSystemIdentifier(std::string const &);
};

class DividerNumberFromString
{
public:
  static std::string GetNumberOfRoute(std::string const &);
};