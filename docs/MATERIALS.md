# Materiales impresos

La base de materiales del análisis «como se imprimió» (`src/fea/materials.*`). Cada registro guarda solo los números
que su fuente publica; lo que falta lo completan reglas escritas, cada una con su fuente. Los números son datos
medidos y publicados (no se copió texto ni código); cada uno se puede rastrear hasta la tabla o el párrafo de donde
salió. Todos se tomaron del texto o de las tablas, nunca leídos de un gráfico.

Ejes del material (los de `OrthotropicMaterial`): 1 a lo largo del cordón, 2 a través de los cordones en la capa,
3 a través de las capas. Unidades: MPa.

## Fuentes

### Literatura

| Registro | Fuente | Impresión y ensayo | Qué da | Qué no da |
|---|---|---|---|---|
| `song2017-pla` | Y. Song, Y. Li, W. Song, K. Yee, K.-Y. Lee y V. L. Tagarielli, *Measurements of the mechanical response of unidirectional 3D-printed PLA*, Materials & Design 123:154–164, 2017. Manuscrito aceptado en el repositorio del Imperial College: <https://spiral.imperial.ac.uk/server/api/core/bitstreams/c34d2e00-a3ba-486b-8d40-77599a169575/content> | PLA IMAKR, Flashforge Replicator, boquilla 0,4 mm, 220 °C, capas de 0,2 mm, 60 mm/s, porosidad 1,46 %; todas las capas en la misma dirección; probetas mecanizadas de bloques | Tabla 6: E11 3,98 GPa, E22 4,04 GPa, ν12 0,34, ν23 0,37, G12 1,50 GPa, G23 1,47 GPa (por isotropía transversal). Tabla 4 (2,5·10⁻⁴ s⁻¹): tracción última 54,89 MPa a 0°, 61,42 a 45°, 46,24 a 90°; compresión última 98,40 a 0°, 99,71 a 45°, 98,08 a 90° | Ningún número a través de las capas; el texto dice que esa dirección fue «la más débil y la más frágil» |
| `casavola2016-pla`, `casavola2016-abs` | C. Casavola, A. Cazzato, V. Moramarco y C. Pappalettere, *Orthotropic mechanical properties of fused deposition modelling parts described by classical laminate theory*, Materials & Design 90:453–458, 2016. Preprint en el repositorio del Politecnico di Bari: <https://iris.poliba.it/retrieve/handle/11589/58649/329641/Pre_Print%20Materials%20and%20Design%2090%20(2016)%20453%BF458.pdf> | RepRap Prusa i3, boquilla 0,4 mm, capas de 0,35 mm, cordón de 0,70 mm; ABS a 225 °C (cama 90 °C), PLA a 200 °C (cama 55 °C); ASTM D638 y D3518 | Probetas de una capa. PLA: E1 3,12 GPa, E2 2,77 GPa, ν12 0,330, G12 1246,57 MPa, tracción 50,23 / 40,68 / 22,49 MPa a 0° / 45° / 90°. ABS: 1,79 y 1,15 GPa, ν12 0,344, G12 808,58 MPa, 26,11 / 11,97 / 6,71 MPa | Nada a través de las capas |
| `cantrell2017-abs` | J. Cantrell y otros, *Experimental characterization of the mechanical properties of 3D-printed ABS and polycarbonate parts*, Rapid Prototyping Journal 23(4), 2017. Manuscrito aceptado en la Universidad de Florida: <https://web.mae.ufl.edu/rapidpro/pages/3D%20Printing%20Paper%20Final%20Manuscript.pdf> | ABS, Ultimaker 2, boquilla 0,4 mm, capas de 0,1 mm, 235 °C, cama 105 °C, 100 % de relleno; ASTM D638 tipo IV y D5379 (corte) con correlación de imágenes | Tablas II y IV, trama [+45/−45]: plano E 1960 MPa, ν 0,36, tracción última 32,8 MPa, G 740 MPa, corte último 28,8 MPa; parado (Z) E 2040 MPa, ν 0,36, 30,0 MPa, G 670 MPa, corte 25,8 MPa. También [0/90] y de canto | — |
| (regla) | S.-H. Ahn, M. Montero, D. Odell, S. Roundy y P. K. Wright, *Anisotropic material properties of fused deposition modeling ABS*, Rapid Prototyping Journal 8(4):248–257, 2002 | Stratasys FDM 1650, ABS P400 | Compresión de 80 a 90 % de la del ABS inyectado; las probetas construidas paradas, 15 % menos que las acostadas | Los valores por trama solo están en gráficos de barras: no se usaron |

