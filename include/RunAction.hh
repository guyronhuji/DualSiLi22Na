#ifndef DUALSILI22NA_RUN_ACTION_HH
#define DUALSILI22NA_RUN_ACTION_HH

#include "DetectorParameters.hh"
#include "EventAction.hh"
#include "G4UserRunAction.hh"

#include <cstdint>
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
  G4bool HpgeOutputGateCannotPass(const EventRecord& record) const;

private:
  void OpenShard();
  void CloseShard();
  void OpenProgress();
  void CloseProgress();
  void WriteProgress(G4bool force);
  void CombineShardsOnMaster(const G4Run* run);
  std::string ShardDirectory() const;
  std::string ShardPath() const;
  std::string ProgressPath() const;
  void WriteHeader();
  G4bool PassesHpgeOutputGate(G4double eHPGe1, G4double eHPGe2,
                              G4double eHPGe3) const;

  std::shared_ptr<DetectorParameters> fParameters;
  std::ofstream fShard;
  std::ofstream fProgress;
  std::vector<char> fShardBuffer;
  G4bool fIsMaster = false;
  G4int fThreadId = -1;
  std::uint64_t fEventsProcessed = 0;
  G4int fEventsWritten = 0;
  G4int fEventsWithSiLiEnergy = 0;
  G4int fEventsWithHPGeEnergy = 0;
};

#endif
