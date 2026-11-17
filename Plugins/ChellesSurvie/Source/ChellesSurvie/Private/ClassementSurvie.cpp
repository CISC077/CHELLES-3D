// CHELLES 3D - Dernier Refuge : classement des joueurs (ce PC + serveur du VPS)
#include "ClassementSurvie.h"

#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const int32 MaxLocal = 20;

	FString FichierLocal()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChellesSurvie"), TEXT("Classement.txt"));
	}

	/** Lit une liste de lignes de classement dans la reponse JSON du serveur. */
	void LireListe(const TSharedPtr<FJsonObject>& Racine, const TCHAR* Champ, TArray<FLigneClassement>& Liste)
	{
		const TArray<TSharedPtr<FJsonValue>>* Valeurs = nullptr;
		if (!Racine.IsValid() || !Racine->TryGetArrayField(Champ, Valeurs) || !Valeurs)
		{
			return;
		}
		Liste.Reset();
		for (const TSharedPtr<FJsonValue>& Valeur : *Valeurs)
		{
			const TSharedPtr<FJsonObject> O = Valeur.IsValid() ? Valeur->AsObject() : nullptr;
			if (!O.IsValid())
			{
				continue;
			}
			FLigneClassement L;
			FString Pseudo;
			O->TryGetStringField(TEXT("pseudo"), Pseudo);
			L.Pseudo = FLigneClassement::PseudoPropre(Pseudo);
			O->TryGetNumberField(TEXT("score"), L.Score);
			O->TryGetNumberField(TEXT("vague"), L.Vague);
			O->TryGetNumberField(TEXT("kills"), L.Kills);
			O->TryGetNumberField(TEXT("tetes"), L.Tetes);
			O->TryGetNumberField(TEXT("joueurs"), L.Joueurs);
			O->TryGetStringField(TEXT("quand"), L.Date);
			L.Date = L.Date.Left(10);
			Liste.Add(L);
		}
	}
}

FClassementSurvie& FClassementSurvie::Get()
{
	static FClassementSurvie Instance;
	return Instance;
}

void FClassementSurvie::ChargerReglages()
{
	bReglagesLus = true;
	bEnLigneActif = true;
	Adresse = TEXT("http://51.178.54.208:8080");
	TArray<FString> Lignes;
	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesSurvie"), TEXT("Refuge.txt"));
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		return;
	}
	for (const FString& Brute : Lignes)
	{
		FString Cle, Valeur;
		if (!FLigneReglage::Decouper(Brute, Cle, Valeur))
		{
			continue;
		}
		if (Cle == TEXT("ClassementEnLigne"))
		{
			bEnLigneActif = FLigneReglage::EstOui(Valeur);
		}
		else if (Cle == TEXT("AdresseClassement") && Valeur.StartsWith(TEXT("http")))
		{
			Adresse = Valeur;
			Adresse.RemoveFromEnd(TEXT("/"));
		}
	}
	if (!bEnLigneActif)
	{
		EtatEnLigne = EEtatClassementEnLigne::Coupe;
	}
}

FString FClassementSurvie::PseudoLocal(const FString& NomDeSecours)
{
	FString Pseudo;
	if (GConfig)
	{
		GConfig->GetString(TEXT("ChellesChat"), TEXT("Pseudo"), Pseudo, GGameUserSettingsIni);
	}
	Pseudo = Pseudo.TrimStartAndEnd();
	return FLigneClassement::PseudoPropre(Pseudo.Len() >= 3 ? Pseudo : NomDeSecours);
}

// ---- Ce PC ----------------------------------------------------------------------

void FClassementSurvie::ChargerLocal()
{
	bLocalLu = true;
	ListeLocale.Reset();
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *FichierLocal()))
	{
		return;
	}
	for (const FString& Brute : Lignes)
	{
		FLigneClassement L;
		if (FLigneClassement::DepuisTexte(Brute, L))
		{
			FClassementCalculs::Inserer(ListeLocale, L, MaxLocal);
		}
	}
}

