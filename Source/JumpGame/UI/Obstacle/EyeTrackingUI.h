﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EyeTrackingUI.generated.h"

/**
 * 
 */
UCLASS()
class JUMPGAME_API UEyeTrackingUI : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UImage* TargetLocationImage;
};