### Fabricantes

**Bambu Lab.** Hojas técnicas oficiales (store.bblcdn.com, enlazadas desde las páginas de producto de
us.store.bambulab.com). Ensayos ISO 527 / GB/T 1040 (tracción) e ISO 178 / GB/T 9341 (flexión). Probetas con 100 %
de relleno, recocidas y secadas; la barra Z se imprime parada. No dicen la altura de capa ni la boquilla, ni dan un
valor a través de los cordones en la capa.

| Registro | Material (hoja) | E X-Y / Z | Tracción X-Y / Z | Impresión de la probeta | Hoja |
|---|---|---|---|---|---|
| `bambu-pla-basic` | PLA Basic (V3.0) | 2580 ± 220 / 2060 ± 170 | 35 ± 4 / 31 ± 3 | 220 °C, 200 mm/s | <https://store.bblcdn.com/s1/default/58b85d0f3db94878854a28fdb8a0006e/Bambu_PLA_Basic_Technical_Data_Sheet.pdf> |
| `bambu-pla-matte` | PLA Matte (V3.0) | 1960 ± 180 / 1670 ± 140 | 30 ± 5 / 22 ± 4 | 220 °C, 200 mm/s | <https://store.bblcdn.com/s7/default/5b061f2feeac4ba88f355a33248bbda7/Bambu_PLA_Matte_Technical_Data_Sheet.pdf> |
| `bambu-pla-tough-plus` | PLA Tough+ (V3.0) | 1860 ± 70 / 1920 ± 20 | 34,9 ± 1 / 20,9 ± 1 | 245 °C, 150 mm/s | <https://store.bblcdn.com/s7/default/80a85371db2e40bda97069d283fed76c/BambuPLA_Tough_TechnicalData_Sheet_(2).pdf> |
| `bambu-petg-basic` | PETG Basic (V3.0) | 2780 ± 65 / 2550 ± 100 | 51 ± 1 / 35 ± 6 | 255 °C, 150 mm/s | <https://store.bblcdn.com/s1/default/cb94589bf7994fdcbfa833badefae9cd/Bambu_PETG_Basic_Technical_Data_Sheet.pdf> |
| `bambu-petg-hf` | PETG HF (V1.0) | 1810 ± 190 / 1540 ± 130 | 34 ± 4 / 23 ± 4 | 255 °C, 200 mm/s | <https://store.bblcdn.com/3a230e260a3a47c2b0db0156e07eef91.pdf> |
| `bambu-abs` | ABS (V3.0) | 2200 ± 190 / 1960 ± 110 | 33 ± 3 / 28 ± 2 | 260 °C, 200 mm/s | <https://store.bblcdn.com/s7/default/23b4cf2b83d5470bb96d19970b5f3ae8/Bambu_ABS_Technical_Data_Sheet_V3.pdf> |
| `bambu-asa` | ASA (V3.0) | 2450 ± 270 / 2120 ± 260 | 37 ± 3 / 31 ± 4 | 260 °C, 200 mm/s | <https://store.bblcdn.com/ad7b08230c164e72856cffbe06bb7dc9.pdf> |
| `bambu-pla-cf` | PLA-CF (V2.0) | 2790 ± 120 / 2160 ± 90 | 38 ± 4 / 26 ± 2 | 230 °C, 180 mm/s | <https://store.bblcdn.com/842f399d4e274507953a5231126ec8c1.pdf> |
| `bambu-petg-cf` | PETG-CF (V3.0) | 2460 ± 230 / 1340 ± 150 | 35 ± 5 / 29 ± 4 | 255 °C, 150 mm/s | <https://store.bblcdn.com/626e4f424bf345ae965ad0ddfcaf2459.pdf> |
| `bambu-pa6-cf` | PA6-CF (V3.0) | 4430 ± 310 / 2170 ± 230 | 102 ± 7 / 48 ± 6 | 280 °C, 100 mm/s | <https://store.bblcdn.com/b2abc59ac250492b979a52f1ce3e61b3.pdf> |
| `bambu-paht-cf` | PAHT-CF, seco (V3.0) | 3860 ± 230 / 2180 ± 130 | 92 ± 7 / 47 ± 5 | 290 °C, 100 mm/s | <https://store.bblcdn.com/7166b20bc68f4c87b32c972b856eeb4b.pdf> |

