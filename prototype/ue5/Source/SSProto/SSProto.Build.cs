using System.IO;
using UnrealBuildTool;

public class SSProto : ModuleRules
{
	public SSProto(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });

		// The engine-agnostic core (prototype/core) is compiled into this module from its own folder:
		// Private/SSProtoCore.cpp includes its sources, nothing is copied.
		string CoreDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "core"));
		PrivateIncludePaths.Add(Path.Combine(CoreDir, "include"));
	}
}
