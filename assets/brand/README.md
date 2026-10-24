# Tisma brand assets

Logos, application icons, splash screens and interface icons of Tisma, for the engine's interface. They belong to the
project owner: the logos are the owner's official artwork and every other file was drawn for Tisma from them. None of
them comes from another program. They are images, not code; using them does not bring any code into this repository.

## Files

| Folder | Files | What they are |
|---|---|---|
| `logos/` | `tisma_isotipo.jpg` (1024×1536), `tisma_logo_horizontal.jpg` (1600×900), `tisma_logo_vertical.jpg` (710×1080) | The owner's official logos: the symbol alone, and the symbol with the "TISMΛ" wordmark and the tagline. |
| `app/` | `tisma.svg`, `tisma_{32,128,192,512}px.png`, `tisma_mac_128px.png`, `tisma.ico` (16 to 256 px) | Application icon: the pyramid in the hexagon. The `.ico` uses a heavier outline at 32 px and below. |
| `app/` | `tisma_192px_transparent.png`, `tisma_192px_grayscale.png` | The same icon with a transparent hexagon, and in gray (disabled or "not connected" states). |
| `app/` | `tisma_gcodeviewer.svg`, `tisma_gcodeviewer_{32,128,192}px.png`, `tisma_gcodeviewer_mac_128px.png`, `tisma_gcodeviewer.ico` | Icon of the G-code viewer: the application icon with a badge. |
| `splash/` | `tisma_splash.jpg`, `tisma_splash_gcodeviewer.jpg` (600×600) | Start screens with the logo, the wordmark and the tagline. |
| `icons/` | 21 `tisma_*.svg` | Interface icons (list below). |

### Interface icons

All are 24×24 (`viewBox="0 0 24 24"`) and drawn in one gray, `#808080`: recolor that gray to the text color of the
light or dark theme.

| Icon | Use |
|---|---|
| `tisma_plate` | The plate (a bed with a part on it). |
| `tisma_print` | Print settings (stacked layers). |
| `tisma_filament` | Filament (a spool). |
| `tisma_printer` | Printer. |
| `tisma_more` | More (three dots). |
| `tisma_folder` | Open a file. |
| `tisma_save` | Save or export. |
| `tisma_slice` | Slice. |
| `tisma_send` | Send to a printer (a paper plane). |
| `tisma_engineering` | Engineering, structural analysis. |
| `tisma_calibrate` | Calibrations (a calibration tower). |
| `tisma_vibration` | Vibration (input shaping) calibration. |
| `tisma_view` | Views (a cube). |
| `tisma_view_top`, `tisma_view_front`, `tisma_view_back`, `tisma_view_left`, `tisma_view_right` | Standard views. |
| `tisma_zoom_in`, `tisma_zoom_out`, `tisma_zoom_fit` | Zoom in, zoom out, frame the part. |

## Palette

Sampled from the official logo.

| Name | Color | Use |
|---|---|---|
| Purple | `#7A24C9` | Brand color: accents, the main button, selection. |
| Purple light | `#9D40E9` | Top of the pyramid; hover. |
| Purple dark | `#5B0FA7` | Bottom of the pyramid; pressed. |
| Lavender | `#D9BDF5` | Soft accent backgrounds. |
| Ink | `#0B0B0B` | Outline of the logo, text on light backgrounds. |
| Surface | `#F2F2F4` | Light background of the splash. |
| Facet gray | `#B2B2BC` to `#DCDCE0` | Side facets of the pyramid. |
