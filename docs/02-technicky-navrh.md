# Do it Asphalt – technické poznámky

Verzia 2.1 · 2026-11-24 · popisuje kód tak, ako je v repozitári (po druhej vlne nákupných budov)

Tento dokument vysvetľuje, ako je hra Asphalt-self postavená, pre čitateľa, ktorý kód nevidel. Pravidlá hry a ovládanie sú v `01-herny-dizajn.md`, modely v `03-3d-modely.md`.

---

## 1. Prostredie

| | |
|---|---|
| Engine | Unreal Engine 5.8.4, C++ |
| Systém | macOS (Apple Silicon) |
| Skripty | `Scripts/` (kapitola 9) |
| Modely | skripty pre Blender 5.3, výstup GLB, import skriptom v editore |
| Materiály | vytvára `Tools/UE/create_materials.py` |

## 2. Dva moduly

**`AsphaltCore`** (`Source/AsphaltCore`) je pravidlá hry bez Unrealu. Závisí len od modulu `Core`, nepoužíva žiadny `UObject` a všetko sa dá testovať s explicitným časovým krokom `Dt`. Súbory:

| Súbor | Obsah |
|---|---|
| `LotMap.h/.cpp` | geometria L-tvaru, indexy políčok, sektory, vstupné úseky |
| `LotGrid.h/.cpp` | `FLotGrid`: mriežka pokrytia s tromi stavmi |
| `LotBalance.h` | všetky čísla hry (ceny, kapacity, rýchlosti, výrobňa, úrovne vylepšení robotov) |
| `LotLayout.h/.cpp` | rozloženie sveta: polohy výrobne, koryta, štartu, centra, východu, troch nákupných budov (`ShopPos`, `ShopYawDeg`, `ShopPoint`), státí robotov (`PaverDock`) |
| `LotGame.h/.cpp` | `FLotGame`: celé pravidlá, obchod, odomykanie, ukladanie |
| `LotSaveIO.h` | čítanie a zápis bajtov uloženia |
| `Tests/` | automatické testy (`DIA_TEST`) |

**`DoItAsphalt`** (`Source/DoItAsphalt`) je tenká vrstva nad enginom: zobrazuje stav, číta vstup a odovzdáva ho jadru. Podadresáre:

| Adresár | Obsah |
|---|---|
| `Player/` | postava v prvej osobe, vstup (`DIAInput`), ovládač |
| `World/` | herné objekty: zem (`ADIAGroundActor`), predmety (`DIAMovable`), auto (`ADIAPaverCar`), roboti (`ADIAAutoPaver`), výrobňa, nákupné budovy (`ADIAShopBuilding`) a ich body nákupu (`ADIAShopPoint`), východ, tabule, centrum, okraj parkoviska, `DIAToolPath.h` |
| `Game/` | `UDIAWorldSubsystem`, ukladanie (`UDIASaveGame`), herný režim, cheaty, načítanie assetov |
| `UI/` | HUD, obchod, menu, obrazovka výhry, formátovanie textov (`DIAFormat`), legenda ovládania (`DIAControlsLegend`) |
| `Tests/` | testy, ktoré potrebujú svet enginu |

Modul `DoItAsphalt` sa kompiluje s `bUseUnity = false`. Každý testovací súbor má vo vlastnom anonymnom priestore mien pomocné funkcie s krátkymi názvami (napríklad `Move`, `TickFor`, `MiddleOfLot`) a pri unity buildoch sa dva takéto názvy zlúčili do jednej jednotky a kolidovali; preto sa každý súbor kompiluje samostatne.

## 3. Mriežka pokrytia

`FLotGrid` je štvorec **4 000 × 4 000** texelov. Jeden texel má 5 cm, políčko 1 × 1 m je teda 20 × 20 = 400 texelov. Každý texel je v jednom z dvoch stavov (`ELotTexel`): voľný (0) alebo hotový (1). Surový asfalt ani hladenie hra nemá.

