#ifndef DUALSILI22NA_PRIMARY_GENERATOR_ACTION_HH
#define DUALSILI22NA_PRIMARY_GENERATOR_ACTION_HH

#include "DetectorParameters.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

#include <memory>

class G4ParticleGun;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  explicit PrimaryGeneratorAction(std::shared_ptr<DetectorParameters> parameters);
  ~PrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event* event) override;

private:
  void ConfigureIon();

  std::shared_ptr<DetectorParameters> fParameters;
  G4ParticleGun* fParticleGun = nullptr;
  G4bool fIonConfigured = false;
};

#endif
