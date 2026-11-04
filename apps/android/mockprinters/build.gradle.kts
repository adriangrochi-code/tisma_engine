import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// Printer simulators (Moonraker, OctoPrint) written from their public API
// documentation, for the tests and for an emulator: ./gradlew :mockprinters:run
plugins {
    alias(libs.plugins.kotlin.jvm)
    alias(libs.plugins.kotlin.serialization)
    application
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

kotlin {
    compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
}

application {
    mainClass.set("net.tisma.mockprinters.MainKt")
}

tasks.named<JavaExec>("run") {
    args = listOf(project.findProperty("port")?.toString() ?: "7125")
}

dependencies {
    implementation(libs.kotlinx.serialization.json)
    // The Bambu simulator speaks MQTT with the client's own packet helpers.
    implementation(project(":printers"))
}
