#include "ProtoGameMode.h"

#include "Engine/World.h"
#include "ProtoBench.h"

AProtoGameMode::AProtoGameMode()
{
	DefaultPawnClass = nullptr;
}

void AProtoGameMode::StartPlay()
{
	Super::StartPlay();
	GetWorld()->SpawnActor<AProtoBench>();
}
