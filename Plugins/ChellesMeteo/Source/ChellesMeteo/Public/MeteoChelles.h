// CHELLES 3D - Meteo du quartier (UE 4.27)
// - Ciel GoodSky cale sur l'heure reelle du PC : soleil de Chelles, bleu nuit apres le coucher, ciel orange vers 20h en hiver.
// - Neige en hiver (effet PS_Snow du pack WinterTown) autour de chaque joueur.
// - Sapins de Noel du Christmas_pack avec guirlandes lumineuses et petites lumieres colorees qui clignotent la nuit.
// Rien n'est modifie dans la map : tout est cree au lancement de la partie (Play) et disparait a la fin.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeteoChelles.generated.h"

class UCurveLinearColor;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UParticleSystemComponent;
class UPointLightComponent;
class USkyLightComponent;
class UStaticMeshComponent;

/** Reglages lus dans Plugins/ChellesMeteo/Meteo.txt */
struct FReglagesMeteo
{
	bool bActif = true;
	FString Mode = TEXT("Reel");      // Reel / Accelere / Fixe
	float DureeJourneeMinutes = 24.f;
	float HeureFixe = 19.f;
	float LeverFixe = -1.f;           // -1 = calcule
	float CoucherFixe = -1.f;

	float BleuDebut = 30.f, BleuPlein = 90.f, OrangeDebut = 150.f, OrangePlein = 180.f, OrangeFinMatin = 120.f;
	FLinearColor BleuNuit = FLinearColor(0.03f, 0.12f, 0.75f);
	FLinearColor OrangeNuit = FLinearColor(1.f, 0.42f, 0.12f);
	float LuminositeBleu = 2.2f, LuminositeOrange = 1.3f;
	float LuminositeMinBleu = 0.06f, LuminositeMinOrange = 0.05f;
	FLinearColor BrouillardBleu = FLinearColor(0.02f, 0.08f, 0.45f);
	FLinearColor BrouillardOrange = FLinearColor(0.55f, 0.22f, 0.06f);
	float AmbianceNuit = 0.55f;
	float TailleCiel = -1.f;          // -1 = auto

	FString Neige = TEXT("Auto");
	int32 NeigeDebut = 1115, NeigeFin = 228;   // MMJJ
	FString NeigeEffet = TEXT("/Game/WinterTown/Particles/PS_Snow");
	int32 NeigeGrille = 1;
	float NeigeEcart = 15.f, NeigeHauteur = 12.f, NeigeTaille = 1.f;

	FString Sapins = TEXT("Auto");
	int32 SapinsDebut = 1115, SapinsFin = 106;
	int32 SapinsAuto = 12;
	float SapinsEcart = 30.f, SapinsRayon = 250.f, TailleSapin = 1.3f;
	int32 LumieresParSapin = 4;
	float PuissanceLumieres = 2500.f, DistanceLumieres = 80.f, Clignotement = 1.f;
};

/** Un sapin pose par la meteo, avec ses petites lumieres. */
USTRUCT()
struct FSapinMeteo
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	AActor* Sapin = nullptr;

	UPROPERTY(Transient)
	TArray<AActor*> Guirlandes;

	UPROPERTY(Transient)
	TArray<UPointLightComponent*> Lumieres;

	float Phase = 0.f;
	bool bManuel = false;
};

UCLASS(NotPlaceable, Transient)
class CHELLESMETEO_API AMeteoChelles : public AActor
{
	GENERATED_BODY()

public:
	AMeteoChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Meteo [Info|Heure 19.5|Auto|Hiver 0/1|Neige 0/1|Sapins 0/1|PoserSapin|RetirerSapin|Relire] */
	void Commande(const TArray<FString>& Arguments);

	/** Vrai quand il neige (lu par d'autres plugins si besoin). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Meteo")
	bool bNeigeEnCours = false;

	/** Heure affichee par le ciel (heure reelle ou forcee), 0-24. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Meteo")
	float HeureActuelle = 12.f;

private:
	void LireReglages();
	void Demarrer();
	float CalculerHeure() const;
	void CalculerSoleil();
	float HeureCiel(float HeureReelle) const;
	float HeureReelleDepuisCiel(float HeureCielGoodSky) const;
	void Poids(float HeureReelle, float& Bleu, float& Orange) const;
	bool PeriodeActive(const FString& Reglage, int32 Debut, int32 Fin) const;

	// Ciel
	void PreparerCiel();
	AActor* TrouverOuCreerGoodSky(class ADirectionalLight* Lumiere);
	void FabriquerCourbes();
	void AppliquerHeure(float DeltaSeconds);

	// Neige
	void MettreAJourNeige();

	// Sapins
	void MettreAJourSapins();
	void PoserTousLesSapins();
	void RetirerTousLesSapins();
	bool PoserUnSapin(const FVector& Sol, float Lacet, bool bManuel);
	void AnimerLumieres(float DeltaSeconds);
	void PoserSapinDevantJoueur();
	void RetirerSapinProche();
	TArray<FVector4> LireSapinsManuels() const;

	FReglagesMeteo Reglages;

	// Ciel GoodSky et lumieres du niveau
	UPROPERTY(Transient)
	AActor* Ciel = nullptr;
	bool bCielCree = false;

	TWeakObjectPtr<UDirectionalLightComponent> Soleil;
	TWeakObjectPtr<UExponentialHeightFogComponent> Brouillard;
	FLinearColor BrouillardOrigine = FLinearColor::White;
	FLinearColor BrouillardDirectionnelOrigine = FLinearColor::White;
	TWeakObjectPtr<USkyLightComponent> Ambiance;
	float AmbianceOrigine = 1.f;

	UPROPERTY(Transient)
	TArray<UCurveLinearColor*> CourbesOrigine;

	UPROPERTY(Transient)
	TArray<UCurveLinearColor*> CourbesMeteo;

	TArray<FName> NomsCourbes;
	int32 JourCourbes = -1;

	float Lever = 8.3f;    // heures locales
	float Coucher = 17.f;
	float Depart = 0.f;     // pour le mode Accelere
	float HeureForcee = -1.f;
	int32 ForceHiver = -1;  // -1 auto, 0 non, 1 oui
	int32 ForceNeige = -1;
	int32 ForceSapins = -1;

	// Neige
	UPROPERTY(Transient)
	TArray<UParticleSystemComponent*> Flocons;

	// Sapins
	UPROPERTY(Transient)
	TArray<FSapinMeteo> SapinsPoses;
	bool bSapinsVisibles = false;
	float Temps = 0.f;
	float ProchainEtat = 0.f;
	float NuitActuelle = 0.f;
};
