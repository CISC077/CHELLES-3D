// CHELLES 3D - Module du mode photo (26/11/2026)
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "ModePhotoChelles.h"

class FChellesPhotoModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FModePhotoChelles::Get();
		Commande = MakeUnique<FAutoConsoleCommand>(
			TEXT("Chelles.Photo"),
			TEXT("CHELLES 3D : entre ou sort du mode photo (meme effet que la touche F12)."),
			FConsoleCommandDelegate::CreateLambda([]() { FModePhotoChelles::Get().Basculer(); }));
	}

	virtual void ShutdownModule() override
	{
		Commande.Reset();
		FModePhotoChelles::Detruire();
	}

private:
	TUniquePtr<FAutoConsoleCommand> Commande;
};

IMPLEMENT_MODULE(FChellesPhotoModule, ChellesPhoto)
