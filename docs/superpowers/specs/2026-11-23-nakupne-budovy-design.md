# Nákupné budovy a traja roboti – návrh

2026-11-23 · stav: čaká na schválenie · kód píše Sonnet 5.5, modely sú hotové (Fable 5.2)

## 1. Zámer

Dnes sa všetko kupuje v jednom paneli na klávese `Tab`, odkiaľkoľvek. Používateľ chce, aby nákup mal miesto vo svete: tri malé budovy pri výrobni, každá pre jednu skupinu vecí. K tomu chce, aby sa dali kúpiť ďalší roboti (dnes je v hre jeden).

Modely budov `SM_ToolShop`, `SM_PaverDepot` a `SM_Garage` sú hotové a importované aj s kolíziou z kvádrov. Ich rozmery, počiatky a body pre kód sú v `docs/03-3d-modely.md`, časť 4.5. Tento dokument popisuje, čo má pribudnúť v kóde hry.

## 2. Čo rozhodol používateľ

| Otázka | Rozhodnutie |
|---|---|
| Kde budovy stoja | na južnom pruhu pri výrobni a pevnej výdajni |
| Čo s obchodom na `Tab` | zrušiť, kupuje sa len v budovách |
| Viac robotov | hneď, najviac traja |
| Garáž | vjazdná, auto do nej vojde |
| Rozdelenie | 1. budova: ručné veci (miska, vedro …); 2. budova: robot, jeho vylepšenia a ďalší roboti; 3. budova: autá |

Všetko ostatné v tomto dokumente je môj návrh. Kde som musel vybrať sám, je to označené **(N)**.

## 3. Ciele, mimo rozsahu, čomu sa vyhnúť

**Ciele**

- Tri budovy stoja vo svete, majú kolíziu, názov na tabuli a dá sa pri nich kupovať klávesom `E`.
- Každá budova ponúka len svoju skupinu vecí; spolu pokrývajú všetko, čo sa dnes dá kúpiť.
- `Tab` už obchod neotvára.
- Dajú sa kúpiť až traja roboti, ktorí pracujú naraz a nelezú si do sektorov.
- Autom sa dá vojsť do garáže a kúpené auto sa v nej objaví.
- Uložená hra z dnešného formátu sa načíta.

**Mimo rozsahu**

- Roboti sa po práci nevracajú do státí; státia sú len miesto, kde sa po kúpe objavia.
- Žiadne animácie budov (roleta, dvere, markíza sa nehýbu) a žiadny interiér kontajnera.
- Ceny a pravidlá odomykania existujúcich vecí sa nemenia.
- Roboti sa navzájom ani s budovami nezrážajú (jadro ich vedie po priamke ako dnes). Platí to aj pre výjazd zo státia: robot môže viditeľne prejsť cez stenu stanice alebo cez výrobňu. Je to prijaté.

**Čomu sa vyhnúť**

- Hráč nesmie ostať bez možnosti kúpiť niečo, čo dnes kúpiť vie.
- Kolízia budov nesmie zavrieť vjazd do garáže ani prístup k pultu.
- Zmena nesmie rozbiť uložené hry.

## 4. Umiestnenie

Južný pruh je pás zeme široký 12 m pod parkoviskom (Y −11 200 … −10 000 cm), na vonkajšom okraji má plot. Stojí na ňom výrobňa (`PlantPos` −50; −10 420) a pevná výdajňa (`VehicleStationPos` −1 500; −10 450).

```
            P A R K O V I S K O
 ========================================================= okraj (Y = -10 000)
  [GARAZ]>    [VYDAJNA]   volne   [VYROBNA]  [NARADIE]  [ROBOTY]
 --------------------------------------------------------- plot  (Y = -11 200)
   X = -3000    -1500               -50         900        1800
```

| Budova | Poloha (cm) | Otočenie | Kam mieri predná strana |
|---|---|---|---|
| `SM_ToolShop` | (900; −10 500) | 90° | k parkovisku (+Y) |
| `SM_PaverDepot` | (1 800; −10 550) | 90° | k parkovisku (+Y) |
| `SM_Garage` | (−3 000; −10 450) | 0° | pozdĺž pruhu na východ (+X), k výdajni |

