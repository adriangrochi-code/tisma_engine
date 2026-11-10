import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// Clients of the printers on the network (docs/spec/features/printer_connections.md),
// in plain Kotlin, tested on the JVM against the simulators of :mockprinters.
plugins {
    alias(libs.plugins.kotlin.jvm)
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

kotlin {
    compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
}

dependencies {
    // The HTTP layer and JSON of the cloud client are shared.
    api(project(":cloud"))
    testImplementation(project(":mockprinters"))
    testImplementation(libs.kotlin.test)
    testImplementation(libs.junit)
}

// A phone's small heap: the tests that stream big files must pass within it (BambuPackTest).
tasks.withType<Test>().configureEach { maxHeapSize = "256m" }
