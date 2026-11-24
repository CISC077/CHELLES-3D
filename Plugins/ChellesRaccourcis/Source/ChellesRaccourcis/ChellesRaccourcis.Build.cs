using UnrealBuildTool;

public class ChellesRaccourcis : ModuleRules
{
	public ChellesRaccourcis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore", // EKeys / FKey (touches F1 a F12)
			"Slate",
			"SlateCore", // petit message a l'ecran
			"UMG"        // menu ARMEMENTS : fermer l'ancien menu d'inventaire du kit FPS (UUserWidget)
		});
	}
}
