# Do it Asphalt-self – plán zmien

Verzia 0.2 · 2026-11-20 · vychádza z `Do_it_Asphalt-self.md` · zvolený variant so surovým pásom

> **Prekonané (2026-11-22):** surový asfalt, hladidlo a pásový finišer sa z hry odstránili. Hráč len leje asfalt na zem (fyzika tekutiny ostala) a vyliaty asfalt je rovno hotový. Platí `01-herny-dizajn.md`; tento dokument zostáva ako história návrhu, takže kapitoly o surovom páse, hladidle a finišeri už neplatia.

Tento dokument hovorí, ako z hotovej hry v1 (šesťuholníkové parkovisko, batoh, shardy) spraviť hru podľa novej dokumentácie. Rozpis úloh po krokoch je v `superpowers/plans/2026-11-20-asphalt-self.md`. Všetko, čo nová dokumentácia neurčila a rozhodol som sám, je v kapitole 6 a dá sa zmeniť jednou vetou.

---

## 1. Čo sa mení

| | Teraz (v1) | Po zmene (Asphalt-self) |
|---|---|---|
| Kamera | tretia osoba | **prvá osoba**, podržaním tlačidla detailný pracovný pohľad |
| Mapa | šesťuholník, 499 buniek, asi 27 × 24 m | **L s ramenami 100 m**, 30 000 štvorcov 1 × 1 m, v rohu nákupné centrum 100 × 100 m |
| Asfalt na políčko | 20 l, hotové pri 97 % | **10 l, hotové pri 95 %** (9,5 l), zvyšok sa zjednotí sám |
| Odmena | 1 peniaz za liter + 1 shard za bunku | **10 kreditov za hotový štvorec**, raz; shardy zanikajú |
| Nosenie asfaltu | batoh 5 – 200 l (13 úrovní) | lyžica + miska 2,5 l → lopatka 1 l → vedro 10 l → kufor 60 l → vozík 200 l |
| Liatie | lišta 0,2 – 2 m pred hráčom | z nádoby v ruke na voľné miesto; na hotový asfalt sa liať nedá |
| Výrobňa | 13 úrovní, 15 – 1000 l/min | **6 úrovní, 150 – 5 200 l/min**, na štarte 80 l v zásobe |
| Stroje | prenosné miesto, robot 20 l/min, auto s nádržou 500 l | presuvná výdajňa, **pásový finišer**, automat 120/170/220 políčok/min, auto 260/300 políčok/min |
| Odomykanie | kiosk na ceste po vyasfaltovaní 5 buniek | **počet hotových políčok** (10, 100, 350, 800, 6 000, 30 000) + príjazdový úsek |
| Dĺžka hry | asi 50 minút | **asi 2 h 15 min** |
| Vzhľad | stavenisko za plotom | parkovisko pred nákupným centrom, tlmené farby, jedna akcentová |

## 2. Čo z existujúcej hry ostáva

Kostra hry sa nemení, mení sa jej obsah:

- **Dva moduly.** `AsphaltCore` (pravidlá bez enginu, všetko testovateľné s explicitným `Dt`) a tenká engine vrstva `DoItAsphalt`.
- **Mriežka pokrytia** (`FCoverageGrid`): tá istá maska je logika aj textúra zeme. Ostáva razenie pásu (`Stamp`), vsypanie do políčka (`DepositIntoCell`), dokončenie pri prahu a bezplatné doliatie zvyšku.
- **Jedno miesto pre čísla** (`AsphaltBalance.h`) a pre polohy (`AsphaltLayout.h`).
- **Výrobňa so zásobníkom**, plnenie nádob (`MoveFromPlant`), presuvná výdajňa čerpajúca z toho istého zásobníka.
- **Automat riadený z jadra** (`TickAutoPaver`), kinematické auto, brána a výhra.
- **Uloženie** do vlastného binárneho formátu so zálohou a CRC; zvýši sa len číslo verzie.
- **Obchod na Tab, HUD v Slate, cheaty `dia.*`, `shot.sh`**, testy jadra aj engine vrstvy, simulácia celého prechodu hry.
- **Výroba modelov** Blender skriptami s manifestom a náhradnými tvarmi v hre.

