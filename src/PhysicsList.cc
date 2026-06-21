#include "PhysicsList.hh"

#include "G4DecayPhysics.hh"
#include "G4EmParameters.hh"
#include "G4EmStandardPhysics.hh"
#include "G4HadronicParameters.hh"
#include "G4RadioactiveDecayPhysics.hh"
#include "G4SystemOfUnits.hh"

PhysicsList::PhysicsList() {
  SetVerboseLevel(0);

  G4HadronicParameters::Instance()->SetTimeThresholdForRadioactiveDecay(
      1.0e30 * second);

  RegisterPhysics(new G4EmStandardPhysics());
  RegisterPhysics(new G4DecayPhysics());
  RegisterPhysics(new G4RadioactiveDecayPhysics());

  auto em = G4EmParameters::Instance();
  em->SetFluo(false);
  em->SetAuger(false);
  em->SetPixe(false);
  em->SetDeexActiveRegion("DefaultRegionForTheWorld", false, false, false);
}

void PhysicsList::SetCuts() {
  SetCutsWithDefault();
}
