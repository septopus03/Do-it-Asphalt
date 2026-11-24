# Nákupné budovy a traja roboti Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Obchod na `Tab` nahradia tri budovy pri výrobni, v ktorých sa kupuje klávesom `E`, a dajú sa kúpiť až traja roboti.

**Architecture:** Jadro (`AsphaltCore`) dostane polohy budov, priradenie vecí budovám a pole troch stavov robota; pravidlá ostávajú v `FLotGame`. Vrstva hry (`DoItAsphalt`) pridá jedného aktora budovy a jedného aktora bodu nákupu, doterajší panel obchodu sa filtruje podľa budovy. Kolízia budov je v modeloch (kvádre z `manifest.json`), kód ju len zapne.

**Tech Stack:** Unreal Engine 5.8.4, C++, Slate, Automation Tests (`DIA_TEST`), bash skripty v `Scripts/`.

**Spec:** `docs/superpowers/specs/2026-11-23-nakupne-budovy-design.md`. Rozmery modelov a ich body sú v `docs/03-3d-modely.md`, časť 4.5.

## Modely

| Značka | Kto úlohu robí | Parameter pri spúšťaní subagenta |
|---|---|---|
| **[Sonnet 5.5]** | každá úloha (C++, testy, shell) aj kontrola kódu po úlohe | `model: "sonnet"` |
| **[Fable 5.2]** | riadi plán, pozerá snímky z hry, mení 3D modely, ak treba | – |

## Global Constraints

- Engine: `/Users/Shared/Epic Games/UE_5.8` (5.8.4). API enginu overuj v zdrojákoch na disku, nie z pamäti.
- Žiadne Blueprinty, UMG ani ručne vytvorené `.uasset`. Modely `SM_ToolShop`, `SM_PaverDepot`, `SM_Garage` a `Tools/ModelGen/` sa nemenia; ak rozmer nesedí, úloha sa zastaví a nahlási to.
- Identifikátory a komentáre po anglicky, texty v hre po anglicky cez `LOCTEXT` / `NSLOCTEXT`, dokumentácia v `docs/` po slovensky.
- Ceny a pravidlá odomykania existujúcich vecí sa nemenia. Nové ceny: robot 2 = 6 000, robot 3 = 12 000.
- Cesta k projektu obsahuje medzery: každá cesta v každom príkaze je v úvodzovkách.
- Nové testy sú deklarované makrom `DIA_TEST` v `#if WITH_DEV_AUTOMATION_TESTS`: jadro `DoItAsphalt.Lot.*` v `Source/AsphaltCore/Private/Tests/`, hra `DoItAsphalt.Game.*` v `Source/DoItAsphalt/Tests/` (svet pre testy: `Tests/DIATestWorld.h`).
- Pred koncom každej úlohy prejde `Scripts/build.sh` aj celý `Scripts/test.sh` (`FAILED 0`, počet `PASSED` neklesne pod základ z Task 0). Pred každým spustením over `pgrep -fl "UnrealEditor-Cmd|UnrealBuildTool"`: skripty nikdy nebežia dva naraz.
- V priečinku sú necommitnuté zmeny iných prác na vetve `main`. **Žiadne commity, stash, reset ani prepínanie vetvy.** Krok „Commit“ v tomto pláne nie je; úloha končí zoznamom zmenených súborov.
- `Do it Asphalt - dokumentacia.docx`, `Config/`, `Build/`, `.Rhistory`, `.Rapp.history` sú používateľove: nemeniť.
- Súradnice: svet v cm, +X východ, +Y k parkovisku od južného pruhu; model budovy je v metroch v docs/03, v kóde v cm. Prevod bodu modelu `(Lx, Ly)` do sveta: `Pos + (Lx·cos θ − Ly·sin θ, Lx·sin θ + Ly·cos θ)`, θ = otočenie budovy.

## Review Focus