- Jeden texel nesie 0,025 l asfaltu; 95 % políčka je 380 texelov.
- Operácie: `Stamp` (razenie pásu), `PourDisc` (kruh z nádoby), `DepositIntoCell` (vsypanie do políčka), `CompleteCell` (dokončenie).
- Vyliaty asfalt je rovno hotový. Políčko je dokončené pri 380 hotových texeloch (95 %); zvyšok sa dolepí zadarmo. Lialo sa môže len na voľné texely.
- Každý sektor (75 sektorov po 20 × 20 políčok) má vlastný **špinavý obdĺžnik**: najmenší obdĺžnik, ktorý sa zmenil od posledného odovzdania zmien. `ConsumeDirtyRects` vydá najviac jeden obdĺžnik na sektor.

Indexovanie políčok: riadky 0 – 99 majú 200 stĺpcov, index = riadok · 200 + stĺpec; od riadku 100 má riadok 100 stĺpcov (kvadrant s centrom chýba), index = 20 000 + (riadok − 100) · 100 + stĺpec. Spolu 30 000 políčok. Sektory sa indexujú rovnako po 20 × 20 políčok (prvých 5 radov po 10 sektoroch, ďalších po 5).

### 3.1 Tekutina (`FLotFluid`)

Pri zapnutej tekutine (`FLotGrid::SetFluidEnabled`; zapína ju `UDIAWorldSubsystem::Initialize`, jadro ju má štandardne vypnutú, aby fungovali staré testy) `ApplyCandidates` zaplatí texely, ale zem nezmení: z ich objemu položí `FLotFluid` niekoľko kôp (4 – 8 jednotiek na kopu, jednotka = film jedného texelu). Jeden krok (`FluidStepSeconds` = 1/30 s) beží v štyroch Jacobiho prechodoch nad 32 × 32 dlaždicami, ktoré existujú len tam, kde je asfalt a o jednu dlaždicu ďalej:

1. toky cez steny medzi texelmi (virtuálne rúry: `zotrvačnosť · starý tok + tuhosť · teplota² · rozdiel výšok nad filmom`),
2. podiel výtoku, ktorý texel naozaj má (nikdy viac, než drží),
3. skutočné toky,
4. nová hrúbka, teplota (nesie ju tok) a druh (väčší podiel vyhrá), potom chladnutie, pokrytie (`OnSet` pre voľný texel s ≥ 0,5 jednotky), zatuhnutie a uvoľnenie prázdnych dlaždíc.

Steny sú okraje mriežky a kvadrant nákupného centra. Výpočet je deterministický (žiadna náhoda). Stabilitu drží zotrvačnosť < 1 a tuhosť 0,2 (λ·k = 1,6 < 3,7). Ručné nástroje liejú tekutinou, stroje (`PourFromCar`, robot, vozík) cez `FInstantPour` hneď.

`FLotGame::Tick` krokuje tekutinu pevným krokom (najviac 6 krokov na `Tick`), pokrytie účtuje cez `ApplyPour`, takže odmena za políčko, odomykanie a udalosti idú tou istou cestou. `SettleFluid` necháva asfalt zatuhnúť naraz; ukladanie ho volá na **kópii** hry (`MakeSaveBlob`), takže uložená hra drží len zem a bežiaca hra pokračuje. `LoadFromBytes` zachová nastavenie tekutiny.

## 4. Zem a textúra pokrytia

`ADIAGroundActor` zobrazuje mriežku ako textúru `PF_R8G8`: kanál R je 255, ak texel nie je voľný, kanál G je 255, ak je hotový (pri dvoch stavoch sú teda obe rovnaké). Materiál `M_Ground` z nich poskladá farbu zeme.

- Na CPU sa drží kópia textúry (kvôli behu `-nullrhi` v testoch) a do GPU sa posielajú len zmenené obdĺžniky.
- Celé nahratie (16 miliónov texelov) prebehne raz pri načítaní uloženia alebo novej hre. Trvalo asi **25 ms** (25,3 a 24,3 ms v dvoch zalogovaných behoch na tomto stroji; nie je to presné meranie).
- Obrys sektora prvého robota je parameter `SectorRect` materiálu `M_Ground`: oranžový (0,6; 0,1; 0,0), široký 10 cm. Materiál má len jeden takýto parameter, preto obrysy robotov 2 a 3 kreslí `ADIAGroundActor` ako štyri tenké oranžové pásy (kocky na zemi, šírka `OutlineWidthCm` = 10 cm) na robota; `SyncSectorRect` ich nastaví pri každej zmene sektora a `GetPaverOutline` / `GetOutlineStrip` ich prezradia testom.