Zaniká: šesťuholníková geometria (`HexCoord`, `HexMap`), shardy, úrovne batohu a nástroja, špeciál „dve ruky", nákup špeciálov pri kioskoch, kruhový plot.

## 3. Návrh riešenia

### 3.1 Mapa a mriežka

- Svet je štvorec ±100 m okolo stredu. Nákupné centrum zaberá kvadrant X > 0, Y > 0; parkovisko je zvyšok. Model centra tam sadne bez otočenia – jeho fasády k parkovisku sú strany −X a −Y.
- `FHexMap` nahradí `FLotMap`: políčko má súradnice (stĺpec, riadok) 0 – 199, index sa počíta priamo, bez tabuliek. 30 000 políčok.
- Mriežka pokrytia: **4000 × 4000 texelov po 5 cm**, 400 texelov na políčko, 0,025 l na texel. Lyžica (0,25 l) je presne 10 texelov, miska 100, vedro celé políčko.
- Dnešné pomocné tabuľky mriežky (zoznam texelov pre každú bunku) by pri tejto veľkosti zabrali desiatky MB. Pri štvorcoch ich netreba – políčko texela je delenie.
- Čiary políčok kreslí materiál zo svetových súradníc, textúra obrysov odpadá.
- Okolo vonkajších strán L je 12 m pás zeme mimo parkoviska: stojí na ňom výrobňa, vedú ním príjazdové cesty a končí plotom.

### 3.2 Tri stavy asfaltu

Každý texel zeme je **voľný**, **surový** alebo **hotový**. Do postupu políčka sa počíta len hotový asfalt.

- **Z nádoby v ruke** (lyžica, lopatka, miska, vedro) sa asfalt rozleje sám – padá rovno ako hotový.
- **Z klapky kufra a z výpustu vozíka** vzniká surový asfalt: hrubá kôpka, ktorá sa nepočíta.
- **Hladidlo a finišer** menia surový asfalt na hotový. Asfalt to nestojí.
- **Automat a auto** kladú rovno hotový asfalt a surový pod sebou dokončia.
- Liať sa dá len na voľné miesto. Na surový ani hotový asfalt sa nevyleje nič a asfalt ostane v nádobe.
- Políčko je hotové, keď má 95 % plochy hotovej. Zvyšok – voľný aj surový – sa zjednotí zadarmo a pripíše sa 10 kreditov.
- Zarovnanie a spevnenie je v pravidlách jeden krok; rozdiel je len v obraze (hotový asfalt je tmavší a hladký).

Ako sa leje:

- Miska a lopatka majú stopu (lopatka 10 cm, miska 15 cm). Kým hráč drží tlačidlo, zalievajú sa voľné texely v stope okolo miesta, kam mieri – najprv tie bližšie k hráčovi.
- Vedro vyleje naraz: do políčka pod zámerným bodom, od strany hráča; čo sa nezmestí, ostane vo vedre.
- Naberanie: podržaním tlačidla pri vani sa opakuje záber (lyžica 0,25 l, lopatka 1 l) zo zásoby do misky. Vedro, kufor a vozík sa plnia pod výpustom rýchlosťou výroby výrobne – vyššia úroveň teda plní rýchlejšie.
- Kufor stojí a klapkou sype surový asfalt do políčka pred sebou. Vozík ho kladie v páse širokom 1 m, kým ho hráč pomaly tlačí.

### 3.3 Vybavenie

