#include "PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4IonTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr G4double kTwoPi = 6.2831853071795864769;
constexpr G4double kFourPi = 12.5663706143591729539;
constexpr G4double kElectronMassKeV = 510.99895;

G4double ConeSolidAngle(G4double halfAngle) {
  return kTwoPi * (1.0 - std::cos(halfAngle));
}

G4ThreeVector OrthogonalUnit(const G4ThreeVector& axis) {
  const auto unit = axis.unit();
  if (std::abs(unit.z()) < 0.9) {
    return unit.cross(G4ThreeVector(0.0, 0.0, 1.0)).unit();
  }
  return unit.cross(G4ThreeVector(0.0, 1.0, 0.0)).unit();
}
}

PrimaryGeneratorAction::PrimaryGeneratorAction(
    std::shared_ptr<DetectorParameters> parameters)
    : fParameters(std::move(parameters)), fParticleGun(new G4ParticleGun(1)) {
  fParticleGun->SetParticleEnergy(0.0);
  fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0.0, 0.0, 1.0));

  G4cout << "Source: default 22Na decay time = "
         << fParameters->decayTime / second << " s" << G4endl;
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() {
  delete fParticleGun;
}

void PrimaryGeneratorAction::ConfigureIon() {
  auto ion = G4IonTable::GetIonTable()->GetIon(11, 22, 0.0);
  if (!ion) {
    G4Exception("PrimaryGeneratorAction::ConfigureIon", "Missing22Na",
                FatalException, "Could not obtain the 22Na ion definition.");
  }

  fParticleGun->SetParticleDefinition(ion);
  fIonConfigured = true;

  G4cout << "Source: configured ion at rest: 22Na" << G4endl
         << "Source: configured decay time = "
         << fParameters->decayTime / second << " s" << G4endl;
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event) {
  if (fParameters->fast22NaPrimaries || fParameters->importanceSampling) {
    GenerateExplicit22NaPrimaries(event);
    return;
  }

  GenerateIonPrimaries(event);
}

void PrimaryGeneratorAction::GenerateIonPrimaries(G4Event* event) {
  if (!fIonConfigured) {
    ConfigureIon();
  }

  const auto radius = fParameters->sourceRadius * std::sqrt(G4UniformRand());
  const auto phi = kTwoPi * G4UniformRand();
  const auto z = (G4UniformRand() - 0.5) * fParameters->sourceThickness;
  const G4ThreeVector position(radius * std::cos(phi), radius * std::sin(phi), z);

  fParticleGun->SetParticlePosition(position);
  fParticleGun->SetParticleEnergy(0.0);

  // The source time is intentionally set far beyond the isotope lifetime.
  // This prevents long-lived 22Na from producing effectively no decays in
  // finite Geant4 event runs when radioactive decay timing is sampled.
  fParticleGun->SetParticleTime(fParameters->decayTime);

  fParticleGun->GeneratePrimaryVertex(event);
}

void PrimaryGeneratorAction::ConfigureExplicitPrimaries() {
  auto table = G4ParticleTable::GetParticleTable();
  fPositron = table->FindParticle("e+");
  fGamma = table->FindParticle("gamma");
  if (!fPositron || !fGamma) {
    G4Exception("PrimaryGeneratorAction::ConfigureExplicitPrimaries",
                "MissingParticle", FatalException,
                "Could not obtain e+ or gamma particle definitions.");
  }

  fExplicitPrimariesConfigured = true;
  G4cout << "Source: configured explicit fast 22Na primaries";
  if (fParameters->importanceSampling) {
    G4cout << " with importance mode " << fParameters->importanceMode
           << ", cone half-angle = "
           << fParameters->importanceConeHalfAngle / deg << " deg";
  }
  G4cout << G4endl;
}

