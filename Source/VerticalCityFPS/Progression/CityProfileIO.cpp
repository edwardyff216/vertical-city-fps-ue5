#include "Progression/CityProfileIO.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
using FObj = TSharedPtr<FJsonObject>;
using FVals = TArray<TSharedPtr<FJsonValue>>;
constexpr int64 MaxExact = 9007199254740991LL;

bool Number(const FObj& O, const TCHAR* Key, int64 Min, int64 Max, int64& Into)
{
    double N;
    if (!O->HasTypedField<EJson::Number>(Key) || !O->TryGetNumberField(Key,N) || !FMath::IsFinite(N) || N < Min || N > Max || double(int64(N)) != N) return false;
    Into = int64(N);
    return true;
}

bool Str(const FObj& O, const TCHAR* Key, FString& Out)
{
    return O->HasTypedField<EJson::String>(Key) && O->TryGetStringField(Key, Out);
}

bool Names(const FObj& O,const TCHAR* Key,TArray<FName>& Out)
{
    const FVals* Arr;
    if (!O->TryGetArrayField(Key,Arr)) return false;
    for (const auto& V:*Arr)
    {
        FString S;
        if (V->Type != EJson::String || !V->TryGetString(S) || S.IsEmpty() || Out.Contains(FName(*S))) return false;
        Out.Emplace(*S);
    }
    return true;
}

bool Known(const TArray<FCityChoice>& Set,FName Id)
{
    return Set.ContainsByPredicate([Id](const FCityChoice& C){return C.Id == Id;});
}

bool DecodeItem(const TSharedPtr<FJsonValue>& V, bool bAllowEmpty, TSet<FString>& Seen, FCityItem& W)
{
    const FObj* Obj;
    if (!V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) return false;
    const FObj& O=*Obj;
    if (O->Values.IsEmpty()) return bAllowEmpty;
    FString Kind,Frame;
    int64 Rarity,Roll;
    if (!Str(O,TEXT("id"),W.Id) || W.Id.Len()!=32 || Seen.Contains(W.Id) ||
        !Str(O,TEXT("template"),Kind) || !Str(O,TEXT("frame"),Frame) ||
        !Number(O,TEXT("level"),1,MaxExact,W.Level) || !Number(O,TEXT("rarity"),1,5,Rarity) ||
        !Number(O,TEXT("damage_roll"),1,10000000,Roll) || !Names(O,TEXT("affixes"),W.Affixes)) return false;
    W.Kind = FName(*Kind);
    W.Rarity = int32(Rarity);
    W.DamageRoll = int32(Roll);
    const FCityKind* K=FCityItemCatalog::Kind(W.Kind);
    if (!K || K->Frame != FName(*Frame) || W.Affixes.Num()>W.Rarity-1) return false;
    const auto Pool=FCityItemCatalog::Pool(W.Kind,W.Rarity);
    for (FName A:W.Affixes) if (!Pool.Contains(A)) return false;
    Seen.Add(W.Id);
    return true;
}

