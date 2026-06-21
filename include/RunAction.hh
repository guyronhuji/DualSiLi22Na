#ifndef DUALSILI22NA_RUN_ACTION_HH
#define DUALSILI22NA_RUN_ACTION_HH

#include "DetectorParameters.hh"
#include "EventAction.hh"
#include "G4UserRunAction.hh"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

class RunAction : public G4UserRunAction {
public:
  explicit RunAction(std::shared_ptr<DetectorParameters> parameters);

  void BeginOfRunAction(const G4Run* run) override;
  void EndOfRunAction(const G4Run* run) override;

  void FillEvent(const EventRecord& record);

private:
  void OpenShard();
  void CloseShard();
  void CombineShardsOnMaster(const G4Run* run);
  std::string ShardDirectory() const;
  std::string ShardPath() const;
  void WriteHeader();

  std::shared_ptr<DetectorParameters> fParameters;
  std::ofstream fShard;
  std::vector<char> fShardBuffer;
  G4bool fIsMaster = false;
  G4int fThreadId = -1;
  G4int fEventsWritten = 0;
  G4int fEventsWithSiLiEnergy = 0;
  G4int fEventsWithHPGeEnergy = 0;
};

#endif
