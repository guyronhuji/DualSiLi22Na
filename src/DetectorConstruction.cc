#include "DetectorConstruction.hh"

#include "SensitiveDetector.hh"

#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4GenericMessenger.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4SDManager.hh"
#include "G4SubtractionSolid.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"

#include <cmath>
#include <memory>
#include <string>

namespace {
G4VisAttributes* Vis(const G4Colour& colour, G4bool solid = true) {
  auto vis = new G4VisAttributes(colour);
  vis->SetForceSolid(solid);
  return vis;
}

G4ThreeVector RotatedZ(const G4RotationMatrix* rotation, G4double z) {
  return (*rotation) * G4ThreeVector(0.0, 0.0, z);
}
}

DetectorConstruction::DetectorConstruction(
    std::shared_ptr<DetectorParameters> parameters)
    : fParameters(std::move(parameters)) {
  fGeometryMessenger =
      new G4GenericMessenger(this, "/geometry/", "Geometry controls");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "sourceRadius", "mm", fParameters->sourceRadius, "22Na source radius");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "sourceThickness", "um", fParameters->sourceThickness,
      "22Na source disk thickness");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "kaptonThickness", "um", fParameters->kaptonThickness,
      "Each Kapton sheet thickness");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "siliActiveDiameter", "mm", fParameters->siliActiveDiameter,
      "Si(Li) active crystal diameter");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "siliActiveThickness", "mm", fParameters->siliActiveThickness,
      "Si(Li) active crystal thickness");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "siliCrystalFaceGap", "mm", fParameters->siliCrystalFaceGap,
      "Gap from nearest Kapton outer face to active Si(Li) front face");
  fGeometryMessenger->DeclarePropertyWithUnit(
      "hpgeFrontDistance", "cm", fParameters->hpgeFrontDistance,
      "Distance from source center to each HPGe detector front face");

  fSourceMessenger = new G4GenericMessenger(this, "/source/", "Source controls");
  fSourceMessenger->DeclarePropertyWithUnit(
      "decayTime", "s", fParameters->decayTime,
      "Time assigned to the 22Na ion before radioactive decay sampling");
  fSourceMessenger->DeclareProperty(
      "fast22NaPrimaries", fParameters->fast22NaPrimaries,
      "Generate explicit beta+, 511 keV, and 1274.5 keV primaries instead of a radioactive 22Na ion");
  fSourceMessenger->DeclareProperty(
      "importanceSampling", fParameters->importanceSampling,
      "Disabled: the previous fast weighted surrogate did not preserve the full 22Na decay/transport spectrum");
  fSourceMessenger->DeclareProperty(
      "importanceMode", fParameters->importanceMode,
      "Importance mode. Supported: hpgeTriple");
  fSourceMessenger->DeclarePropertyWithUnit(
      "importanceConeHalfAngle", "deg", fParameters->importanceConeHalfAngle,
      "Half-angle for HPGe gamma direction bias cones");
  fSourceMessenger->DeclarePropertyWithUnit(
      "betaEndpointEnergy", "keV", fParameters->betaEndpointEnergy,
      "Endpoint kinetic energy for the explicit allowed beta+ spectrum");
  fSourceMessenger->DeclareProperty(
      "suppressFastPositronAnnihilation",
      fParameters->suppressFastPositronAnnihilation,
      "Kill low-energy positrons in fast explicit-primary mode to avoid double-counting manual 511 keV gammas");
  fSourceMessenger->DeclarePropertyWithUnit(
      "positronKillEnergy", "keV", fParameters->positronKillEnergy,
      "Kinetic-energy threshold used when suppressing fast-mode positron annihilation");
  fSourceMessenger->DeclareProperty(
      "positronOnly", fParameters->positronOnlyMode,
      "Emit a single positron per event, isotropically, with energy sampled from betaSpectrumFile");
  fSourceMessenger->DeclareProperty(
      "betaSpectrumFile", fParameters->betaSpectrumFile,
      "4-column spectrum file for positronOnly mode: col 2 = energy (keV), col 3 = dN/dE");

  fOutputMessenger = new G4GenericMessenger(this, "/output/", "Output controls");
  fOutputMessenger->DeclareProperty(
      "fileName", fParameters->outputFileName,
      "Final combined output file name. Parquet is the supported default.");
  fOutputMessenger->DeclareProperty(
      "parquetPythonCommand", fParameters->parquetPythonCommand,
      "Python command used by the master thread to combine CSV shards into Parquet.");
  fOutputMessenger->DeclareProperty(
      "truthOutput", fParameters->truthOutput,
      "Write optional truth-level particle counters and primary vertex fields");
  fOutputMessenger->DeclareProperty(
      "hpgeGateEnabled", fParameters->hpgeGateEnabled,
      "Only write event rows passing the configured HPGe energy gate");
  fOutputMessenger->DeclarePropertyWithUnit(
      "hpgeGateMin", "keV", fParameters->hpgeGateMinEnergy,
      "Lower HPGe energy gate bound for gated output");
  fOutputMessenger->DeclarePropertyWithUnit(
      "hpgeGateMax", "keV", fParameters->hpgeGateMaxEnergy,
      "Upper HPGe energy gate bound for gated output");
  fOutputMessenger->DeclareProperty(
      "hpgeGateMode", fParameters->hpgeGateMode,
      "HPGe output gate mode: any, hpge12, all, or hpge123");
}