1. **Panel ostane otvorený, keď už nemá byť** (hráč odíde, nová alebo načítaná hra, otvorí sa menu alebo výhra): panel sa zavrie a myš sa vráti kamere. → Task 5 (`ShopPoint.ClosesWhenAway`, `ShopPoint.ClosesOnReset`).
2. **V garáži stojí auto T4, aj nakrivo**: terminál je v dosahu, výzva patrí nákupu a nie autu, a poistka pri kúpe vyššieho stupňa postaví auto na počiatok garáže, nie do steny. → Task 2 (`Shops.CarFallbackInGarage`), Task 5 (`ShopPoint.GarageWithT4`).
3. **Staré uloženie (verzia 4 alebo 5) s robotom uprostred práce alebo pri výdajni**: načíta sa, robot je robot 1 a pokračuje; roboti 2 a 3 nie sú. → Task 6 (`Save.LoadsOlderPaver`).
4. **Menej voľných sektorov než robotov** (koniec hry, jeden nedokončený sektor): nadbytočný robot čaká a neprepína sa; ručné pridelenie hotového sektora nič nerozbije. → Task 6 (`AutoPaver.FewerSectorsThanPavers`).
5. **Vybraný robot neexistuje** (po načítaní hry s menej robotmi, po novej hre): výber sa vráti na robota 1; `H` ani mapa nikdy neoslovia nekúpeného robota. → Task 8 (`PaverMap.SelectionClamps`).

---

### Task 0: Základ **[Sonnet 5.5]**

- [ ] **Step 1:** Over, že nebeží iný `UnrealEditor-Cmd` ani `UnrealBuildTool`. Spusti `Scripts/build.sh`, potom `Scripts/test.sh`. Zapíš počet `PASSED` (základ) a hodnotu `FLotGame::SaveVersion` v `Source/AsphaltCore/Public/LotGame.h` (plán počíta s 5; ak je iná, v Task 6 platí „o jedna vyššia“).
- [ ] **Step 2:** Ak build alebo testy neprejdú ešte pred zmenou, zastav sa a nahlás presnú chybu.

---

### Task 1: Budovy v rozložení (jadro) **[Sonnet 5.5]**

**Files:**
- Modify: `Source/AsphaltCore/Public/LotBalance.h`, `Source/AsphaltCore/Public/LotLayout.h`, `Source/AsphaltCore/Private/LotLayout.cpp`, `Source/AsphaltCore/Private/LotGame.cpp` (`PlaceBoughtItem`)
- Test: `Source/AsphaltCore/Private/Tests/LotLayoutTest.cpp` (nový), úprava `LotMachinesTest.cpp`, `Source/DoItAsphalt/Tests/CarTest.cpp` tam, kde čakajú staré miesto auta

**Interfaces – Produces:**

```cpp
// LotBalance.h
enum class ELotShop : uint8 { Tools, Robots, Garage, Count };

// LotLayout.h
ASPHALTCORE_API FVector2D ShopPos(ELotShop Shop);      // Tools (900, -10500), Robots (1800, -10550), Garage (-3000, -10500)
ASPHALTCORE_API double ShopYawDeg(ELotShop Shop);      // 90, 90, 0
ASPHALTCORE_API FVector2D ShopLocalToWorld(ELotShop Shop, FVector2D LocalCm);
ASPHALTCORE_API FVector2D ShopPoint(ELotShop Shop);    // where the player stands to buy; local (222, -130), (185, 245), (430, 295)
inline constexpr int32 NumPaverDocks = 3;
ASPHALTCORE_API FVector2D PaverDock(int32 Index);      // local (-35, -220), (-35, -60), (-35, 100) of Robots; zero vector outside 0..2
inline constexpr double PaverDockYawDeg = 90.0;
inline constexpr double CarSpawnYawDeg = 0.0;
```

`ItemSpawn(ELotItem::Car)` vracia `ShopPos(Garage)`, `ItemSpawn(ELotItem::AutoPaver)` vracia `PaverDock(0)`. `PlaceBoughtItem(Car)` nastaví aj `WorldState.CarYawDeg = CarSpawnYawDeg`.

- [ ] **Step 1: Test `DoItAsphalt.Lot.Layout.Shops`** s tvrdeniami (tolerancia 0,01):
  - `ShopPoint(Tools) == (1030, -10278)`, `ShopPoint(Robots) == (1555, -10365)`, `ShopPoint(Garage) == (-2570, -10205)`
  - `PaverDock(0..2) == (2020, -10585), (1860, -10585), (1700, -10585)`; `PaverDock(3)` a `PaverDock(-1)` sú nulové
  - každé `ShopPos(...).Y` leží v `(-10000 - StripCm, -10000)`
  - `ItemSpawn(Car) == (-3000, -10500)`; po `BuyItem(Car)` (s kreditmi a odomknutím cez `Debug…`) je `World().CarPos` to isté a `CarYawDeg == 0`
  - po kúpe robota je `GetAutoPaverPos() == PaverDock(0)`
