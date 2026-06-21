#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "DetectorParameters.hh"
#include "PhysicsList.hh"

#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4ios.hh"

#ifdef DUALSILI_ENABLE_VIS
#include "G4UIExecutive.hh"
#include "G4VisExecutive.hh"
#endif

#include <memory>

int main(int argc, char** argv) {
  auto parameters = std::make_shared<DetectorParameters>();

  auto runManager = G4RunManagerFactory::CreateRunManager(
      G4RunManagerType::MTOnly, true);
  runManager->SetUserInitialization(new DetectorConstruction(parameters));
  runManager->SetUserInitialization(new PhysicsList());
  runManager->SetUserInitialization(new ActionInitialization(parameters));

#ifdef DUALSILI_ENABLE_VIS
  std::unique_ptr<G4VisExecutive> visManager(new G4VisExecutive());
  visManager->Initialize();
#endif

  auto uiManager = G4UImanager::GetUIpointer();

  if (argc > 1) {
    uiManager->ApplyCommand("/control/execute " + G4String(argv[1]));
  } else {
#ifdef DUALSILI_ENABLE_VIS
#ifdef DUALSILI_ENABLE_QT
    auto ui = new G4UIExecutive(argc, argv, "qt");
    uiManager->ApplyCommand("/control/execute macros/vis_qt.mac");
#else
    auto ui = new G4UIExecutive(argc, argv);
    uiManager->ApplyCommand("/control/execute macros/vis.mac");
#endif
    ui->SessionStart();
    delete ui;
#else
    G4cerr << "No macro supplied. This executable was built without "
           << "DUALSILI_ENABLE_VIS; run with a macro file or rebuild with "
           << "-DDUALSILI_ENABLE_VIS=ON for interactive visualization."
           << G4endl;
#endif
  }

  delete runManager;
  return 0;
}