DetectorConstruction::~DetectorConstruction() {
  delete fGeometryMessenger;
  delete fSourceMessenger;
  delete fOutputMessenger;
}

G4Material* DetectorConstruction::BuildSourceMaterial() {
  auto nist = G4NistManager::Instance();
  if (auto material =
          nist->FindOrBuildMaterial(fParameters->sourceMaterial, false, false)) {
    return material;
  }

  auto sodium = nist->FindOrBuildElement("Na");
  auto chlorine = nist->FindOrBuildElement("Cl");
  auto material = new G4Material("NaClLowDensity", fParameters->sourceDensity, 2);
  material->AddElement(sodium, 1);
  material->AddElement(chlorine, 1);
  return material;
}

G4Material* DetectorConstruction::GetKapton() {
  auto nist = G4NistManager::Instance();
  if (auto kapton = nist->FindOrBuildMaterial("G4_KAPTON", false, false)) {
    return kapton;
  }

  auto h = nist->FindOrBuildElement("H");
  auto c = nist->FindOrBuildElement("C");
  auto n = nist->FindOrBuildElement("N");
  auto o = nist->FindOrBuildElement("O");
  auto kapton = new G4Material("Kapton", 1.42 * g / cm3, 4);
  kapton->AddElement(h, 10);
  kapton->AddElement(c, 22);
  kapton->AddElement(n, 2);
  kapton->AddElement(o, 5);
  return kapton;
}

G4Material* DetectorConstruction::GetMylar() {
  auto nist = G4NistManager::Instance();
  if (auto mylar = nist->FindOrBuildMaterial("G4_MYLAR", false, false)) {
    return mylar;
  }

  auto h = nist->FindOrBuildElement("H");
  auto c = nist->FindOrBuildElement("C");
  auto o = nist->FindOrBuildElement("O");
  auto mylar = new G4Material("Mylar", 1.39 * g / cm3, 3);
  mylar->AddElement(h, 8);
  mylar->AddElement(c, 10);
  mylar->AddElement(o, 4);
  return mylar;
}

