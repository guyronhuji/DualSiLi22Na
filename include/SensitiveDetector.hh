#ifndef DUALSILI22NA_SENSITIVE_DETECTOR_HH
#define DUALSILI22NA_SENSITIVE_DETECTOR_HH

#include "EventAction.hh"
#include "G4VSensitiveDetector.hh"

class SensitiveDetector : public G4VSensitiveDetector {
public:
  SensitiveDetector(const G4String& name, DetectorId detectorId);

  G4bool ProcessHits(G4Step* step, G4TouchableHistory* history) override;

private:
  DetectorId fDetectorId;
};

#endif
