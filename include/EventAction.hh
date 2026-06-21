#ifndef DUALSILI22NA_EVENT_ACTION_HH
#define DUALSILI22NA_EVENT_ACTION_HH

#include "G4UserEventAction.hh"
#include "globals.hh"

#include <array>

enum class DetectorId : G4int {
  SiLi1 = 0,
  SiLi2 = 1,
  HPGe1 = 2,
  HPGe2 = 3,
  HPGe3 = 4,
  Count = 5
};

struct EventRecord {
  G4int eventID = -1;
  G4double eventWeight = 1.0;
  std::array<G4double, static_cast<std::size_t>(DetectorId::Count)> energy{};
  std::array<G4int, static_cast<std::size_t>(DetectorId::Count)> hits{};
  std::array<G4double, static_cast<std::size_t>(DetectorId::Count)> firstTime{};
  G4double primaryTime = 0.0;
  G4double vertexX = 0.0;
  G4double vertexY = 0.0;
  G4double vertexZ = 0.0;
  G4int positronsCreated = 0;
  G4int gammas511Created = 0;
  G4int gammas1274Created = 0;
};

class RunAction;

class EventAction : public G4UserEventAction {
public:
  explicit EventAction(RunAction* runAction);

  void BeginOfEventAction(const G4Event* event) override;
  void EndOfEventAction(const G4Event* event) override;

  void AddEnergy(DetectorId detector, G4double edep, G4double globalTime);
  void CountSecondary(const G4String& particleName, G4double kineticEnergy);

private:
  RunAction* fRunAction = nullptr;
  EventRecord fRecord;
};

#endif
