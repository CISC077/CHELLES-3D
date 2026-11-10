// CHELLES 3D - Armes : nouvelles apparences des armes FPS_Game + tirs speciaux en plus du tir normal (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArmesTypes.h"
#include "ArmesChelles.generated.h"

class AArmesRelais;
class APawn;
class APlayerController;
class UMeshComponent;
class UPrimitiveComponent;
class SWidget;

/** Un maillage d'origine de l'arme et le nouveau modele pose par-dessus. */
struct FArmesPaire
{
	TWeakObjectPtr<UMeshComponent> Origine;
	TWeakObjectPtr<UMeshComponent> Nouveau;
	TWeakObjectPtr<UObject> Modele;
	bool bRenduPrincipal = true;
	bool bProfondeur = true;
	bool bOmbre = true;
};

/** Accessoire de l'ancienne arme cache (lunette, silencieux...). */
struct FArmesMasque
{
	TWeakObjectPtr<UPrimitiveComponent> Composant;
	bool bRenduPrincipal = true;
	bool bProfondeur = true;
	bool bOmbre = true;
};

struct FArmeHabillee
{
	FString Cle;
	TArray<FArmesPaire> Paires;
	TArray<FArmesMasque> Accessoires;
};

/**
 * Pose au lancement du jeu sur chaque machine (pas copie par le reseau).
 *
 * 1. APPARENCES : chaque arme du kit FPS_Game (enfant de BP_WeaponBase) recoit un nouveau modele
 *    (packs FPS_Weapon_Bundle, FPS_Assault_Pack, FPSAssets) pose PAR-DESSUS l'ancien, qui devient
 *    invisible mais reste en place : animations, mains, viseur, munitions et tirs du Blueprint ne
 *    changent pas. Rien n'est modifie dans les Blueprints : retirer le plugin = armes d'origine.
 *
 * 2. TIRS SPECIAUX : quand l'arme en main tire (ses munitions baissent), un tir special part EN PLUS :
 *    laser, boule de feu, glacial ou foudre. Touche O = changer le tir special de l'arme en main.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AArmesChelles : public AActor
{
	GENERATED_BODY()

public:
	AArmesChelles();

	virtual void Tick(float DeltaSeconds) override;

	/** Serveur : un joueur demande un tir special. */
	void TirServeur(AArmesRelais* Relais, uint8 Type, FVector Origine, FVector Direction);

	/** Console : Chelles.Arme ... (reglage de l'apparence de l'arme en main). */
	void Commande(const TArray<FString>& Arguments);

	/** Console : Chelles.Armes 0 / 1 (apparences d'origine / nouvelles). */
	void ActiverApparences(bool bOui);

	/** Affiche un message au joueur (et dans le journal). */
	void Message(const FString& Texte);

	/** Vrai si la classe est une arme du kit FPS_Game (enfant de BP_WeaponBase). */
	bool EstArme(UClass* Classe);

	ChellesArmes::FReglagesArmes Reglages;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	// ---- Apparences ----
	void Habiller(AActor* Arme);
	void Deshabiller(FArmeHabillee& Infos);
	void ToutRetirer();
	void ToutRecharger();
	void Synchroniser();
	void Scanner();
	void Apparu(AActor* Acteur);
	FTransform Ajustement(UMeshComponent* Origine, UObject* NouveauModele, const ChellesArmes::FReglageApparence& Manuel, AActor* Arme) const;

	TMap<TWeakObjectPtr<AActor>, FArmeHabillee> Habillees;
	TArray<TWeakObjectPtr<AActor>> AVerifier;
	TMap<TWeakObjectPtr<UClass>, bool> CacheClasses;
	TSet<FString> ModelesAbsents;
	FDelegateHandle AbonnementApparition;
	float ProchainScan = 0.f;

	// ---- Tirs speciaux (machine du joueur) ----
	APlayerController* GetPC() const;
	AArmesRelais* MonRelais() const;
	AActor* ArmeEnMain(APawn* Pion);
	FString NomArme(AActor* Arme) const;
	uint8 TirChoisi(AActor* Arme);
	void ChoisirTir(AActor* Arme, uint8 Tir);
	void LierClavier();
	void Changer();
	void SuivreTirs();
	void TirerLocal(uint8 Tir, AActor* Arme, APlayerController* PC, APawn* Pion);

	TMap<FString, uint8> Choix;
	TWeakObjectPtr<AActor> ArmeSuivie;
	int64 MunitionsVues = -1;
	float DernierTirLocal[ChellesArmes::NombreTirs];
	bool bLieClavier = false;

	// ---- Serveur ----
	void GererRelais();
	bool PvpActif() const;
	float ProchainRelais = 0.f;

	// ---- Ecran ----
	void CreerEcran();
	void RetirerEcran();
	TSharedPtr<SWidget> Ecran;
	FString Info;
	float AgeInfo = 100.f;
	FString Etiquette;
	bool bCurseur = false;
};