bool Decode(const FObj& O, FCityProfile& P)
{
    int64 Version;
    FString Result;
    const FVals *Equipped,*Bag;
    if (!O || !Number(O,TEXT("version"),1,1,Version) || !O->HasTypedField<EJson::Boolean>(TEXT("active")) ||
        !O->TryGetBoolField(TEXT("active"),P.bActive) || !Str(O,TEXT("last_result"),Result) ||
        !(Result.IsEmpty() || Result==TEXT("defeat") || Result==TEXT("victory") || Result==TEXT("interrupted")) ||
        !O->TryGetArrayField(TEXT("equipped"),Equipped) || Equipped->Num()!=2 || !O->TryGetArrayField(TEXT("bag"),Bag)) return false;
    P.Result=FName(*Result);
    if (O->HasField(TEXT("parts")) && !Number(O,TEXT("parts"),0,MaxExact,P.Parts)) return false;
    TSet<FString> Seen;
    for (int32 I=0; I<2; ++I) if (!DecodeItem((*Equipped)[I],true,Seen,P.Equipped[I])) return false;
    for (const auto& V:*Bag) if (!DecodeItem(V,false,Seen,P.Bag.AddDefaulted_GetRef())) return false;
    if (O->HasField(TEXT("skill_tree")))
    {
        const FObj* Tree;
        int64 Points;
        if (!O->TryGetObjectField(TEXT("skill_tree"),Tree) || !Tree || !Tree->IsValid() ||
            !Number(*Tree,TEXT("points"),2,4,Points) || !Number(*Tree,TEXT("respecs"),0,MaxExact,P.Respecs)) return false;
        P.Points=int32(Points);
        TArray<FName> Current,Other;
        if (!Names(*Tree,TEXT("nodes"),Current)) return false;
        if ((*Tree)->HasField(TEXT("active_page")) || (*Tree)->HasField(TEXT("other_nodes")))
        {
            FString Page;
            if (!Str(*Tree,TEXT("active_page"),Page) || !(Page==TEXT("survival") || Page==TEXT("gunplay")) || !Names(*Tree,TEXT("other_nodes"),Other)) return false;
            P.Page = Page==TEXT("gunplay") ? 1 : 0;
        }
        else if (Current.Num() && !Current.ContainsByPredicate([](FName N){return N!=TEXT("reload_drill") && N!=TEXT("kill_feed") && N!=TEXT("slide_reload");})) P.Page=1;
        P.Nodes[P.Page]=MoveTemp(Current);
        P.Nodes[1-P.Page]=MoveTemp(Other);
        for (const TArray<FName>& Page:P.Nodes)
        {
            if (Page.Num()>P.Points) return false;
            for (FName Id:Page)
            {
                const FName Req=FCityItemCatalog::Prerequisite(Id);
                if (!Known(FCityItemCatalog::Nodes(),Id) || (!Req.IsNone() && !Page.Contains(Req))) return false;
            }
        }
    }
    FString Character,Ultimate;
    if (Str(O,TEXT("character_id"),Character) && Character==TEXT("white_01")) P.Character=TEXT("white_01");
    if (Str(O,TEXT("ultimate_id"),Ultimate) && Known(FCityItemCatalog::Ultimates(),FName(*Ultimate))) P.Ultimate=FName(*Ultimate);
    const FVals* Skills;
    if (O->TryGetArrayField(TEXT("active_skill_ids"),Skills))
    {
        FName Pair[2];
        for (int32 I=0; I<FMath::Min(2,Skills->Num()); ++I)
        {
            FString Id;
            if ((*Skills)[I]->Type != EJson::String || !(*Skills)[I]->TryGetString(Id)) continue;
            FName N(*Id);
            if (Known(FCityItemCatalog::Skills(),N) && N!=Pair[1-I]) Pair[I]=N;
        }
        for (int32 I=0; I<2; ++I)
            if (Pair[I].IsNone()) Pair[I]=Pair[1-I]!=P.Skills[I] ? P.Skills[I] : P.Skills[1-I];
        P.Skills[0]=Pair[0]; P.Skills[1]=Pair[1];
    }
    if (O->HasField(TEXT("reward_ledger")))
    {
        const FObj* Ledger;
        if (!O->TryGetObjectField(TEXT("reward_ledger"),Ledger) || !Ledger || !Ledger->IsValid()) return false;
        for (const auto& KV:(*Ledger)->Values)
        {
            const FVals* Ids;
            if (KV.Key.IsEmpty() || !KV.Value->TryGetArray(Ids) || Ids->Num()!=2) return false;
            auto& Entry=P.Rewards.Add(FString(*KV.Key));
            for (const auto& V:*Ids)
            {
                FString Id;
                if (V->Type!=EJson::String || !V->TryGetString(Id) || !Seen.Contains(Id) || Entry.Contains(Id)) return false;
                Entry.Add(Id);
            }
        }
    }
    return true;
}

FVals EncodeNames(const TArray<FName>& Ns)
{
    FVals A;
    for (FName N:Ns) A.Add(MakeShared<FJsonValueString>(N.ToString()));
    return A;
}