> Zmena pri realizácii (2026-11-23): garáž pôvodne stála na Y −10 500. Posunula sa na Y −10 450, čo je os jazdného pruhu výdajne, lebo auto T4 (šírka 310 cm) idúce rovno z garáže by na pôvodnom mieste zavadilo o stĺpik výdajne asi o 5 cm.

**(N)** Stanica robotov stojí na východ od výrobne vedľa náradia, nie medzi výdajňou a výrobňou, ako bolo na náčrte pri otázke. Medzi výdajňou a výrobňou je len 8,5 m a auto, ktoré výdajňou prejde na východ, tam potrebuje miesto na otočenie na parkovisko; stanica by ho zavrela (ostal by 1 m).

**(N)** Kontajner s náradím je o meter ďalej od výrobne než na náčrte (X = 900), aby mal odstup od kiosku vstupu 1, ktorý stojí na (450; −10 150).

**(N)** Garáž leží pozdĺž pruhu, lebo je dlhá 12,15 m a naprieč by sa do 12 m pruhu nezmestila. Auto z nej vyjde rovno smerom k výdajni. Polohy sú návrh: ak pri stavbe vyjde, že sa budova prekrýva s výrobňou, výdajňou, plotom alebo obrubníkom, posunie sa pozdĺž pruhu a zapíše sa to sem.

Polohy a otočenia patria do `LotLayout`, rozmery budov do `DIAModelSizes.h`.

## 5. Čo sa kde kupuje

| Budova | Veci |
|---|---|
| Náradie (`SM_ToolShop`) | vedro, kanister, vaňa, vozíček, vozík, **úrovne výrobne** **(N)** |
| Roboti (`SM_PaverDepot`) | robot 1, robot 2, robot 3, zrýchlenie Fast, zrýchlenie Faster, **presuvná výdajňa** **(N)** |
| Garáž (`SM_Garage`) | auto a jeho stupne (`Car`, `CarT2`, `CarT3`, `CarT4`) |

**(N)** Úrovne výrobne idú do kontajnera s náradím, lebo sú základná mechanika od prvej minúty. Presuvná výdajňa ide k robotom, lebo slúži strojom.

Každá vec patrí práve jednej budove. Priradenie je v jadre (tabuľka vedľa cien v `LotBalance`), aby ho videli testy aj panel.

## 6. Ako sa kupuje

- Každá budova má **bod nákupu**. Keď je hráč pešo v dosahu, HUD ukáže výzvu (napríklad „E – Tools“) a `E` otvorí panel obchodu.
  - Náradie: pred pultom (`COUNTER` z docs/03).
  - Roboti: pred ovládacím pultom (`CONSOLE`).
  - Garáž: pri **termináli na vnútornej strane pravej steny hneď za bránou** (`TERMINAL` z docs/03, v osiach modelu (4,30; 3,55), vo svete (−2 570; −10 155)) **(N)**. Pri zadnej stene by sa k nemu hráč nedostal: zaparkované auto T4 tam necháva 20 cm a jeho kolízia blokuje. Ak je v dosahu aj auto, platí doterajšie pravidlo výberu najbližšej veci.
- Panel je **doterajší panel obchodu**, len ukáže karty jednej budovy a v nadpise má jej názov. Vzhľad kariet, stavy (kúpiť, málo kreditov, zamknuté, vlastnené) a ukladanie po nákupe ostávajú.
- Panel sa zavrie klávesom `Esc`, opätovným `E`, alebo keď hráč odíde z dosahu.
- Pás „IN HAND“ (výber nádoby do ruky) ostáva len v paneli budovy s náradím. Klávesy `1`–`4` fungujú všade ako doteraz.
- **`Tab` nerobí nič** **(N)**: obchod je zrušený a nádobu do ruky vyberajú klávesy `1`–`4`. Nápoveď ovládania a texty, ktoré `Tab` spomínajú, sa upravia.
- Z auta sa nekupuje; hráč musí vystúpiť.

## 7. Budovy vo svete

