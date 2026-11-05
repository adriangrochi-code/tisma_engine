# Firmar las versiones (guía para el dueño)

Los programas de Tisma se actualizan solos (`docs/spec/features/app_updates.md`). Solo instalan lo que está firmado
con la **clave del proyecto**. La clave privada vive **únicamente en tu pendrive**: nunca en GitHub, en los secretos de
GitHub ni en ningún repositorio (decisión del dueño, 2026-11-04).

Lo que pasa en cada versión:

1. La CI de GitHub arma los paquetes y deja la versión como **borrador**, con una lista `updates.json` sin firmar.
2. Vos firmás en tu computadora con la clave del pendrive (`tools/sign_release.py`). El script comprueba todo, sube
   la lista firmada y publica la versión.

Si el borrador no se firma, nadie recibe la actualización: no hay nada que deshacer.

## Lo que hace falta en la computadora (una vez)

- **Git para Windows**, que trae **Git Bash** y, dentro, `openssl` 3. Todos los comandos de esta guía van en Git Bash.
- **Python 3**, de python.org: al instalarlo, marcá «Add python.exe to PATH».
- **GitHub CLI** (`gh`), de cli.github.com. Después, una vez: `gh auth login` (elegí GitHub.com y el navegador).
- Una copia del repositorio `tisma_engine` (para tener `tools/sign_release.py`):
  `git clone https://github.com/adriangrochi-code/tisma_engine`.

## Crear la clave (una sola vez en la vida del proyecto)

1. Enchufá el pendrive. En Git Bash, la unidad `E:` es `/e` (cambiá la letra si es otra).
2. Creá la clave en el pendrive:

   ```sh
   mkdir -p /e/tisma
   openssl genpkey -algorithm ed25519 -out /e/tisma/clave.pem
   ```

3. **Hacé una segunda copia** en otro pendrive y guardalo en otro lugar (si se pierden los dos, los programas ya
   instalados no van a aceptar más actualizaciones):

   ```sh
   cp /e/tisma/clave.pem /f/tisma/clave.pem
   ```

4. Sacá la **clave pública** (64 letras y números). No es secreta:

   ```sh
   cd tisma_engine
   python tools/sign_release.py public-key --key /e/tisma/clave.pem
   ```

5. Poné la clave pública en GitHub, para que los programas la lleven adentro:
   - en el repositorio `tisma_engine`: **Settings › Secrets and variables › Actions › Variables › New repository
     variable**;
   - nombre `TISMA_UPDATES_KEY`, valor la clave pública. Es una **variable**, no un secreto: es pública.
6. Pasale la misma clave pública a la sesión de Android: la app la toma con `-Ptisma.updatesKey=…`.

Nunca subas `clave.pem` a ningún lado, ni la mandes por mensaje o correo. Si alguna vez se copia a otra
computadora, avisá: hay que hacer una clave nueva y los programas ya instalados se actualizan a mano una vez.

## Firmar cada versión

1. Cuando la CI termina, la versión aparece en GitHub, en **Releases**, como **Draft** (por ejemplo `v1.0.0`).
2. Enchufá el pendrive y, en Git Bash, dentro de `tisma_engine` (con `git pull` antes):

   ```sh
   python tools/sign_release.py sign v1.0.0 --key /e/tisma/clave.pem --public-key <la clave pública>
   ```

   El script:
   - baja el borrador;
   - comprueba cada paquete contra su tamaño y su SHA-256;
   - firma cada paquete y la lista;
   - comprueba las firmas con la clave pública;
   - sube `updates.json` y `updates.json.sig`;
   - publica la versión;
   - pone la lista en la versión `updates`, que es la que leen los programas.
3. Al final dice «publicada». Desenchufá el pendrive.

Si algo no coincide, el script **no publica nada** y dice qué pasó:

| Mensaje | Qué hacer |
|---|---|
| «la clave no es la de la clave pública dada: ¿es el pendrive correcto?» | Enchufaste otro pendrive o pusiste otra clave pública |
| «su tamaño o su SHA-256 no coincide… ¿se bajó mal?» | Probá de nuevo; si sigue, avisá: el borrador puede estar dañado |
| «gh … no está instalado» u «openssl … no está instalado» | Falta instalar algo de la lista de arriba, o no estás en Git Bash |

Para probar sin publicar: agregá `--no-upload`.

## Por qué así

- Con la clave solo en el pendrive, aunque alguien entre a la cuenta de GitHub no puede publicar una actualización
  que los programas acepten.
- La firma es Ed25519 (RFC 8032), la misma que comprueba la app de Android. Cada paquete lleva su firma dentro de la
  lista, y la lista lleva la suya en `updates.json.sig`.