TSharedPtr<FJsonValue> EncodeItem(const FCityItem& W)
{
    FObj O=MakeShared<FJsonObject>();
    if (const FCityKind* K=FCityItemCatalog::Kind(W.Kind))
    {
        O->SetStringField(TEXT("id"),W.Id);
        O->SetStringField(TEXT("template"),W.Kind.ToString());
        O->SetStringField(TEXT("frame"),K->Frame.ToString());
        O->SetNumberField(TEXT("level"),double(W.Level)); O->SetNumberField(TEXT("rarity"),W.Rarity);
        O->SetNumberField(TEXT("damage_roll"),W.DamageRoll);
        O->SetArrayField(TEXT("affixes"),EncodeNames(W.Affixes));
    }
    return MakeShared<FJsonValueObject>(O);
}

FObj Encode(const FCityProfile& P)
{
    FObj O=MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("version"),1);
    O->SetStringField(TEXT("character_id"),P.Character.ToString());
    O->SetArrayField(TEXT("active_skill_ids"),EncodeNames({P.Skills[0],P.Skills[1]}));
    O->SetStringField(TEXT("ultimate_id"),P.Ultimate.ToString());
    O->SetBoolField(TEXT("active"),P.bActive);
    O->SetStringField(TEXT("last_result"),P.Result.IsNone()?FString():P.Result.ToString());
    O->SetNumberField(TEXT("parts"),double(P.Parts));
    O->SetArrayField(TEXT("equipped"),{EncodeItem(P.Equipped[0]),EncodeItem(P.Equipped[1])});
    FVals Bag;
    for (const FCityItem& W:P.Bag) Bag.Add(EncodeItem(W));
    O->SetArrayField(TEXT("bag"),MoveTemp(Bag));
    FObj Tree=MakeShared<FJsonObject>();
    Tree->SetNumberField(TEXT("points"),P.Points); Tree->SetNumberField(TEXT("respecs"),double(P.Respecs));
    Tree->SetStringField(TEXT("active_page"),P.Page==0?TEXT("survival"):TEXT("gunplay"));
    Tree->SetArrayField(TEXT("nodes"),EncodeNames(P.Nodes[P.Page]));
    Tree->SetArrayField(TEXT("other_nodes"),EncodeNames(P.Nodes[1-P.Page]));
    O->SetObjectField(TEXT("skill_tree"),Tree);
    FObj Ledger=MakeShared<FJsonObject>();
    for (const auto& KV:P.Rewards)
    {
        FVals Ids;
        for (const FString& Id:KV.Value) Ids.Add(MakeShared<FJsonValueString>(Id));
        Ledger->SetArrayField(KV.Key,Ids);
    }
    O->SetObjectField(TEXT("reward_ledger"),Ledger);
    return O;
}
}

bool CityProfileIO::Read(const FString& File, FCityProfile& Out)
{
    FString Text;
    FObj Obj;
    FCityProfile Candidate;
    if (!FFileHelper::LoadFileToString(Text,*File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Obj) || !Decode(Obj,Candidate)) return false;
    Out=MoveTemp(Candidate);
    return true;
}

bool CityProfileIO::Write(const FString& File, const FCityProfile& State, FString& Error)
{
    Error=TEXT("无法保存，操作未生效。检查磁盘后可重试。");
    auto& FS=IFileManager::Get();
    if (!FS.MakeDirectory(*FPaths::GetPath(File),true)) return false;
    FObj Obj=Encode(State);
    FString Text;
    FCityProfile Verified;
    const FString Tmp=File+TEXT(".tmp"), Backup=File+TEXT(".bak"), BackupTmp=File+TEXT(".bak.tmp");
    if (!FJsonSerializer::Serialize(Obj.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)) ||
        !FFileHelper::SaveStringToFile(Text,*Tmp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) || !Read(Tmp,Verified)) return false;
    FCityProfile Previous;
    if (FS.FileExists(*File) && Read(File,Previous))
    {
        if (FS.Copy(*BackupTmp,*File)!=COPY_OK || !Read(BackupTmp,Previous) || !FS.Move(*Backup,*BackupTmp,true,false,false,true)) return false;
    }
    if (!FS.Move(*File,*Tmp,true,false,false,true)) return false;
    Error.Reset();
    return true;
}