- Jeden druh aktora pre všetky tri budovy (druh budovy je parameter): model, kolízia z modelu (kvádre z importu, blokujú hráča aj auto), text na bielej tabuli a bod nákupu.
- Text tabule: **TOOLS**, **ROBOTS**, **GARAGE** **(N)**, tmavým písmom ako na tabuli nákupného centra. Poloha a rozmer tabule sú v docs/03.
- Kúpené auto sa objaví v garáži na jej počiatku, otočené von z brány: `LotLayout::ItemSpawn(ELotItem::Car)` sa zmení na počiatok garáže a pribudne konštanta s otočením (0°). Kúpa vyššieho stupňa auto nepresúva; doterajšia poistka (`UpdateTier`: ak sa väčšie auto na mieste s niečím prekrýva, presunie sa na miesto kúpy) ostáva a mieri tiež do garáže. Testy, ktoré počítajú so starým miestom (−1 200; −9 400), sa upravia.
- Kúpený robot číslo *n* sa objaví v státí *n* (`DOCKS` z docs/03), otočený k parkovisku.
- Miesta, kde sa dnes objavujú vozíček, vozík a presuvná výdajňa, sa nemenia.

## 8. Traja roboti

**Nákup**

| Vec | Cena **(N)** | Potrebuje |
|---|---|---|
| Robot 1 (`AutoPaver`) | 3 000 (bez zmeny) | odomknutie AutoPaver |
| Robot 2 | 6 000 | robot 1 |
| Robot 3 | 12 000 | robot 2 |
| Fast, Faster | bez zmeny | robot 1 |

Zrýchlenia Fast a Faster platia pre **všetkých** robotov naraz **(N)**.

**Správanie**

- Každý robot má vlastný stav: sektor, krok, polohu, fázu, nádrž (2 000 l). Slučka práce jedného robota sa nemení (do sektora, práca, výdajňa, plnenie, späť).
- Prepínač **AUTO** je jeden pre všetkých **(N)**. V režime AUTO si robot vyberie najbližší sektor s voľným políčkom, **ktorý nemá pridelený iný robot**.
- Pri výdajni sa môže plniť viac robotov naraz; každý berie zo zásobníka výrobne svojou rýchlosťou, v poradí čísel robotov.
- Dvaja roboti nikdy nemajú ten istý sektor. Ak hráč pridelí robotovi sektor iného robota, ten druhý ho stratí a ostane voľný (v režime AUTO si nájde iný).

**Ovládanie**

- Mapa sektorov (`G`) má prepínač robota 1 / 2 / 3 (len kúpení). Kliknutie na sektor ho pridelí **vybranému** robotovi. Mapa ukazuje pridelené sektory všetkých robotov, vybraný je zvýraznený.
- `H` pošle vybraného robota do sektora, v ktorom hráč stojí **(N)**. Po štarte je vybraný robot 1.
- HUD má jeden riadok na každého kúpeného robota („Paver 1: …“, „Paver 2: …“).
- Každý pridelený sektor má na zemi oranžový obrys ako dnes.

**Ukladanie**

- Formát uloženia dostane novú verziu: číslo o jedna vyššie, než bude v strome vo chvíli, keď sa začne písať (dnes je to 5 z necommitnutej práce na stupňoch auta, takže nová bude 6). Načítajú sa aj všetky verzie, ktoré načíta dnešný kód (4 a 5); ich robot sa stane robotom 1.
- Nové veci `AutoPaver2` a `AutoPaver3` sa pridajú **na koniec** `ELotItem` (za `CarT4`), aby sa poradie uložených príznakov starších verzií neposunulo.

## 9. Prístupy, ktoré som zvažoval

**Panel obchodu**

1. *Doterajší panel s filtrom podľa budovy* – vybrané. Najmenej nového kódu, vzhľad a testy panela ostávajú.
2. Tri nové panely – každá budova by mohla vyzerať inak, ale trojnásobok práce a údržby bez úžitku pre hráča.
3. Nákup priamo vo svete (tlačidlá na pulte) – pôsobivejšie, ale treba nové modely pultov a nový spôsob mierenia; nad rámec.

**Traja roboti v jadre**

1. *Pole troch stavov robota v `FLotGame`* – vybrané. Dnešné členy `AutoPaver…` sa presunú do štruktúry a funkcie dostanú číslo robota; logika jedného robota ostáva.
2. Samostatná trieda robota s vlastným tickom – čistejšie, ale väčší zásah do ukladania a testov naraz.

