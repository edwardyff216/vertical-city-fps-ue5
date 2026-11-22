#include "Progression/CityProgression.h"
#include "Progression/CityProfileIO.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

namespace
{
bool Contains(const TArray<FCityChoice>& Entries,FName Id)
{
    return Entries.ContainsByPredicate([Id](const FCityChoice& Choice){return Choice.Id==Id;});
}

FCityItem* Find(FCityProfile& P,const FString& Id)
{
    if (Id.IsEmpty()) return nullptr;
    for (auto& W:P.Equipped) if (W.Id==Id) return &W;
    return P.Bag.FindByPredicate([&Id](const FCityItem& W){return W.Id==Id;});
}

TUniquePtr<FSystemWideCriticalSection> LockSlot(const FString& Path)
{
    const FString MutexName=TEXT("VerticalCityProfile-")+FMD5::HashAnsiString(*FPaths::ConvertRelativePathToFull(Path).ToLower());
    return MakeUnique<FSystemWideCriticalSection>(MutexName,FTimespan::FromMilliseconds(2));
}

bool IsSlot(int32 Slot){return Slot==0 || Slot==1;}

TArray<FCityItem> RollPair(int32 Seed,FName Kind)
{
    FRandomStream Rng(Seed);
    const FName RoomWeapons[]={TEXT("rifle"),TEXT("shotgun"),TEXT("sword")};
    TArray<FCityItem> Pair;
    for(int I=0;I<2;++I)
    {
        const FName Pick=Kind.IsNone()?RoomWeapons[Rng.RandRange(0,2)]:Kind;
        Pair.Add(FCityItemCatalog::Generate(Pick,Rng));
    }
    return Pair;
}
}

FString UCityProgression::DefaultPath()
{
    FString Override;
    if (FParse::Value(FCommandLine::Get(),TEXT("CityProfile="),Override))
        return FPaths::ConvertRelativePathToFull(Override);
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("SaveGames/City/profile.json"));
}

bool UCityProgression::OpenDefault()
{
    if (bLoaded) return true;
    const FString Default=DefaultPath();
    const FString Import=Default+TEXT(".imported.json");
    auto& FS=IFileManager::Get();
    FString Choice;
    const bool bNative=FFileHelper::LoadFileToString(Choice,*(Default+TEXT(".slot"))) && Choice==TEXT("native");
    return OpenProfile(!bNative && (FS.FileExists(*Import) || FS.FileExists(*(Import+TEXT(".bak")))) ? Import : Default);
}

bool UCityProgression::OpenProfile(const FString& AbsoluteFile)
{
    const FString Target=FPaths::ConvertRelativePathToFull(AbsoluteFile);
    if (!SlotLock || !FPaths::IsSamePath(Target,File))
    {
        auto CandidateLock=LockSlot(Target);
        if (!CandidateLock->IsValid())
        {
            Error=TEXT("另一个游戏进程正在使用此档案。关闭该进程后可重试。");
            return false;
        }
        SlotLock=MoveTemp(CandidateLock);
    }
    File=Target;
    SessionUltimate=NAME_None;
    bLoaded=false;
    Abilities.Reset(); AbilityTickets.Reset();
    FCityProfile Candidate;
    auto& FS=IFileManager::Get();
    bool bBackup=false;
    if (!FS.FileExists(*File) && !FS.FileExists(*(File+TEXT(".bak"))))
    {
        FRandomStream Rng(FMath::Rand());
        Candidate.Equipped[0]=FCityItemCatalog::Generate(TEXT("rifle"),Rng);
        Candidate.Equipped[1]=FCityItemCatalog::Generate(TEXT("shotgun"),Rng);
        Candidate.Bag={FCityItemCatalog::Generate(TEXT("sword"),Rng),FCityItemCatalog::Generate(TEXT("rifle"),Rng)};
        if (!Commit(Candidate)) return false;
    }
    else
    {
        if (!CityProfileIO::Read(File,Candidate))
        {
            if (!CityProfileIO::Read(File+TEXT(".bak"),Candidate))
            {
                Error=TEXT("主档与备份都无法读取。修复存档后重试，或导入独立旧档。");
                return false;
            }
            bBackup=true;
        }
        if (Candidate.bActive)
        {
            Candidate.Equipped[0]={}; Candidate.Equipped[1]={};
            Candidate.Result=TEXT("interrupted"); Candidate.bActive=false; Candidate.Rewards.Reset();
            if (!Commit(Candidate)) return false;
        }
        else if (bBackup)
        {
            if (!Commit(Candidate)) return false;
            Error=TEXT("已从备份恢复存档。");
        }
        else {State=MoveTemp(Candidate); Error.Reset();}
    }
    bLoaded=true;
    return true;
}