## 5. Jedno miesto pre jadro: `UDIAWorldSubsystem`

`UDIAWorldSubsystem` vlastní jedinú inštanciu `FLotGame` a rozosiela všetky udalosti. Nikto iný jadro nevytvára. Po každom kroku hry zavolá `Dispatch`, ktorý spustí `OnCellsCompleted`, `OnEntryCompleted`, `OnUnlocked`, `OnExitOpened` a vždy ako posledné `OnStateChanged`. Ďalšie delegáty: `OnItemBought`, `OnWon`, `OnGameReset`. Čas jedného kroku sa orezáva na 1 s (`MaxTickSeconds`), aby dlhý zásek nepreskočil hru.

Všetko, čo sa týka kreditov, litrov a udalostí, sa účtuje na **jednom mieste**: `FLotGame::ApplyPour`. Každé liatie (ruka, vozíček, vozík, robot, auto) prejde touto funkciou, ktorá vráti `FLotEvents` (vyliate litre, získané kredity, dokončené políčka a vstupné úseky, odomknutia, otvorený východ). Odomknutie sa počíta zo zeme: krok i je odomknutý, ak je hotový vstupný úsek i a zároveň je hotových `UnlockCells[i]` políčok.

`FDIAToolPath` (`World/DIAToolPath.h`) je spoločná pomôcka vozíka a auta: z posunutia nástroja urobí dráhu a zaručí, že sa nič nepoloží cez skok, teda cez miesto, ktoré nástroj nepreišiel. Používa `JumpSlack` 1,5 a `MoveToleranceCm` 0,01.

## 6. Ukladanie (formát „DIA2“)

Uloženie má dve vrstvy.

**Jadro** (`FLotGame::SaveToBytes` / `LoadFromBytes`) zapíše bajty v tomto poradí:

1. hlavička: magické číslo `0x32414944` („DIA2“), verzia formátu (`SaveVersion` = 7; verzie 4, 5 a 6 sa ešte načítajú: verzia 6 má 15 príznakov kúpených vecí a tri roboty bez úrovní vylepšení, verzia 5 má 13 príznakov a jedného robota, verzia 4 má 11 príznakov, starý `CarWideBar` je dnes `CarT2` a `CarT3`, `CarT4` sa nevlastnia; robot starších verzií sa stane robotom 1, ďalší nie sú kúpení), počet políčok (30 000) a veľkosť mriežky (4 000);
2. kredity, čas hry, vyliate litre, úroveň výrobne a jej zásoba;
3. kúpené veci (jeden príznak na `ELotItem`, vo verzii 7 ich je 13; globálne `AutoPaverFast` a `AutoPaverFaster` z enumu zmizli, preto loader mapuje príznaky starších verzií na nové veci podľa mena a nie podľa indexu), ruka a jej obsah, zostávajúci čas výmeny, litre vo vozíčku a vo vozíku, čo je v ruke chytené;
4. stav robotov: blok pre každého z troch; vo verzii 7 začína dvoma `i32` (úroveň rýchlosti a úroveň kapacity), potom sektor, krok, poloha, fáza, rozpočet, nádrž (vo verziách 4 a 5 je len jeden blok, bez úrovní);
5. nádrž auta a príznak výhry;
6. poloha a natočenie hráča, polohy a natočenie predmetov, poloha a natočenie auta, príznak, že hráč sedí v aute, a prepínač AUTO robotov (jeden pre všetkých);
7. zem: bit na každé políčko (hotové), počet rozpracovaných políčok a pre každé jeho index a 400 texelov po 1 bite (4 + 50 bajtov).

