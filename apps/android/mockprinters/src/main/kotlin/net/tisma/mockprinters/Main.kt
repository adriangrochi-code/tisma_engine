package net.tisma.mockprinters

/**
 * Runs a simulated Klipper (Moonraker) and a simulated OctoPrint, for trying
 * the app on an emulator (which reaches this machine at 10.0.2.2):
 * ./gradlew :mockprinters:run [-Pport=7125]   (OctoPrint on the next port)
 */
fun main(args: Array<String>) {
    val port = args.firstOrNull()?.toIntOrNull() ?: 7125
    MockMoonraker(port, "0.0.0.0").start()
    val octo = MockOctoPrint(port + 1, "0.0.0.0").start()
    val prusa = MockPrusaLink(port + 2, "0.0.0.0").start() as MockPrusaLink
    MockDuet(port + 3, "0.0.0.0").start()
    println("Moonraker simulator: http://10.0.2.2:$port from an emulator (no key)")
    println("OctoPrint simulator: http://10.0.2.2:${port + 1} from an emulator, API key \"${octo.apiKey}\"")
    println("PrusaLink simulator: http://10.0.2.2:${port + 2}, user \"${prusa.user}\", password \"${prusa.password}\"")
    println("Duet simulator: http://10.0.2.2:${port + 3}, board password \"reprap\"")
    // Bambu Lab listens on the loopback only in this simulator: from an emulator, `adb reverse` its MQTT, FTPS and
    // FTPS data ports (port + 5, + 6 and + 8; the camera on + 7). What it received is on HTTP at port + 4.
    val bambu = MockBambu(mqtt = port + 5, ftp = port + 6, camera = port + 7, passivePort = port + 8).start()
    com.sun.net.httpserver.HttpServer.create(java.net.InetSocketAddress("0.0.0.0", port + 4), 0).apply {
        createContext("/_tisma/received") { ex ->
            val body = bambu.received().toByteArray()
            ex.responseHeaders.add("Content-Type", "application/json")
            ex.sendResponseHeaders(200, body.size.toLong())
            ex.responseBody.use { it.write(body) }
        }
        start()
    }
    // The beta's brands (printer_connections.md, FF1, EL1, CR1), on the loopback too: `adb reverse` their ports.
    val flashforge = MockFlashforge(http = port + 10, tcp = port + 11).start()
    val elegoo = MockElegoo(port = port + 12).start()
    val creality = MockCreality(port = port + 13).start()
    creality.push("""{"model":"","modelVersion":"F012","hostname":"K1C-gate","printJobTime":0,"printLeftTime":0}""")
    println("Flashforge simulator: ${flashforge.url}, serial \"${flashforge.serial}\", check code \"${flashforge.checkCode}\"")
    println("Elegoo simulator: ${elegoo.url}; Creality (stock) simulator: ws on ${creality.port}")
    println("Bambu Lab simulator: ${bambu.url}, access code \"${bambu.accessCode}\" (adb reverse tcp:${bambu.mqttPort} tcp:${bambu.mqttPort}; same for ${bambu.ftpPort} and ${port + 8}); received: http://10.0.2.2:${port + 4}/_tisma/received")
    println("Upload a G-code with «print» to start; each status read moves the print forward.")
    Thread.currentThread().join()
}