bool UCityProgression::ImportLegacy(const FString& Source)
{
    if ((bLoaded && State.bActive) || File.EndsWith(TEXT(".imported.json"))) return false;
    const FString Imported=(File.IsEmpty()?DefaultPath():File)+TEXT(".imported.json");
    auto ImportLock=LockSlot(Imported);
    if (!ImportLock->IsValid()) {Error=TEXT("导入槽正由其他进程使用。");return false;}
    if (FPaths::IsSamePath(Source,Imported) || IFileManager::Get().FileExists(*Imported) || IFileManager::Get().FileExists(*(Imported+TEXT(".bak"))))
    {
        Error=TEXT("独立导入槽已有存档。");
        return false;
    }
    FCityProfile Candidate;
    if (!CityProfileIO::Read(Source,Candidate) && !CityProfileIO::Read(Source+TEXT(".bak"),Candidate))
    {
        Error=TEXT("旧档及备份均无法读取，保留当前档案。");
        return false;
    }
    if (Candidate.bActive)
    {
        Candidate.Equipped[0]={}; Candidate.Equipped[1]={};
        Candidate.Result=TEXT("interrupted"); Candidate.bActive=false;
    }
    Candidate.Rewards.Reset();
    if (!CityProfileIO::Write(Imported,Candidate,Error)) return false;
    const bool bOpened=OpenProfile(Imported);
    if (bOpened) OnEquipmentChanged.Broadcast();
    return bOpened;
}

bool UCityProgression::SwitchArchive()
{
    if (bLoaded && State.bActive) return false;
    const FString Suffix=TEXT(".imported.json");
    const bool FromImport=File.EndsWith(Suffix);
    const FString Base=FromImport?File.LeftChop(Suffix.Len()):File;
    const FString Next=FromImport?Base:Base+Suffix;
    const FString OldFile=File;
    if (!IFileManager::Get().FileExists(*Next) && !IFileManager::Get().FileExists(*(Next+TEXT(".bak"))))
    {
        Error=TEXT("目标档案不存在。"); return false;
    }
    if (!OpenProfile(Next))
    {
        const FString Failure=Error;
        OpenProfile(OldFile); Error=Failure;
        return false;
    }
    const FString Marker=Base+TEXT(".slot");
    if (!FFileHelper::SaveStringToFile(FromImport?TEXT("native"):TEXT("imported"),*(Marker+TEXT(".tmp"))) ||
        !IFileManager::Get().Move(*Marker,*(Marker+TEXT(".tmp")),true,false,false,true))
    {
        OpenProfile(OldFile); Error=TEXT("无法保存档案选择。"); return false;
    }
    OnEquipmentChanged.Broadcast();
    return true;
}

bool UCityProgression::Commit(const FCityProfile& Next)
{
    if (File.IsEmpty() || !CityProfileIO::Write(File,Next,Error)) return false;
    State=Next;
    return true;
}

bool UCityProgression::BeginRun()
{
    if (!OpenDefault() || !IsPrep() || State.Equipped[0].Id.IsEmpty() || State.Equipped[1].Id.IsEmpty())
    {
        if (bLoaded) Error=TEXT("请装备主手和副手后出战。");
        return false;
    }
    FCityProfile N=State;
    N.bActive=true; N.Rewards.Reset(); N.Result=NAME_None;
    if (!Commit(N)) return false;
    Abilities.Reset(); AbilityTickets.Reset();
    OnEquipmentChanged.Broadcast();
    return true;
}

bool UCityProgression::FinishRun(bool bWon)
{
    if (!bLoaded || !State.bActive) return false;
    FCityProfile N=State;
    N.bActive=false; N.Result=bWon?FName(TEXT("victory")):FName(TEXT("defeat"));
    N.Rewards.Reset();
    if (bWon)
    {
        N.Points=FMath::Min(N.Points+1,4);
        for (FCityItem& W:N.Equipped) if (!W.Id.IsEmpty()) ++W.Level;
    }
    else {N.Equipped[0]={}; N.Equipped[1]={};}
    if (!Commit(N)) return false;
    Abilities.Reset(); AbilityTickets.Reset(); ActiveTicket.Reset();
    OnEquipmentChanged.Broadcast();
    return true;
}