`LoadFromBytes` všetko najprv načíta do dočasnej hry, skontroluje a až po úspechu ju vymení za skutočnú, takže poškodený súbor hru nepokazí. Kontroly verzie 7 navyše: úrovne sú 0 až `MaxPaverUpgradeLevel` (2), nevlastnený robot má obe úrovne 0 a nádrž robota nie je väčšia než kapacita jeho úrovne kapacity (plus tolerancia); telo vo formáte 7 s menšou verziou v hlavičke sa odmietne. **Prechod zo starších uložení:** staré globálne príznaky `AutoPaverFast` a `AutoPaverFaster` sa zmenia na úroveň rýchlosti 1, resp. 2 **každého vlastneného robota** (rýchlosť bola spoločná, takže hra ostane taká rýchla, aká bola); úroveň kapacity starých uložení je 0. Príznaky „drží sa“ sa pri načítaní odvodzujú. Uloženie z verzie 1 (so surovým asfaltom, 2 bity na texel) sa odmietne, rovnako ako staršie magické číslo `0x31414944` (test `Lot.Save.RejectsV1`); po zmene treba začať novú hru.

**Engine vrstva** (`UDIASaveGame`) uloží bajty jadra ako pole `Blob` do slotu `DoItAsphalt_Slot0` a zároveň do zálohy `DoItAsphalt_Slot0_bak`. Na konci bajtov je pätka s magickým číslom „DIAE“ a kontrolným súčtom CRC32; kontrolný súčet sa overí pred čítaním enginom. Poškodený slot sa nahradí zálohou, ak sú poškodené obe, hra začne odznova. Parameter `-DIASlot=<názov>` zmení slot (skript `shot.sh` používa `DIA_Shot`).

- Automatické uloženie: každých 30 s hry (`AutosaveIntervalSeconds`), po každom nákupe (`BuyItem`, `BuyPlant`), po výhre a pri `GameMode::EndPlay`. Zapína sa až po odchode z hlavného menu a pod automatickými testami sa pre skutočný slot preskakuje.
- `MakeSaveBlob` ukladá držaný predmet položený na kópii hry: na pozícii jeho aktéra len ak `CanBePutDownHere` prejde, inak na poslednom platnom mieste, ktoré jadro pozná. Poloha auta sa zapisuje vždy a poloha hráča aj počas jazdy, takže hra uložená v aute sa načíta v aute.

## 7. Vstup, ruky a stroje

