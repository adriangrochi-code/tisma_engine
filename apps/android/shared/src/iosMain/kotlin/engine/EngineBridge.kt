package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import net.tisma.io.lines

/** A slice's summary as the iPhone shows it (the engine's `--summary`, the same keys Android reads). */
data class SliceSummary(val printTime: String, val printTimeS: Double, val filamentMm: Double, val filamentG: Double, val layers: Int)

/**
 * The engine's catalog and summaries for the SwiftUI screens (mobile_app.md, rules 11 and 12). The engine itself runs
 * in Swift (tisma_run_cli); this reads what it needs and what it writes, with the same code as Android.
 */
/**
 * What the plate's printer card draws (settings_panel.md, rules 34 to 36): the printer's [shape] (open, enclosed, delta,
 * belt, resin, pellets), the nozzle's look, and, for a Bambu Lab, its plate's [bed] family and name ([plate], empty for
 * the printer's own choice).
 */
data class PrinterCardLook(val shape: String, val hardened: Boolean, val highFlow: Boolean, val bambu: Boolean,
                           val bed: String, val plate: String, val bedArea: List<Double> = listOf(0.0, 0.0, 220.0, 220.0))

object EngineBridge {
    /** The card's look from the printer's profile (`tisma-engine profile`'s JSON) and its name. */
    fun cardLook(profileJson: String, name: String): PrinterCardLook {
        val values = runCatching { (Json.parseToJsonElement(profileJson) as? JsonObject)?.let { SettingsLayout.flatten(it) } }
            .getOrNull().orEmpty()
        fun text(k: String) = (values[k] as? JsonPrimitive)?.content.orEmpty()
        val plate = text("bambu.plate")
        // The bed's outline (machine.bed, [x, y] corners) as min x, min y, max x, max y, for the plate's 3D view.
        val corners = (values["machine.bed"] as? kotlinx.serialization.json.JsonArray).orEmpty().mapNotNull { c ->
            val a = c as? kotlinx.serialization.json.JsonArray
            val x = (a?.getOrNull(0) as? JsonPrimitive)?.doubleOrNull; val y = (a?.getOrNull(1) as? JsonPrimitive)?.doubleOrNull
            if (x != null && y != null) x to y else null
        }
        val area = if (corners.size < 3) listOf(0.0, 0.0, 220.0, 220.0)
            else listOf(corners.minOf { it.first }, corners.minOf { it.second }, corners.maxOf { it.first }, corners.maxOf { it.second })
        return PrinterCardLook(printerShape { values[it] }.name.lowercase(), text("machine.hardened_nozzle") == "true",
            highFlow(name), name.contains("Bambu", ignoreCase = true), bedFamily(plate).name.lowercase(), plate, area)
    }

    /** The catalog of printers and filaments the app carries (resources/catalog), or null if it does not read. */
    fun catalog(printersJson: String, filamentsJson: String): Catalog? = runCatching { Catalog.parse(printersJson, filamentsJson) }.getOrNull()

    /** The arguments of `tisma-engine profile` for a printer of the catalog, its nozzle and a filament. */
    fun profileArgs(printer: String, nozzle: Double, filament: String?, out: String): List<String> =
        listOf("profile", "--printer", printer, "--nozzle", nozzle.toString(), "-o", out) +
            (filament?.let { listOf("--filament", it) } ?: emptyList())

    /** The preview's layers of the G-code at [path] (read line by line), or null when it does not read. */
    fun preview(path: String): GcodeLayers? = runCatching {
        net.tisma.io.FileInput(path).use { GcodeLayers.read(it.lines()) }
    }.getOrNull()

    /**
     * The printer's profile at [profilePath] with the person's changes over it, written to [outPath] for --config.
     * Temperatures of 0 or less keep the filament's.
     */
    fun applyChanges(profilePath: String, outPath: String, layerHeight: Double, infillPercent: Double, supports: Boolean,
                     nozzleTemp: Double, bedTemp: Double): Boolean = runCatching {
        val text = net.tisma.io.FileInput(profilePath).use { it.lines().joinToString("\n") }
        val merged = PrintChanges(layerHeight, infillPercent, supports, nozzleTemp.takeIf { it > 0 }, bedTemp.takeIf { it > 0 }).over(text)
        net.tisma.io.FileOutput(outPath).use { val b = merged.encodeToByteArray(); it.write(b, 0, b.size) }
        true
    }.getOrDefault(false)

    /** Reads `--summary`; null when the text is not a summary. */
    fun summary(json: String): SliceSummary? {
        val o = runCatching { Json.parseToJsonElement(json) as? JsonObject }.getOrNull() ?: return null
        fun num(k: String) = (o[k] as? JsonPrimitive)?.doubleOrNull ?: 0.0
        return SliceSummary((o["print_time"] as? JsonPrimitive)?.contentOrNull.orEmpty(), num("print_time_s"),
            num("filament_mm"), num("filament_g"), num("layers").toInt())
    }
}
