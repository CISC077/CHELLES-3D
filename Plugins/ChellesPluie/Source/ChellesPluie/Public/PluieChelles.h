// CHELLES 3D - Pluie du quartier (UE 4.27)
// - Averses : gouttes qui tombent autour de la camera + bruit de pluie (plus doux sous un toit).
// - Flaques qui brillent (reflets) posees sur les routes en MI_AsphaltRoad, sol mouille et plus sombre.
// - Ciel un peu plus gris (brouillard + couleurs ternes) pendant l'averse.
// - Pendant une averse, la neige du plugin ChellesMeteo est mise en pause, puis revient.
// Rien n'est modifie dans la map ni dans MI_AsphaltRoad : tout est cree au lancement (Play) et disparait a la fin.
// Les materiaux / le son sont crees une fois par Tools\Pluie\INSTALLER_PLUIE.bat (dossier /Game/Chelles/Pluie).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PluieChelles.generated.h"

class UAudioComponent;
class UDecalComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UProceduralMeshComponent;

/** Reglages lus dans Plugins/ChellesPluie/Pluie.txt */
struct FReglagesPluie
{
	bool bActif = true;
	FString Mode = TEXT("Auto");      // Auto / Oui / Non
	int32 ChancePluie = 30;           // % de chance d'averse par tranche
	int32 TrancheMinutes = 20;        // duree d'une tranche (une averse dure 1 ou plusieurs tranches)
	float FonduSecondes = 25.f;       // debut / fin d'averse en douceur

	float VitesseGouttes = 9.f;       // m/s
	float Inclinaison = 6.f;          // degres (vent)
	int32 NombreGouttes = 3500;       // gouttes par motif (2 motifs en COMPLETE, 1 en LEGERE)
	float TailleZone = 30.f;          // m, carre de pluie autour de la camera
	float HauteurZone = 20.f;         // m
	float LongueurGoutte = 0.55f;     // m
	float LargeurGoutte = 1.2f;       // cm
	FLinearColor CouleurGouttes = FLinearColor(0.75f, 0.8f, 0.9f);
	float LuminositeJour = 0.6f, LuminositeNuit = 0.12f, OpaciteGouttes = 0.35f;

	float VolumeSon = 0.8f;
	float VolumeAbri = 0.4f;          // sous un toit / un pont

	int32 Flaques = 160;              // nombre de flaques (mode COMPLETE)
	float TailleFlaqueMin = 0.8f, TailleFlaqueMax = 2.6f;  // m
	float RayonFlaques = 600.f;       // m autour du point de depart
	TArray<FString> MotsRoute = { TEXT("AsphaltRoad") };
	float RemplissageMinutes = 2.f;   // temps pour que les flaques se remplissent
	float SechageMinutes = 6.f;       // temps pour qu'elles disparaissent apres la pluie

	bool bSolMouille = true;
	float RayonSolMouille = 70.f;     // m autour de la camera
	float Brouillard = 1.8f;          // x densite du brouillard pendant l'averse
	float Couleurs = 0.82f;           // saturation pendant l'averse (1 = normal)
	bool bCouperNeige = true;
};

UCLASS(NotPlaceable, Transient)
class CHELLESPLUIE_API APluieChelles : public AActor
{
	GENERATED_BODY()

public:
	APluieChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Pluie [Info|Oui|Non|Auto|Relire] */
	void Commande(const TArray<FString>& Arguments);

	/** Vrai pendant une averse (lu par d'autres plugins si besoin). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pluie")
	bool bPluieEnCours = false;

	/** 0 = sec, 1 = averse complete (fondu). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pluie")
	float Force = 0.f;

	/** 0 = sol sec, 1 = flaques pleines. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pluie")
	float Humidite = 0.f;

private:
	void LireReglages();
	/** 0 = pas de pluie, 1 = LEGERE, 2 = COMPLETE (OPTIONS > AVANCES > PLUIE). */
	int32 NiveauOptions() const;
	bool AverseVoulue() const;
	void Preparer();
	void FabriquerGouttes(UProceduralMeshComponent* C, int32 Graine) const;
	void PoserFlaques();
	void MettreAJourNiveau();
	void MettreAJourGouttes(float DeltaSeconds);
	void MettreAJourAbri();
	void MettreAJourAmbiance();
	void MettreAJourNeige();
	float Jour() const;
	void Message(const FString& Texte, bool bEcran = false) const;

	FReglagesPluie Reglages;

	UPROPERTY(Transient)
	TArray<UProceduralMeshComponent*> Gouttes;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MatGouttes = nullptr;

	UPROPERTY(Transient)
	UAudioComponent* Son = nullptr;

	UPROPERTY(Transient)
	TArray<UDecalComponent*> DecalsFlaques;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> MatFlaques;

	UPROPERTY(Transient)
	UDecalComponent* SolMouille = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MatSolMouille = nullptr;

	UPROPERTY(Transient)
	UPostProcessComponent* Ambiance = nullptr;

	TWeakObjectPtr<UExponentialHeightFogComponent> Brouillard;
	float BrouillardOrigine = -1.f;

	int32 Niveau = 2;          // niveau des options en cours
	int32 ForceMode = -1;      // console : -1 auto, 0 non, 1 oui
	bool bPret = false;
	bool bFlaquesPosees = false;
	bool bAbri = false;
	float AbriLisse = 0.f;
	double Temps = 0.0;
	float ProchainEtat = 0.f;
	float ProchainAbri = 0.f;
	float DernierEnvoi = -1.f;
	bool bNeigeCoupee = false;
};
