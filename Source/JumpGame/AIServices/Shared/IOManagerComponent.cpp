#include "IOManagerComponent.h"

#include "IOHandlers/IPCHandler.h"
#include "IOHandlers/SocketHandler.h"
#include "JumpGame/Utils/FastLogger.h"

UIOManagerComponent::UIOManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UIOManagerComponent::SendGameMessage(const FMessageUnion& Message)
{
	EMessageType MessageType = Message.Header.Type;
	if (!IOHandlers.Contains(MessageType))
	{
		return false;
	}

	FFastLogger::LogConsole(TEXT("UIOManagerComponent::SendGameMessage"));
	IOHandlers[MessageType]->SendGameMessage(Message);
	return true;
}

void UIOManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// TODO: 방어코드 재설정
	if (!GetWorld()->GetMapName().Contains(TEXT("InGameLevel")))
	{
		return ;
	}
	
	// IOHandler 초기화
	TSharedPtr<IIOHandlerInterface> IPCReadHandler = MakeShared<FIPCHandler>();
	IPCReadHandler->SetReaderMode();
	TSharedPtr<IIOHandlerInterface> IPCSendHandler = MakeShared<FIPCHandler>();
	IPCSendHandler->SetWriterMode();
	TSharedPtr<IIOHandlerInterface> SocketHandler = MakeShared<FSocketHandler>();
	
	RegisterIOHandler(EMessageType::Ping, SocketHandler);
	RegisterIOHandler(EMessageType::Pong, SocketHandler);
	RegisterIOHandler(EMessageType::QuizNotify, SocketHandler);
	RegisterIOHandler(EMessageType::WaveRequest, SocketHandler);
	RegisterIOHandler(EMessageType::WaveResponse, SocketHandler);
	
	RegisterIOHandler(EMessageType::EyeTrackingNotifyMessage, IPCSendHandler);
	RegisterIOHandler(EMessageType::EyeTrackingResponseMessage, IPCReadHandler);
	RegisterIOHandler(EMessageType::EyeTrackingRequestMessage, IPCReadHandler);
	
	// queue 초기화
	for (auto& Handler : IOHandlers)
	{
		MessageQueue[Handler.Key] = std::queue<FMessageUnion>();
	}

	if (!IPCReadHandler->Init(IOHandlerInitInfo, &MessageQueue))
	{
		TSharedPtr<FIPCHandler> SharedIPC = StaticCastSharedPtr<FIPCHandler>(IPCReadHandler);
		RetryReadConnectToPipe(SharedIPC);
	}

	if (!IPCSendHandler->Init(IOHandlerInitInfo, &MessageQueue))
	{
		TSharedPtr<FIPCHandler> SharedIPC = StaticCastSharedPtr<FIPCHandler>(IPCSendHandler);
		RetrySendConnectToPipe(SharedIPC);
	}
	
	SocketHandler->Init(IOHandlerInitInfo, &MessageQueue);
	
	// for (int32 i = 0; i < 5; i++)
	// {
	// 	FMessageUnion MessageUnion;
	// 	MessageUnion.EyeTrackingRequestMessage.Header.Type = EMessageType::EyeTrackingRequestMessage;
	// 	MessageUnion.EyeTrackingRequestMessage.Header.PayloadSize = sizeof(FEyeTrackingRequestMessage);
	// 	MessageUnion.EyeTrackingRequestMessage.Header.PlayerID = 1;
	// 	MessageUnion.EyeTrackingRequestMessage.Header.SessionID[0] = 1;
	//
	// 	MessageUnion.EyeTrackingRequestMessage.QuizID = 10;
	// 	MessageUnion.EyeTrackingRequestMessage.Width = 1000;
	// 	MessageUnion.EyeTrackingRequestMessage.Height = 1000;
	// 	
	// 	if (i < 4)
	// 	{
	// 		MessageUnion.EyeTrackingRequestMessage.Start = i + 1;
	// 		MessageUnion.EyeTrackingRequestMessage.End = 0;
	// 	}
	// 	else
	// 	{
	// 		MessageUnion.EyeTrackingRequestMessage.Start = 0;
	// 		MessageUnion.EyeTrackingRequestMessage.End = 1;
	// 	}
	//
	// 	MessageQueue[EMessageType::EyeTrackingRequestMessage].push(MessageUnion);
	// }

	// for (int32 i = 0; i < 100; i++)
	// {
	// 	FMessageUnion MessageUnion;
	// 	MessageUnion.EyeTrackingResponseMessage.Header.Type = EMessageType::EyeTrackingResponseMessage;
	// 	MessageUnion.EyeTrackingResponseMessage.Header.PayloadSize = sizeof(FEyeTrackingResponseMessage);
	// 	MessageUnion.EyeTrackingResponseMessage.Header.PlayerID = 1;
	// 	MessageUnion.EyeTrackingResponseMessage.Header.SessionID[0] = 1;
	//
	// 	MessageUnion.EyeTrackingResponseMessage.QuizID = 10;
	// 	MessageUnion.EyeTrackingResponseMessage.X = FMath::RandRange(50, 950);
	// 	MessageUnion.EyeTrackingResponseMessage.Y = FMath::RandRange(50, 950);
	// 	MessageUnion.EyeTrackingResponseMessage.bBlink = 0;
	// 	MessageUnion.EyeTrackingResponseMessage.State = 100;
	//
	// 	MessageQueue[EMessageType::EyeTrackingResponseMessage].push(MessageUnion);
	// }

	// 사운드 퀴즈 Dummy Message
	/*for (int32 i = 1; i <= 21; i++)
	{
		FWavResponseMessage ResponseMessage;
		ResponseMessage.QuizID = i;
		ResponseMessage.Similarity = FMath::RandRange(0, 100);
		FString DummyStr = TEXT("한글한글한글");
		FTCHARToUTF8 Converted(*DummyStr);
		uint32 Len = Converted.Length();

		FMemory::Memcpy(ResponseMessage.Message, &Len, sizeof(uint32));
		FMemory::Memcpy(ResponseMessage.Message+sizeof(uint32), Converted.Get(), Len);

		FMessageUnion MessageUnion;
		FMemory::Memcpy(&MessageUnion, &ResponseMessage, sizeof(FWavResponseMessage));
		MessageQueue[EMessageType::WaveResponse].push(MessageUnion);
	}*/
}

