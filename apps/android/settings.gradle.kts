// The Tisma Android app (docs/spec/features/mobile_app.md). It is built with
// Gradle on its own and is not part of the engine's CMake build.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "tisma-android"

// cloud: the Tisma Cloud client, plain Kotlin (no Android), tested on the JVM.
// mockcloud: a test server written from docs/spec/features/cloud.md.
// printers: clients of the printers on the network (printer_connections.md).
// mockprinters: printer simulators written from the public API documentation.
// shared: Kotlin Multiplatform logic shared with the iPhone app (apps/ios/PLAN.md).
// app: the Android app.
include(":shared", ":cloud", ":mockcloud", ":printers", ":mockprinters", ":app", ":baselineprofile")