- Vstup sa definuje v kóde (`DIAInput.cpp`), nie v assetoch. Ovládač (`DIAPlayerController.cpp`) prekladá vstup na volania jadra.
- **Ruka** je v jadre stav: miska, vedro, kanister alebo vaňa (`ELotHand`), s kapacitou a hladinou; lyžica len plní misku. Ruka lieva tekutinou (`FLotFluid`), hladidlo hra nemá.
- **Predmety** (vozíček, vozík, výdajňa) sú aktéri `ADIAMovable`. Jadro pozná ich polohu a či sú držané; držanie sa určuje v jadre, nie v aktérovi.
- **Auto** (`ADIAPaverCar`) je kinematický pawn so štyrmi stupňami (`LotBalance::NumCarLevels`; tabuľky `CarTankL` 6 000 / 8 000 / 12 000 / 18 000 l, `CarRateCellsPerMin` 260 / 300 / 380 / 480, `CarWidthCm` 300 / 360 / 420 / 500 a `CarBarBehindCm` 230 / 280 / 340 / 410; stupeň je `FLotGame::GetCarLevel()` 0 až 3, najvyšší kúpený). Aktér ukazuje model a obálku aktuálneho stupňa (`DIAModelSizes::PaverCarCm[]`, `PaverCarBodyCm[]`; `SM_PaverCar`, `_T2` až `_T4`) a pri zmene stupňa (kúpa, načítanie, nová hra) vymení mesh, rozmer kolíznej škatule, výšku nad zemou (počiatok je stred škatule, kolesá ostávajú na zemi), rameno a pivot kamery, vzdialenosť lišty (`GetBarPoint`, `Pour`) aj bočnú vzdialenosť východu; ak sa väčšia škatuľa prekrýva, auto sa presunie na `LotLayout::ItemSpawn(Car)` a jadro dostane novú polohu. Auto má nádrž, ktorú plní len v dosahu výdajne (`FLotGame::LoadCar`, 500 l/s zo zásobníka výrobne, polomer `VehicleStationRadiusCm`); kamera je tretia osoba (`USpringArmComponent`). `PourFromCar` zaplatí z nádrže a `Stamp`-om položí hotový asfalt pod lištou. Rýchlosť auta pri liatí obmedzuje `CarPaveSpeedCmS = sadzba · 10 000 / (60 · šírka)`.
- **Roboti** (`ADIAAutoPaver`) sú v jadre pole `FLotPaver Pavers[MaxAutoPavers]` (`MaxAutoPavers` = 3): každý má sektor, krok, polohu, fázu, rozpočet a nádrž a všetky funkcie (`AssignAutoPaverSector`, `GetAutoPaverSector`, `GetAutoPaverPhase` …) berú číslo robota 0 až 2. Robot n je kúpený, keď je kúpená `AutoPaverItem[n]` (`AutoPaver`, `AutoPaver2`, `AutoPaver3`; kupujú sa v poradí, `GetNumAutoPavers`). Sektor dostane povelom `AssignAutoPaverSector` (reštartuje postup; sektor má najviac jedného robota, takže ho robot, ktorý ho mal, stratí; `FindAutoPaverInSector`) z mapy `SDIAPaverMap` (`M`) alebo klávesom `H`; robota, ktorému povel patrí, určuje `ADIAHUD::GetSelectedPaver` (nastavuje ho prepínač na mape, `SelectPaver`; po štarte je to robot 1). V režime AUTO (jeden prepínač pre všetkých) si robot bez sektora vyberie najbližší sektor s voľným políčkom, ktorý nemá iný robot. Jadro vedie každého fázami `ELotPaverPhase` (`Idle`, `ToSector`, `Working`, `ToStation`, `Loading`); nádrž dopĺňa v najbližšej výdajni (`GetNumStations`, `FindNearestStation`), pri jednej výdajni aj viacerí naraz. **Vylepšenia sú po robotoch:** `FLotPaver::Upgrade[2]` drží úroveň rýchlosti a kapacity (`ELotPaverUpgrade { Speed, Capacity }`, `MaxPaverUpgradeLevel` = 2, teda úrovne 0 až 2). Tabuľky v `LotBalance.h`: `AutoPaverRateCellsPerMin[3]` = 120 / 150 / 180 (podľa úrovne rýchlosti), `AutoPaverTankLByLevel[3]` = 2 000 / 3 000 / 4 500 l (podľa úrovne kapacity) a `PaverUpgradePrice[2][2]` = {20 000, 60 000} pre rýchlosť, {5 000, 15 000} pre kapacitu (riadok je druh, stĺpec úroveň, z ktorej sa kupuje); static_assert viaže veľkosti polí na `MaxPaverUpgradeLevel` a `ELotPaverUpgrade::Count`. API `FLotGame`: `GetPaverUpgradeLevel(Paver, Upgrade)` (−1, ak robot nie je kúpený), `GetPaverUpgradePrice` (cena ďalšej úrovne, −1 na maxime alebo bez robota), `CanBuyPaverUpgrade`, `BuyPaverUpgrade` (kúpená rýchlosť platí hneď, kúpená kapacita zväčší nádrž, ale nenaplní ju), `GetAutoPaverRateCellsPerMin(Paver)` a `GetAutoPaverTankCapacity(Paver)` (0 bez robota) a `DebugSetPaverUpgrade` (len pre vlastnených robotov, zmenšená kapacita skráti nádrž). `TickAutoPaver`, `DebugFillTanks` aj kontrola nádrže pri načítaní používajú hodnoty daného robota; static_assert o strope rozpočtu platí pre najvyššiu rýchlosť. Staré bezparametrové `GetAutoPaverLevel()`, `GetAutoPaverRateCellsPerMin()` a `GetAutoPaverTankCapacity()` zmizli. `SnapToCore` postaví robota po načítaní na uložené miesto. `ADIAGameMode::EnsureAutoPavers` drží jedného aktéra na každého kúpeného robota; nový robot sa objaví v státí `LotLayout::PaverDock(n)`. Výdajne sú dve: pevná (`ADIAVehicleStation` na `LotLayout::VehicleStationPos`) a kúpená presuvná Station.
- **Nákupné budovy.** `ELotShop` (`Tools`, `Robots`, `Garage`) a tabuľka `LotBalance::ItemShop` (plus `PlantShop` pre výrobňu) priraďujú každú vec práve jednej budove; priradenie je v jadre, aby ho videli testy aj panel. `LotLayout` drží polohu a otočenie budovy (`ShopPos`, `ShopYawDeg`), prevod bodu z osí modelu do sveta (`ShopLocalToWorld`), bod nákupu (`ShopPoint`) a státia robotov (`PaverDock`); kúpené auto sa objaví na počiatku garáže (`ItemSpawn(Car)`, otočenie `CarSpawnYawDeg`); os státia garáže je os jazdnej dráhy pevnej výdajne (`ShopPos(Garage).Y` sa berie z `VehicleStationPos.Y`), aby auto vyšlo rovno cez bránku výdajne; roboty 2 a 3 majú pri nákupe východisko v státí 1 a 2 (`ItemSpawn`). Budovu v hre stavia `ADIAShopBuilding` (jeden aktér pre všetky tri; druh je parameter): mesh, kolíziu z kvádrov importu a text na tabuli (TOOLS, ROBOTS, GARAGE). Bod nákupu je `ADIAShopPoint` bez kolízie vo výške stredu kapsuly hráča: `E` pri ňom zavolá `ADIAHUD::OpenShop(Shop)`. Panel `SDIAShopPanel` je jeden pre všetky budovy a ukáže riadky modelu `FDIAShopModel::RowsOf(Shop)`; `GetOpenShop` vráti otvorenú budovu. Model má `NumRows` = 1 + `ELotItem::Count` + `MaxAutoPavers` · 2 = 20 riadkov: riadok výrobne, riadky predmetov a na konci šesť riadkov vylepšení robotov (`FirstUpgradeRow`, `UpgradeRow(Paver, Upgrade)`); riadok vylepšenia má `bPaverUpgrade`, `Paver`, `Upgrade` a `Shop = Robots`, takže `RowsOf(Robots)` vráti 10 riadkov a kód, ktorý predpokladá „každý riadok Robots je predmet", musí `bPaverUpgrade` skontrolovať. `FDIAShopTarget` nesie aj `bPaverUpgrade`, `Paver`, `Upgrade` a očakávanú úroveň (`UpgradeLevel`, ako `PlantLevel`), aby zastaraný klik nekúpil nesprávnu úroveň; `Buy` ide cez `UDIAWorldSubsystem::BuyPaverUpgrade(Paver, Upgrade)` (zrkadlí `BuyItem`: vysiela `OnPaverUpgradeBought` a `OnStateChanged`, ukladá; oznam ukazuje HUD, panel vlastný nepridáva). Kľúč modelu obsahuje aj úrovne všetkých robotov, takže zmena úrovne prestaví riadky. **Rozloženie Robots:** Tools a Garage ostávajú v rovnomernej mriežke (`SUniformGridPanel`), pre Robots stavia `BuildRobotsLayout` štyri stĺpce rovnakej šírky (`SHorizontalBox`): stĺpec 0 má kartu Portable Station, stĺpce 1 až 3 kartu robota a pod ňou kartu Speed a Capacity vo fixne veľkom bloku (`Visibility` Hidden, nie Collapsed, kým robot nie je kúpený, aby sa panel nehýbal; v rezervovanom mieste stojí tlmený text „Upgrades open once the robot is bought"). Výšky kariet sú konštanty v `AsphaltHud` (`DIAHudStyle.h`): `ShopRobotCardHeight` 200, `ShopUpgradeCardHeight` 156, `ShopUpgradeHintFont` 16. Pre testy má panel prístupové funkcie (`IsRobotsLayout`, `FindUpgradeCard`, `FindUpgradeHint`, `FindBuyButton`) a `SDIAShopButton::SimulateClick()`, lebo headless test nemá myš ani zachytenie. Ovládač (`CloseShopWhenAway`) panel zavrie, keď je hráč ďalej než `DIAShopPoint::CloseRangeCm` (300 cm) od bodu nákupu, a `E` pri otvorenom paneli ho zavrie skôr, než sa hľadá iný predmet; pri nástupe do auta a pri novej hre sa zavrie tiež. Rozmery budov sú v `DIAModelSizes.h`.
- **Mapa robotov na `M`.** `Input.PaverMap` je v `DIAInput.cpp` namapovaná na `EKeys::M` (predtým `G`, ktoré dnes nie je viazané na nič); `M` nepoužíva nič iné (test `Game.Input.PaverMapOnM`). Texty „Paver: waiting – press M for the map", oznam „Auto Paver bought – it paves on its own (M = map)" a pätu mapy „M / Esc close" (`DIAPaverMap::LegendText(NumPavers)`) drží kód.
- **Legenda ovládania** (`UI/DIAControlsLegend.h/.cpp`) je **jedna tabuľka**: riadok má klávesové čiapky a text akcie, čiapka nesie kľúče, ktoré zastupuje, ako dvojice `(FKey, člen FDIAInput)`, takže ju test porovná so skutočným kontextom mapovania. Dva varianty: `OnFoot()` (12 riadkov: W A S D move, Shift sprint, Mouse look, LMB scoop / pour, RMB work view, Q a 1-4 next / pick tool, R fill, E use / shop, H send robot, M paver map, V check view, Esc pause) a `InCar()` (W S throttle, A D steer, Mouse look, LMB pour, E get out, M paver map, Esc pause; v aute totiž nerobia nič ruka, `R`, `RMB`, `V`, `H` a šprint, ale `M` mapu otvorí). Widget `SDIAHudOverlay::BuildControlsLegend()` stavia obe verzie v ľavom dolnom rohu (čierna tabuľa s žltou linkou cez `AsphaltHud::MakePlate`, konštanty `Legend*` v `DIAHudStyle.h`); ktorá platí, rozhoduje `World().bPlayerInCar`, druhá je Collapsed. Viditeľnosť určuje `ADIAHUD::IsControlsLegendShown()` (nie je menu, výhra, obchod ani mapa robotov), čo platí aj bez overlayu v testoch. `F1` ani iný vypínač neexistuje. Testy v `Tests/ControlsLegendTest.cpp`: `Game.Hud.ControlsLegend.Bindings` (každý uvedený kláves je naviazaný na uvedenú akciu a každé mapovanie kontextu je v tabuľke, takže zabudnúť nový kláves sa nedá), `.Visibility`, `Game.Input.PaverMapOnM`, `Game.Hud.PaverMapKeyText`.
- **Zvuk** (`UDIAAudioSubsystem`) vzniká len v hre, ktorá sa dá počuť (nie v testoch). Slučky majú fade a 3D slučky sedia na výrobni, robote a výdajni. Hlasitosť určuje `DIASettings` (Master cez `FApp::SetVolumeMultiplier`, Effects a Ambience v subsystéme).
- **Nastavenia** (`DIASettings`) sú mimo uloženej hry v `GameUserSettings.ini`; menu ich mení hneď a ukladá pri odchode.

