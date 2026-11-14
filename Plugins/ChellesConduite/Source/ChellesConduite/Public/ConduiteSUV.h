// CHELLES 3D - Conduite du SUV : la voiture de la roue TAB tient la route comme la motoneige.
// Tout se fait pendant le jeu : aucun fichier du pack DriveableCar_basic n'est modifie.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConduiteSUV.generated.h"

class APawn;

/** Valeurs d'origine d'une classe de roue (remises a la fin du jeu). */
USTRUCT()
struct FRoueOrigineChelles
{
	GENERATED_BODY()

	UPROPERTY()
	UClass* Classe = nullptr;

	UPROPERTY()
	UObject* Pneu = nullptr;

	float LatStiffValue = 0.f;
	float LongStiffValue = 0.f;
	float SteerAngle = 0.f;
	float SuspensionDampingRatio = 0.f;
};

UCLASS(NotPlaceable, Transient)
class CHELLESCONDUITE_API AConduiteSUV : public AActor
{
	GENERATED_BODY()

public:
	AConduiteSUV();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.SUV [Info|0|1|Relire] */
	void Commande(const TArray<FString>& Arguments);

private:
	void LireReglages();
	bool EstConcerne(const APawn* Pion) const;
	void ChercherVehicules();
	void Preparer(APawn* Pion);
	void RegleRoue(UClass* ClasseRoue, bool bAvant);
	void RemettreRoues();
	void Aider(APawn* Pion, float Dt);

	// ---- Reglages (ConduiteSUV.txt) ----
	bool bActif = true;
	TArray<FString> Vehicules;
	float Adherence = 10.f;        // vitesse a laquelle la glisse sur le cote est supprimee (par seconde)
	float VirageMaxDeg = 70.f;     // vitesse de rotation maxi (degres par seconde)
	float RayonMini = 6.f;         // rayon de virage mini (m)
	float AccelLaterale = 9.f;     // force maxi dans les virages (m/s2, 9.8 = 1 g)
	float AntiRoulis = 6.f;        // empeche la voiture de se pencher / se retourner
	float Appui = 0.4f;            // la voiture est plaquee au sol en roulant vite (0 = rien)
	float AbaisserCentre = -40.f;  // centre de gravite descendu (cm, negatif = plus bas)
	float Friction = 2.0f;         // adherence des pneus (x)
	float Rigidite = 1.5f;         // rigidite laterale des pneus (x)
	float Braquage = 40.f;         // angle maxi des roues avant (degres, 0 = garder celui du pack)
	float Amortissement = 0.7f;    // amortisseurs (0 = garder ceux du pack)

	UPROPERTY()
	TArray<UObject*> Pneus;

	UPROPERTY()
	TArray<FRoueOrigineChelles> Origines;

	TSet<TWeakObjectPtr<APawn>> Prets;
	TArray<TWeakObjectPtr<APawn>> Suivis;
	float ProchaineRecherche = 0.f;
};
