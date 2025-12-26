#pragma once

#include <sc-memory/sc_addr.hpp>
#include <sc-memory/sc_keynodes.hpp>

class SchedulingKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_build_weekly_schedule{
    "action_build_weekly_schedule", ScType::ConstNodeClass};
  static inline ScKeynode const action_import_staff_from_csv{
    "action_import_staff_from_csv", ScType::ConstNodeClass};

  // Professions
  static inline ScKeynode const concept_employee{"concept_employee", ScType::ConstNodeClass};
  static inline ScKeynode const concept_cook{"concept_cook", ScType::ConstNodeClass};
  static inline ScKeynode const concept_waiter{"concept_waiter", ScType::ConstNodeClass};
  static inline ScKeynode const concept_cleaner{"concept_cleaner", ScType::ConstNodeClass};
  static inline ScKeynode const concept_admin{"concept_admin", ScType::ConstNodeClass};

  // Shifts
  static inline ScKeynode const concept_shift{"concept_shift", ScType::ConstNodeClass};
  static inline ScKeynode const concept_morning_shift{"concept_morning_shift", ScType::ConstNodeClass};
  static inline ScKeynode const concept_day_shift{"concept_day_shift", ScType::ConstNodeClass};
  static inline ScKeynode const concept_night_shift{"concept_night_shift", ScType::ConstNodeClass};

  // Relations
  static inline ScKeynode const nrel_assigned_to_shift{"nrel_assigned_to_shift", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shift_type{"nrel_shift_type", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shift_day{"nrel_shift_day", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_allowed_shift{"nrel_allowed_shift", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_can_not_work{"nrel_can_not_work", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_workload{"nrel_workload", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_file_content{"nrel_file_content", ScType::ConstNodeNonRole};

  // Weekdays
  static inline ScKeynode const concept_weekday{"concept_weekday", ScType::ConstNodeClass};
  static inline ScKeynode const monday{"monday", ScType::ConstNode};
  static inline ScKeynode const tuesday{"tuesday", ScType::ConstNode};
  static inline ScKeynode const wednesday{"wednesday", ScType::ConstNode};
  static inline ScKeynode const thursday{"thursday", ScType::ConstNode};
  static inline ScKeynode const friday{"friday", ScType::ConstNode};
  static inline ScKeynode const saturday{"saturday", ScType::ConstNode};
  static inline ScKeynode const sunday{"sunday", ScType::ConstNode};

  // Schedule structure
  static inline ScKeynode const concept_schedule{"concept_schedule", ScType::ConstNodeClass};
  static inline ScKeynode const concept_shift_assignment{"concept_shift_assignment", ScType::ConstNodeClass};
  
  // Shift requirements
  static inline ScKeynode const concept_shift_requirements{"concept_shift_requirements", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_required_count{"nrel_required_count", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_max_shifts_per_week{"nrel_max_shifts_per_week", ScType::ConstNodeNonRole};
  
  // Bipartite graph (двудольный граф)
  static inline ScKeynode const concept_bipartite_graph{"concept_bipartite_graph", ScType::ConstNodeClass};
  static inline ScKeynode const concept_shift_slot{"concept_shift_slot", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_left_part{"nrel_left_part", ScType::ConstNodeNonRole};   // Левая доля (сотрудники)
  static inline ScKeynode const nrel_right_part{"nrel_right_part", ScType::ConstNodeNonRole}; // Правая доля (слоты)
  static inline ScKeynode const nrel_can_work{"nrel_can_work", ScType::ConstNodeNonRole};     // Ребро графа: сотрудник может работать в слоте
  static inline ScKeynode const nrel_slot_profession{"nrel_slot_profession", ScType::ConstNodeNonRole};
};