- [ ] **Step 2:** Spusti `Scripts/build.sh` – čaká sa chyba prekladu (chýbajúce funkcie).
- [ ] **Step 3:** Doplň enum a funkcie podľa Interfaces; `ShopLocalToWorld` podľa vzorca z Global Constraints.
- [ ] **Step 4:** Uprav existujúce testy, ktoré majú staré miesto auta `(-1200, -9400)` alebo robota `(-600, -9700)` zapísané číslom; testy, ktoré volajú `ItemSpawn`, ostávajú.
- [ ] **Step 5:** `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.

---

### Task 2: Aktor budovy vo svete **[Sonnet 5.5]**

**Files:**
- Create: `Source/DoItAsphalt/World/DIAShopBuilding.h`, `.cpp`
- Modify: `Source/DoItAsphalt/Game/DIAModelSizes.h`, `Source/DoItAsphalt/Game/DIAGameMode.cpp` (`StartPlay`, `EnsureAutoPaver`), `Source/DoItAsphalt/UI/DIAFormat.h`, `.cpp`
- Test: `Source/DoItAsphalt/Tests/ShopBuildingTest.cpp` (nový)

**Interfaces – Consumes:** Task 1 (`ELotShop`, `ShopPos`, `ShopYawDeg`, `ShopPoint`, `PaverDockYawDeg`).

**Interfaces – Produces:**

```cpp
// DIAModelSizes.h (cm, envelopes from manifest.json)
inline const FVector ToolShopCm(353.0, 608.0, 400.0);
inline const FVector PaverDepotCm(350.0, 620.0, 341.0);
inline const FVector GarageCm(1215.0, 800.0, 559.0);

// DIAFormat.h
DOITASPHALT_API FText ShopName(ELotShop Shop);          // "Tools", "Robots", "Garage"

// DIAShopBuilding.h
namespace DIAShopBuilding
{
	DOITASPHALT_API const TCHAR* ModelName(ELotShop Shop);   // SM_ToolShop, SM_PaverDepot, SM_Garage
	DOITASPHALT_API FVector SignCentreCm(ELotShop Shop);     // local: (116, -130, 312), (174, -60, 298), (557, 0, 456)
	DOITASPHALT_API FVector2D SignAreaCm(ELotShop Shop);     // (240, 50), (240, 45), (300, 46)
	DOITASPHALT_API FTransform MakeTransform(ELotShop Shop); // ShopPos, Z = 0, yaw ShopYawDeg
}
UCLASS(NotBlueprintable) class ADIAShopBuilding : public AActor
{
	ELotShop Kind = ELotShop::Tools;            // set before BeginPlay (spawn deferred, like ADIAEntryBoard::Entry)
	UStaticMeshComponent* GetBodyMesh() const;
	UTextRenderComponent* GetTextComponent() const;
};
```

Správanie: model cez `DIAAssets::SetModelOrFallback` (ako ostatní aktori), kolízia zapnutá a berie sa jednoduchá kolízia modelu (kvádre z importu), blokuje hráča aj auto. Text tabule je `ShopName(Kind)` verzálkami, tmavou farbou `DIABoard::SignTextColor`, 1 cm pred tabuľou smerom +X modelu, vpísaný do `SignAreaCm` rovnakým výpočtom ako tabuľa nákupného centra v `DIAEntryBoard`. `ADIAGameMode::StartPlay` postaví všetky tri hneď za pevnou výdajňou. `EnsureAutoPaver` spawnuje robota s otočením `PaverDockYawDeg`.

- [ ] **Step 1: Testy** `DoItAsphalt.Game.Shops.*`:
  - `Stand`: po štarte hry sú práve tri `ADIAShopBuilding`, každý na `MakeTransform(Kind)`, s modelom `ModelName(Kind)` (nie náhradnou kockou).
  - `NoOverlap`: obálky (AABB) troch budov sa nepretínajú navzájom, s `ADIAPlantActor` ani s `ADIAVehicleStation`; všetky ležia v pruhu `Y ∈ (-11200, -10000)`.
  - `GarageDoorOpen`: sweep kvádra s polovicami `(410, 250, 170)` cm (auto T4) z `ShopLocalToWorld(Garage, (900, 0))` do `ShopPos(Garage)` vo výške stredu 175 cm nenarazí na garáž; ten istý sweep naprieč bočnou stenou (`(0, 600)` → `(0, 0)`) narazí.
  - `PointsFree`: kapsula hráča (rozmery z `DIACharacter`) na každom `ShopPoint(Kind)` sa neprekrýva s kolíziou žiadnej budovy.
  - `SignText`: text každej tabule je `ShopName(Kind)` verzálkami.
  - `CarSpawnsInGarage`: po kúpe auta stojí `ADIAPaverCar` na `ShopPos(Garage)` s otočením 0; po `DebugGrantItem(CarT4)` sa jeho kolízia neprekrýva s garážou.
  - `CarFallbackInGarage` (Review Focus 2): auto stupňa 1 postavené do steny garáže (`ShopLocalToWorld(Garage, (0, 300))`, otočenie 90) po udelení `CarT2` skončí na `ShopPos(Garage)` s otočením `CarSpawnYawDeg`.
  - `PaverSpawnsInDock`: po kúpe robota stojí `ADIAAutoPaver` na `PaverDock(0)` s otočením 90.
- [ ] **Step 2:** `Scripts/build.sh` – čaká sa chyba prekladu.
- [ ] **Step 3:** Implementuj podľa Interfaces. Poistka v `ADIAPaverCar::UpdateTier` (`DIAPaverCar.cpp`, presun na `ItemSpawn(Car)`) nastaví aj otočenie `CarSpawnYawDeg`.
- [ ] **Step 4:** `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.
- [ ] **Step 5:** `Scripts/shot.sh` (prečítaj si ho): snímka každej budovy spredu; cesty k PNG uveď v hlásení.

