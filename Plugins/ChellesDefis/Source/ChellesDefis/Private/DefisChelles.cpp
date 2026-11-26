// CHELLES 3D - Defi de la semaine : lecture sur le serveur du VPS, bandeau, XP de reussite
#include "DefisChelles.h"

#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

#include "SDefisBandeau.h"

namespace
{
	const TCHAR* SectionIni = TEXT("ChellesDefis");

	bool EstOui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("True"), ESearchCase::IgnoreCase) || V == TEXT("1");
	}

	/** Le premier acteur du monde dont la classe porte ce nom (ex. "PouvoirsChelles"), sans compiler avec son plugin. */
	AActor* ActeurDeClasse(UWorld* Monde, const TCHAR* NomClasse)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(Monde); It; ++It)
		{
			if (It->GetClass()->GetName() == NomClasse)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Variable numerique d'un objet, trouvee par son nom. */
	FNumericProperty* VariableNombre(const UObject* Objet, const TCHAR* Nom)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Objet->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(Nom, ESearchCase::IgnoreCase))
			{
				return CastField<FNumericProperty>(*It);
			}
		}
		return nullptr;
	}
}

FDefisChelles& FDefisChelles::Get()
{
	static FDefisChelles Instance;
	return Instance;
}

void FDefisChelles::Demarrer()
{
	if (!TickHandle.IsValid())
	{
		TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FDefisChelles::Tick), 0.25f);
	}
}

void FDefisChelles::Arreter()
{
	if (TickHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	RetirerBandeau();
}

void FDefisChelles::LireReglages()
{
	bReglagesLus = true;
	R = FReglages();
	TArray<FString> Lignes;
	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesDefis"), TEXT("Defis.txt"));
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		FString Cle, Valeur;
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		if (Cle == TEXT("Actif")) { R.bActif = EstOui(Valeur); }
		else if (Cle == TEXT("Adresse") && Valeur.StartsWith(TEXT("http"))) { R.Adresse = Valeur; R.Adresse.RemoveFromEnd(TEXT("/")); }
		else if (Cle == TEXT("DureeAffichageSecondes")) { R.Duree = FMath::Clamp(FCString::Atof(*Valeur), 3.f, 120.f); }
		else if (Cle == TEXT("DelaiAvantSecondes")) { R.DelaiAvant = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 120.f); }
		else if (Cle == TEXT("RafraichirMinutes")) { R.RafraichirMinutes = FMath::Clamp(FCString::Atof(*Valeur), 1.f, 120.f); }
	}
}

FString FDefisChelles::PseudoLocal()
{
	FString Pseudo;
	if (GConfig)
	{
		GConfig->GetString(TEXT("ChellesChat"), TEXT("Pseudo"), Pseudo, GGameUserSettingsIni);
	}
	Pseudo.TrimStartAndEndInline();
	return Pseudo.Len() >= 3 ? Pseudo.Left(24) : FString();
}

void FDefisChelles::NouveauMonde(UWorld* Monde)
{
	if (!bReglagesLus)
	{
		LireReglages();
	}
	if (!R.bActif || !Monde)
	{
		return;
	}
	RetirerBandeau();
	MondeActuel = Monde;
	bMontreCettePartie = false;
	bMontrerDesQuePret = true;
	PretDepuis = -1.0;
	Demarrer();
	Demander();
}

void FDefisChelles::CommandeMontrer()
{
	LireReglages();
	bMontrerDesQuePret = true;
	bMontreCettePartie = false;
	PretDepuis = -1.0;
	Demarrer();
	Demander();
	UE_LOG(LogTemp, Log, TEXT("[Defis] Defi de la semaine redemande a %s"), *R.Adresse);
}

void FDefisChelles::Demander()
{
	if (bDemandeEnCours || !R.bActif)
	{
		return;
	}
	bDemandeEnCours = true;
	DerniereDemande = FPlatformTime::Seconds();

	FString Url = R.Adresse + TEXT("/api/defis/semaine");
	const FString Pseudo = PseudoLocal();
	if (!Pseudo.IsEmpty())
	{
		Url += TEXT("?pseudo=") + FGenericPlatformHttp::UrlEncode(Pseudo);
	}
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Requete = FHttpModule::Get().CreateRequest();
	Requete->SetURL(Url);
	Requete->SetVerb(TEXT("GET"));
	Requete->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Requete->SetTimeout(10.f);
	Requete->OnProcessRequestComplete().BindRaw(this, &FDefisChelles::Reponse);
	Requete->ProcessRequest();
}