| Vec | Cena | Čo je v jadre |
|---|---|---|
| Lyžica + miska | štart | nádoba v ruke 2,5 l, záber 0,25 l |
| Hladidlo | štart | pás 40 cm: surový → hotový (šírka bola pôvodne navrhnutá 30 cm, pri ladení sa zväčšila na 40 cm; v kóde `TrowelWidthCm`) |
| Lopatka | 10 | záber 1 l; dá sa z nej liať priamo |
| Vedro | 35 | nádoba v ruke 10 l, plní sa pod výpustom, z kufra, vozíka a výdajne |
| Kufor | 110 | fyzický predmet: poloha, 60 l, ťahanie; klapka sype surový asfalt; zásoba pre vedro |
| Vozík | 340 | fyzický predmet: poloha, 200 l, tlačenie; surový pás 1 m; zásoba pre vedro |
| Presuvná výdajňa | 900, od 350 | ako dnes, plní vedro, kufor aj vozík |
| Pásový finišer | 1 400, od 350 | vedený hráčom, lišta 1 m: surový → hotový, do 85 políčok/min (chôdza 1,42 m/s) |
| Automat | 3 000, od 800 | pracuje v pridelenom sektore 20 × 20 m, čerpá zo zásobníka, 120 políčok/min; vylepšenia 9 000 (170) a 22 000 (220) |
| Auto | 12 000, od 6 000 | čerpá zo zásobníka, lišta 3,0 m, 260 políčok/min; stupne II až IV za 24 000 / 48 000 / 96 000 (lišta 3,6 / 4,2 / 5,0 m, 300 / 380 / 480 políčok/min, nádrž 8 000 / 12 000 / 18 000 l); výrobňa ostáva na 5 200 l/min, takže stupeň IV spolu s najlepším robotom (7 000 l/min) ju môže predbehnúť |
| Výrobňa | 100 / 400 / 1 400 / 8 000 / 24 000 | 6 úrovní; zásobník pojme 4 minúty výroby |

Šprint (100 políčok) a rýchla výmena nástrojov (10 políčok) sú zadarmo – dokumentácia im cenu nedáva.

Parkovisko je rozdelené na 75 pevných sektorov 20 × 20 m. Automat hráč pošle do sektora, v ktorom práve stojí; stroj ho prejde riadok po riadku a potom čaká na ďalší.

### 3.4 Odomykanie

Šesť krokov odomykania sedí na šesť strán obrysu L. Každá strana má príjazdový úsek (navrhujem 6 × 2 m = 12 políčok v strede strany; na dvoch stranách pri nákupnom centre leží pred jeho vchodom). Krok sa odomkne, keď je hotový jeho úsek **a zároveň** celkový počet políčok. Kupuje sa všetko na jednom mieste, v obchode na Tab; kiosk pri úseku ostáva len ako tabuľa s tým, čo úsek odomyká. Posledný úsek je výjazd s bránou.

### 3.5 Hráč a obraz

- Kamera vo výške očí, telo Mannequina ostáva kvôli rukám a tieňu; nástroj je pripojený k ruke. Batoh, prilba a asfaltovačka sa prestanú zobrazovať.
- Zem: jedna plocha 224 × 224 m, textúra pokrytia 4096², nahráva sa len zmenený výrez.
- Kulisy: nákupné centrum, obrubníky po vonkajšom obryse, svetlá chladná hmla do diaľky.
- HUD: kredity, postup „137 / 30 000", obsah nádoby v ruke, zásobník, najbližší krok odomykania.

## 4. Modely

### 4.1 Nové – hotové

Všetky prešli `check.py` (rozmery, trojuholníky, paleta, orientácia plôch). GLB sú v `SourceAssets/Models`, náhľady v `SourceAssets/Previews`. Do Unrealu ešte **nie sú naimportované** – to je krok kódu (`Tools/UE/import_models.py`).