## 8. Testovanie

Testy sa deklarujú makrom `DIA_TEST` a bežia cez `Scripts/test.sh`. Posledný plný beh na tomto stroji (2026-11-24, po vylepšeniach jednotlivých robotov, mape na M a legende ovládania): **317 testov, všetky prešli**.

| Skupina | Počet |
|---|---|
| `DoItAsphalt.Lot.*` (jadro) | 124 |
| `DoItAsphalt.Game.*` (engine) | 192 |
| `DoItAsphalt.Core.Smoke` | 1 |

Najväčšie podskupiny `Lot`: AutoPaver 22, Save 16, Fluid 14, Hand 10, Movable 8, Stamp 6, Car 6, Shop 4. Najväčšie podskupiny `Game`: Movable 25, SaveSlot 17, Car 15, Shop 14, Character 14, Ui 12, AutoPaver 12, Shops 8, Hud 8, Interaction 6, Exit 6.

`DoItAsphalt.Lot.BalanceSim` je simulácia celej hry vopred daným správaním (bez človeka). Kontroluje, že prvé políčko padne za 40 – 80 s, 10 políčok do 6 minút, 100 do 18, 800 do 42, 6 000 do 78 minút a že celá hra trvá 105 – 165 minút. Simulovaný hráč kupuje vždy najlacnejšiu vec alebo vylepšenie (aj roboty 2 a 3 a všetky vylepšenia robotov; `ItemSeconds` zapisuje čas každej kúpy) a test hlási, že každý robot dosiahol úroveň 2 rýchlosti aj kapacity, že robot 2 je kúpený aspoň 15 minút po robotovi 1, robot 3 aspoň 15 minút po robotovi 2 a že robot 3 pracuje aspoň 10 minút pred východom (len `CarT4` ostane nekúpený). V jednom behu vyšlo 23:04 pre 800 políčok, 52:34 pre 6 000 políčok a 1:48:24 pre celú hru, len asi 3 minúty nad dolným okrajom okna (kapitola 9 herného dizajnu). Druhá polovica je riadená kreditmi, takže každá zmena ceny posunie východ približne o 10 s na 1 000 kreditov.