void FDefisChelles::Reponse(FHttpRequestPtr Requete, FHttpResponsePtr Rep, bool bReussi)
{
	bDemandeEnCours = false;
	TSharedPtr<FJsonObject> Racine;
	if (bReussi && Rep.IsValid() && Rep->GetResponseCode() == 200)
	{
		const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Rep->GetContentAsString());
		FJsonSerializer::Deserialize(Lecteur, Racine);
	}
	if (!Racine.IsValid() || !Racine->HasField(TEXT("titre")))
	{
		if (!bErreurDite)
		{
			UE_LOG(LogTemp, Log, TEXT("[Defis] Serveur des defis injoignable (%s) : pas de defi cette fois."), *R.Adresse);
			bErreurDite = true;
		}
		return;
	}
	bErreurDite = false;

	const bool bEtaitReussi = Defi.bValide && Defi.bMoiReussi && Defi.Semaine == Racine->GetStringField(TEXT("semaine"));
	FDefiSemaine D;
	D.bValide = true;
	Racine->TryGetStringField(TEXT("semaine"), D.Semaine);
	Racine->TryGetNumberField(TEXT("numero"), D.Numero);
	Racine->TryGetStringField(TEXT("id"), D.Id);
	Racine->TryGetStringField(TEXT("titre"), D.Titre);
	Racine->TryGetStringField(TEXT("texte"), D.Texte);
	Racine->TryGetStringField(TEXT("type"), D.Type);
	Racine->TryGetStringField(TEXT("objectifTexte"), D.ObjectifTexte);
	Racine->TryGetNumberField(TEXT("xp"), D.XP);
	Racine->TryGetBoolField(TEXT("verifiable"), D.bVerifiable);
	Racine->TryGetStringField(TEXT("finTexte"), D.FinTexte);
	Racine->TryGetNumberField(TEXT("reussis"), D.Reussis);
	double Reste = 0.0;
	Racine->TryGetNumberField(TEXT("resteSecondes"), Reste);
	D.FinSecondes = FPlatformTime::Seconds() + Reste;

	const TArray<TSharedPtr<FJsonValue>>* Top = nullptr;
	if (Racine->TryGetArrayField(TEXT("top"), Top) && Top && Top->Num() > 0 && (*Top)[0].IsValid())
	{
		const TSharedPtr<FJsonObject> P = (*Top)[0]->AsObject();
		FString Pseudo, Texte;
		if (P.IsValid() && P->TryGetStringField(TEXT("pseudo"), Pseudo) && P->TryGetStringField(TEXT("texte"), Texte))
		{
			D.Premier = Pseudo + TEXT(" : ") + Texte;
		}
	}
	const TSharedPtr<FJsonObject>* Moi = nullptr;
	if (Racine->TryGetObjectField(TEXT("moi"), Moi) && Moi && Moi->IsValid())
	{
		D.bMoi = true;
		(*Moi)->TryGetStringField(TEXT("texte"), D.MoiTexte);
		(*Moi)->TryGetNumberField(TEXT("rang"), D.MoiRang);
		(*Moi)->TryGetBoolField(TEXT("reussi"), D.bMoiReussi);
	}
	Defi = D;

	// Defi reussi (vu pour la 1re fois sur ce PC cette semaine) : XP + bandeau "DEFI REUSSI"
	if (Defi.bVerifiable && Defi.bMoiReussi && !bEtaitReussi)
	{
		FString DejaRecompense;
		if (GConfig)
		{
			GConfig->GetString(SectionIni, TEXT("Recompense"), DejaRecompense, GGameUserSettingsIni);
		}
		if (DejaRecompense != Defi.Semaine + TEXT(":") + Defi.Id)
		{
			bReussiteAMontrer = true;
		}
	}
}

bool FDefisChelles::JoueurPret(UWorld* Monde) const
{
	APlayerController* PC = Monde ? Monde->GetFirstPlayerController() : nullptr;
	// Dans le quartier, menus fermes : un personnage, et pas de curseur de souris (les menus montrent le curseur)
	return PC && PC->IsLocalController() && PC->GetPawn() && !PC->bShowMouseCursor;
}

