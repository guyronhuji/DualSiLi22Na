#ifndef DUALSILI22NA_TRACKING_ACTION_HH
#define DUALSILI22NA_TRACKING_ACTION_HH

#include "G4UserTrackingAction.hh"
#include "globals.hh"

class EventAction;

class TrackingAction : public G4UserTrackingAction {
public:
  TrackingAction(EventAction* eventAction, G4bool countTruth);

  void PreUserTrackingAction(const G4Track* track) override;

private:
  EventAction* fEventAction = nullptr;
  G4bool fCountTruth = false;
};

#endif
