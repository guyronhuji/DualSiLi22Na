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
}

RunAction::RunAction(std::shared_ptr<DetectorParameters> parameters)
    : fParameters(std::move(parameters)) {}

void RunAction::BeginOfRunAction(const G4Run*) {
  fIsMaster = G4Threading::IsMasterThread();
  fThreadId = G4Threading::G4GetThreadId();
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
    return;
  }

  OpenShard();
}

void RunAction::EndOfRunAction(const G4Run* run) {
  if (!fIsMaster) {
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

void RunAction::WriteHeader() {
  fShard << "eventID,event_weight,E_SiLi_1_keV,E_SiLi_2_keV,E_SiLi_sum_keV,"
            "E_HPGe_1_keV,E_HPGe_2_keV,E_HPGe_3_keV,"
            "E_total_all_detectors_keV,Nhit_SiLi_1,Nhit_SiLi_2,"
            "Nhit_HPGe_1,Nhit_HPGe_2,Nhit_HPGe_3,tfirst_SiLi_1_ns,"
            "tfirst_SiLi_2_ns,tfirst_HPGe_1_ns,tfirst_HPGe_2_ns,"
            "tfirst_HPGe_3_ns";
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
                "Make sure python3, pyarrow, and pandas are available.");
  }
}

void RunAction::FillEvent(const EventRecord& record) {
  if (!fShard.is_open()) {
    return;
  }

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
         << timeNs(record.firstTime[Idx(DetectorId::HPGe3)]);

  if (fParameters->truthOutput) {
    fShard << ',' << record.primaryTime / ns << ',' << record.vertexX / mm
           << ',' << record.vertexY / mm << ',' << record.vertexZ / mm << ','
           << record.positronsCreated << ',' << record.gammas511Created << ','
           << record.gammas1274Created;
  }
  fShard << '\n';
  ++fEventsWritten;
}
