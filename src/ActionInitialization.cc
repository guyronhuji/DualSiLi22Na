#include "ActionInitialization.hh"

#include "EventAction.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "TrackingAction.hh"

ActionInitialization::ActionInitialization(
    std::shared_ptr<DetectorParameters> parameters)
    : fParameters(std::move(parameters)) {}

void ActionInitialization::BuildForMaster() const {
  SetUserAction(new RunAction(fParameters));
}

void ActionInitialization::Build() const {
  SetUserAction(new PrimaryGeneratorAction(fParameters));

  auto runAction = new RunAction(fParameters);
  SetUserAction(runAction);

  auto eventAction = new EventAction(runAction);
  SetUserAction(eventAction);
  SetUserAction(new TrackingAction(eventAction, fParameters->truthOutput));
}
