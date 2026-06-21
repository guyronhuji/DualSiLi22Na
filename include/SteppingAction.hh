#ifndef DUALSILI22NA_STEPPING_ACTION_HH
#define DUALSILI22NA_STEPPING_ACTION_HH

#include "DetectorParameters.hh"
#include "G4UserSteppingAction.hh"

#include <memory>

class SteppingAction : public G4UserSteppingAction {
public:
  explicit SteppingAction(std::shared_ptr<DetectorParameters> parameters);

  void UserSteppingAction(const G4Step* step) override;

private:
  std::shared_ptr<DetectorParameters> fParameters;
};

#endif