---

### Task 3: Priradenie vecí budovám (jadro) **[Sonnet 5.5]**

**Files:**
- Modify: `Source/AsphaltCore/Public/LotBalance.h`
- Test: `Source/AsphaltCore/Private/Tests/LotBalanceTest.cpp`

**Interfaces – Produces:**

```cpp
/** The building that sells the item, indexed by ELotItem. */
inline constexpr ELotShop ItemShop[] = { /* Bucket, Canister, Tub, Trolley, Cart */ Tools ×5, /* Station */ Robots,
	/* AutoPaver, AutoPaverFast, AutoPaverFaster */ Robots ×3, /* Car, CarT2, CarT3, CarT4 */ Garage ×4 };
inline constexpr ELotShop PlantShop = ELotShop::Tools;
```

So `static_assert` na dĺžku ako pri `ItemPrice`.

- [ ] **Step 1: Test `DoItAsphalt.Lot.Balance.ItemShop`**: tabuľka má presne hodnoty hore; `PlantShop == Tools`; každá budova predáva aspoň jednu vec.
- [ ] **Step 2:** Doplň tabuľku. `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.

---

### Task 4: Panel obchodu podľa budovy **[Sonnet 5.5]**

**Files:**
- Modify: `Source/DoItAsphalt/UI/DIAFormat.h`, `.cpp` (`FDIAShopRow`, `FDIAShopModel`), `UI/SDIAShopPanel.h`, `.cpp`, `UI/DIAHUD.h`, `.cpp`, `Game/DIACheats.cpp` (`dia.Shop`)
- Test: `Source/DoItAsphalt/Tests/ShopTest.cpp`, `UiTextTest.cpp`

**Interfaces – Consumes:** `LotBalance::ItemShop`, `PlantShop`, `DIAFormat::ShopName`.

**Interfaces – Produces:**

```cpp
// DIAFormat.h
struct FDIAShopRow { /* ... */ ELotShop Shop = ELotShop::Tools; };   // the plant row: PlantShop
DOITASPHALT_API FText ShopSubtitle(ELotShop Shop);   // "Hand tools and the plant", "Robots and the portable station", "Trucks"

// SDIAShopPanel.h
void SetShop(ELotShop Shop);            // rebuilds the grid: only the rows of the shop, in model order, 4 per line
ELotShop GetShop() const;