Para tener en cuenta:
- La hoja V2.0 de PLA Basic (solo disponible como copia de un revendedor) daba E 2680 / 2160, tracción 39 / 35; la V3.0
  oficial da 2580 / 2060 y 35 / 31. Se usa la V3.0.
- Dos valores raros, tal como están en las hojas oficiales: en PETG Basic el módulo de tracción (2780) supera al de
  flexión (1950); en PLA Tough+ el E en Z (1920) supera al de X-Y (1860).
- Las hojas también dan resistencia a la flexión e impacto ISO 179, que todavía no se usan.

**eSUN.** Ensayos GB/T 1040 (tracción) y GB/T 9341 (flexión). Ninguna hoja da el módulo de Young: se usa el de
flexión. Impresas con 2 paredes, 3 capas sólidas arriba y abajo, 100 % de relleno, Bambu P1S, boquilla 0,4 mm.

| Registro | Producto (hoja) | Tracción X-Y / Z | Módulo de flexión X-Y / Z | Impresión | Hoja |
|---|---|---|---|---|---|
| `esun-pla-plus` | PLA+ (V1.0, febrero de 2026, oficial) | 70,37 / 36,36 | 3259,39 / 2873,94 | 220 °C, cama 65 °C | <https://www.esun3d.com/media/esun/catalog/certification/product/PLA_3KG1.75MM/PLA_-TDS-en.pdf> |
| `esun-petg-basic` | PETG Basic (V1.0 de julio de 2024 y V2.0 de julio de 2025, mismos números) | 68,41 / 35,84 | 2636,66 / 2207,6 | 255 °C | copias de revendedores (la oficial da 404): <https://f00.psgsm.net/product/929592/esun-PETG-Basic-TDS3.pdf>, <https://3d.nice-cdn.com/upload/file/PETG-Basic-TDS.pdf> |

No se usaron:
- Las hojas V4.0 de eSUN (PETG y ABS+, 2021): sus valores salen de probetas inyectadas, no impresas.
- Los valores X-Y / Z de ABS+ de la página de MatterHackers: no aparecen en ninguna hoja de eSUN.

### CNC Kitchen

Ensayos de Stefan Hermann en su propia máquina de tracción, sin norma. «Parada» quiere decir impresa de pie: mide la
unión entre capas. Son la mejor fuente pública de cómo cambia esa unión con las condiciones de impresión.

