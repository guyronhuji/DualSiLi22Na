#ifndef DUALSILI22NA_DETECTOR_PARAMETERS_HH
#define DUALSILI22NA_DETECTOR_PARAMETERS_HH

#include "G4String.hh"
#include "G4SystemOfUnits.hh"

struct DetectorParameters {
  G4double sourceRadius = 2.5 * mm;
  G4double sourceThickness = 10.0 * um;
  G4String sourceMaterial = "NaClLowDensity";
  G4double sourceDensity = 0.10 * g / cm3;

  G4double kaptonRadius = 3.0 * mm;
  G4double kaptonThickness = 1.0 * um;
  G4double geometryClearance = 0.01 * um;

  G4double siliActiveDiameter = 50.0 * mm;
  G4double siliActiveThickness = 5.0 * mm;
  G4double siliFrontDeadLayer = 0.1 * um;
  G4double siliRearDeadLayer = 0.5 * um;
  G4double siliEntranceWindowThickness = 0.5 * um;
  G4double siliVacuumGap = 1.0 * mm;
  G4double siliHousingWallThickness = 1.0 * mm;
  G4double siliCrystalFaceGap = 2.0 * mm;

  G4double hpgeCrystalDiameter = 60.0 * mm;
  G4double hpgeCrystalLength = 60.0 * mm;
  G4double hpgeBoreRadius = 5.0 * mm;
  G4double hpgeBoreDepth = 45.0 * mm;
  G4double hpgeFrontDeadLayer = 0.7 * mm;
  G4double hpgeSideDeadLayer = 0.7 * mm;
  G4double hpgeFrontWindowThickness = 0.5 * mm;
  G4double hpgeVacuumGap = 1.0 * mm;
  G4double hpgeHousingThickness = 1.0 * mm;
  G4double hpgeFrontDistance = 33.0 * mm;

  G4double decayTime = 1.0e20 * second;
  G4bool fast22NaPrimaries = false;
  G4bool importanceSampling = false;
  G4String importanceMode = "hpgeTriple";
  G4double importanceConeHalfAngle = 45.0 * deg;
  G4double betaEndpointEnergy = 545.0 * keV;
  G4bool suppressFastPositronAnnihilation = true;
  G4double positronKillEnergy = 100.0 * eV;

  G4bool positronOnlyMode = false;
  G4String betaSpectrumFile = "";

  G4String outputFileName = "output/dual_sili_22na.parquet";
#ifdef __APPLE__
  G4String parquetPythonCommand = "/usr/bin/arch -arm64 python3";
#else
  G4String parquetPythonCommand = "python3";
#endif
  G4bool truthOutput = false;
};

#endif