// DIAHUD.h
void OpenShop(ELotShop Shop);           // SetShop + open; closes the paver map like SetShopOpen(true) did
ELotShop GetOpenShop() const;           // ELotShop::Count while closed
void SetShopOpen(bool bOpen);           // stays; true opens the shop that was open last (Tools at the start)
// ToggleShop() is removed
```

Nadpis panela je `ShopName(Shop)` (namiesto „SHOP“), podnadpis `ShopSubtitle(Shop)`. Pás „IN HAND“ je viditeľný len pre `Tools`. Text dole: „E / Esc - close“. `dia.Shop <Tools|Robots|Garage>` otvorí danú budovu, bez mena zavrie. `Tab` v tejto úlohe ešte volá `SetShopOpen(!IsShopOpen())` (dočasne, zmizne v Task 5).

- [ ] **Step 1: Testy** `DoItAsphalt.Game.Shop.*`:
  - `RowsByBuilding`: Tools ukazuje riadok výrobne a 5 vecí, Robots 4, Garage 4; zjednotenie sú všetky riadky modelu a žiadny nie je v dvoch budovách.
  - `TitlePerBuilding`: nadpis a podnadpis pre každú budovu.
  - `HandStripOnlyInTools`.
  - `BuyInItsBuilding`: kúpa vedra v Tools, robota v Robots, auta v Garage prejde tak ako doteraz kúpa v jednom paneli.
- [ ] **Step 2:** Uprav existujúce testy panela tak, aby pred kúpou otvorili správnu budovu; ich očakávania o stavoch kariet sa nemenia.
- [ ] **Step 3:** Implementuj. `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.

---

### Task 5: Bod nákupu, `E` a koniec `Tab` **[Sonnet 5.5]**

**Files:**
- Create: `Source/DoItAsphalt/World/DIAShopPoint.h`, `.cpp`
- Modify: `Game/DIAGameMode.cpp`, `Player/DIAPlayerController.h`, `.cpp`, `Player/DIAInput.h`, `.cpp`, `UI/DIAFormat.h`, `.cpp`, `UI/SDIAShopPanel.h` (komentár o `Tab`)
- Test: `Source/DoItAsphalt/Tests/InteractionTest.cpp`

**Interfaces – Consumes:** `LotLayout::ShopPoint`, `ADIAHUD::OpenShop`, `GetOpenShop`, `SetShopOpen`.

**Interfaces – Produces:**

```cpp
// DIAFormat.h
DOITASPHALT_API FText ShopPrompt(ELotShop Shop);     // "E – Tools", "E – Robots", "E – Garage" (same dash as CarPrompt)

// DIAShopPoint.h
namespace DIAShopPoint { inline constexpr double CloseRangeCm = 300.0; }
UCLASS(NotBlueprintable) class ADIAShopPoint : public AActor, public IDIAInteractable
{
	ELotShop Kind = ELotShop::Tools;    // set before BeginPlay
	virtual FText GetPrompt(const ADIACharacter* By) const override;   // ShopPrompt(Kind)
	virtual void Interact(ADIACharacter* By) override;                 // HUD OpenShop(Kind)
};
```

Bod nemá kolíziu (subsystém ho potom meria k počiatku) a stojí na `ShopPoint(Kind)` vo výške stredu kapsuly hráča. Registruje sa cez `UDIAWorldSubsystem::RegisterInteractable` ako ostatné. `ADIAGameMode::StartPlay` postaví tri body spolu s budovami.

Pravidlá v `ADIAPlayerController`:
- `E` pri otvorenom paneli ho zavrie skôr, než sa hľadá čokoľvek iné.
- Každý tick: ak je panel otvorený a hráč sedí v aute alebo je ďalej než `CloseRangeCm` od `ShopPoint(GetOpenShop())`, panel sa zavrie.
- Akcia `Input.Shop`, jej mapovanie na `Tab` a `OnShop` zmiznú.

- [ ] **Step 1: Testy** `DoItAsphalt.Game.Interaction.ShopPoint.*`:
  - `PromptAndOpen`: hráč na `ShopPoint(Kind)` má výzvu `ShopPrompt(Kind)`; interakcia otvorí panel s `GetOpenShop() == Kind` (pre všetky tri).
  - `ECloses`: druhé `E` panel zavrie.
  - `ClosesWhenAway` (Review Focus 1): hráč presunutý 301 cm od bodu → po ticku je panel zavretý a `IsUiInput()` je false.
  - `ClosesOnReset` (Review Focus 1): `NewGame()` aj `LoadGame()` pri otvorenom paneli ho zavrú.
  - `GarageWithT4` (Review Focus 2): auto T4 na `ShopPos(Garage)`; kapsula hráča na `ShopPoint(Garage)` sa s ním neprekrýva a `FindInteractable` vráti bod nákupu, nie auto.
  - `TabUnmapped`: kontext vstupu nemá mapovanie na `EKeys::Tab`.
