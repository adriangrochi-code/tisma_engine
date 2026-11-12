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
object EngineBridge {
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
