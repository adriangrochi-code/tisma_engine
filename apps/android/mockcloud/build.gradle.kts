import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// A Tisma Cloud test server written from docs/spec/features/cloud.md, for the
// tests and for trying the app on an emulator: ./gradlew :mockcloud:run
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
    mainClass.set("net.tisma.mockcloud.MainKt")
}

tasks.named<JavaExec>("run") {
    args = listOf(project.findProperty("port")?.toString() ?: "8765")
}

dependencies {
    implementation(libs.kotlinx.serialization.json)
}