- [ ] **Step 2:** Implementuj. `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.
- [ ] **Step 3:** `Scripts/shot.sh`: snímka otvoreného panela každej budovy (cez `dia.Shop <meno>`); cesty k PNG uveď v hlásení.

---

### Task 6: Traja roboti v jadre a ukladanie **[Sonnet 5.5]**

**Files:**
- Modify: `Source/AsphaltCore/Public/LotBalance.h`, `LotGame.h`, `Source/AsphaltCore/Private/LotGame.cpp`
- Modify (len aby sa preložili, s robotom 0): `Source/DoItAsphalt/World/DIAAutoPaver.cpp`, `World/DIAGroundActor.cpp`, `UI/SDIAPaverMap.cpp`, `UI/DIAFormat.cpp`, `Game/DIAAudio.cpp`, `Game/DIACheats.cpp`, `Player/DIAPlayerController.cpp`, testy vrstvy hry
- Test: `Source/AsphaltCore/Private/Tests/LotMachinesTest.cpp`, `LotSaveTest.cpp`, `LotBalanceTest.cpp`

**Interfaces – Produces:**

```cpp
// LotBalance.h – the two new items go at the END of ELotItem (stored flags of older saves keep their order)
enum class ELotItem : uint8 { /* ... */ CarT4, AutoPaver2, AutoPaver3, Count };
inline constexpr int32 MaxAutoPavers = 3;
inline constexpr ELotItem AutoPaverItem[MaxAutoPavers] = {ELotItem::AutoPaver, ELotItem::AutoPaver2, ELotItem::AutoPaver3};
// ItemPrice += {6000, 12000}; ItemUnlock += {AutoPaver, AutoPaver}; ItemRequires += {AutoPaver, AutoPaver2}; ItemShop += {Robots, Robots}

// LotGame.h
struct ASPHALTCORE_API FLotPaver
{
	int32 Sector = INDEX_NONE; int32 Step = 0; FVector2D Pos = FVector2D::ZeroVector;
	ELotPaverPhase Phase = ELotPaverPhase::Idle; double Budget = 0.0; double Tank = 0.0; double SearchCooldown = 0.0;
};
int32 GetNumAutoPavers() const;                          // 0..3; robots are owned in order
bool AssignAutoPaverSector(int32 Paver, int32 Sector);   // false for a robot that is not owned or an invalid sector
int32 FindAutoPaverInSector(int32 Sector) const;         // the robot assigned to the sector; INDEX_NONE
int32 GetAutoPaverSector(int32 Paver) const;             // likewise GetAutoPaverStep, GetAutoPaverPos, GetAutoPaverPhase,
                                                         // GetAutoPaverTank, IsAutoPaverTravelling, IsAutoPaverStarved: all take the robot
