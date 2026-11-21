// CHELLES 3D - Roue des ARMES sur TAB (2026-11-21, etapes 2 et 3 de la refonte "menu armes", Prompt2026.md)
//
//  Maintenir TAB : une roue facon GTA apparait (fond gris-bleu transparent), le temps ralentit en solo.
//  La souris vise une arme, on relache TAB (ou clic gauche) pour la prendre. Clic droit : annuler.
//  Au centre : nom de l'arme et munitions (chargeur / reserve). En bas a droite : le personnage joue.
//
//  DATA-DRIVEN : la liste des armes vient du tableau /Game/Chelles/Donnees/DT_Armes
//  (une ligne FChellesArmeLigne par arme, voir RaccourcisTypes.h). Ajouter une arme = ajouter une ligne.
//  Sans tableau, une liste par defaut (armes FPS_Game + arsenal ChellesArmes) sert de secours.
//  Les armes ramassees qui ne sont pas dans le tableau sont ajoutees a la roue toutes seules.
//
//  Les modes (Soldat, Spider-Male...) sont sur F1-F12 : TAB est repris a l'ancienne roue des modes
//  (plugin ChellesRoue, non modifie : on retire seulement sa liaison TAB pendant le jeu).
#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RoueArmes.generated.h"

class APawn;
class APlayerController;
class SRoueArmes;
class UInputComponent;

/** Une arme de la liste (tableau DT_Armes ou secours). */
struct FArmeDeLaRoue
{
	FText Nom;
	/** Blueprint de l'arme FPS_Game (nul pour l'arsenal). */
	TSoftClassPtr<AActor> Classe;
	/** Blaster, Bazooka... (vide = arme FPS_Game). */
	FString Arsenal;
	TSoftObjectPtr<UTexture2D> Icone;
	FLinearColor Couleur = FLinearColor(0.f, 0.f, 0.f, 0.f);
	FString Categorie;
	bool bDonner = false;
};

/** Ce que represente une case de la roue ouverte. */
struct FCibleRoueArmes
{
	int32 Ligne = INDEX_NONE;       // index dans la liste des armes (INDEX_NONE = arme ramassee hors tableau)
	int32 Inventaire = INDEX_NONE;  // case dans UT_Inventory (INDEX_NONE = pas ramassee)
	TWeakObjectPtr<UObject> Instance;
	FString Arsenal;
	TSoftClassPtr<AActor> Classe;
	bool bDonner = false;
};

UCLASS(NotPlaceable, Transient)
class CHELLESRACCOURCIS_API ARoueArmes : public AActor
{
	GENERATED_BODY()

public:
	ARoueArmes();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.RoueArmes [Info|Relire|0|1] */
	void Commande(const TArray<FString>& Arguments);

private:
	// ---- Reglages (Raccourcis.txt, section ROUE DES ARMES) ----
	void ChargerReglages();
	void ChargerTableau();
	void ListeParDefaut();
	bool bActif = true;
	FKey Touche;
	float Ralenti = 0.3f;
	float Sensibilite = 8.f;
	bool bToutesLesArmes = true;
	bool bDonnerToutes = false;
	bool bPrendreTab = true;
	bool bAnimation = true;
	FString CheminTableau;
	bool bDepuisTableau = false;
	TArray<FArmeDeLaRoue> Armes;

	// ---- Clavier ----
	APlayerController* GetPC() const;
	void LierClavier();
	void ReprendreTabALaRoueDesModes();
	bool bLieClavier = false;
	TWeakObjectPtr<APlayerController> PCLie;
	bool bTabRepris = false;
	float ProchaineVerification = 0.f;
	TWeakObjectPtr<AActor> RoueModes;

	// ---- Ouvrir / fermer ----
	void Ouvrir();
	void Relacher();
	void Valider();
	void Annuler();
	void Fermer();
	void SourisX(float Valeur);
	void SourisY(float Valeur);
	void ManetteX(float Valeur);
	void ManetteY(float Valeur);
	void MettreAJourSurvol();
	void Remplir();
	void MettreAJourCentre();

	bool bOuverte = false;
	bool bIgnoreVue = false;
	float AncienRalenti = 1.f;
	FVector2D Curseur = FVector2D::ZeroVector;
	FVector2D Manette = FVector2D::ZeroVector;
	int32 Survol = INDEX_NONE;
	float Apparition = 0.f;
	TSharedPtr<SRoueArmes> Widget;

	UPROPERTY()
	UInputComponent* EntreesRoue = nullptr;

	/** Images des cases gardees en memoire tant que la roue les montre. */
	UPROPERTY()
	TArray<UTexture2D*> Images;

	/** Ce que represente chaque case ouverte. */
	TArray<FCibleRoueArmes> Cibles;

	// ---- Armes ----
	void Choisir(int32 Index);
	bool PrendreCase(APawn* Pion, int32 Inventaire);
	int32 Donner(APawn* Pion, UClass* Classe);
	bool ArsenalEnMain() const;
	AActor* RelaisArsenal() const;
	bool LancerCommande(const FString& Nom, const TArray<FString>& Arguments) const;
	/** Arsenal choisi en dernier par la roue (pour l'allumer tant qu'il est en main). */
	FString DernierArsenal;

	// ---- Personnage en bas a droite ----
	AActor* Roue() const;
	void LireMode(FText& Nom, FLinearColor& Couleur) const;

	void Message(const FString& Texte) const;
};
