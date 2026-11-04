package net.tisma.cloud

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull

/**
 * A request that failed. [status] is the HTTP code, or 0 when no answer came
 * (no network, a timeout) or the answer could not be read. [detail] is the
 * server's text (`detail`, cloud.md rule 4), which the interface shows.
 */
class CloudException(
    val status: Int,
    val detail: String,
    /** True when no answer came at all. */
    val network: Boolean = false,
    cause: Throwable? = null,
) :
    Exception(detail.ifEmpty { "HTTP $status" }, cause) {

    enum class Kind {
        /** No answer: no network, the server is down or too slow. */
        NETWORK,
        /** The answer was not what the API says. */
        UNEXPECTED,
        /** 401: the session expired or was closed; sign in again (rule 2). */
        EXPIRED,
        /** 402: the plan does not include this calculation (rule 10b). */
        PLAN,
        /** 413: the file is too large (rule 4). */
        TOO_LARGE,
        /** 422: wrong parameters (rule 4). */
        BAD_PARAMS,
        /** 429: too many requests (rule 4). */
        TOO_MANY,
        /** Any other error of the server. */
        SERVER,
    }

    val kind: Kind
        get() = when {
            network -> Kind.NETWORK
            status == 0 -> Kind.UNEXPECTED
            status == 401 -> Kind.EXPIRED
            status == 402 -> Kind.PLAN
            status == 413 -> Kind.TOO_LARGE
            status == 422 -> Kind.BAD_PARAMS
            status == 429 -> Kind.TOO_MANY
            else -> Kind.SERVER
        }

    companion object {
        /**
         * The server's text from an error body: `detail` as text, or the first
         * message of a list of validation errors; else the HTTP code.
         */
        fun detailOf(body: String, status: Int): String {
            val text = runCatching {
                val o = Json.parseToJsonElement(body) as? JsonObject
                when (val d = o?.get("detail")) {
                    is JsonPrimitive -> d.contentOrNull
                    is JsonArray -> ((d.firstOrNull() as? JsonObject)?.get("msg") as? JsonPrimitive)?.contentOrNull
                    else -> null
                }
            }.getOrNull()
            return text?.takeIf { it.isNotBlank() } ?: "HTTP $status"
        }
    }
}
