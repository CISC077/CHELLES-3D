// CHELLES 3D - Arsenal (2026-11-17) : blaster, bazooka guide, sabre lumineux, grenade a zombies (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArsenalTypes.h"
#include "ArsenalChelles.generated.h"

class AArsenalRelais;
class AArsenalVue;
class ARatelierArsenal;
class UAudioComponent;
class APawn;
class APlayerController;
class SWidget;
class UInputComponent;
class UPrimitiveComponent;

/** Partie de l'arme normale cachee pendant que l'arme de l'arsenal est en main. */
struct FArsenalMasque
{
	TWeakObjectPtr<UPrimitiveComponent> Composant;
	bool bRenduPrincipal = true;
	bool bProfondeur = true;
	bool bOmbre = true;
};

/**
 * Pose au lancement du jeu sur chaque machine (comme AArmesChelles).
 *
 * Touche N : ARME NORMALE -> BLASTER -> BAZOOKA -> SABRE LUMINEUX -> GRENADE A ZOMBIES -> ARME NORMALE.
 * Pendant qu'une arme de l'arsenal est en main, l'arme du kit FPS_Game est cachee et son tir (clic gauche)
 * est bloque ; la molette rend l'arme normale. Rien n'est modifie dans les Blueprints.
 *
 * Le joueur demande au serveur (AArsenalRelais) ; le serveur cree les tirs, vus par tous.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AArsenalChelles : public AActor
{
	GENERATED_BODY()

public:
	AArsenalChelles();

	virtual void Tick(float DeltaSeconds) override;

	/** Arme de l'arsenal en main du joueur de cette machine (Aucune = arme normale). */
	static uint8 ArmeLocale(UWorld* Monde);

	/** Console : Chelles.Arsenal Blaster|Bazooka|Sabre|Grenade|Aucune|Recharger */
	void Commande(const TArray<FString>& Arguments);

	// ---- Serveur ----
	void TirServeur(AArsenalRelais* Relais, uint8 Arme, FVector Origine, FVector Direction);
	void SabreServeur(AArsenalRelais* Relais, FVector Camera, FVector Direction);
	void FlammesServeur(AArsenalRelais* Relais, FVector Camera, FVector Direction);
	void PrendreServeur(AArsenalRelais* Relais, ARatelierArsenal* Ratelier);

	// ---- Machine du joueur (appele par le relais) ----
	void AvisLocal(const FString& Texte);
	void ArmeLourdePrise(uint8 Quelle);

	ChellesArsenal::FReglagesArsenal Reglages;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	APlayerController* GetPC() const;
	AArsenalRelais* MonRelais() const;

	// ---- Choix de l'arme ----
	void LierClavier();
	void Changer();
	void Choisir(uint8 NouvelleArme, bool bMessage);
	void SuivreJoueur(APlayerController* PC, APawn* Pion);
	void BloquerTirNormal(APlayerController* PC, bool bOui);
	void MasquerArmeNormale(APawn* Pion);
	void RemontrerArmeNormale();
	void PlacerVue(APawn* Pion);

	// ---- Tir (machine du joueur) ----
	void ClicPresse();
	void ClicRelache();
	void EssayerTir();
	bool Visee(APlayerController* PC, APawn* Pion, FVector& Camera, FVector& Avant, FVector& Cible) const;
	float Cadence(uint8 Quelle) const;

	uint8 Arme = 0;
	bool bTenu = false;
	float DernierTir = -100.f;
	float GuidageJusqua = -100.f;
	float ProchaineVisee = 0.f;
	TWeakObjectPtr<APawn> PionSuivi;
	TWeakObjectPtr<AActor> ArmeNormale;
	bool bArmeNormaleConnue = false;
	TArray<FArsenalMasque> Masques;

	UPROPERTY()
	UInputComponent* EntreesTir = nullptr;
	TWeakObjectPtr<APlayerController> PCBloque;

	UPROPERTY()
	AArsenalVue* Vue = nullptr;

	bool bLieClavier = false;

	// ---- Serveur ----
	void GererRelais();
	float ProchainRelais = 0.f;

	// ---- Armes lourdes, rateliers et puissance selon l'XP (2026-11-20) ----
	/** L'arme peut-elle etre prise en main par le joueur de cette machine ? Sinon OutRaison explique pourquoi. */
	bool Disponible(uint8 Quelle, FString* OutRaison = nullptr) const;
	int32 ChargeLocale(uint8 Quelle) const;
	int32 XPLocale() const;
	int32 XPDuJoueur(APlayerController* PC) const;
	float Puissance(APlayerController* PC) const;
	void PoserRateliers();
	void SuivreRateliers(APlayerController* PC, APawn* Pion);
	void ArreterSonFlammes();
	bool bRateliersPoses = false;
	/** 25/11/2026 : nouvel essai si aucun ratelier n'a pu etre pose (pas encore de joueur, etc.) */
	int32 EssaisRateliers = 0;
	float ProchainEssaiRateliers = 4.f;
	FString AideRatelier;
	TWeakObjectPtr<UAudioComponent> SonFlammes;
	int32 JetsFlammes = 0;

	// ---- Ecran ----
	void Message(const FString& Texte);
	void CreerEcran();
	void RetirerEcran();
	TSharedPtr<SWidget> Ecran;
	FString Info;
	float AgeInfo = 100.f;
	FString Etiquette;
	bool bCurseur = false;
};
