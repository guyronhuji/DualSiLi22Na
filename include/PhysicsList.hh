#ifndef DUALSILI22NA_PHYSICS_LIST_HH
#define DUALSILI22NA_PHYSICS_LIST_HH

#include "G4VModularPhysicsList.hh"

class PhysicsList : public G4VModularPhysicsList {
public:
  PhysicsList();
  ~PhysicsList() override = default;

  void SetCuts() override;
};

#endif
