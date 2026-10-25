#pragma once
#include "CoreMinimal.h"
#include "Combat/InteractionEvent.h"

class FAfterfallEventDedup
{
public:
	bool IsSeen(const FGuid& Id) const { return Seen.Contains(Id); }
	void MarkSeen(const FGuid& Id) { Seen.Add(Id); }
	void Reset() { Seen.Empty(); }
private:
	TSet<FGuid> Seen;
};