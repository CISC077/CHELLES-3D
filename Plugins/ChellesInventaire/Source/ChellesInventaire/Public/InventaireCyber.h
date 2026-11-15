// CHELLES 3D - Menu bibliotheque d'armes de FPS_Game (W_WeaponInventory_UI et sa version Skin_B)
//
//  1. Pendant le jeu, quand le menu est ouvert, ses cadres blancs, ses textes, ses boutons et ses
//     icones prennent un style cyberpunk (cyan / magenta sur fond violet sombre).
//  2. Le bouton QUITTER (CloseInventory_BTN) ferme vraiment le menu : il est retire de l'ecran,
//     la souris est cachee et le jeu reprend les touches.
//  Rien n'est ecrit dans les fichiers du projet : le widget du pack n'est pas modifie.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventaireCyber.generated.h"

class UUserWidget;
class UWidget;

/** Reglages lus dans Plugins\ChellesInventaire\Inventaire.txt */
struct FReglagesInventaire
{
	bool bActif = true;
	bool bQuitterRepare = true;
	FLinearColor Cadres = FLinearColor(0.f, 0.94f, 1.f);
	FLinearColor Texte = FLinearColor(0.f, 0.94f, 1.f);
	FLinearColor Lueur = FLinearColor(1.f, 0.f, 0.78f);
	FLinearColor Fond = FLinearColor(0.047f, 0.f, 0.118f);
	float OpaciteFond = 0.85f;
	FLinearColor Bouton = FLinearColor(0.157f, 0.f, 0.275f);
	FLinearColor BoutonSurvol = FLinearColor(1.f, 0.f, 0.78f);
	FLinearColor BoutonClic = FLinearColor(0.f, 0.94f, 1.f);
	FLinearColor Icones = FLinearColor(0.78f, 1.f, 1.f);
};

UCLASS(NotPlaceable, Transient)
class CHELLESINVENTAIRE_API AInventaireCyber : public AActor
{
	GENERATED_BODY()

public:
	AInventaireCyber();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Inventaire [Info|0|1|Relire|Fermer] */
	void Commande(const TArray<FString>& Arguments);

	/** Ferme tous les menus d'inventaire ouverts (appele par le bouton QUITTER). */
	UFUNCTION()
	void FermerInventaire();

private:
	void ChargerReglages();
	static bool EstMenuInventaire(const UUserWidget* Widget);
	void Styliser(UUserWidget* Widget, int32 Profondeur);
	void StyliserUn(UWidget* Element);
	void BrancherQuitter(UUserWidget* Menu);
	void RendreLaMain(UUserWidget* Menu);

	FReglagesInventaire Reglages;
	float ProchainControle = 0.f;

	/** Elements deja colores (on ne les recolore pas a chaque controle). */
	TSet<TWeakObjectPtr<UWidget>> DejaStylises;
	/** Menus dont le bouton QUITTER est deja branche. */
	TSet<TWeakObjectPtr<UUserWidget>> DejaBranches;
	/** Menus vus ouverts au dernier controle. */
	TArray<TWeakObjectPtr<UUserWidget>> Ouverts;
};