bool UCityProgression::ChooseAbility(FString Ticket,FName Id)
{
    static const TArray<FName> Valid={TEXT("reload"),TEXT("vitality"),TEXT("slide_refund"),TEXT("swap_haste"),TEXT("counter"),TEXT("riposte")};
    if (!bLoaded || !State.bActive || !Valid.Contains(Id) || Ticket.IsEmpty() || AbilityTickets.Contains(Ticket)) return false;
    AbilityTickets.Add(Ticket); ++Abilities.FindOrAdd(Id);
    return true;
}

FCityWeaponStats UCityProgression::GetStats(int32 Slot) const
{
    FCityWeaponStats S=FCityItemCatalog::Stats(GetWeapon(Slot));
    S.Reload *= (HasNode(TEXT("reload_drill")) ? .9f : 1.f)/(1.f+.18f*AbilityLevel(TEXT("reload")));
    return S;
}

const FCityItem* UCityProgression::FindItem(const FString& Id) const
{
    if (Id.IsEmpty()) return nullptr;
    for (const auto& W:State.Equipped) if (W.Id==Id) return &W;
    return State.Bag.FindByPredicate([&Id](const FCityItem& W){return W.Id==Id;});
}

bool UCityProgression::SwitchPage(int32 Page)
{
    if (!IsPrep() || !IsSlot(Page)) return false;
    if (Page==State.Page) return true;
    FCityProfile N=State; N.Page=Page;
    return Commit(N);
}

bool UCityProgression::Unlock(FName Id)
{
    const auto& Page=State.Nodes[State.Page];
    const FName Req=FCityItemCatalog::Prerequisite(Id);
    if (!IsPrep() || !Contains(FCityItemCatalog::Nodes(),Id) || Page.Num()>=State.Points || Page.Contains(Id) || (!Req.IsNone() && !Page.Contains(Req))) return false;
    FCityProfile N=State; N.Nodes[N.Page].Add(Id); return Commit(N);
}

bool UCityProgression::Respec()
{
    const int64 Fee=2+2*State.Respecs;
    if (!IsPrep() || State.Nodes[State.Page].IsEmpty() || State.Parts<Fee) return false;
    FCityProfile N=State;
    N.Parts-=Fee; ++N.Respecs; N.Nodes[N.Page].Reset(); return Commit(N);
}

bool UCityProgression::SetActiveSkill(int32 Slot,FName Id)
{
    if (!IsPrep() || !IsSlot(Slot) || !Contains(FCityItemCatalog::Skills(),Id) || State.Skills[1-Slot]==Id) return false;
    FCityProfile N=State; N.Skills[Slot]=Id; return Commit(N);
}

bool UCityProgression::SelectSessionUltimate(FName Id)
{
    if (!bLoaded || !State.bActive || LabProfile.IsSet() || !Contains(FCityItemCatalog::Ultimates(),Id)) return false;
    SessionUltimate=Id;
    return true;
}
bool UCityProgression::SetUltimate(FName Id)
{
    if (!IsPrep() || !Contains(FCityItemCatalog::Ultimates(),Id)) return false;
    FCityProfile N=State; N.Ultimate=Id;
    if (!Commit(N)) return false;
    SessionUltimate=NAME_None;
    return true;
}

bool UCityProgression::SetCharacter(FName Id)
{
    if (!IsPrep() || LabProfile.IsSet() || (Id!=TEXT("s07") && Id!=TEXT("white_01"))) return false;
    if (Id==State.Character) return true;
    FCityProfile Next=State;
    Next.Character=Id;
    if (!Commit(Next)) return false;
    OnEquipmentChanged.Broadcast();
    return true;
}

