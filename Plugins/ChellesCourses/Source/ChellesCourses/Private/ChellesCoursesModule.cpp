// CHELLES 3D - Module du plugin Courses : ajoute le gestionnaire des courses au lancement du jeu (sur chaque
// machine qui affiche le jeu) et les commandes de la console.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#include "CoursesChelles.h"

namespace
{
	ACoursesChelles* TrouverCourses(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<ACoursesChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	ACoursesChelles* TrouverOuPrevenir(UWorld* Monde)
	{
		ACoursesChelles* C = TrouverCourses(Monde);
		if (!C)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Courses] Pas de courses dans cette partie (plugin ChellesCourses, Actif=Oui dans Courses.txt ?)."));
		}
		return C;
	}

	/** Chelles.Course 2 : montre le depart de la course 2. Sans numero : ecran COURSES. */
	void ConsoleCourse(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (ACoursesChelles* C = TrouverOuPrevenir(Monde))
		{
			if (Arguments.Num() == 0)
			{
				C->OuvrirMenu();
			}
			else
			{
				C->MontrerDepart(FCString::Atoi(*Arguments[0]) - 1);
			}
		}
	}

	void ConsoleCourseFin(UWorld* Monde)
	{
		if (ACoursesChelles* C = TrouverOuPrevenir(Monde))
		{
			C->Abandonner(TEXT("Course arrêtée (console)."));
		}
	}

	void ConsoleCoursesRefaire(UWorld* Monde)
	{
		if (ACoursesChelles* C = TrouverOuPrevenir(Monde))
		{
			C->Refaire();
		}
	}

	void ConsoleCourseCreer(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (ACoursesChelles* C = TrouverOuPrevenir(Monde))
		{
			C->CommencerEnregistrement(FString::Join(Arguments, TEXT(" ")));
		}
	}

	void ConsoleCourseCreerFin(UWorld* Monde)
	{
		if (ACoursesChelles* C = TrouverOuPrevenir(Monde))
		{
			C->FinirEnregistrement();
		}
	}

	void ConsoleCourses(UWorld* Monde)
	{
		if (ACoursesChelles* C = TrouverOuPrevenir(Monde))
		{
			C->DecrireEtat();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCmdCourse(TEXT("Chelles.Course"),
		TEXT("Montre le depart d'une course : Chelles.Course 1 / 2 / 3... Sans numero : ecran COURSES."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleCourse));

	FAutoConsoleCommandWithWorld GCmdCourseFin(TEXT("Chelles.CourseFin"),
		TEXT("Abandonne la course en cours."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleCourseFin));

	FAutoConsoleCommandWithWorld GCmdCoursesRefaire(TEXT("Chelles.CoursesRefaire"),
		TEXT("Refait les courses automatiques a partir des rues de la circulation (les courses faites main restent)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleCoursesRefaire));

	FAutoConsoleCommandWithWorldAndArgs GCmdCourseCreer(TEXT("Chelles.CourseCreer"),
		TEXT("Enregistre votre propre course : Chelles.CourseCreer NOM, faites le parcours, puis Chelles.CourseCreerFin."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleCourseCreer));

	FAutoConsoleCommandWithWorld GCmdCourseCreerFin(TEXT("Chelles.CourseCreerFin"),
		TEXT("Termine l'enregistrement de votre course (l'arrivee = votre position)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleCourseCreerFin));

	FAutoConsoleCommandWithWorld GCmdCourses(TEXT("Chelles.Courses"),
		TEXT("Affiche les courses et vos records dans le journal."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleCourses));
}

class FChellesCoursesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesCoursesModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		if (!Monde || !Monde->IsGameWorld())
		{
			return;
		}
		// Chaque machine qui affiche le jeu a son propre gestionnaire (pas le serveur dedie, qui n'a pas d'ecran)
		if (Monde->GetNetMode() == NM_DedicatedServer || TrouverCourses(Monde))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Monde->SpawnActor<ACoursesChelles>(ACoursesChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesCoursesModule, ChellesCourses)