| Registro o dato | Artículo | Material e impresión | Resultado |
|---|---|---|---|
| `cnckitchen-pla-210`, `cnckitchen-petg-245` | [Temperatura de extrusión](https://www.cnckitchen.com/blog/the-influence-of-extrusion-temperature-on-layer-adhesion) | PLA y PETG de dasFilament, Prusa MK3, minibarras, 4 por temperatura | PLA parada: 190 °C 20 MPa (subextruida), 200 °C 37, 210 °C 39, 230 °C 40, 250 °C 37, 270 °C 32; acostada «alrededor de 60 MPa». PETG parada: 200 °C 18, 215 °C 22, 230 °C 30, 245 °C 32, 260 °C 24; acostada 55 MPa |
| módulos de esos registros | [PLA, PETG y ASA](https://cnckitchen.com/blog/comparing-pla-petg-amp-asa-feat-prusament) | Prusament, perfiles de serie del MK3S | Módulo en flexión de tres puntos: PLA 3300, ASA 2300, PETG 1900 MPa. Gancho, carga de rotura acostado / parado: PLA 73 / 40 kg (55 %), PETG 55 / 25 kg (46 %), ASA 57 / 17 kg (29 %) |
| `cnckitchen-abs-open`, `cnckitchen-abs-chamber-65` | [Cámara calefaccionada](https://www.cnckitchen.com/blog/does-a-heated-chamber-make-your-3d-prints-stronger) | ABS de eSUN, QIDI X-Max 3 | Acostada 39 MPa; parada 21 MPa con la cámara abierta (≤ 26 °C), 23 MPa pasiva (~50 °C), 29 MPa a 65 °C (75 % de la acostada) |
| `cnckitchen-petg-clear` | [PETG transparente](https://www.cnckitchen.com/blog/transparent-fdm-3d-prints-are-clearly-stronger) | Capas de 0,12 mm, cordón de 0,50 mm, 15 mm/s, sin ventilador | Ajustes de referencia 52 / 34 MPa (65 %); ajustes «transparentes» 59 / 44 MPa (75 %) |
| (dato) | [Ancho de extrusión](https://www.cnckitchen.com/blog/the-effect-of-extrusion-width-on-strength-and-quality-of-3d-prints) | PLA SpoolWorks, MK2S, 210 °C, capas de 0,16 mm | La unión entre capas es máxima hacia el 150 % del ancho (solo en gráfico); ganchos parados: 2 paredes al 100 % 20 kg, 4 al 100 % 33 kg, 3 al 133 % 37 kg, 2 al 200 % 39 kg |

Los registros de CNC Kitchen toman los módulos de otra fuente: los de PLA y PETG, del módulo en flexión que el mismo
autor midió en Prusament del mismo polímero; los de ABS, del registro `cantrell2017-abs`. El 60 MPa del PLA acostado es
aproximado («alrededor de»). La serie de temperaturas y la de la cámara quedan para el modelo de la unión entre capas
según la temperatura (A3, simulación del enfriamiento).

## Reglas para lo que la fuente no da

Están en `as_printed` (`src/fea/materials.hpp`), en este orden:

| Constante | Regla | Fuente de la regla |
|---|---|---|
| e1 | A lo largo del cordón; si no, el X-Y de trama mixta; si no, el de flexión | — |
| e2 | A través de los cordones; si no, e1 | Song 2017: E22 a 2 % de E11 en impresión densa |
| e3 | Z; si no, flexión Z; si no, e2 | Cantrell 2017: el módulo parado igual al acostado |
| ν12, ν13, ν23 | Dado; si no, 0,35; ν13 = ν12; ν23 = ν12 si no se da | Las fuentes dan de 0,33 a 0,38 |
| G12 | Dado; si no, e1 / (2 (1 + ν12)) | Song 2017 y Cantrell 2017 lo cumplen a 3 % |
| G13 = G23 | Dado; si no, min(e2, e3) / (2 (1 + ν23)) | Song 2017 (isotropía transversal) |
| Xt | A lo largo del cordón; si no, el X-Y de trama mixta | Es lo único que dan las hojas técnicas |
| Yt | A través de los cordones; si no, 0,842 Xt (46,24 / 54,89), pero no menos que el Z medido | Song 2017, tabla 4 |
| Zt | Z; si no, 0,65 Xt (39 / 60), pero no más que Yt | CNC Kitchen, PLA a 210 °C; Song 2017: Z es la dirección más débil |
| Xc, Yc | Dados; si no, iguales a la tracción (del lado seguro: Song 2017 midió casi el doble) | — |
| Zc | max(Zt, 0,85 Xc) | Ahn 2002: compresión de probetas paradas 15 % menor |
| S12 | Dado; si no, 0,878 Yt (28,8 / 32,8) | Cantrell 2017, acostada |
| S13 = S23 | Dado; si no, 0,86 Zt (25,8 / 30,0) | Cantrell 2017, parada |

Límites que hay que tener presentes:
- Las hojas técnicas miden barras con trama mixta: su X-Y no es la resistencia a lo largo de un cordón (Song 2017
  midió más a 45° que a 0°).
- Ninguna fuente de fabricante dice la altura de capa; la unión entre capas depende mucho de ella, de la temperatura y
  del ventilador (CNC Kitchen). Los valores valen para las condiciones de cada ensayo.
- Ningún PETG impreso con filamento tiene publicadas las nueve constantes ortótropas que se hayan podido conseguir;
  Romeijn y otros 2022 (Additive Manufacturing, doi 10.1016/j.addma.2022.103145) las da para PETG extruido de pellets,
  pero no se pudo leer.