G4VPhysicalVolume* DetectorConstruction::Construct() {
  const G4bool checkOverlaps = true;
  auto nist = G4NistManager::Instance();

  auto worldMaterial = nist->FindOrBuildMaterial("G4_Galactic");
  auto worldSolid = new G4Box("World", 1.5 * m, 1.5 * m, 1.5 * m);
  fWorldLV = new G4LogicalVolume(worldSolid, worldMaterial, "World");
  fWorldLV->SetVisAttributes(G4VisAttributes::GetInvisible());

  auto worldPV = new G4PVPlacement(nullptr, G4ThreeVector(), fWorldLV, "World",
                                   nullptr, false, 0, checkOverlaps);

  /*
                           HPGe_3 (+Y)
                                |
                                |
  HPGe_2 (-X)  ----  source / Si(Li) stack  ----  HPGe_1 (+X)

                                |
                             SiLi_1 (+Z)
                             Kapton
                             22Na source
                             Kapton
                             SiLi_2 (-Z)
  */
  BuildSourcePackage(fWorldLV);
  BuildSiLiDetector(fWorldLV, +1);
  BuildSiLiDetector(fWorldLV, -1);
  BuildHPGeDetector(fWorldLV, 1, G4ThreeVector(1.0, 0.0, 0.0));
  BuildHPGeDetector(fWorldLV, 2, G4ThreeVector(-1.0, 0.0, 0.0));
  BuildHPGeDetector(fWorldLV, 3, G4ThreeVector(0.0, 1.0, 0.0));

  G4cout << "Source: radius = " << fParameters->sourceRadius / mm
         << " mm, thickness = " << fParameters->sourceThickness / um
         << " um" << G4endl
         << "Si(Li): active diameter = "
         << fParameters->siliActiveDiameter / mm
         << " mm, active thickness = "
         << fParameters->siliActiveThickness / mm
         << " mm, active face gap = "
         << fParameters->siliCrystalFaceGap / mm << " mm" << G4endl;

  return worldPV;
}

void DetectorConstruction::BuildSourcePackage(G4LogicalVolume* world) {
  const G4bool checkOverlaps = true;
  auto sourceMaterial = BuildSourceMaterial();
  auto kaptonMaterial = GetKapton();

  auto sourceSolid =
      new G4Tubs("Na22SourceDisk", 0.0, fParameters->sourceRadius,
                 0.5 * fParameters->sourceThickness, 0.0, 360.0 * deg);
  auto sourceLV =
      new G4LogicalVolume(sourceSolid, sourceMaterial, "Na22Source");
  sourceLV->SetVisAttributes(Vis(G4Colour(1.0, 0.18, 0.02, 0.95)));
  new G4PVPlacement(nullptr, G4ThreeVector(), sourceLV, "Na22Source", world,
                    false, 0, checkOverlaps);

  auto kaptonSolid =
      new G4Tubs("KaptonSheet", 0.0, fParameters->kaptonRadius,
                 0.5 * fParameters->kaptonThickness, 0.0, 360.0 * deg);
  auto kaptonPlusLV =
      new G4LogicalVolume(kaptonSolid, kaptonMaterial, "KaptonPlus");
  auto kaptonMinusLV =
      new G4LogicalVolume(kaptonSolid, kaptonMaterial, "KaptonMinus");
  auto kaptonVis = Vis(G4Colour(1.0, 0.78, 0.05, 0.78));
  kaptonPlusLV->SetVisAttributes(kaptonVis);
  kaptonMinusLV->SetVisAttributes(kaptonVis);

  const auto kaptonCenter =
      0.5 * fParameters->sourceThickness + fParameters->geometryClearance +
      0.5 * fParameters->kaptonThickness;
  new G4PVPlacement(nullptr, G4ThreeVector(0.0, 0.0, kaptonCenter),
                    kaptonPlusLV, "KaptonPlus", world, false, 0,
                    checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0.0, 0.0, -kaptonCenter),
                    kaptonMinusLV, "KaptonMinus", world, false, 0,
                    checkOverlaps);
}