void FDefisChelles::DonnerXP(UWorld* Monde)
{
	const FString Marque = Defi.Semaine + TEXT(":") + Defi.Id;
	if (Defi.XP > 0)
	{
		AActor* Pouvoirs = ActeurDeClasse(Monde, TEXT("PouvoirsChelles"));
		FNumericProperty* P = VariableNombre(Pouvoirs, TEXT("XPLocale"));
		if (!P)
		{
			// On reessaiera a la prochaine partie : la recompense n'est pas marquee comme donnee
			UE_LOG(LogTemp, Warning, TEXT("[Defis] Pouvoirs introuvables : l'XP du defi sera donnee a la prochaine partie."));
			return;
		}
		void* Adresse = P->ContainerPtrToValuePtr<void>(Pouvoirs);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Adresse, P->GetFloatingPointPropertyValue(Adresse) + (double)Defi.XP);
		}
		else
		{
			P->SetIntPropertyValue(Adresse, P->GetSignedIntPropertyValue(Adresse) + (int64)Defi.XP);
		}
		UE_LOG(LogTemp, Log, TEXT("[Defis] Defi %s reussi : +%d XP (pouvoirs)."), *Defi.Id, Defi.XP);
	}
	if (GConfig)
	{
		GConfig->SetString(SectionIni, TEXT("Recompense"), *Marque, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void FDefisChelles::Montrer(bool bReussite)
{
	UWorld* Monde = MondeActuel.Get();
	UGameViewportClient* Vue = Monde ? Monde->GetGameViewport() : nullptr;
	if (!Vue || !Defi.bValide)
	{
		return;
	}
	if (!Bandeau.IsValid() || VueActuelle.Get() != Vue)
	{
		RetirerBandeau();
		Bandeau = SNew(SDefisBandeau);
		// Couche 7 : au-dessus du HUD (3), du Dernier Refuge (5) et des missions (6), sous les menus
		Vue->AddViewportWidgetContent(Bandeau.ToSharedRef(), 7);
		VueActuelle = Vue;
	}
	Bandeau->Montrer(Defi, bReussite, bReussite ? FMath::Max(8.f, R.Duree * 0.7f) : R.Duree);
}

void FDefisChelles::RetirerBandeau()
{
	if (Bandeau.IsValid())
	{
		if (UGameViewportClient* Vue = VueActuelle.Get())
		{
			Vue->RemoveViewportWidgetContent(Bandeau.ToSharedRef());
		}
		Bandeau.Reset();
	}
	VueActuelle.Reset();
}

bool FDefisChelles::Tick(float DeltaSeconds)
{
	UWorld* Monde = MondeActuel.Get();
	if (!Monde || !R.bActif)
	{
		if (Bandeau.IsValid())
		{
			RetirerBandeau();
		}
		return true;
	}
	const double Maintenant = FPlatformTime::Seconds();

	// Bandeau termine : on le retire de l'ecran
	if (Bandeau.IsValid() && !Bandeau->EstVisible())
	{
		RetirerBandeau();
	}

	// Le joueur doit etre dans le quartier, menus fermes, depuis DelaiAvant secondes
	if (JoueurPret(Monde))
	{
		if (PretDepuis < 0.0)
		{
			PretDepuis = Maintenant;
		}
	}
	else
	{
		PretDepuis = -1.0;
	}
	const bool bPret = PretDepuis >= 0.0 && Maintenant - PretDepuis >= R.DelaiAvant;

	if (bPret && Defi.bValide && bReussiteAMontrer && !Bandeau.IsValid())
	{
		bReussiteAMontrer = false;
		DonnerXP(Monde);
		Montrer(true);
	}
	else if (bPret && Defi.bValide && bMontrerDesQuePret && !bMontreCettePartie && !Bandeau.IsValid())
	{
		bMontrerDesQuePret = false;
		bMontreCettePartie = true;
		Montrer(false);
	}

	// De temps en temps : le defi a-t-il ete reussi entre-temps (partie du DERNIER REFUGE envoyee) ?
	if (Maintenant - DerniereDemande > R.RafraichirMinutes * 60.0)
	{
		Demander();
	}
	return true;
}

void FDefisChelles::DecrireEtat() const
{
	UE_LOG(LogTemp, Log, TEXT("[Defis] Actif=%s Adresse=%s Pseudo=%s"), R.bActif ? TEXT("Oui") : TEXT("Non"), *R.Adresse, *PseudoLocal());
	if (!Defi.bValide)
	{
		UE_LOG(LogTemp, Log, TEXT("[Defis] Pas encore de defi recu du serveur."));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Defis] %s (%s) : %s [%s] objectif %s, +%d XP, fin %s"), *Defi.Semaine, *Defi.Id, *Defi.Titre, *Defi.Type,
		*Defi.ObjectifTexte, Defi.XP, *Defi.FinTexte);
	UE_LOG(LogTemp, Log, TEXT("[Defis] Moi : %s, rang %d, reussi %s. En tete : %s. %d joueur(s) ont reussi."),
		*Defi.MoiTexte, Defi.MoiRang, Defi.bMoiReussi ? TEXT("oui") : TEXT("non"), *Defi.Premier, Defi.Reussis);
}
