#include "TestUtils.hpp"
#include <sc-memory/sc_memory.hpp>
#include "../../keynodes/scheduling-keynodes.hpp"

namespace TestUtils
{

ScAddr FindEmployeeByName(ScAgentContext & ctx, std::string const & targetName)
{
  ScIterator5Ptr it = ctx.CreateIterator5(
      ScType::ConstNode, ScType::ConstCommonArc, ScType::ConstNodeLink,
      ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf);
  while (it->Next())
  {
    std::string name;
    ctx.GetLinkContent(it->Get(2), name);
    if (name == targetName)
    {
      return it->Get(0);
    }
  }
  return ScAddr();
}

int CountEmployeesByProfession(ScAgentContext & ctx, ScAddr const & profession)
{
  int count = 0;
  ScIterator3Ptr it = ctx.CreateIterator3(
      profession, ScType::ConstPermPosArc, ScType::ConstNode);
  while (it->Next())
  {
    count++;
  }
  return count;
}

bool HasAllowedShift(ScAgentContext & ctx, ScAddr const & employee, ScAddr const & shift)
{
  ScIterator5Ptr it = ctx.CreateIterator5(
      employee, ScType::ConstCommonArc, shift,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_allowed_shift);
  return it->Next();
}

int CountAllowedShifts(ScAgentContext & ctx, ScAddr const & employee)
{
  int count = 0;
  ScIterator5Ptr it = ctx.CreateIterator5(
      employee, ScType::ConstCommonArc, ScType::ConstNode,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_allowed_shift);
  while (it->Next())
  {
    count++;
  }
  return count;
}

int GetEmployeeWorkload(ScAgentContext & ctx, ScAddr const & employee)
{
  ScIterator5Ptr it = ctx.CreateIterator5(
      employee, ScType::ConstCommonArc, ScType::ConstNodeLink,
      ScType::ConstPermPosArc, SchedulingKeynodes::nrel_workload);
  if (it->Next())
  {
    std::string content;
    ctx.GetLinkContent(it->Get(2), content);
    try
    {
      return std::stoi(content);
    }
    catch (...)
    {
      return -1;
    }
  }
  return 0;
}

int GetEmployeeWorkloadByName(ScAgentContext & ctx, std::string const & name)
{
  ScAddr emp = FindEmployeeByName(ctx, name);
  if (!emp.IsValid())
    return -1;
  return GetEmployeeWorkload(ctx, emp);
}

}  // namespace TestUtils
