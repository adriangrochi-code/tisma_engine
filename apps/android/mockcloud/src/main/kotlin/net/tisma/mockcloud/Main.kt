package net.tisma.mockcloud

/**
 * Runs the test server, for trying the app on an emulator (which reaches this
 * machine at 10.0.2.2): ./gradlew :mockcloud:run [-Pport=8765]
 */
fun main(args: Array<String>) {
    val port = args.firstOrNull()?.toIntOrNull() ?: 8765
    val cloud = MockCloud(port = port, host = "0.0.0.0").start()
    println("Tisma Cloud test server on port $port (from an emulator: http://10.0.2.2:$port)")
    println("Accounts (password \"password1\"): ana@example.com (Maker), free@example.com (Free), pro@example.com (Pro)")
    println("Confirmation and reset code: ${cloud.mailedCode}")
    println("Controls: POST /mock/grant {email,hours}, /mock/disable {email}, /mock/plan {email,plan}, /mock/expire,")
    println("          /mock/fail-next {status,detail}, /mock/max-upload {bytes}")
    Thread.currentThread().join()
}
