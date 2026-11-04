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
    // Bambu Lab listens on the loopback only in this simulator: from an emulator, use `adb reverse`.
    val bambu = MockBambu().start()
    println("Bambu Lab simulator: ${bambu.url}, access code \"${bambu.accessCode}\" (adb reverse tcp:${bambu.mqttPort} tcp:${bambu.mqttPort}; same for ${bambu.ftpPort})")
    println("Upload a G-code with «print» to start; each status read moves the print forward.")
    Thread.currentThread().join()
}
