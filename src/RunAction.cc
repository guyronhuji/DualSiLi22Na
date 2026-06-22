#include "RunAction.hh"

#include "G4Run.hh"
#include "G4Exception.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"
#include "G4ios.hh"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
constexpr std::streamsize kShardBufferSize = 4 * 1024 * 1024;
std::atomic<G4int> gEventsWritten{0};
std::atomic<G4int> gEventsWithSiLiEnergy{0};
std::atomic<G4int> gEventsWithHPGeEnergy{0};

std::size_t Idx(DetectorId id) {
  return static_cast<std::size_t>(id);
}

std::string QuoteForShell(const std::string& value) {
  std::string result = "'";
  for (char c : value) {
    if (c == '\'') {
      result += "'\\''";
    } else {
      result += c;
    }
  }
  result += "'";
  return result;
}

std::string ToStdString(const G4String& value) {
  return std::string(value.data());
}

bool InGate(G4double energy, G4double minEnergy, G4double maxEnergy) {
  return energy >= minEnergy && energy <= maxEnergy;
}
}

RunAction::RunAction(std::shared_ptr<DetectorParameters> parameters)
    : fParameters(std::move(parameters)) {}

void RunAction::BeginOfRunAction(const G4Run*) {
  fIsMaster = G4Threading::IsMasterThread();
  fThreadId = G4Threading::G4GetThreadId();
  fEventsProcessed = 0;
  fEventsWritten = 0;
  fEventsWithSiLiEnergy = 0;
  fEventsWithHPGeEnergy = 0;

  if (fIsMaster) {
    gEventsWritten = 0;
    gEventsWithSiLiEnergy = 0;
    gEventsWithHPGeEnergy = 0;
    std::filesystem::remove_all(ShardDirectory());
    std::filesystem::create_directories(ShardDirectory());
    G4cout << "Output: final Parquet file will be "
           << fParameters->outputFileName << G4endl
           << "Output: worker CSV shards will be written under "
           << ShardDirectory() << G4endl;
    if (fParameters->hpgeGateEnabled) {
      G4cout << "Output: HPGe gated row writing enabled: mode="
             << fParameters->hpgeGateMode << ", window=["
             << fParameters->hpgeGateMinEnergy / keV << ", "
             << fParameters->hpgeGateMaxEnergy / keV << "] keV"
             << G4endl;
    }
    return;
  }

  OpenShard();
  OpenProgress();
}

void RunAction::EndOfRunAction(const G4Run* run) {
  if (!fIsMaster) {
    WriteProgress(true);
    CloseProgress();
    CloseShard();
    gEventsWritten += fEventsWritten;
    gEventsWithSiLiEnergy += fEventsWithSiLiEnergy;
    gEventsWithHPGeEnergy += fEventsWithHPGeEnergy;
    return;
  }

  CombineShardsOnMaster(run);

  G4cout << G4endl
         << "Run summary" << G4endl
         << "  number of generated events: " << run->GetNumberOfEvent()
         << G4endl
         << "  number of events with nonzero Si(Li) energy: "
         << gEventsWithSiLiEnergy.load() << G4endl
         << "  number of events with nonzero HPGe energy: "
         << gEventsWithHPGeEnergy.load() << G4endl
         << "  number of event rows written: " << gEventsWritten.load()
         << G4endl
         << "  final combined output: " << fParameters->outputFileName
         << G4endl;
  if (fParameters->hpgeGateEnabled) {
    G4cout << "  HPGe output gate: mode=" << fParameters->hpgeGateMode
           << ", window=[" << fParameters->hpgeGateMinEnergy / keV << ", "
           << fParameters->hpgeGateMaxEnergy / keV << "] keV" << G4endl;
  }
}

void RunAction::OpenShard() {
  std::filesystem::create_directories(ShardDirectory());
  fShardBuffer.assign(kShardBufferSize, '\0');
  fShard.rdbuf()->pubsetbuf(fShardBuffer.data(), fShardBuffer.size());
  fShard.open(ShardPath(), std::ios::out | std::ios::trunc);
  if (!fShard) {
    G4Exception("RunAction::OpenShard", "ShardOpenFailed", FatalException,
                ("Could not open shard file " + ShardPath()).c_str());
  }
  WriteHeader();
}

void RunAction::CloseShard() {
  if (fShard.is_open()) {
    fShard.close();
  }
}

