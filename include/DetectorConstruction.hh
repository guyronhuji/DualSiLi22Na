#ifndef DUALSILI22NA_DETECTOR_CONSTRUCTION_HH
#define DUALSILI22NA_DETECTOR_CONSTRUCTION_HH

#include "DetectorParameters.hh"
#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "G4VUserDetectorConstruction.hh"

#include <memory>

class G4GenericMessenger;
class G4LogicalVolume;
class G4Material;

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  explicit DetectorConstruction(std::shared_ptr<DetectorParameters> parameters);
  ~DetectorConstruction() override;

  G4VPhysicalVolume* Construct() override;
  void ConstructSDandField() override;

private:
  G4Material* BuildSourceMaterial();
  G4Material* GetKapton();
  G4Material* GetMylar();

  void BuildSourcePackage(G4LogicalVolume* world);
  void BuildSiLiDetector(G4LogicalVolume* world, G4int side);
  void BuildHPGeDetector(G4LogicalVolume* world, G4int index,
                         const G4ThreeVector& outwardDirection);
  void ApplyVisualAttributes();
  void PrintGeometryDiagnostics(const G4String& name,
                                const G4ThreeVector& frontFace,
                                const G4ThreeVector& crystalCenter,
                                const G4ThreeVector& pointing) const;

  std::shared_ptr<DetectorParameters> fParameters;
  G4GenericMessenger* fGeometryMessenger = nullptr;
  G4GenericMessenger* fSourceMessenger = nullptr;
  G4GenericMessenger* fOutputMessenger = nullptr;

  G4LogicalVolume* fWorldLV = nullptr;
  G4LogicalVolume* fSiLi1ActiveLV = nullptr;
  G4LogicalVolume* fSiLi2ActiveLV = nullptr;
  G4LogicalVolume* fHPGe1ActiveLV = nullptr;
  G4LogicalVolume* fHPGe2ActiveLV = nullptr;
  G4LogicalVolume* fHPGe3ActiveLV = nullptr;
};

#endif