static constexpr int32 SaveVersion = 6;                  // one above the value recorded in Task 0
```

`GetAutoPaverLevel`, `GetAutoPaverRateCellsPerMin`, `SetAutoPaverAuto`, `IsAutoPaverAuto`, `GetAutoPaverTankCapacity` ostávajú bez čísla robota (platia pre všetkých). Getter pre nekúpeného alebo neplatného robota vracia hodnotu prázdneho `FLotPaver`.

Pravidlá:
- Slučka jedného robota sa nemení; `TickAutoPaver` beží pre každého kúpeného v poradí čísel (preto sa pri výdajni delia o zásobník v tomto poradí).
- AUTO: voľný robot berie najbližší sektor s voľným políčkom, ktorý **nemá iný robot**.
- `AssignAutoPaverSector(P, S)`: ak má `S` iný robot, ten ho stratí (`Sector = INDEX_NONE`, `Step = 0`, `Budget = 0`, `Phase = Idle`; nádrž a poloha ostávajú).
- Kúpený robot *n* dostane `Pos = LotLayout::PaverDock(n)`.
- Uloženie verzie 6 zapisuje `bAutoPaverAuto` a `MaxAutoPavers` blokov (sektor, krok, poloha, fáza, rozpočet, nádrž). Načítanie prijme verzie 4, 5 a 6; staršie majú jeden blok, ktorý je robot 0, a ostatní dostanú prázdny stav na svojom státí. Kontroly z dnešného `LoadFromBytes` platia pre každého robota; navyše je neplatné, ak dvaja majú ten istý sektor alebo ak má stav robot, ktorý nie je kúpený.

- [ ] **Step 1: Testy** `DoItAsphalt.Lot.AutoPaver.*`:
  - `SecondNeedsFirst`: `AutoPaver2` sa neponúka bez `AutoPaver`, `AutoPaver3` bez `AutoPaver2`; ceny 6 000 a 12 000; `ItemShop` je Robots.
  - `SpawnInDocks`: traja kúpení stoja na `PaverDock(0..2)`.
  - `AutoPicksDifferentSectors`: dvaja roboti v AUTO na prázdnom parkovisku majú po 5 s hry rôzne sektory.
  - `AssignSteals`: robot 0 má sektor 7; `AssignAutoPaverSector(1, 7)` → robot 1 má 7, robot 0 `INDEX_NONE` a `Idle`, `FindAutoPaverInSector(7) == 1`.
  - `FewerSectorsThanPavers` (Review Focus 4): všetko hotové okrem sektora 3, traja roboti v AUTO, 60 s hry: práve jeden má sektor 3, ostatní dvaja sú celý čas `Idle` bez sektora; `AssignAutoPaverSector(1, <hotový sektor>)` vráti true a robot ho prejde bez položenia litra.
  - `SharedStock`: dvaja prázdni roboti pri výdajni, zásoba 100 l: po kroku je súčet nádrží ≤ 100 a zásoba ≥ 0.
  - `SpeedForAll`: po kúpe Fast kladú obaja roboti rýchlosťou úrovne 1.
- [ ] **Step 2: Testy** `DoItAsphalt.Lot.Save.*`:
  - `ThreePaversRoundTrip`: traja roboti v rôznych fázach sa po uložení a načítaní zhodujú vo všetkých poliach.
  - `LoadsOlderPaver` (Review Focus 3): blob verzie 5 (postav ho rovnakým spôsobom, akým `LotSaveTest.cpp` stavia blob verzie 4) s robotom vo fáze `Working` v sektore 12, krok 40, nádrž 900 l → načíta sa, robot 0 má presne tento stav, `GetNumAutoPavers() == 1`; to isté pre fázu `Loading`.
  - `RejectsSharedSector`: blob, kde dvaja roboti majú ten istý sektor, sa odmietne a hra ostane nedotknutá.
- [ ] **Step 3:** Implementuj jadro; volania vo vrstve hry prepíš na robota 0 (správanie s jedným robotom sa nemení). Panel budovy Robots má odteraz 6 kariet (pribudli `AutoPaver2` a `AutoPaver3`): uprav počet v teste `Shop.RowsByBuilding` z Task 4 a všade, kde sa počíta s 13 vecami alebo 14 riadkami obchodu.
- [ ] **Step 4:** `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.

---

### Task 7: Aktori robotov, obrysy, zvuk, cheaty **[Sonnet 5.5]**

**Files:**
- Modify: `Source/DoItAsphalt/World/DIAAutoPaver.h`, `.cpp`, `Game/DIAGameMode.h`, `.cpp`, `World/DIAGroundActor.cpp`, `Game/DIAAudio.cpp`, `Game/DIACheats.cpp`
- Test: `Source/DoItAsphalt/Tests/AutoPaverTest.cpp`

**Interfaces – Consumes:** Task 6.

**Interfaces – Produces:**

```cpp
// ADIAAutoPaver
int32 Index = 0;                       // which robot of the core it shows; set before BeginPlay (spawn deferred)
// ADIAGameMode
void EnsureAutoPavers();               // replaces EnsureAutoPaver: exactly one actor per owned robot, none for the others
```

Obrys prideleného sektora na zemi sa kreslí pre každého robota. Zvuk robota hrá na mieste pracujúceho robota, ktorý je najbližšie k hráčovi. `dia.Item` pozná `AutoPaver2` a `AutoPaver3`.