void DetectorConstruction::BuildSiLiDetector(G4LogicalVolume* world, G4int side) {
  const G4bool checkOverlaps = true;
  auto nist = G4NistManager::Instance();
  auto silicon = nist->FindOrBuildMaterial("G4_Si");
  auto vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  auto aluminum = nist->FindOrBuildMaterial("G4_Al");
  auto mylar = GetMylar();

  const G4String prefix = side > 0 ? "SiLi1" : "SiLi2";
  const auto activeRadius = 0.5 * fParameters->siliActiveDiameter;
  const auto activeHalf = 0.5 * fParameters->siliActiveThickness;
  const auto sourceHalf = 0.5 * fParameters->sourceThickness;
  const auto kaptonOuter =
      sourceHalf + fParameters->geometryClearance + fParameters->kaptonThickness;
  const auto activeFrontU = kaptonOuter + fParameters->siliCrystalFaceGap;
  const auto activeCenterU = activeFrontU + activeHalf;

  auto toGlobalZ = [side](G4double u) { return side * u; };

  auto activeSolid = new G4Tubs(prefix + "ActiveSolid", 0.0, activeRadius,
                                activeHalf, 0.0, 360.0 * deg);
  auto activeLV = new G4LogicalVolume(activeSolid, silicon, prefix + "Active");
  activeLV->SetVisAttributes(Vis(G4Colour(0.05, 0.30, 1.0, 0.88)));
  new G4PVPlacement(nullptr, G4ThreeVector(0.0, 0.0, toGlobalZ(activeCenterU)),
                    activeLV, prefix + "Active", world, false, 0,
                    checkOverlaps);

  auto frontDeadSolid =
      new G4Tubs(prefix + "DeadLayerFrontSolid", 0.0, activeRadius,
                 0.5 * fParameters->siliFrontDeadLayer, 0.0, 360.0 * deg);
  auto frontDeadLV = new G4LogicalVolume(frontDeadSolid, silicon,
                                         prefix + "DeadLayerFront");
  frontDeadLV->SetVisAttributes(Vis(G4Colour(0.35, 0.62, 1.0, 0.35)));
  new G4PVPlacement(nullptr,
                    G4ThreeVector(
                        0.0, 0.0,
                        toGlobalZ(activeFrontU -
                                  0.5 * fParameters->siliFrontDeadLayer)),
                    frontDeadLV, prefix + "DeadLayerFront", world, false, 0,
                    checkOverlaps);

  auto rearDeadSolid =
      new G4Tubs(prefix + "DeadLayerRearSolid", 0.0, activeRadius,
                 0.5 * fParameters->siliRearDeadLayer, 0.0, 360.0 * deg);
  auto rearDeadLV =
      new G4LogicalVolume(rearDeadSolid, silicon, prefix + "DeadLayerRear");
  rearDeadLV->SetVisAttributes(Vis(G4Colour(0.35, 0.62, 1.0, 0.35)));
  new G4PVPlacement(nullptr,
                    G4ThreeVector(
                        0.0, 0.0,
                        toGlobalZ(activeFrontU +
                                  fParameters->siliActiveThickness +
                                  0.5 * fParameters->siliRearDeadLayer)),
                    rearDeadLV, prefix + "DeadLayerRear", world, false, 0,
                    checkOverlaps);

  auto windowSolid =
      new G4Tubs(prefix + "EntranceWindowSolid", 0.0, activeRadius,
                 0.5 * fParameters->siliEntranceWindowThickness, 0.0,
                 360.0 * deg);
  auto windowLV =
      new G4LogicalVolume(windowSolid, mylar, prefix + "EntranceWindow");
  windowLV->SetVisAttributes(Vis(G4Colour(0.75, 0.75, 0.82, 0.35)));
  new G4PVPlacement(nullptr,
                    G4ThreeVector(
                        0.0, 0.0,
                        toGlobalZ(activeFrontU -
                                  fParameters->siliFrontDeadLayer -
                                  0.5 *
                                      fParameters->siliEntranceWindowThickness)),
                    windowLV, prefix + "EntranceWindow", world, false, 0,
                    checkOverlaps);

  const auto housingInner = activeRadius + fParameters->siliVacuumGap;
  const auto housingOuter = housingInner + fParameters->siliHousingWallThickness;
  const auto housingFrontU = activeFrontU - fParameters->siliFrontDeadLayer -
                             fParameters->siliEntranceWindowThickness;
  const auto housingRearU = activeFrontU + fParameters->siliActiveThickness +
                            fParameters->siliRearDeadLayer;
  const auto housingHalf = 0.5 * (housingRearU - housingFrontU);
  const auto housingCenterU = 0.5 * (housingRearU + housingFrontU);

  auto sideHousingSolid =
      new G4Tubs(prefix + "HousingSideSolid", housingInner, housingOuter,
                 housingHalf, 0.0, 360.0 * deg);
  auto sideHousingLV =
      new G4LogicalVolume(sideHousingSolid, aluminum, prefix + "Housing");
  sideHousingLV->SetVisAttributes(Vis(G4Colour(0.55, 0.55, 0.55, 0.35)));
  new G4PVPlacement(nullptr,
                    G4ThreeVector(0.0, 0.0, toGlobalZ(housingCenterU)),
                    sideHousingLV, prefix + "Housing", world, false, 0,
                    checkOverlaps);

  auto rearCapSolid =
      new G4Tubs(prefix + "RearCapSolid", 0.0, housingOuter,
                 0.5 * fParameters->siliHousingWallThickness, 0.0,
                 360.0 * deg);
  auto rearCapLV =
      new G4LogicalVolume(rearCapSolid, aluminum, prefix + "RearCap");
  rearCapLV->SetVisAttributes(Vis(G4Colour(0.55, 0.55, 0.55, 0.35)));
  new G4PVPlacement(nullptr,
                    G4ThreeVector(
                        0.0, 0.0,
                        toGlobalZ(activeFrontU +
                                  fParameters->siliActiveThickness +
                                  fParameters->siliRearDeadLayer +
                                  0.5 *
                                      fParameters->siliHousingWallThickness)),
                    rearCapLV, prefix + "RearCap", world, false, 0,
                    checkOverlaps);

  auto vacuumSolid = new G4Tubs(prefix + "VacuumGapSolid", activeRadius,
                                housingInner, housingHalf, 0.0,
                                360.0 * deg);
  auto vacuumLV =
      new G4LogicalVolume(vacuumSolid, vacuum, prefix + "VacuumGap");
  vacuumLV->SetVisAttributes(G4VisAttributes::GetInvisible());
  new G4PVPlacement(nullptr,
                    G4ThreeVector(0.0, 0.0, toGlobalZ(housingCenterU)),
                    vacuumLV, prefix + "VacuumGap", world, false, 0,
                    checkOverlaps);

  if (side > 0) {
    fSiLi1ActiveLV = activeLV;
  } else {
    fSiLi2ActiveLV = activeLV;
  }

  G4cout << prefix << ": active front z = " << toGlobalZ(activeFrontU) / mm
         << " mm, active center z = " << toGlobalZ(activeCenterU) / mm
         << " mm, front face points toward source along "
         << (side > 0 ? "-Z" : "+Z") << G4endl;
}

