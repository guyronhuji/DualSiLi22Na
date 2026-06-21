#include "SteppingAction.hh"

#include "G4ParticleDefinition.hh"
#include "G4Step.hh"
#include "G4Track.hh"

#include <utility>

SteppingAction::SteppingAction(std::shared_ptr<DetectorParameters> parameters)
    : fParameters(std::move(parameters)) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
  if (!fParameters->suppressFastPositronAnnihilation ||
      !(fParameters->fast22NaPrimaries || fParameters->importanceSampling)) {
    return;
  }

  auto track = step->GetTrack();
  if (track->GetDefinition()->GetParticleName() != "e+") {
    return;
  }

  if (track->GetKineticEnergy() <= fParameters->positronKillEnergy) {
    track->SetTrackStatus(fStopAndKill);
  }
}