Skripty overujú aj obraz: `Scripts/shot.sh <názov> "<cheaty>"` spustí hru v okne, vykoná cheaty a uloží snímku.

## 9. Skripty a nástroje

| Príkaz | Čo robí |
|---|---|
| `Scripts/build.sh` | skompiluje `DoItAsphaltEditor` |
| `Scripts/test.sh [filter]` | spustí automatické testy |
| `Scripts/shot.sh [--menu] <názov> ["cheaty"]` | snímka hry do `Saved/Screenshots/<názov>.png` |
| `Scripts/uepy.sh <skript.py>` | spustí skript pre editor bez okna |
| `Scripts/package.sh` | zabalí Shipping `.app` (v iCloude zlyhá na `codesign`) |
| `Tools/ModelGen/build_all.sh` | vygeneruje a skontroluje všetky modely |

Poznámka k `Tools/UE/create_materials.py`: pri spustení cez `uepy.sh` editor spadne na tvrdení `!IsRooted()` už pri druhom materiáli (`M_DIA_Flat`), pretože `MaterialEditingLibrary.DeleteAllMaterialExpressions` (aj `DeleteMaterialExpression` samotné) zlyhá na načítanom materiáli. Skript vypíše `DIA_OK` len pre `M_Ground`. Materiály sú hotové v `Content/Materials/` a skript netreba spúšťať znova. Zmenu materiálu, ktorú skript vytvára, treba robiť v editore alebo skript opraviť; obídenie (zmazať materiál a vytvoriť ho odznova) sa nevyskúšalo. Po každom behu ostane proces `CrashReportClient`, ktorý treba ukončiť, a prepísaný `Content/Materials/M_Ground.uasset` treba vrátiť z gitu.