| Názov | Rozmery D × Š × V (m) | Počiatok | Dôležité body |
|---|---|---|---|
| `SM_Spoon` | 0,20 × 0,045 × 0,018 | stred rukoväte | naberák smeruje do +X |
| `SM_Bowl` | 0,25 × 0,25 × 0,09 | stred dna | vnútri: dno Z = 0,012 (r 0,060), okraj Z = 0,09 (r 0,117) |
| `SM_Trowel` | 0,28 × 0,12 × 0,092 | stred rukoväte | pracovná plocha Z = −0,075 |
| `SM_Scoop` | 0,34 × 0,12 × 0,07 | stred rukoväte | spodná hrana ústia (0,27; 0; −0,035) |
| `SM_Bucket` | 0,29 × 0,31 × 0,385 | stred dna | uši (0; ±0,147; 0,215), rúčka (0; 0; 0,37); vnútri dno r 0,096, okraj r 0,132 |
| `SM_Trolley` | 0,96 × 0,52 × 0,90 | stred pôdorysu, zem | rukoväť (−0,48; 0; 0,88); ústie (0,435; 0; 0); vaňa hore 0,68 × 0,48 (Z = 0,60) (nahradil kufor `SM_Suitcase`) |
| `SM_Cart` | 1,26 × 0,67 × 0,99 | stred pôdorysu, zem | madlo (−0,63; 0; 0,97); ústie výpustu (0,585; 0; 0,06), šírka 0,5 m; vaňa hore 0,95 × 0,66 (Z = 0,76), dole 0,66 × 0,44 (Z = 0,30) |
| `SM_Finisher` | 1,32 × 1,07 × 1,0 | na zemi pod stredom prednej lišty | lišta 1,0 m na osi Y v X = 0; valec v X = −0,58; madlá (−1,15; ±0,25; 0,98) |
| `SM_Trough` | 1,28 × 0,60 × 0,50 | stred pôdorysu, zem | hladina asfaltu Z = 0,44 |
| `SM_Curb` | 0,15 × 2,0 × 0,12 | stred pôdorysu, zem | radí sa po osi Y, skosená hrana na +X (k parkovisku) |
| `SM_PaverCar_WideBar` | 0,14 × 3,5 × 0,17 | **počiatok auta** | pripája sa k autu bez posunu, zakryje pôvodnú lištu 3,0 m |
| `SM_Mall` | 100 × 100 × 14,3 | stred pôdorysu, zem | fasády k parkovisku −X a −Y; vchod v strede poľa s = 0 – 10 m; tabuľa na názov 16 × 4 m, stred vo výške 10,6 m |

Obsah nádob (hladinu) zobrazuje hra plochým diskom z palety `DIA_Asphalt`; vnútorné rozmery sú v tabuľke.

Do spoločných pomôcok pribudli tri veci: tvar `tub` (otvorená nádoba so šikmými stenami), náhľad bez postavy pre ručné nástroje a voliteľný smer kamery prvého náhľadu.

### 4.2 Existujúce

- **Ostávajú:** výrobňa a jej dva prístavky (objavia sa na úrovni 3 a 5), presuvná výdajňa, automat, auto, kus cesty, zábrana, brána, plot, kužeľ, sud, reflektor, kiosk.
- **Prestanú sa používať:** 7 asfaltovačiek, 4 batohy, prilba, zámok, kopa hliny. Súbory navrhujem nechať, kým nová hra nebeží.

### 4.3 Čo som zámerne nerobil

- **Valček, dusadlo, lata s vodováhou** – spomínajú sa v texte o štýle a tempe, ale nie v zozname vybavenia a nemajú cenu (pozri otázku O1).
- **Ruky** – používa sa Mannequin.
- **Prefarbenie starých strojov** – nový štýl chce jednu akcentovú farbu; staré stroje sú žlté s oranžovými prvkami. Nové modely držia rovnaký kľúč (stroj žltý, nástroje kov, oranžová len tam, kde sa chytá a ovláda). Zmena celej palety je úprava dvoch tabuliek farieb, ak ju budeš chcieť.

## 5. Postup

Práca pôjde na vetve `asphalt-self`; stav v1 ostane označený značkou. Kód píše Sonnet 5.5, modely Fable 5.2. Každá fáza končí zelenými testami a snímkou obrazovky.

| Fáza | Obsah | Kto |
|---|---|---|
| 1 | Jadro: `FLotMap`, mriežka 4000² s tromi stavmi, prah 95 %, 10 kreditov za políčko, bez shardov | Sonnet |
| 2 | Jadro: nádoby v ruke, naberanie, liatie, hladidlo, výrobňa 6 úrovní, obchod a odomykanie podľa počtu | Sonnet |
| 3 | Jadro: kufor, vozík, finišer, automat so sektormi, auto zo zásobníka, uloženie v2 | Sonnet |
| 4 | Svet: zem 224 m, rozmiestnenie, nákupné centrum, obrubníky, import nových modelov | Sonnet |
| 5 | Hráč v prvej osobe, nástroj v ruke, pracovný pohľad, HUD a obchod | Sonnet |
| 6 | Kufor, vozík, výdajňa a finišer vo svete | Sonnet |
| 7 | Automat, auto, výjazd a výhra vo svete | Sonnet |
| 8 | Cheaty, simulácia prechodu na 2 h 15 min, dokumentácia v2, README | Sonnet |
| priebežne | Ďalšie modely podľa potreby (lampa parkoviska, prípadne valček a spol.) | Fable |

