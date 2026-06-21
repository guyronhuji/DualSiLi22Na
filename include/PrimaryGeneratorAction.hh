#ifndef DUALSILI22NA_PRIMARY_GENERATOR_ACTION_HH
#define DUALSILI22NA_PRIMARY_GENERATOR_ACTION_HH

#include "DetectorParameters.hh"
#include "G4ThreeVector.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

#include <memory>

class G4ParticleGun;
class G4ParticleDefinition;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  explicit PrimaryGeneratorAction(std::shared_ptr<DetectorParameters> parameters);
  ~PrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event* event) override;

private:
  void ConfigureIon();
  void ConfigureExplicitPrimaries();
  void GenerateIonPrimaries(G4Event* event);
  void GenerateExplicit22NaPrimaries(G4Event* event);
  G4double SampleBetaKineticEnergy() const;
  G4ThreeVector SampleIsotropicDirection() const;
  G4ThreeVector SampleConeDirection(const G4ThreeVector& axis,
                                    G4double halfAngle) const;
  G4ThreeVector SampleHpgeBackToBackDirection(G4double halfAngle) const;
  G4double HpgeTripleImportanceWeight(G4double halfAngle) const;

  std::shared_ptr<DetectorParameters> fParameters;
  G4ParticleGun* fParticleGun = nullptr;
  G4ParticleDefinition* fPositron = nullptr;
  G4ParticleDefinition* fGamma = nullptr;
  G4bool fIonConfigured = false;
  G4bool fExplicitPrimariesConfigured = false;
};

#endif
