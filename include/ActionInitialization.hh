#ifndef DUALSILI22NA_ACTION_INITIALIZATION_HH
#define DUALSILI22NA_ACTION_INITIALIZATION_HH

#include "DetectorParameters.hh"
#include "G4VUserActionInitialization.hh"

#include <memory>

class ActionInitialization : public G4VUserActionInitialization {
public:
  explicit ActionInitialization(std::shared_ptr<DetectorParameters> parameters);

  void BuildForMaster() const override;
  void Build() const override;

private:
  std::shared_ptr<DetectorParameters> fParameters;
};

#endif