bool UCityProgression::Equip(const FString& Id,int32 Slot)
{
    if (!bLoaded || (State.bActive && ActiveTicket.IsEmpty()) || !IsSlot(Slot) || Id.IsEmpty()) return false;
    FCityProfile N=State;
    if (N.Equipped[1-Slot].Id==Id) Swap(N.Equipped[0],N.Equipped[1]);
    else
    {
        const int32 I=N.Bag.IndexOfByPredicate([&Id](const FCityItem& W){return W.Id==Id;});
        if (I==INDEX_NONE) return false;
        FCityItem Old=MoveTemp(N.Equipped[Slot]);
        N.Equipped[Slot]=N.Bag[I]; N.Bag.RemoveAt(I);
        if (!Old.Id.IsEmpty()) N.Bag.Add(MoveTemp(Old));
    }
    if (!Commit(N)) return false;
    OnEquipmentChanged.Broadcast(); return true;
}

bool UCityProgression::ClaimBasicWeapons()
{
    int32 Needed=int32(State.Equipped[0].Id.IsEmpty())+int32(State.Equipped[1].Id.IsEmpty())-State.Bag.Num();
    if (!IsPrep() || Needed<=0) return false;
    FCityProfile N=State;
    for (int32 I=0; I<2 && Needed>0; ++I)
    {
        if (!N.Equipped[I].Id.IsEmpty()) continue;
        N.Equipped[I].Id=FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
        N.Equipped[I].Kind=I==0?TEXT("rifle"):TEXT("shotgun");
        --Needed;
    }
    if (!Commit(N)) return false;
    OnEquipmentChanged.Broadcast(); return true;
}

bool UCityProgression::Salvage(const TArray<FString>& Ids)
{
    if (!IsPrep() || Ids.IsEmpty()) return false;
    FCityProfile N=State;
    for (const auto& Id:Ids)
    {
        const int32 I=N.Bag.IndexOfByPredicate([&Id](const FCityItem& W){return W.Id==Id;});
        if (I==INDEX_NONE) return false;
        N.Parts+=N.Bag[I].Rarity; N.Bag.RemoveAt(I);
    }
    return Commit(N);
}

bool UCityProgression::Manufacture(FName Kind)
{
    if (!IsPrep() || State.Parts<5 || !FCityItemCatalog::Kind(Kind)) return false;
    FCityProfile N=State;
    FRandomStream Rng(FMath::Rand());
    N.Parts-=5; N.Bag.Add(FCityItemCatalog::Generate(Kind,Rng)); return Commit(N);
}

bool UCityProgression::Reroll(const FString& Id,int32 Index,FName Expected)
{
    if (!IsPrep()) return false;
    FCityProfile N=State;
    FCityItem* W=Find(N,Id);
    if (!W || !W->Affixes.IsValidIndex(Index) || W->Affixes[Index]!=Expected || N.Parts<W->Rarity) return false;
    auto Pool=FCityItemCatalog::Pool(W->Kind,W->Rarity);
    for (FName A:W->Affixes) Pool.Remove(A);
    if (Pool.IsEmpty()) return false;
    N.Parts-=W->Rarity;
    W->Affixes[Index]=Pool[FMath::RandRange(0,Pool.Num()-1)];
    if (!Commit(N)) return false;
    OnEquipmentChanged.Broadcast(); return true;
}

FString UCityProgression::PreviewReward(int32 Seed,FName Kind,bool bNamesOnly) const
{
    if (!Kind.IsNone() && !FCityItemCatalog::Kind(Kind)) return {};
    auto Pair=RollPair(Seed,Kind);
    if (bNamesOnly)
    {
        const FString& First = FCityItemCatalog::Kind(Pair[0].Kind)->Label;
        if (Pair[0].Kind == Pair[1].Kind) return First + TEXT(" ×2");
        return First + TEXT(" / ") + FCityItemCatalog::Kind(Pair[1].Kind)->Label;
    }
    return FCityItemCatalog::Describe(Pair[0])+TEXT("\n\n")+FCityItemCatalog::Describe(Pair[1]);
}

bool UCityProgression::GrantReward(const FString& Ticket,int32 Seed,FName Kind)
{
    if (!bLoaded || !State.bActive || Ticket.IsEmpty() || (!Kind.IsNone() && !FCityItemCatalog::Kind(Kind))) return false;
    if (State.Rewards.Contains(Ticket)) return true;
    FCityProfile N=State;
    TArray<FString>& Ids=N.Rewards.Add(Ticket);
    for (FCityItem& W:RollPair(Seed,Kind))
    {
        Ids.Add(W.Id); N.Bag.Add(MoveTemp(W));
    }
    return Commit(N);
}