void DetectorConstruction::BuildHPGeDetector(
    G4LogicalVolume* world, G4int index, const G4ThreeVector& outwardDirection) {
  const G4bool checkOverlaps = true;
  auto nist = G4NistManager::Instance();
  auto vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  auto aluminum = nist->FindOrBuildMaterial("G4_Al");
  auto germanium = nist->FindOrBuildMaterial("G4_Ge");

  const G4String prefix = "HPGe" + std::to_string(index);
  const auto crystalRadius = 0.5 * fParameters->hpgeCrystalDiameter;
  const auto crystalHalf = 0.5 * fParameters->hpgeCrystalLength;
  const auto activeRadius = crystalRadius;
  const auto detectorRadius = activeRadius + fParameters->hpgeSideDeadLayer;
  const auto housingInner = detectorRadius + fParameters->hpgeVacuumGap;
  const auto housingOuter = housingInner + fParameters->hpgeHousingThickness;
  const auto totalLength = fParameters->hpgeFrontWindowThickness +
                           fParameters->hpgeVacuumGap +
                           fParameters->hpgeFrontDeadLayer +
                           fParameters->hpgeCrystalLength +
                           fParameters->hpgeVacuumGap +
                           fParameters->hpgeHousingThickness;
  const auto envelopeHalf = 0.5 * totalLength;
  const auto activeFrontU = fParameters->hpgeFrontWindowThickness +
                            fParameters->hpgeVacuumGap +
                            fParameters->hpgeFrontDeadLayer;
  const auto activeCenterLocalZ = -envelopeHalf + activeFrontU + crystalHalf;

  auto envSolid =
      new G4Tubs(prefix + "EnvelopeSolid", 0.0, housingOuter, envelopeHalf,
                 0.0, 360.0 * deg);
  auto envLV =
      new G4LogicalVolume(envSolid, vacuum, prefix + "Envelope");
  envLV->SetVisAttributes(G4VisAttributes::GetInvisible());

  auto rotation = new G4RotationMatrix();
  if (outwardDirection.x() > 0.5) {
    rotation->rotateY(90.0 * deg);
  } else if (outwardDirection.x() < -0.5) {
    rotation->rotateY(-90.0 * deg);
  } else if (outwardDirection.y() > 0.5) {
    rotation->rotateX(-90.0 * deg);
  }

  const auto center = outwardDirection * (fParameters->hpgeFrontDistance +
                                          envelopeHalf);
  new G4PVPlacement(rotation, center, envLV, prefix + "Assembly", world, false,
                    0, checkOverlaps);

  auto activeTube =
      new G4Tubs(prefix + "ActiveTube", 0.0, activeRadius, crystalHalf, 0.0,
                 360.0 * deg);
  auto bore = new G4Tubs(prefix + "Bore", 0.0, fParameters->hpgeBoreRadius,
                         0.5 * fParameters->hpgeBoreDepth, 0.0,
                         360.0 * deg);
  const auto boreZ = crystalHalf - 0.5 * fParameters->hpgeBoreDepth;
  auto activeSolid =
      new G4SubtractionSolid(prefix + "ActiveSolid", activeTube, bore, nullptr,
                             G4ThreeVector(0.0, 0.0, boreZ));
  auto activeLV = new G4LogicalVolume(activeSolid, germanium, prefix + "Active");
  activeLV->SetVisAttributes(Vis(G4Colour(0.0, 0.72, 0.25, 0.86)));
  new G4PVPlacement(nullptr, G4ThreeVector(0.0, 0.0, activeCenterLocalZ),
                    activeLV, prefix + "Active", envLV, false, 0,
                    checkOverlaps);

  auto frontDeadSolid =
      new G4Tubs(prefix + "FrontDeadSolid", 0.0, activeRadius,
                 0.5 * fParameters->hpgeFrontDeadLayer, 0.0, 360.0 * deg);
  auto frontDeadLV =
      new G4LogicalVolume(frontDeadSolid, germanium, prefix + "FrontDeadLayer");
  frontDeadLV->SetVisAttributes(Vis(G4Colour(0.2, 0.9, 0.45, 0.25)));
  new G4PVPlacement(
      nullptr,
      G4ThreeVector(0.0, 0.0,
                    -envelopeHalf + fParameters->hpgeFrontWindowThickness +
                        fParameters->hpgeVacuumGap +
                        0.5 * fParameters->hpgeFrontDeadLayer),
      frontDeadLV, prefix + "FrontDeadLayer", envLV, false, 0, checkOverlaps);

  auto sideDeadSolid =
      new G4Tubs(prefix + "SideDeadSolid", activeRadius, detectorRadius,
                 crystalHalf, 0.0, 360.0 * deg);
  auto sideDeadLV =
      new G4LogicalVolume(sideDeadSolid, germanium, prefix + "SideDeadLayer");
  sideDeadLV->SetVisAttributes(Vis(G4Colour(0.2, 0.9, 0.45, 0.25)));
  new G4PVPlacement(nullptr, G4ThreeVector(0.0, 0.0, activeCenterLocalZ),
                    sideDeadLV, prefix + "SideDeadLayer", envLV, false, 0,
                    checkOverlaps);

  auto windowSolid =
      new G4Tubs(prefix + "FrontWindowSolid", 0.0, detectorRadius,
                 0.5 * fParameters->hpgeFrontWindowThickness, 0.0,
                 360.0 * deg);
  auto windowLV =
      new G4LogicalVolume(windowSolid, aluminum, prefix + "FrontWindow");
  windowLV->SetVisAttributes(Vis(G4Colour(0.78, 0.80, 0.85, 0.45)));
  new G4PVPlacement(
      nullptr,
      G4ThreeVector(0.0, 0.0,
                    -envelopeHalf +
                        0.5 * fParameters->hpgeFrontWindowThickness),
      windowLV, prefix + "FrontWindow", envLV, false, 0, checkOverlaps);

  const auto housingSideHalf =
      0.5 * (totalLength - fParameters->hpgeHousingThickness);
  const auto housingSideCenter =
      -envelopeHalf + housingSideHalf;
  auto housingSideSolid =
      new G4Tubs(prefix + "HousingSideSolid", housingInner, housingOuter,
                 housingSideHalf, 0.0, 360.0 * deg);
  auto housingSideLV =
      new G4LogicalVolume(housingSideSolid, aluminum, prefix + "Housing");
  housingSideLV->SetVisAttributes(Vis(G4Colour(0.46, 0.48, 0.50, 0.32)));
  new G4PVPlacement(nullptr, G4ThreeVector(0.0, 0.0, housingSideCenter), housingSideLV,
                    prefix + "Housing", envLV, false, 0, checkOverlaps);

  auto rearCapSolid =
      new G4Tubs(prefix + "RearCapSolid", 0.0, housingOuter,
                 0.5 * fParameters->hpgeHousingThickness, 0.0, 360.0 * deg);
  auto rearCapLV =
      new G4LogicalVolume(rearCapSolid, aluminum, prefix + "RearCap");
  rearCapLV->SetVisAttributes(Vis(G4Colour(0.46, 0.48, 0.50, 0.32)));
  new G4PVPlacement(
      nullptr,
      G4ThreeVector(0.0, 0.0,
                    envelopeHalf - 0.5 * fParameters->hpgeHousingThickness),
      rearCapLV, prefix + "RearCap", envLV, false, 0, checkOverlaps);

  if (index == 1) {
    fHPGe1ActiveLV = activeLV;
  } else if (index == 2) {
    fHPGe2ActiveLV = activeLV;
  } else {
    fHPGe3ActiveLV = activeLV;
  }

  const auto frontFace = outwardDirection * fParameters->hpgeFrontDistance;
  const auto crystalCenter = center + RotatedZ(rotation, activeCenterLocalZ);
  const auto pointing = -outwardDirection;
  PrintGeometryDiagnostics(prefix, frontFace, crystalCenter, pointing);
}

