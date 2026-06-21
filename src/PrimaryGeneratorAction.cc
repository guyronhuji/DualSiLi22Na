#include "PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4IonTable.hh"
#include "G4ParticleGun.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <cmath>

namespace {
constexpr G4double kTwoPi = 6.2831853071795864769;
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
