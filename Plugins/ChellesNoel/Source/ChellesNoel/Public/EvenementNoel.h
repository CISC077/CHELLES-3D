// CHELLES 3D - Evenement de Noel saisonnier (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvenementNoel.generated.h"

class ACharacter;
class UStaticMesh;
class UMaterialParameterCollection;

/**
 * Ajoute tout seul au lancement du jeu (plugin ChellesNoel), rien a poser.
 *
 * Du 1er decembre au 1er janvier (date du PC, reglable dans Plugins/ChellesNoel/Noel.txt) :
 *  - la neige tombe : les acteurs portant le tag "ChellesNoelSaison" (flocons et empreintes poses par
 *    Tools/Noel/INSTALLER_NOEL.bat) sont montres ; le reste de l'annee ils sont caches et arretes ;
 *  - le sol devient blanc : parametre "SaisonNeige" de la collection /Game/Chelles/Neige/MPC_Empreintes (0 ou 1),
 *    lu par le materiau du Landscape cree par l'installation ;
 *  - chaque passant (classe dont le nom contient "Passant") recoit un bonnet de Noel du pack Christmas_pack.
 *  - le ciel orange le soir vient du cycle jour/nuit GoodSky deja installe (on n'y touche pas).
 */
UCLASS(meta = (DisplayName = "Evenement de Noel (Chelles)"))
class CHELLESNOEL_API AEvenementNoel : public AActor
{
	GENERATED_BODY()

public:
	AEvenementNoel();

	/** Vrai si on est dans la periode de Noel (calcule au lancement). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noel")
	bool bNoelActif = false;

	/** Tag des acteurs a montrer seulement pendant Noel (flocons, empreintes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noel")
	FName TagSaison = TEXT("ChellesNoelSaison");

	/** Collection lue par le materiau du sol de Noel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noel")
	FSoftObjectPath CollectionNoel = FSoftObjectPath(TEXT("/Game/Chelles/Neige/MPC_Empreintes.MPC_Empreintes"));

	/** Parametre de la collection : 0 = sol normal, 1 = sol enneige. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noel")
	FName ParametreSaison = TEXT("SaisonNeige");

	/** Mot present dans le nom de classe des passants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noel")
	FString MotCleClasse = TEXT("Passant");

	/** Les personnages dont le modele contient ce mot sont deja habilles pour Noel (pas de bonnet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noel")
	FString MotCleDejaHabille = TEXT("Christmas");

	/** Vrai si Date est entre Debut et Fin (jour/mois, Fin comprise, passage du nouvel an gere). */
	static bool EstDansLaPeriode(const FDateTime& Date, int32 JDebut, int32 MDebut, int32 JFin, int32 MFin);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	void LireReglages();
	void AppliquerSaison();
	void HabillerLesPassants();
	bool PoserBonnet(ACharacter* Passant);

	// reglages lus dans Noel.txt
	FString Mode = TEXT("Auto");
	int32 JourDebut = 1;
	int32 MoisDebut = 12;
	int32 JourFin = 1;
	int32 MoisFin = 1;
	TArray<int32> NumerosBonnets = {1, 2, 3, 4};
	float TailleBonnet = 24.f;
	float Enfoncement = 4.f;
	float TournerBonnet = 0.f;
	float PencherBonnet = 0.f;

	UPROPERTY()
	TArray<UStaticMesh*> Bonnets;

	FTimerHandle Minuterie;
	FTimerHandle MinuterieSaison;
	int32 NbBonnets = 0;
};