void PrimaryGeneratorAction::GenerateExplicit22NaPrimaries(G4Event* event) {
  if (!fExplicitPrimariesConfigured) {
    ConfigureExplicitPrimaries();
  }

  if (fParameters->importanceSampling &&
      fParameters->importanceMode != "hpgeTriple") {
    G4Exception("PrimaryGeneratorAction::GenerateExplicit22NaPrimaries",
                "UnsupportedImportanceMode", FatalException,
                ("Unsupported importance mode: " +
                 std::string(fParameters->importanceMode.data()))
                    .c_str());
  }

  const auto radius = fParameters->sourceRadius * std::sqrt(G4UniformRand());
  const auto phi = kTwoPi * G4UniformRand();
  const auto z = (G4UniformRand() - 0.5) * fParameters->sourceThickness;
  const G4ThreeVector position(radius * std::cos(phi), radius * std::sin(phi), z);

  const auto halfAngle = fParameters->importanceConeHalfAngle;
  const auto eventWeight = fParameters->importanceSampling
                               ? HpgeTripleImportanceWeight(halfAngle)
                               : 1.0;

  auto shoot = [&](G4ParticleDefinition* particle, G4double energy,
                   const G4ThreeVector& direction) {
    fParticleGun->SetParticleDefinition(particle);
    fParticleGun->SetParticlePosition(position);
    fParticleGun->SetParticleTime(0.0);
    fParticleGun->SetParticleEnergy(energy);
    fParticleGun->SetParticleMomentumDirection(direction.unit());
    fParticleGun->SetParticleWeight(eventWeight);
    fParticleGun->GeneratePrimaryVertex(event);
  };

  shoot(fPositron, SampleBetaKineticEnergy(), SampleIsotropicDirection());

  G4ThreeVector annihilationDirection;
  G4ThreeVector gamma1274Direction;
  if (fParameters->importanceSampling) {
    annihilationDirection = SampleHpgeBackToBackDirection(halfAngle);
    gamma1274Direction = SampleConeDirection(G4ThreeVector(0.0, 1.0, 0.0),
                                             halfAngle);
  } else {
    annihilationDirection = SampleIsotropicDirection();
    gamma1274Direction = SampleIsotropicDirection();
  }

  shoot(fGamma, 511.0 * keV, annihilationDirection);
  shoot(fGamma, 511.0 * keV, -annihilationDirection);
  shoot(fGamma, 1274.5 * keV, gamma1274Direction);
}

G4double PrimaryGeneratorAction::SampleBetaKineticEnergy() const {
  const auto endpointKeV = fParameters->betaEndpointEnergy / keV;
  const auto maxShape = std::sqrt(endpointKeV * (endpointKeV + 2.0 * kElectronMassKeV)) *
                        (endpointKeV + kElectronMassKeV) *
                        endpointKeV * endpointKeV;

  while (true) {
    const auto kineticKeV = endpointKeV * G4UniformRand();
    const auto momentumKeV =
        std::sqrt(kineticKeV * (kineticKeV + 2.0 * kElectronMassKeV));
    const auto shape = momentumKeV * (kineticKeV + kElectronMassKeV) *
                       std::pow(endpointKeV - kineticKeV, 2.0);
    if (G4UniformRand() * maxShape <= shape) {
      return kineticKeV * keV;
    }
  }
}

G4ThreeVector PrimaryGeneratorAction::SampleIsotropicDirection() const {
  const auto cosTheta = 2.0 * G4UniformRand() - 1.0;
  const auto sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
  const auto phi = kTwoPi * G4UniformRand();
  return G4ThreeVector(sinTheta * std::cos(phi), sinTheta * std::sin(phi),
                       cosTheta);
}

G4ThreeVector PrimaryGeneratorAction::SampleConeDirection(
    const G4ThreeVector& axis, G4double halfAngle) const {
  const auto unit = axis.unit();
  const auto u = OrthogonalUnit(unit);
  const auto v = unit.cross(u).unit();
  const auto cosTheta = 1.0 - G4UniformRand() * (1.0 - std::cos(halfAngle));
  const auto sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
  const auto phi = kTwoPi * G4UniformRand();
  return (cosTheta * unit + sinTheta * std::cos(phi) * u +
          sinTheta * std::sin(phi) * v)
      .unit();
}

G4ThreeVector PrimaryGeneratorAction::SampleHpgeBackToBackDirection(
    G4double halfAngle) const {
  const auto axis = G4UniformRand() < 0.5 ? G4ThreeVector(1.0, 0.0, 0.0)
                                         : G4ThreeVector(-1.0, 0.0, 0.0);
  return SampleConeDirection(axis, halfAngle);
}

G4double PrimaryGeneratorAction::HpgeTripleImportanceWeight(
    G4double halfAngle) const {
  const auto coneOmega = ConeSolidAngle(halfAngle);
  const auto annihilationUnionOmega = 2.0 * coneOmega;
  const auto gamma1274Omega = coneOmega;
  return (annihilationUnionOmega / kFourPi) * (gamma1274Omega / kFourPi);
}
