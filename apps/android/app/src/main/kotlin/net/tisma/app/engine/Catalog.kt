package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull

/**
 * The engine's catalog of printers and filaments (docs/CATALOG.md), read from
 * the same JSON files the desktop embeds, for the phone's printer card and
 * pickers (mobile_app.md, rules 6 and 11). The profile itself is made by the
 * engine (EngineRunner.profile).
 */
class Catalog(val printers: List<PrinterModel>, val filaments: List<FilamentModel>) {

    data class PrinterModel(
        val vendor: String,
        val model: String,
        val buildVolume: List<Double>,
        val nozzles: List<Double>,
        val filamentDiameter: Double,
        val firmware: String,
    ) {
        /** The name the engine finds it by. */
        val name get() = "$vendor $model"
    }

    data class FilamentModel(val vendor: String, val name: String, val material: String, val diameter: Double) {
        val fullName get() = "$vendor $name"
    }

    /**
     * Filaments that fit a printer (same diameter): the printer's own brand first, then PLA (the usual first
     * material), then the rest by material and name. QA: a Bambu Lab printer listed «Anycubic ABS» first.
     */
    fun filamentsFor(p: PrinterModel?): List<FilamentModel> =
        filaments.filter { p == null || it.diameter == 0.0 || kotlin.math.abs(it.diameter - p.filamentDiameter) < 0.01 }
            .sortedWith(compareBy({ if (p != null && sameBrand(it.vendor, p.vendor)) 0 else 1 },
                { if (it.material.equals("PLA", ignoreCase = true)) 0 else 1 }, { it.material }, { it.fullName }))

    /** «Bambu Lab» and «BambuLab» are one brand. */
    private fun sameBrand(a: String, b: String) = a.filter { it.isLetterOrDigit() }.equals(b.filter { it.isLetterOrDigit() }, ignoreCase = true)

    fun search(query: String): List<PrinterModel> {
        val words = query.lowercase().split(' ').filter { it.isNotBlank() }
        return printers.filter { p -> words.all { p.name.lowercase().contains(it) } }.sortedBy { it.name.lowercase() }
    }

    fun printer(name: String?): PrinterModel? = printers.firstOrNull { it.name == name }
    fun filament(name: String?): FilamentModel? = filaments.firstOrNull { it.fullName == name }

    companion object {
        fun parse(printersJson: String, filamentsJson: String): Catalog {
            fun arr(t: String) = (Json.parseToJsonElement(t) as? JsonArray)?.filterIsInstance<JsonObject>() ?: emptyList()
            fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
            fun JsonObject.d(k: String) = (this[k] as? JsonPrimitive)?.doubleOrNull ?: 0.0
            fun JsonObject.ds(k: String) = (this[k] as? JsonArray)?.mapNotNull { (it as? JsonPrimitive)?.doubleOrNull } ?: emptyList()
            val printers = arr(printersJson).map {
                PrinterModel(it.s("vendor"), it.s("model"), it.ds("build_volume"), it.ds("nozzle_diameters").ifEmpty { listOf(0.4) },
                    it.d("filament_diameter").takeIf { d -> d > 0 } ?: 1.75, it.s("firmware"))
            }
            val filaments = arr(filamentsJson).map { FilamentModel(it.s("vendor"), it.s("name"), it.s("material"), it.d("diameter")) }
            return Catalog(printers, filaments)
        }
    }
}
