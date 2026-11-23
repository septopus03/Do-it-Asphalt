# Do it Asphalt – špecifikácia 3D modelov

Verzia 2.0 · 2026-11-21 · modely vytvára **Fable 5.2**

Tento dokument je zmluva medzi vetvou modelov a vetvou kódu. Kód počíta s rozmermi a názvami podľa tabuliek nižšie. Obsahuje 45 modelov: 12 nových pre hru Asphalt-self, ostatné ostali z verzie 1, a tri z druhého kola (`SM_Canister`, `SM_Tub`, `SM_VehicleStation`). `SM_Scoop` ostal v manifeste, ale hra ho už nepoužíva (malá naberačka sa odstránila).

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

1. Doplniť riadok do `manifest.json` s obálkou modelu (to je „test").
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

## 4. Modely

Rozmery sú obálka D × Š × V (X × Y × Z) v metroch, prevzatá z `Tools/ModelGen/manifest.json`; „Tri“ je horný limit trojuholníkov. Všetky modely sú v `Content/Models/` a v `SourceAssets/Models/`.

### 4.1 Nové modely (15)

| Asset | Rozmery | Obálka v osiach | Tri | Čo to je a kde sa používa | Skript |
|---|---|---|---|---|---|
| `SM_Spoon` | 0,199 × 0,045 × 0,018 | X −0,055 … 0,144 | 200 | lyžica v ruke k miske | `hand_tools.py` |
| `SM_Bowl` | 0,25 × 0,25 × 0,09 | stred pôdorysu, Z od 0 | 400 | miska v ruke (štartovacia nádoba) | `hand_tools.py` |
| `SM_Trowel` | 0,28 × 0,12 × 0,092 | X −0,14 … 0,14 | 250 | hladidlo (hra ho od 2026-11-22 nepoužíva, model ostáva v repozitári) | `hand_tools.py` |
| `SM_Scoop` | 0,34 × 0,12 × 0,07 | X −0,07 … 0,27 | 250 | malá naberačka (hra ju od 2026-11-23 nepoužíva, model ostáva v repozitári) | `hand_tools.py` |
| `SM_Bucket` | 0,288 × 0,31 × 0,385 | stred pôdorysu, Z od 0 | 600 | vedro v ruke | `hand_tools.py` |
| `SM_Suitcase` | 0,42 × 0,508 × 1,055 | X −0,05 … 0,37, Z od 0 | 1000 | kufor, ťahaný, s klapkou | `suitcase.py` |
| `SM_Cart` | 1,26 × 0,67 × 0,99 | X −0,65 … 0,61, Z od 0 | 1500 | vozík, tlačený | `cart.py` |
| `SM_Finisher` | 1,32 × 1,072 × 1,0 | X −1,22 … 0,1, Z od 0 | 2000 | pásový finišer (hra ho od 2026-11-22 nepoužíva, model ostáva v repozitári) | `finisher.py` |
| `SM_Trough` | 1,28 × 0,60 × 0,50 | stred pôdorysu, Z od 0 | 400 | koryto s asfaltom pred výrobňou | `lot_props.py` |
| `SM_Curb` | 0,15 × 2,0 × 0,12 | stred pôdorysu, Z od 0 | 40 | diel obrubníka okolo parkoviska | `lot_props.py` |
| `SM_PaverCar_WideBar` | 0,16 × 3,5 × 0,18 | X −2,32 … −2,16, Z 0,02 … 0,2 | 400 | široká lišta auta (kúpa CarWideBar) | `lot_props.py` |
| `SM_Mall` | 100 × 100 × 14,3 | X, Y −50 … 50 | 4000 | nákupné centrum, zaberá kvadrant parkoviska | `mall.py` |
| `SM_Canister` | 0,29 × 0,23 × 0,40 | stred pôdorysu, Z od 0 | 700 | kanister 20 l v ruke | `vessels.py` |
| `SM_Tub` | 0,51 × 0,58 × 0,22 | X −0,245 … 0,265, Z od 0 | 700 | vaňa 36 l v ruke | `vessels.py` |
| `SM_VehicleStation` | 8,0 × 5,45 × 4,6 | X −4 … 4, Y −2,95 … 2,5, Z od 0 | 3000 | pevná výdajňa asfaltu pre auto a robota (bránka s 4 m jazdným pruhom) | `vehicle_station.py` |

`SM_Mall` má pri importe obdĺžnikovú kolíziu (`BOX_COLLISION_MODELS = {"SM_Mall"}` v `Tools/UE/import_models.py`); ostatné modely majú konvexnú kolíziu s 18 stenami.

### 4.2 Modely z verzie 1, ktoré hra používa

`SM_AutoPaver` (robot), `SM_Barrier`, `SM_ExitGate_Frame` a `SM_ExitGate_Barrier` (brána východu), `SM_Fence`, `SM_Kiosk`, `SM_PaverCar` (auto), `SM_Plant`, `SM_Plant_AddonA`, `SM_Plant_AddonB`, `SM_PortableStation` (výdajňa), `SM_RoadStub`. Ich rozmery sú v `manifest.json` a v `Source/DoItAsphalt/Game/DIAModelSizes.h`.

### 4.3 Modely z verzie 1, ktoré hra už nepoužíva

Ostali v repozitári, hra ich nespúšťa; používajú ich len testy: `SM_AxisTest` (overenie osí), `SM_Backpack_S/M/L/XL`, `SM_Barrel`, `SM_Cone`, `SM_DirtPile`, `SM_Helmet`, `SM_Lamp`, `SM_Lock` a sedem asfaltovačiek `SM_Spreader_020` až `_200`.

## 5. Nové pomocné funkcie v `Tools/ModelGen/common.py`

- `tub(top_xy, bottom_xy, z0, z1, wall, floor, center_xy, mat)`: otvorená obdĺžniková nádoba so šikmými stenami (koryto, násypka): `top_xy` a `bottom_xy` sú vonkajšie rozmery hore a dole, `wall` hrúbka steny, `floor` hrúbka dna. Je vidieť zvnútra aj zvonka, má 36 trojuholníkov. Používa ju `SM_Trough` (`lot_props.py`) a korba vozíka `SM_Cart` (`cart.py`).
- `finish(parts, name, silhouette=True, **preview_options)`: skontroluje orientáciu plôch, zlúči časti do jedného meshu, zapíše GLB a vyrenderuje náhľady. S `silhouette=False` sa model rámuje sám bez sivej siluety postavy (ručné nástroje by vedľa nej boli len pár pixelov).
- `render_previews(..., silhouette, front)`: parameter `front=(x, y, z)` určuje, kde stojí kamera náhľadu „a“ voči modelu (v osiach hry), pre modely, ktorých zaujímavá strana nie je +X / +Y (používa ho `mall.py`).

## 6. Kontrolný zoznam pred odovzdaním modelu

- `check.py` vypísal `OK` (rozmery, počet trojuholníkov, materiály len z palety).
- Na náhľadoch nie sú čierne ani chýbajúce plochy (obrátené normály) a nič sa neprelína tak, aby blikalo.
- Silueta je rozpoznateľná aj v malom náhľade.
- Model stojí na zemi (najnižší bod Z = 0), ak ho hra nedrží v ruke; nevznáša sa a nie je zapustený.
- Použitých je najviac 5 materiálov palety.