void UIOManagerComponent::RegisterIOHandler(const EMessageType& MessageType, TSharedPtr<IIOHandlerInterface> Handler)
{
	IOHandlers.Add(MessageType, Handler);
}

void UIOManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (auto& Handler : IOHandlers)
	{
		Handler.Value->ReceiveMessage();
	}
}

bool UIOManagerComponent::PopMessage(const EMessageType& MessageType, FMessageUnion& OutMessage)
{
	if (MessageQueue.find(MessageType) != MessageQueue.end())
	{
		if (MessageQueue[MessageType].empty())
		{
			return false;
		}
		FMessageUnion Message = MessageQueue[MessageType].front();
		MessageQueue[MessageType].pop();
		OutMessage = Message;
		return true;
	}
	return false;
}

void UIOManagerComponent::RetryReadConnectToPipe(TSharedPtr<FIPCHandler> IPCHandlerToRetry)
{
	TWeakObjectPtr<UIOManagerComponent> WeakThis = this;

	GetWorld()->GetTimerManager().SetTimer(RetryReadTimer, FTimerDelegate::CreateLambda([WeakThis, IPCHandlerToRetry]()
	{
		if (!WeakThis.IsValid())
			return;

		UIOManagerComponent* StrongThis = WeakThis.Get();
		if (IPCHandlerToRetry->Init(StrongThis->IOHandlerInitInfo, &StrongThis->MessageQueue))
		{
			FFastLogger::LogConsole(TEXT("PIPE!!!!!!!!!!! Succedded!! IPCHandler : %s"), *IPCHandlerToRetry->GetPipeName());
		}
		else
		{
			//FFastLogger::LogConsole(TEXT("Reconnecting to pipe... : %s"), *IPCHandlerToRetry->GetPipeName());
			StrongThis->RetryReadConnectToPipe(IPCHandlerToRetry);
		}
	}), RetryInterval, false);
}

void UIOManagerComponent::RetrySendConnectToPipe(TSharedPtr<FIPCHandler> IPCHandlerToRetry)
{
	TWeakObjectPtr<UIOManagerComponent> WeakThis = this;

	GetWorld()->GetTimerManager().SetTimer(RetrySendTimer, FTimerDelegate::CreateLambda([WeakThis, IPCHandlerToRetry]()
	{
		if (!WeakThis.IsValid())
			return;

		UIOManagerComponent* StrongThis = WeakThis.Get();
		if (IPCHandlerToRetry->Init(StrongThis->IOHandlerInitInfo, &StrongThis->MessageQueue))
		{
			FFastLogger::LogConsole(TEXT("PIPE!!!!!!!!!!! Succedded!! IPCHandler : %s"), *IPCHandlerToRetry->GetPipeName());
		}
		else
		{
			//FFastLogger::LogConsole(TEXT("Reconnecting to pipe... : %s"), *IPCHandlerToRetry->GetPipeName());
			StrongThis->RetrySendConnectToPipe(IPCHandlerToRetry);
		}
	}), RetryInterval, false);
}