void DetectorConstruction::PrintGeometryDiagnostics(
    const G4String& name, const G4ThreeVector& frontFace,
    const G4ThreeVector& crystalCenter, const G4ThreeVector& pointing) const {
  G4cout << name << ": front-face position = (" << frontFace.x() / mm << ", "
         << frontFace.y() / mm << ", " << frontFace.z() / mm << ") mm"
         << G4endl
         << name << ": crystal center = (" << crystalCenter.x() / mm << ", "
         << crystalCenter.y() / mm << ", " << crystalCenter.z() / mm << ") mm"
         << G4endl
         << name << ": pointing direction = (" << pointing.x() << ", "
         << pointing.y() << ", " << pointing.z() << ")" << G4endl
         << name << ": front-face distance from source = "
         << frontFace.mag() / cm << " cm" << G4endl;
}

void DetectorConstruction::ConstructSDandField() {
  auto sdManager = G4SDManager::GetSDMpointer();

  auto sili1 = new SensitiveDetector("SiLi_1_SD", DetectorId::SiLi1);
  auto sili2 = new SensitiveDetector("SiLi_2_SD", DetectorId::SiLi2);
  auto hpge1 = new SensitiveDetector("HPGe_1_SD", DetectorId::HPGe1);
  auto hpge2 = new SensitiveDetector("HPGe_2_SD", DetectorId::HPGe2);
  auto hpge3 = new SensitiveDetector("HPGe_3_SD", DetectorId::HPGe3);
  sdManager->AddNewDetector(sili1);
  sdManager->AddNewDetector(sili2);
  sdManager->AddNewDetector(hpge1);
  sdManager->AddNewDetector(hpge2);
  sdManager->AddNewDetector(hpge3);

  if (fSiLi1ActiveLV) {
    fSiLi1ActiveLV->SetSensitiveDetector(sili1);
  }
  if (fSiLi2ActiveLV) {
    fSiLi2ActiveLV->SetSensitiveDetector(sili2);
  }
  if (fHPGe1ActiveLV) {
    fHPGe1ActiveLV->SetSensitiveDetector(hpge1);
  }
  if (fHPGe2ActiveLV) {
    fHPGe2ActiveLV->SetSensitiveDetector(hpge2);
  }
  if (fHPGe3ActiveLV) {
    fHPGe3ActiveLV->SetSensitiveDetector(hpge3);
  }
}