void RunAction::OpenProgress() {
  if (fParameters->progressFileName.empty() ||
      fParameters->progressUpdateInterval <= 0) {
    return;
  }

  const auto path = ProgressPath();
  const auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  fProgress.open(path, std::ios::out | std::ios::trunc);
  if (!fProgress) {
    G4Exception("RunAction::OpenProgress", "ProgressOpenFailed",
                FatalException,
                ("Could not open progress file " + path).c_str());
  }
  WriteProgress(true);
}

void RunAction::CloseProgress() {
  if (fProgress.is_open()) {
    fProgress.close();
  }
}

void RunAction::WriteProgress(G4bool force) {
  if (!fProgress.is_open()) {
    return;
  }
  if (!force &&
      fEventsProcessed %
              static_cast<std::uint64_t>(fParameters->progressUpdateInterval) !=
          0) {
    return;
  }
  fProgress << fEventsProcessed << '\n' << std::flush;
}

std::string RunAction::ShardDirectory() const {
  std::filesystem::path output(ToStdString(fParameters->outputFileName));
  const auto parent = output.has_parent_path() ? output.parent_path()
                                               : std::filesystem::path(".");
  return (parent / (output.stem().string() + "_shards")).string();
}

std::string RunAction::ShardPath() const {
  const auto threadName = fThreadId < 0 ? "serial" : std::to_string(fThreadId);
  return (std::filesystem::path(ShardDirectory()) /
          ("events_t" + threadName + ".csv"))
      .string();
}

std::string RunAction::ProgressPath() const {
  std::filesystem::path path(ToStdString(fParameters->progressFileName));
  const auto threadName = fThreadId < 0 ? "serial" : std::to_string(fThreadId);
  const auto parent = path.has_parent_path() ? path.parent_path()
                                             : std::filesystem::path(".");
  const auto stem = path.stem().string();
  const auto extension = path.extension().string();
  return (parent / (stem + "_t" + threadName + extension)).string();
}

void RunAction::WriteHeader() {
  fShard << "eventID,event_weight,E_SiLi_1_keV,E_SiLi_2_keV,E_SiLi_sum_keV,"
            "E_HPGe_1_keV,E_HPGe_2_keV,E_HPGe_3_keV,"
            "E_total_all_detectors_keV,Nhit_SiLi_1,Nhit_SiLi_2,"
            "Nhit_HPGe_1,Nhit_HPGe_2,Nhit_HPGe_3,tfirst_SiLi_1_ns,"
            "tfirst_SiLi_2_ns,tfirst_HPGe_1_ns,tfirst_HPGe_2_ns,"
            "tfirst_HPGe_3_ns,primary_beta_kinetic_keV";
  if (fParameters->truthOutput) {
    fShard << ",primary_decay_time_ns,primary_vertex_x_mm,primary_vertex_y_mm,"
              "primary_vertex_z_mm,n_positrons_created,n_511keV_gammas_created,"
              "n_1274keV_gammas_created";
  }
  fShard << '\n';
}

void RunAction::CombineShardsOnMaster(const G4Run*) {
  const std::string script = "analysis/combine_shards_to_parquet.py";
  std::ostringstream command;
  command << ToStdString(fParameters->parquetPythonCommand) << ' '
          << QuoteForShell(script)
          << " --input-dir " << QuoteForShell(ShardDirectory())
          << " --output " << QuoteForShell(ToStdString(fParameters->outputFileName));
  if (fParameters->truthOutput) {
    command << " --truth";
  }

  const auto status = std::system(command.str().c_str());
  if (status != 0) {
    G4Exception("RunAction::CombineShardsOnMaster", "ParquetCombineFailed",
                FatalException,
                "Failed to combine CSV shards into the requested Parquet file. "
                "Make sure python3 and pyarrow are available.");
  }
}

G4bool RunAction::PassesHpgeOutputGate(G4double eHPGe1, G4double eHPGe2,
                                       G4double eHPGe3) const {
  if (!fParameters->hpgeGateEnabled) {
    return true;
  }

  const auto minEnergy = fParameters->hpgeGateMinEnergy / keV;
  const auto maxEnergy = fParameters->hpgeGateMaxEnergy / keV;
  const auto hpge1 = InGate(eHPGe1, minEnergy, maxEnergy);
  const auto hpge2 = InGate(eHPGe2, minEnergy, maxEnergy);
  const auto hpge3 = InGate(eHPGe3, minEnergy, maxEnergy);
  const auto mode = ToStdString(fParameters->hpgeGateMode);

  if (mode == "any") {
    return hpge1 || hpge2 || hpge3;
  }
  if (mode == "hpge12" || mode == "backToBack" || mode == "back_to_back") {
    return hpge1 && hpge2;
  }
  if (mode == "all" || mode == "hpge123" || mode == "triple") {
    return hpge1 && hpge2 && hpge3;
  }

  G4Exception("RunAction::PassesHpgeOutputGate", "UnknownHPGeGateMode",
              FatalException,
              ("Unknown /output/hpgeGateMode: " + mode +
               ". Use any, hpge12, all, or hpge123.")
                  .c_str());
  return false;
}