void FClassementSurvie::SauverLocal() const
{
	FString Texte = TEXT("# CHELLES 3D - DERNIER REFUGE : meilleures parties de ce PC\r\n# Score|Vague|Kills|Tetes|Joueurs|Date|Pseudo\r\n");
	for (const FLigneClassement& L : ListeLocale)
	{
		Texte += L.VersTexte() + TEXT("\r\n");
	}
	if (!FFileHelper::SaveStringToFile(Texte, *FichierLocal(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Classement] Impossible d'ecrire %s"), *FichierLocal());
	}
}

const TArray<FLigneClassement>& FClassementSurvie::Local()
{
	if (!bLocalLu)
	{
		ChargerLocal();
	}
	return ListeLocale;
}

int32 FClassementSurvie::AjouterLocal(const FLigneClassement& Ligne, int32& AncienRecord)
{
	Local();
	AncienRecord = FClassementCalculs::Record(ListeLocale);
	const int32 Place = FClassementCalculs::Inserer(ListeLocale, Ligne, MaxLocal);
	SauverLocal();
	UE_LOG(LogTemp, Log, TEXT("[Classement] Partie enregistree sur ce PC : %s, score %d, vague %d (place %d)."), *Ligne.Pseudo, Ligne.Score, Ligne.Vague, Place);
	return Place;
}

// ---- Serveur du VPS ---------------------------------------------------------------

FString FClassementSurvie::Url(const TCHAR* Chemin) const
{
	return Adresse + Chemin;
}

void FClassementSurvie::Rafraichir(bool bForcer)
{
	if (!bReglagesLus)
	{
		ChargerReglages();
	}
	if (!bEnLigneActif)
	{
		EtatEnLigne = EEtatClassementEnLigne::Coupe;
		return;
	}
	const double Maintenant = FPlatformTime::Seconds();
	if (!bForcer && (EtatEnLigne == EEtatClassementEnLigne::Chargement || Maintenant - DerniereDemande < 20.0))
	{
		return;
	}
	DerniereDemande = Maintenant;
	if (EtatEnLigne != EEtatClassementEnLigne::Pret)
	{
		EtatEnLigne = EEtatClassementEnLigne::Chargement;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Requete = FHttpModule::Get().CreateRequest();
	Requete->SetURL(Url(TEXT("/api/classement?limite=10")));
	Requete->SetVerb(TEXT("GET"));
	Requete->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Requete->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Reponse, bool bReussi)
	{
		FClassementSurvie& C = FClassementSurvie::Get();
		TSharedPtr<FJsonObject> Racine;
		if (bReussi && Reponse.IsValid() && Reponse->GetResponseCode() == 200)
		{
			const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Reponse->GetContentAsString());
			FJsonSerializer::Deserialize(Lecteur, Racine);
		}
		if (!Racine.IsValid())
		{
			// On garde l'ancien classement s'il y en avait un
			if (C.EtatEnLigne != EEtatClassementEnLigne::Pret)
			{
				C.EtatEnLigne = EEtatClassementEnLigne::HorsLigne;
			}
			UE_LOG(LogTemp, Log, TEXT("[Classement] Serveur injoignable (%s) : classement de ce PC seulement."), *C.Adresse);
			return;
		}
		LireListe(Racine, TEXT("meilleurs"), C.ListeEnLigne);
		Racine->TryGetNumberField(TEXT("total"), C.TotalEnLigne);
		C.EtatEnLigne = EEtatClassementEnLigne::Pret;
	});
	Requete->ProcessRequest();
}

void FClassementSurvie::Envoyer(const FLigneClassement& Ligne)
{
	if (!bReglagesLus)
	{
		ChargerReglages();
	}
	RangEnvoye = 0;
	if (!bEnLigneActif)
	{
		return;
	}
	const TSharedRef<FJsonObject> Objet = MakeShared<FJsonObject>();
	Objet->SetStringField(TEXT("pseudo"), Ligne.Pseudo);
	Objet->SetNumberField(TEXT("score"), Ligne.Score);
	Objet->SetNumberField(TEXT("vague"), Ligne.Vague);
	Objet->SetNumberField(TEXT("kills"), Ligne.Kills);
	Objet->SetNumberField(TEXT("tetes"), Ligne.Tetes);
	Objet->SetNumberField(TEXT("joueurs"), Ligne.Joueurs);
	FString Corps;
	const TSharedRef<TJsonWriter<>> Ecrivain = TJsonWriterFactory<>::Create(&Corps);
	FJsonSerializer::Serialize(Objet, Ecrivain);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Requete = FHttpModule::Get().CreateRequest();
	Requete->SetURL(Url(TEXT("/api/classement")));
	Requete->SetVerb(TEXT("POST"));
	Requete->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	Requete->SetHeader(TEXT("X-Chelles-Jeu"), TEXT("refuge-1"));
	Requete->SetContentAsString(Corps);
	Requete->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Reponse, bool bReussi)
	{
		FClassementSurvie& C = FClassementSurvie::Get();
		if (bReussi && Reponse.IsValid() && Reponse->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> Racine;
			const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Reponse->GetContentAsString());
			if (FJsonSerializer::Deserialize(Lecteur, Racine) && Racine.IsValid())
			{
				Racine->TryGetNumberField(TEXT("rang"), C.RangEnvoye);
				UE_LOG(LogTemp, Log, TEXT("[Classement] Partie envoyee au serveur : place %d."), C.RangEnvoye);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Classement] Envoi au serveur impossible (code %d) : la partie reste sur ce PC."),
				Reponse.IsValid() ? Reponse->GetResponseCode() : 0);
		}
		C.Rafraichir(true);
	});
	Requete->ProcessRequest();
}

const TArray<FLigneClassement>& FClassementSurvie::AMontrer(FString& Titre)
{
	if (EtatEnLigne == EEtatClassementEnLigne::Pret && ListeEnLigne.Num() > 0)
	{
		Titre = TEXT("CLASSEMENT DES JOUEURS");
		return ListeEnLigne;
	}
	switch (EtatEnLigne)
	{
	case EEtatClassementEnLigne::Chargement: Titre = TEXT("MEILLEURS SCORES DE CE PC  (serveur : chargement...)"); break;
	case EEtatClassementEnLigne::HorsLigne:  Titre = TEXT("MEILLEURS SCORES DE CE PC  (serveur injoignable)"); break;
	default:                                 Titre = TEXT("MEILLEURS SCORES DE CE PC"); break;
	}
	return Local();
}