## 10. Shipping build

Shipping sa skompiluje príkazom `Build.sh DoItAsphalt Mac Shipping -project=<cesta>/DoItAsphalt.uproject` (pri ceste s medzerami v úvodzovkách a zo `bash`, nie zo `zsh`, lebo `Scripts/env.sh` vyžaduje `bash`). Cheaty `dia.*` (`DIACheats.cpp`) sú pod `#if !UE_BUILD_SHIPPING`, takže v Shipping binárke nie sú. Reťazce `TEXT()` sú na Macu v UTF-16, preto sa pri kontrole musí hľadať v UTF-16 aj v ASCII; kontrolu treba overiť na editorovej knižnici `libUnrealEditor-DoItAsphalt.dylib`, kde ich nájde.

## 11. Cheaty

Zoznam 28 príkazov (od druhej vlny aj `dia.PaverUpgrade`, bez `AutoPaverFast` a `AutoPaverFaster` v `dia.Item`) je v kapitole 11 dokumentu `01-herny-dizajn.md`. Implementácia je v `Source/DoItAsphalt/Game/DIACheats.cpp`.

## 12. Zmeny oproti návrhu

Pozri kapitolu 10 dokumentu `01-herny-dizajn.md`. Navyše z technickej strany:

- Návrh počítal s mriežkou `FCoverageGrid`; hra používa `FLotGrid` (dva stavy: voľný, hotový).
- Robot v návrhu nemal obrys sektora; obrys (robot 1) je parameter materiálu zeme.
- `ContinueGame` v `ADIAHUD` je verejná, aby cheat `dia.Load` použil rovnakú cestu ako tlačidlo Continue.
