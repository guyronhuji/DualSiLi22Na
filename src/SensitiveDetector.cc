#include "SensitiveDetector.hh"

#include "EventAction.hh"

#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"

SensitiveDetector::SensitiveDetector(const G4String& name,
                                     DetectorId detectorId)
    : G4VSensitiveDetector(name), fDetectorId(detectorId) {}

G4bool SensitiveDetector::ProcessHits(G4Step* step,
                                      G4TouchableHistory*) {
  const auto edep = step->GetTotalEnergyDeposit();
  if (edep <= 0.0) {
    return false;
  }

  auto eventAction = const_cast<EventAction*>(static_cast<const EventAction*>(
      G4RunManager::GetRunManager()->GetUserEventAction()));
  if (eventAction) {
    eventAction->AddEnergy(fDetectorId, edep,
                           step->GetPreStepPoint()->GetGlobalTime());
  }
  return true;
}