## 10. Poradie práce

Tri kroky; po každom prechádzajú testy a hra sa dá hrať.

1. **Budovy vo svete**: polohy, aktor, kolízia, tabule, auto a robot sa objavujú pri svojich budovách. Obchod na `Tab` ešte funguje.
2. **Rozdelenie obchodu**: priradenie vecí budovám, bod nákupu a `E`, panel s filtrom, `Tab` zrušený.
3. **Traja roboti**: jadro a ukladanie, potom mapa, HUD a `H`, potom aktori a nákup robotov 2 a 3 v stanici.

## 11. Testy a dokumentácia

- Jadro: priradenie vecí budovám pokrýva všetky veci práve raz; roboti v AUTO nedostanú ten istý sektor; pridelenie cudzieho sektora ho druhému zoberie; uloženie a načítanie troch robotov; načítanie dnešnej verzie.
- Hra (testy vrstvy `DoItAsphalt`, lebo rozmery budov sú v `DIAModelSizes.h`): budovy stoja na svojich miestach, neprekrývajú sa s výrobňou, výdajňou ani plotom a ležia v pruhu; výzva a `E` pri každej budove otvorí správne karty; `Tab` obchod neotvorí; kúpené auto stojí v garáži a robot v státí; do garáže sa dá vojsť (kolízia nezaviera bránu) a auto z výdajne sa dostane na parkovisko; terminál v garáži je v dosahu aj so zaparkovaným autom T4.
- `Scripts/test.sh` prechádza celý.
- `docs/01-herny-dizajn.md` (obchod, ovládanie, roboti), `docs/02-technicky-navrh.md` a `docs/03-3d-modely.md` popisujú nový stav.

## 12. Podmienka začiatku

V priečinku práve pracuje iná session na stupňoch auta T2–T4 a mení tie isté súbory. Kód podľa tohto návrhu sa začne písať, až keď používateľ potvrdí, že skončila.

## 13. Čo ostáva na staviteľovi

Názvy tried, funkcií a súborov, presný dosah bodov nákupu, rozloženie prepínača robotov na mape, texty výziev a oznamov (po anglicky, v štýle doterajších) a drobné posuny budov pozdĺž pruhu podľa bodu 4.

## Vlna 2 (2026-11-24)

Dodatok po tom, čo budovy stáli; text vyššie ostáva ako pôvodný návrh a v týchto bodoch je zastaraný. Hotový stav je v `docs/01-herny-dizajn.md` (kapitoly 4.6, 6, 7, 9) a `docs/02-technicky-navrh.md`.

- **Roboti sa kupujú a vylepšujú po jednom.** Globálne zrýchlenia „Fast" a „Faster" (zmienené vyššie) zmizli. Každý robot (Auto Paver 1, 2, 3) má vlastné vylepšenie rýchlosti (120 → 150 → 180 políčok/min) a kapacity nádrže (2 000 → 3 000 → 4 500 l), ktoré sa kupujú v budove Roboti, až keď robot patrí hráčovi.
- **Budova Roboti má nové rozloženie:** Portable Station v stĺpci 0, v stĺpcoch 1 až 3 vždy jeden robot s kartami Speed a Capacity pod ním (ich miesto je rezervované a kým robot nie je kúpený, stojí tam text „Upgrades open once the robot is bought"). Náradie a Garáž ostali v pôvodnej mriežke.
- **Mapa robotov je na `M`** (predtým `G`); text nad mapou, riadok robota v HUD a pätka mapy to hovoria rovnako. Dole vľavo pribudla stále zapnutá **legenda ovládania** „CONTROLS" (v aute kratšia).
- **Ceny sa preladili** podľa simulácie troch robotov: Auto Paver 2 stojí 40 000 a Auto Paver 3 80 000 (v návrhu 6 000 a 12 000), rýchlosť 20 000 a 60 000, kapacita 5 000 a 15 000. Simulovaný hráč otvorí východ asi o 1 h 48 min (okno 105 – 165 min); roboty kúpi asi o 23. (prvý), 53. a 95. minúte. Uloženie má verziu 7.