- [ ] **Step 1: Testy** `DoItAsphalt.Game.AutoPaver.*`: `ThreeActors` (po udelení troch robotov sú traja aktori s `Index` 0, 1, 2 na `PaverDock`), `EachFollowsItsOwn` (po pridelení rôznych sektorov ide každý aktor za polohou svojho robota), `OutlinePerPaver` (dva pridelené sektory → dva obrysy), `RemovedOnNewGame`.
- [ ] **Step 2:** Implementuj. `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.

---

### Task 8: Mapa sektorov, `H` a HUD pre troch robotov **[Sonnet 5.5]**

**Files:**
- Modify: `Source/DoItAsphalt/UI/DIAHUD.h`, `.cpp`, `UI/SDIAPaverMap.h`, `.cpp`, `UI/DIAFormat.h`, `.cpp`, `UI/SDIAHudOverlay.cpp`, `Player/DIAPlayerController.cpp`
- Test: `Source/DoItAsphalt/Tests/HudTest.cpp`, `FormatTest.cpp`, `UiTextTest.cpp`, `AutoPaverTest.cpp`

**Interfaces – Consumes:** Task 6, Task 7.

**Interfaces – Produces:**

```cpp
// ADIAHUD
int32 GetSelectedPaver() const;        // always a robot that is owned; 0 when none or after a new or loaded game
void SelectPaver(int32 Paver);         // ignored for a robot that is not owned

// DIAFormat.h
DOITASPHALT_API FText PaverLine(const FLotGame& Game, int32 Paver);    // one robot owned: today's "Paver: ..."; more: "Paver 2: ..."
DOITASPHALT_API FText PaverSentToast(int32 Paver, int32 NumPavers, int32 Sector);   // "Paver → sector 12" / "Paver 2 → sector 12"
struct FDIAHudView { /* ... */ FText Pavers[LotBalance::MaxAutoPavers]; };          // replaces FText Paver
```

- Mapa (`G`): pri dvoch a viac robotoch má prepínač 1 / 2 / 3 (len kúpení). Kliknutie na sektor ho pridelí vybranému robotovi. Rám sektora a značka polohy sa kreslia pre každého robota; vybraný oranžovo ako dnes, ostatní bielo (`AsphaltHud::SignWhite`), pri značke číslo robota.
- `H` pošle vybraného robota do sektora pod hráčom; doterajšie oznamy ostávajú, „already here“ sa pýta na vybraného.
- HUD ukazuje riadok pre každého kúpeného robota pod sebou.

- [ ] **Step 1: Testy:**
  - `DoItAsphalt.Game.Format.PaverLinePerRobot`: s jedným robotom je text rovnaký ako dnes; s dvoma začínajú riadky „Paver 1:“ a „Paver 2:“.
  - `DoItAsphalt.Game.Hud.PaverLines`: počet neprázdnych riadkov = počet kúpených robotov.
  - `DoItAsphalt.Game.PaverMap.SelectAndAssign`: výber robota 2 a kliknutie na sektor pridelí sektor robotovi 1 (index 1), robot 0 ostane, ako bol.
  - `DoItAsphalt.Game.PaverMap.SelectionClamps` (Review Focus 5): `SelectPaver(2)` s jedným robotom nič nezmení; po `NewGame()` a po načítaní hry s jedným robotom je výber 0; `H` s výberom mimo kúpených nepošle nikoho.
  - `DoItAsphalt.Game.AutoPaver.SendSelected`: `H` s vybraným robotom 2 zmení sektor len jemu.
- [ ] **Step 2:** Implementuj. `Scripts/build.sh`, `Scripts/test.sh` – `FAILED 0`.
- [ ] **Step 3:** `Scripts/shot.sh`: snímka mapy s tromi robotmi a HUD s tromi riadkami.

---

### Task 9: Dokumentácia a záverečné overenie **[Sonnet 5.5]**, snímky posúdi **[Fable 5.2]**

**Files:**
- Modify: `docs/01-herny-dizajn.md` (kap. 4.6 roboti, kap. 5 ovládanie bez `Tab`, kap. 6 panel, kap. 7 obchod: tri budovy a čo ktorá predáva, kap. 8 formát uloženia), `docs/02-technicky-navrh.md` (aktor budovy, bod nákupu, pole robotov), `docs/03-3d-modely.md` (4.5: odsek „Čo ešte nie je zapojené“ nahradiť tým, kde budovy stoja), `README.md`, ak spomína `Tab` alebo jedného robota

- [ ] **Step 1:** Prepíš dotknuté časti tak, aby popisovali nový stav (nie históriu zmeny); čísla ber z kódu.
- [ ] **Step 2:** `Scripts/build.sh`, celý `Scripts/test.sh` – `FAILED 0`, `PASSED` ≥ základ + nové testy.
- [ ] **Step 3:** Prechod hrou cez `Scripts/shot.sh` po `DevStart`: kúpa v každej budove, auto vyjde z garáže cez výdajňu na parkovisko, traja roboti pracujú v troch sektoroch. Snímky a čo sa nepodarilo overiť uveď v hlásení.
