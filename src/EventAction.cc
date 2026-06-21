#include "EventAction.hh"

#include "RunAction.hh"

#include "G4Event.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr G4double kGammaTolerance = 2.0 * keV;
}

EventAction::EventAction(RunAction* runAction) : fRunAction(runAction) {}

void EventAction::BeginOfEventAction(const G4Event* event) {
  fRecord = EventRecord{};
  fRecord.eventID = event->GetEventID();
  std::fill(fRecord.firstTime.begin(), fRecord.firstTime.end(), -1.0);
}

void EventAction::EndOfEventAction(const G4Event* event) {
  if (auto vertex = event->GetPrimaryVertex(0)) {
    if (auto primary = vertex->GetPrimary(0)) {
      fRecord.eventWeight = primary->GetWeight();
    } else {
      fRecord.eventWeight = vertex->GetWeight();
    }
    fRecord.primaryTime = vertex->GetT0();
    const auto pos = vertex->GetPosition();
    fRecord.vertexX = pos.x();
    fRecord.vertexY = pos.y();
    fRecord.vertexZ = pos.z();
  }

  fRunAction->FillEvent(fRecord);
}

void EventAction::AddEnergy(DetectorId detector, G4double edep,
                            G4double globalTime) {
  if (edep <= 0.0) {
    return;
  }

  const auto index = static_cast<std::size_t>(detector);
  fRecord.energy[index] += edep;
  fRecord.hits[index] += 1;
  if (fRecord.firstTime[index] < 0.0 || globalTime < fRecord.firstTime[index]) {
    fRecord.firstTime[index] = globalTime;
  }
}

void EventAction::CountSecondary(const G4String& particleName,
                                 G4double kineticEnergy) {
  if (particleName == "e+") {
    ++fRecord.positronsCreated;
  } else if (particleName == "gamma") {
    if (std::abs(kineticEnergy - 511.0 * keV) < kGammaTolerance) {
      ++fRecord.gammas511Created;
    }
    if (std::abs(kineticEnergy - 1274.5 * keV) < kGammaTolerance) {
      ++fRecord.gammas1274Created;
    }
  }
}
