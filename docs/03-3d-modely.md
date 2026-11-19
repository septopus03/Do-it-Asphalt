# Do it Asphalt – špecifikácia 3D modelov

Verzia 1.0 · 2026-11-19 · modely vytvára **Fable 5.2**

Tento dokument je zmluva medzi vetvou modelov a vetvou kódu. Kód počíta s rozmermi, počiatkami a názvami presne podľa tabuliek nižšie; kým model neexistuje, hra zobrazuje náhradný tvar rovnakých rozmerov.

---

## 1. Ako modely vznikajú

Každý model je **Python skript pre Blender**, nie ručne modelovaný súbor. Skript sa dá kedykoľvek spustiť znova a dá rovnaký výsledok; zmena modelu je zmena v kóde, ktorú vidno v gite.

```
Tools/ModelGen/
├── common.py          paleta, pomocné tvary, export do GLB, render náhľadov
├── manifest.json      očakávané rozmery, limity trojuholníkov, povolené materiály
├── check.py           overí GLB voči manifestu (beží bez Blenderu)
├── build_all.sh       spustí všetky skripty, potom check.py
└── <model>.py         jeden skript na model alebo rodinu modelov

SourceAssets/Models/<Názov>.glb        výstup
SourceAssets/Previews/<Názov>_{a,b,c}.png   náhľady: trištvrte spredu, zboku, zhora
```

Postup pre každý model:

1. Doplniť riadok do `manifest.json` podľa tabuľky v kap. 4 (to je „test").
2. Napísať skript, spustiť `blender -b -P Tools/ModelGen/<model>.py`.
3. `python3 Tools/ModelGen/check.py <Názov>` musí vypísať `OK`.
4. Pozrieť si tri náhľady a prejsť kontrolný zoznam z kap. 5.
5. Opakovať 2 – 4, kým model neprejde.

## 2. Konvencie

| | |
|---|---|
| Súradnice (ako ich uvidí hra) | **+X dopredu, +Y doprava, +Z hore**, jednotky v tabuľkách sú metre |
| Počiatok | podľa stĺpca „Počiatok"; ak nie je uvedené inak, stred pôdorysu na úrovni zeme |
| Štýl | low-poly, ostré hrany s jemným skosením na veľkých tvaroch, žiadne textúry |
| Farby | len materiály z palety (kap. 3), ploché farby |
| Normály | ploché tieňovanie; žiadne obrátené plochy |
| Formát | GLB, jeden mesh na súbor, transformácie aplikované |
| Mierka | postava hráča má 1,8 m; v náhľadoch je pre mierku sivá silueta (neexportuje sa) |

Skripty píšu geometriu priamo v týchto osiach. Prevod do súradníc Blenderu a glTF (iná orientácia osí, opačná ručnosť) rieši výhradne `common.export()`. Správnosť prevodu dokazuje model `SM_AxisTest`: po importe do Unrealu musí mať stred obálky v kladnej časti všetkých troch osí.

## 3. Paleta

Názov materiálu v GLB = názov slotu v Unreali = `MI_<názov>` v `Content/Materials/`.

| Materiál | Farba | Kov | Drsnosť | Použitie |
|---|---|---|---|---|
| `DIA_Yellow` | `#F2B705` | 0 | 0,6 | stroje, hlavná farba hry |
| `DIA_Orange` | `#E8590C` | 0 | 0,6 | výstražné prvky, kužele |
| `DIA_Steel` | `#8A9099` | 1 | 0,4 | rúry, nádrže, lišty |
| `DIA_DarkSteel` | `#3A3F47` | 1 | 0,5 | rámy, podvozky |
| `DIA_Rubber` | `#1B1B1D` | 0 | 0,9 | kolesá, hadice |
| `DIA_Asphalt` | `#232326` | 0 | 0,35 | asfalt, výpuste |
| `DIA_White` | `#F2F2EE` | 0 | 0,7 | značenie, tabule |
| `DIA_Red` | `#C62828` | 0 | 0,6 | závory, zámok |
| `DIA_Glass` | `#7FB6D9` | 0 | 0,1 | okná, ukazovatele |
| `DIA_Concrete` | `#A8A49C` | 0 | 0,9 | základy, obrubníky |
| `DIA_Dirt` | `#8A6B4A` | 0 | 1,0 | kopy hliny |
| `DIA_Green` | `#3E8E41` | 0 | 0,6 | „odomknuté", tlačidlá |

## 4. Zoznam modelov

Rozmery sú obálka D × Š × V (X × Y × Z) v metroch, tolerancia ±2 cm. „Tri" je horný limit trojuholníkov.

### 4.0 Overenie reťazca

| Názov | Rozmery | Počiatok | Tri | Popis |
|---|---|---|---|---|
| `SM_AxisTest` | 1,0 × 0,5 × 0,25 | **roh** kvádra; kváder leží celý v +X, +Y, +Z | 12 | Kváder na dôkaz osí a mierky. V hre sa nepoužíva. |

### 4.1 Hráč

| Názov | Rozmery | Počiatok | Tri | Popis |
|---|---|---|---|---|
| `SM_Spreader_020` … `_200` (7 ks) | 0,58 × (W + 0,10) × 1,0 | na zemi pod stredom lišty | 600 | Asfaltovačka. Rozstrekovacia lišta šírky W leží na osi Y v X = 0, tesne nad zemou, na koncoch kolieska (polomer 6 cm). Od stredu lišty stúpa rukoväť dozadu k bodu (−0,45; 0; 0,95) s priečnym madlom. Krátka hadica z lišty smerom k hráčovi. |
| `SM_Backpack_S` | 0,16 × 0,26 × 0,32 | stred plochy, ktorá sa dotýka chrbta; nádrž ide do −X | 800 | Nádrž na chrbát: valcová nádoba, veko, popruhy, priezor so stupnicou (`DIA_Glass`), vývod hadice dole. |
| `SM_Backpack_M` | 0,22 × 0,32 × 0,42 | rovnako | 800 | Rovnaký dizajn, väčší. |
| `SM_Backpack_L` | 0,28 × 0,38 × 0,52 | rovnako | 800 | Pribudne druhá menšia nádoba po boku. |
| `SM_Backpack_XL` | 0,34 × 0,44 × 0,62 | rovnako | 800 | Dve veľké nádoby, rám z `DIA_DarkSteel`. |
| `SM_Helmet` | 0,28 × 0,24 × 0,16 | stred spodného okraja | 300 | Stavebná prilba so šiltom dopredu, `DIA_Yellow`. |

Sedem šírok W asfaltovačky: 0,2 · 0,3 · 0,5 · 0,8 · 1,0 · 1,5 · 2,0 m (prípona je šírka v cm). Všetkých sedem generuje jeden parametrický skript; od 1,0 m vyššie má lišta stredové podporné koliesko.

### 4.2 Výrobňa a odber

| Názov | Rozmery | Počiatok | Tri | Popis |
|---|---|---|---|---|
| `SM_Plant` | 6,8 × 4,0 × 5,0 | stred pôdorysu tela (telo zaberá X od −3,0 do +3,0), zem | 6000 | Výrobňa asfaltu: betónový základ, šikmý miešací bubon, jedno silo (výška 5 m), komín, dopravník, búdka obsluhy s oknom. **Výpust** smeruje do +X a končí v bode (3,4; 0; 1,8) – pod ním bude stáť hráč. |
| `SM_Plant_AddonA` | 1,6 × 1,6 × 5,5 | stred pôdorysu, zem | 1500 | Druhé silo s rebríkom. Hra ho postaví vedľa výrobne od úrovne 5. |
| `SM_Plant_AddonB` | 2,4 × 1,8 × 3,0 | stred pôdorysu, zem | 1500 | Ležatá zásobná nádrž na nohách. Od úrovne 10. |
| `SM_PortableStation` | 1,2 × 0,8 × 1,3 | stred pôdorysu, zem | 1500 | Nádrž na dvoch kolesách s ojom a výpustným ramenom do +X. Musí byť čitateľné, že sa dá odtiahnuť. |

### 4.3 Cesty a východ

| Názov | Rozmery | Počiatok | Tri | Popis |
|---|---|---|---|---|
| `SM_Kiosk` | 0,6 × 0,8 × 2,2 | stred pôdorysu, zem | 1000 | Terminál vylepšení: stĺp, šikmý panel s obrazovkou (`DIA_Glass`) otočený do +X, veľké tlačidlo (`DIA_Green`). Hore prázdna tabuľa 0,6 × 0,3 m (`DIA_White`), stred vo výške 1,9 m – text na ňu kreslí hra. |
| `SM_Lock` | 0,10 × 0,30 × 0,40 | stred | 300 | Visiaci zámok (`DIA_Red`), hra ho zobrazí pred obrazovkou zamknutého kiosku. |
| `SM_RoadStub` | 12,0 × 3,4 × 0,05 | stred bližšej hrany, zem; cesta ide do +X | 200 | Kus starej hotovej cesty od konca buniek po plot: `DIA_Asphalt`, stredová prerušovaná čiara, obrubníky. |
| `SM_Barrier` | 0,3 × 3,4 × 1,0 | stred pôdorysu, zem | 400 | Pevná cestná zábrana, červeno-biele pruhy. Stojí v línii plota a uzatvára päť ciest, ktoré nie sú východ. |
| `SM_ExitGate_Frame` | 0,4 × 3,8 × 3,0 | stred pôdorysu, zem | 800 | Dva stĺpy a priečny nosník s tabuľou (`DIA_White`, 2,0 × 0,5 m) pre nápis EXIT. Prejazd široký 3,4 m, vysoký 2,6 m. |
| `SM_ExitGate_Barrier` | 0,10 × 3,4 × 0,12 | **koniec pri pánte**; rameno ide do +Y | 200 | Rameno závory s červeno-bielymi pruhmi. Hra ho otáča okolo osi X v počiatku. |

### 4.4 Stroje

| Názov | Rozmery | Počiatok | Tri | Popis |
|---|---|---|---|---|
| `SM_AutoPaver` | 1,0 × 0,8 × 0,6 | stred pôdorysu, zem | 1500 | Robot na pásoch: nízke telo `DIA_Yellow`, vpredu (+X) lišta šírky 0,8 m, hore maják (`DIA_Orange`) a anténa. Milý, nie hrozivý. |
| `SM_PaverCar` | 4,6 × 3,0 × 2,4 | stred pôdorysu, zem | 8000 | Asfaltovacie auto: kabína vpredu (+X), za ňou nádrž 500 l, štyri kolesá, **vzadu** (X = −2,3) rozstrekovacia lišta široká 3,0 m tesne nad zemou. Telo bez lišty je široké 2,2 m. Dvere kabíny vľavo (−Y). |

### 4.5 Kulisy

| Názov | Rozmery | Počiatok | Tri | Popis |
|---|---|---|---|---|
| `SM_Fence` | 0,08 × 2,0 × 1,8 | stred pôdorysu, zem | 300 | Diel mobilného stavebného plota: rám, výplň z pásov, dve betónové pätky. Diely sa radia tesne vedľa seba po osi Y. |
| `SM_Cone` | 0,3 × 0,3 × 0,5 | stred pôdorysu, zem | 150 | Dopravný kužeľ s bielym pruhom. |
| `SM_Barrel` | 0,6 × 0,6 × 0,9 | stred pôdorysu, zem | 250 | Sud s obručami. |
| `SM_Lamp` | 0,4 × 1,2 × 5,0 | päta stĺpa, zem | 300 | Stavebný reflektor na stĺpe, rameno do +Y. |
| `SM_DirtPile` | 2,0 × 2,0 × 1,0 | stred pôdorysu, zem | 300 | Nepravidelná kopa hliny (`DIA_Dirt`). |

**Spolu 30 modelov** (z toho 7 variantov asfaltovačky a 4 batohy).

## 5. Kontrolný zoznam pred odovzdaním modelu

- `check.py` vypísal `OK` (rozmery, počet trojuholníkov, materiály len z palety).
- Na náhľadoch nie sú čierne ani chýbajúce plochy (obrátené normály) a nič sa neprelína tak, aby blikalo.
- Silueta je rozpoznateľná aj v malom náhľade – človek bez popisu povie, čo to je.
- Vedľa siluety postavy 1,8 m model veľkosťou sedí.
- Funkčné body sú tam, kde ich čaká hra: výpust výrobne, lišta asfaltovačky v X = 0, pánt závory v počiatku, tabuľa kiosku vo výške 1,9 m.
- Model stojí na zemi (najnižší bod Z = 0), nevznáša sa a nie je zapustený.
- Použitých je najviac 5 materiálov palety.

## 6. Kde hra modely umiestňuje

Polohy sú v `AsphaltLayout.h`; tu len prehľad, aby mal modelár predstavu o okolí.

- Výrobňa stojí 20 m západne od stredu parkoviska, výpustom k parkovisku; hráč začína pod výpustom.
- Kiosk stojí vpravo od konca každej cesty, obrazovkou k ceste.
- Za koncom každej cesty leží `SM_RoadStub` a vedie až k plotu; tam je na piatich cestách `SM_Barrier`, na ceste 5 brána.
- Plot obieha areál v kruhu s polomerom 26 m (asi 70 dielov), pri každej ceste má medzeru 3,8 m.
- Špeciály (prenosné miesto, robot, auto) sa po kúpe objavia na okraji parkoviska pri svojej ceste.
- Kužele, sudy, kopy hliny a reflektory sú rozmiestnené voľne medzi plotom a parkoviskom, nikdy nie na bunkách.
