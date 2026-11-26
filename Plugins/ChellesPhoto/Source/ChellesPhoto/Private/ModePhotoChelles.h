// CHELLES 3D - Mode photo (26/11/2026)
// F12 (modifiable dans OPTIONS > CONTROLES, ligne MODE PHOTO) : le monde se fige en solo, une camera libre
// part de la vue du joueur, on cadre, on applique un filtre et on enregistre un PNG sans l'interface
// dans Documents\CHELLES 3D - Photos, pret pour TikTok et Discord.
// Tout passe par un pre-processeur Slate : pendant le mode photo, le clavier et la souris pilotent la
// camera et n'arrivent plus au personnage. Aucune dependance vers les autres plugins CHELLES.
#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "InputCoreTypes.h"
#include "Framework/Application/IInputProcessor.h"

class ACameraActor;
class APlayerController;
class APawn;
class AActor;
class UWorld;
class SWidget;
class SModePhotoChelles;

/** Reglages lus dans Plugins\ChellesPhoto\Photo.txt */
struct FReglagesPhoto
{
	bool bFigerEnSolo = true;
	float Vitesse = 600.f;
	float Sensibilite = 0.15f;
	float DistanceMax = 60.f; // metres, 0 = sans limite
	FString Dossier;
	bool bCadreTikTok = false;
	FString Partage;
};

/** Ce que l'aide a l'ecran doit montrer */
struct FEtatPhoto
{
	FString Filtre;
	float Fov = 90.f;
	bool bCadreTikTok = false;
	bool bGrille = false;
	bool bFlou = false;
	bool bPersoCache = false;
	bool bAide = true;
	bool bFige = false;
	FString ToucheMode;
	FString Partage;
};

class FModePhotoChelles : public FTickableGameObject
{
public:
	static FModePhotoChelles& Get();
	static void Detruire();

	void Basculer();
	bool EstActif() const { return bActif; }

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FModePhotoChelles, STATGROUP_Tickables); }

	// Appeles par le pre-processeur
	bool ToucheAppuyee(FSlateApplication& Slate, const FKey& Touche, bool bRepete);
	bool ToucheRelachee(const FKey& Touche);
	bool SourisBouge(const FVector2D& Delta);
	bool Molette(float Delta);

	FEtatPhoto Etat() const;

private:
	void Entrer();
	void Quitter();
	void PrendrePhoto();
	void PhotoRecue(int32 Largeur, int32 Hauteur, const TArray<FColor>& Pixels);
	void AppliquerFiltre();
	void CacherInterface(bool bCacher);
	void LireReglages();
	FKey ToucheDuMode() const;
	bool PeutEntrer(FSlateApplication& Slate) const;
	FString DossierPhotos() const;
	void Message(const FString& Texte, bool bFlash = false);

	TSharedPtr<class FPreProcesseurPhoto> PreProcesseur;
	TSharedPtr<SModePhotoChelles> Ecran;
	FDelegateHandle PoigneeCapture;

	bool bActif = false;
	bool bFige = false;
	float DilatationAvant = 1.f;
	TWeakObjectPtr<UWorld> Monde;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<AActor> CibleAvant;
	TWeakObjectPtr<APawn> Perso;
	bool bHudAvant = true;

	FVector Depart = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FRotator RotationDepart = FRotator::ZeroRotator;
	float FovDepart = 90.f;
	float Fov = 90.f;
	int32 Filtre = 0;
	bool bCadreTikTok = false;
	bool bGrille = false;
	bool bFlou = false;
	bool bPersoCache = false;
	bool bAide = true;
	bool bClicDroit = false;
	bool bCaptureDemandee = false;
	TSet<FKey> Enfoncees;

	/** Interface cachee : widget -> opacite d'avant */
	TArray<TPair<TWeakPtr<SWidget>, float>> Caches;

	FReglagesPhoto Reglages;
};