Riziká, ktoré vidím vopred:

- **Textúra 4096² a jej nahrávanie** pri strojoch, ktoré menia veľké plochy naraz (auto, automat). Overí sa hneď vo fáze 3 meraním, nie až na konci.
- **Prvá osoba s telom Mannequina** – ruky nebudú presne zvierať nástroj bez vlastnej animácie. V1 to malo ako vedomé obmedzenie; v prvej osobe to bude viac vidno.
- **Balans na 2 h 15 min** stojí na simulácii; krivka z dokumentácie sa stane jej testom. Úsek 100 → 800 políčok za 20 minút vychádza pri surovom páse tesne: vozík odvezie 20 políčok na jednu cestu. Ak ho simulácia netrafí ladením rýchlostí a prietokov, úloha sa zastaví a vráti sa s číslami – ceny, kapacity ani výkony zo špecky sa samy nezmenia.

## 6. Moje rozhodnutia a otvorené otázky

Rozhodol som sám, kým nepovieš inak:

| # | Vec | Rozhodnutie |
|---|---|---|
| N1 | Kamera | Prvá osoba podľa novej dokumentácie; nahrádza včerajšiu tretiu osobu. |
| N2 | Majú automat a auto vlastnú nádrž? | Nie, čerpajú priamo zo spoločného zásobníka – dokumentácia k nim viaže výkon výrobne (520 políčok/min = 5 200 l/min). |
| N3 | Veľkosť príjazdového úseku | 12 políčok (6 × 2 m) v strede každej strany. |
| N4 | Kde sa kupuje | Všetko v obchode na Tab. |
| N5 | Kde stojí výrobňa a štart | Za vonkajšou hranou dolnej strany, výpust a vaňa pri prvom štvorci. |
| N6 | Šírka širšej lišty auta | 3,5 m (pomer 300 : 260 voči pôvodným 3,0 m). |
| N7 | Vozík | Priemyselný vozík s korbou, nie nákupný košík. |

| N8 | Čo kladie surový asfalt | Klapka kufra a výpust vozíka. Z ruky sa leje rovno hotový – tak to píše kapitola „Asfaltovanie". Hladidlo má preto prácu od kufra (110 kreditov), čo sedí s rýchlou výmenou nástrojov pri 10 políčkach. |
| N9 | Šírka pásu z vozíka | 1 m, aby 200 l vyšlo presne na 20 políčok a pás sedel pod lištu finišera. |
| N10 | Zarovnanie a spevnenie | Jeden krok. |
| N11 | Sektory automatu | Pevná sieť 20 × 20 m, hráč pridelí sektor, v ktorom stojí. |
| N12 | Rýchlosť plnenia nádob | Rovná sa výrobe výrobne (150 l/min = 2,5 l/s na štarte). |
| N13 | Ruky v prvej osobe | Nie sú vidieť; nástroj visí pred kamerou. Viditeľné ruky potrebujú vlastné animácie, ktoré sa kódom spoľahlivo vyrobiť nedajú. |
| N14 | Kontrolný pohľad | Držaním V sa všetko nehotové zvýrazní oranžovou a HUD ukáže smer a vzdialenosť k najbližšiemu nehotovému políčku. |
| N15 | Vyznačenie príjazdového úseku | Biely rám na zemi a tabuľa s tým, čo úsek odomyká (pri nákupnom centre je tabuľou jeho nápis nad vchodom). |

Rozhodnuté tebou:

- **Zarovnávanie (2026-11-20):** variant so surovým pásom – vozík kladie surový pás, hotový je až po finišeri alebo hladidle.

Ostáva otvorené:

- **O1 – Valček, dusadlo, lata s vodováhou.** V tomto pláne nie sú. Ak ich chceš v hre, potrebujú cenu a miesto v poradí (napr. valček ako širšie hladidlo medzi kufrom a finišerom).
