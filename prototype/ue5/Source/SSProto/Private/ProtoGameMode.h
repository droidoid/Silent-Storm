#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ProtoGameMode.generated.h"

// Starts the bench: no pawn, no HUD; AProtoBench builds the scene and drives the scenario.
UCLASS()
class AProtoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AProtoGameMode();
	virtual void StartPlay() override;
};
