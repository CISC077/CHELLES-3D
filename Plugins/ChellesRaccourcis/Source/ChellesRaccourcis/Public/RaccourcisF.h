// CHELLES 3D - Touches F1 a F12 (2026-11-21, etape 1 de la refonte "menu armes", Prompt2026.md)
//
//  F1 a F11 choisissent directement un MODE de la roue (Soldat, Spider-Male, Spider-Femelle, PANTERA,
//  Motoneige, POLICE, FLASH ECLAIR, FLAMME FIRE, SuperPowers, DYNAMIC COMBAT, HORSE), F12 est reservee.
//  Un menu deroulant en haut de l'ecran montre les touches et le mode joue (il descend quand on appuie
//  sur une touche F, puis remonte). La roue TAB marche toujours ; elle deviendra la roue des ARMES (etape 3).
//
//  Le plugin pilote la roue des modes (plugin ChellesRoue) par reflexion : il appelle ses fonctions
//  ChoisirModeParNom / ModeActuel et lit sa liste Modes. Il ne depend pas de ChellesRoue pour compiler.
//  Une touche F peut aussi prendre une arme ("Case1".."Case4", "Blaster"...), voir Raccourcis.txt.
//
//  DATA-DRIVEN : la liste des touches vient du tableau de donnees /Game/Chelles/Donnees/DT_Raccourcis
//  (une ligne FChellesRaccourciLigne par touche, voir RaccourcisTypes.h). Ajouter une touche = ajouter une ligne.
//  Si le tableau n'existe pas encore, les lignes F1=... de Raccourcis.txt servent de secours.
//
//  Une touche F deja utilisee par le mode joue (ex. F2-F6 en DYNAMIC COMBAT) est laissee a ce mode.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RaccourcisF.generated.h"

class APawn;
class APlayerController;
class SVerticalBox;
class SWidget;

/** Ce que fait une touche. */
struct FRaccourciF
{
	FKey Touche;
	/** Nom d'un mode de la roue, Case1..Case8, Blaster..., Reserve. */
	FString Action;
	/** Texte du menu (vide = nom du mode). */
	FString NomAffiche;
	/** Couleur de la case (alpha 0 = couleur du mode). */
	FLinearColor Couleur = FLinearColor(0.f, 0.f, 0.f, 0.f);
};

UCLASS(NotPlaceable, Transient)
class CHELLESRACCOURCIS_API ARaccourcisF : public AActor
{
	GENERATED_BODY()

public:
	ARaccourcisF();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Raccourcis [Info|Relire|0|1|Menu|F1..F12|Mode <nom>] */
	void Commande(const TArray<FString>& Arguments);

private:
	void ChargerReglages();
	APlayerController* GetPC() const;
	void Appuyer(int32 Numero);
	bool ToucheDejaPrise(APlayerController* PC, const FKey& Touche) const;
	/** Retire les touches de debogage d'Unreal (F1 fil de fer, F2 sans eclairage...) posees sur nos touches. */
	void RetirerTouchesDebogage(APlayerController* PC);
	TWeakObjectPtr<class UPlayerInput> EntreesNettoyees;

	// ---- Modes (roue ChellesRoue) ----
	AActor* Roue() const;
	bool ChoisirMode(const FString& Nom);
	int32 LireModeActuel() const;
	void LireModes();

	// ---- Armes (soldat FPS_Game et arsenal ChellesArmes) ----
	static bool EstSoldat(APawn* Pion);
	bool ChoisirCase(APawn* Pion, int32 Index);
	bool ChoisirArsenal(const FString& Nom);
	bool ArsenalEnMain() const;
	bool LancerCommande(const FString& Commande, const TArray<FString>& Arguments) const;
	static FString NomArme(UObject* Arme);

	// ---- Ecran ----
	void Message(const FString& Texte);
	void MontrerMenu();
	void CreerEcran();
	void RemplirMenu();
	void RetirerEcran();
	float Maintenant() const;

	bool bActif = true;
	bool bAnimation = true;
	bool bMessages = true;
	/** 0 = jamais, 1 = quand on appuie sur une touche F (descend puis remonte), 2 = toujours. */
	int32 AffichageMenu = 1;
	bool bVertical = true;
	/** Chemin du tableau de donnees (Tableau= dans Raccourcis.txt). */
	FString CheminTableau;
	/** Vrai si les touches viennent du tableau (faux = secours Raccourcis.txt). */
	bool bDepuisTableau = false;
	void ChargerTableau();
	/** F11 reste au plein ecran : le mode qui y etait passe sur F12. */
	void LibererF11();
	/** F11 = plein ecran / fenetre (au cas ou Unreal ne l'aurait pas fait). */
	void PleinEcranF11(APlayerController* PC);
	float DureeMenu = 3.f;
	/** 24/11/2026 : mode choisi dans le lobby ([ChellesLobby] ModeDepart), applique au debut de la partie. */
	void ModeDuLobby();
	bool bModeLobbyFini = false;
	int32 EssaisLobby = 0;
	float ProchainEssaiLobby = 0.f;
	TArray<FRaccourciF> Raccourcis;

	/** Noms et couleurs des modes de la roue (lus une fois qu'elle existe). */
	TArray<FString> NomsModes;
	TArray<FLinearColor> CouleursModes;
	/** Nom du mode joue (lu 4 fois par seconde). */
	FString ModeJoue;
	float ProchaineLecture = 0.f;
	TWeakObjectPtr<AActor> RoueTrouvee;

	FString TexteMessage;
	float FinMessage = 0.f;
	float DebutMenu = -100.f;
	TSharedPtr<SWidget> Ecran;
	TSharedPtr<SVerticalBox> ListeVerticale;
	TSharedPtr<class SHorizontalBox> ListeHorizontale;
};