G4bool RunAction::HpgeOutputGateCannotPass(const EventRecord& record) const {
  if (!fParameters->hpgeGateEnabled) {
    return false;
  }

  const auto maxEnergy = fParameters->hpgeGateMaxEnergy;
  const auto eHPGe1 = record.energy[Idx(DetectorId::HPGe1)];
  const auto eHPGe2 = record.energy[Idx(DetectorId::HPGe2)];
  const auto eHPGe3 = record.energy[Idx(DetectorId::HPGe3)];
  const auto mode = ToStdString(fParameters->hpgeGateMode);

  if (mode == "any") {
    return eHPGe1 > maxEnergy && eHPGe2 > maxEnergy && eHPGe3 > maxEnergy;
  }
  if (mode == "hpge12" || mode == "backToBack" || mode == "back_to_back") {
    return eHPGe1 > maxEnergy || eHPGe2 > maxEnergy;
  }
  if (mode == "all" || mode == "hpge123" || mode == "triple") {
    return eHPGe1 > maxEnergy || eHPGe2 > maxEnergy || eHPGe3 > maxEnergy;
  }

  G4Exception("RunAction::HpgeOutputGateCannotPass", "UnknownHPGeGateMode",
              FatalException,
              ("Unknown /output/hpgeGateMode: " + mode +
               ". Use any, hpge12, all, or hpge123.")
                  .c_str());
  return true;
}

void RunAction::FillEvent(const EventRecord& record) {
  if (!fShard.is_open()) {
    return;
  }

  ++fEventsProcessed;
  WriteProgress(false);

  const auto eSiLi1 = record.energy[Idx(DetectorId::SiLi1)] / keV;
  const auto eSiLi2 = record.energy[Idx(DetectorId::SiLi2)] / keV;
  const auto eHPGe1 = record.energy[Idx(DetectorId::HPGe1)] / keV;
  const auto eHPGe2 = record.energy[Idx(DetectorId::HPGe2)] / keV;
  const auto eHPGe3 = record.energy[Idx(DetectorId::HPGe3)] / keV;
  const auto eSiLiSum = eSiLi1 + eSiLi2;
  const auto eHPGeSum = eHPGe1 + eHPGe2 + eHPGe3;

  if (eSiLiSum > 0.0) {
    ++fEventsWithSiLiEnergy;
  }
  if (eHPGeSum > 0.0) {
    ++fEventsWithHPGeEnergy;
  }

  if (!PassesHpgeOutputGate(eHPGe1, eHPGe2, eHPGe3)) {
    return;
  }

  auto timeNs = [](G4double value) {
    return value < 0.0 ? -1.0 : value / ns;
  };

  fShard << std::setprecision(8) << record.eventID << ','
         << record.eventWeight << ',' << eSiLi1 << ',' << eSiLi2 << ','
         << eSiLiSum << ',' << eHPGe1 << ',' << eHPGe2 << ',' << eHPGe3
         << ',' << (eSiLiSum + eHPGeSum) << ','
         << record.hits[Idx(DetectorId::SiLi1)] << ','
         << record.hits[Idx(DetectorId::SiLi2)] << ','
         << record.hits[Idx(DetectorId::HPGe1)] << ','
         << record.hits[Idx(DetectorId::HPGe2)] << ','
         << record.hits[Idx(DetectorId::HPGe3)] << ','
         << timeNs(record.firstTime[Idx(DetectorId::SiLi1)]) << ','
         << timeNs(record.firstTime[Idx(DetectorId::SiLi2)]) << ','
         << timeNs(record.firstTime[Idx(DetectorId::HPGe1)]) << ','
         << timeNs(record.firstTime[Idx(DetectorId::HPGe2)]) << ','
         << timeNs(record.firstTime[Idx(DetectorId::HPGe3)]) << ','
         << record.primaryBetaKineticEnergy / keV;

  if (fParameters->truthOutput) {
    fShard << ',' << record.primaryTime / ns << ',' << record.vertexX / mm
           << ',' << record.vertexY / mm << ',' << record.vertexZ / mm << ','
           << record.positronsCreated << ',' << record.gammas511Created << ','
           << record.gammas1274Created;
  }
  fShard << '\n';
  ++fEventsWritten;
}
