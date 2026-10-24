#include "AfterfallSettingsComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"

UAfterfallSettingsComponent::UAfterfallSettingsComponent() {}

void UAfterfallSettingsComponent::ApplyFOV()
{
	if (ACharacter* C = Cast<ACharacter>(GetOwner()))
	{
		if (UCameraComponent* Cam = C->FindComponentByClass<UCameraComponent>())
		{
			Cam->SetFieldOfView(FOV);
		}
	}
}

void UAfterfallSettingsComponent::ToggleAdminDebug()
{
	bAdminDebug = !bAdminDebug;
}