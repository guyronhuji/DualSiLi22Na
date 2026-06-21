#include "TrackingAction.hh"

#include "EventAction.hh"

#include "G4Track.hh"

TrackingAction::TrackingAction(EventAction* eventAction, G4bool countTruth)
    : fEventAction(eventAction), fCountTruth(countTruth) {}

void TrackingAction::PreUserTrackingAction(const G4Track* track) {
  if (!fCountTruth || !fEventAction || track->GetParentID() == 0) {
    return;
  }

  fEventAction->CountSecondary(track->GetDefinition()->GetParticleName(),
                               track->GetKineticEnergy());
}
