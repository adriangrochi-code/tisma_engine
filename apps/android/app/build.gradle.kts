plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

// The engine's build settings (see «The engine of the desktop» below).
val engineNdk = "29.0.14206865"
val engineAbis = listOf("arm64-v8a", "x86_64")
val engineLibs = layout.buildDirectory.dir("engine/jniLibs")
val buildEngine = (project.findProperty("tisma.engine")?.toString() ?: "true") != "false"
val repoRoot = rootProject.projectDir.parentFile.parentFile
// The engine's catalog of printers and filaments (docs/CATALOG.md), the same files the desktop embeds.
val catalogAssets = layout.buildDirectory.dir("catalog/assets")

android {
    namespace = "net.tisma.app"
    compileSdk = 36
    ndkVersion = engineNdk

    defaultConfig {
        // To be confirmed by the owner before the first public release: an
        // application id cannot change once the app is published.
        applicationId = "net.tismaservidores.tisma"
        minSdk = 28
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    sourceSets["main"].jniLibs.srcDir(engineLibs)
    sourceSets["main"].assets.srcDir(catalogAssets)

    packaging {
        // The engine is a program, not a library: it must be on disk, unpacked,
        // to run (nativeLibraryDir is the one place an app may run files from).
        jniLibs.useLegacyPackaging = true
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
    }
}

kotlin {
    compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) }
}

// ---- The engine of the desktop, built for the phone (mobile_app.md, rule 1) ----
//
// The same `tisma-engine` program the desktop and the cloud nodes run
// (cloud.md, rule 18), built from the repository's root with the NDK for
// each ABI, without the interface and without the GPU, and packed as
// lib/<abi>/libtisma_engine.so so Android unpacks it where it may run.
// -Ptisma.engine=false skips it (the app then offers only the cloud).

val engineTasks = engineAbis.map { abi ->
    tasks.register<Exec>("engine_" + abi.replace("-", "_")) {
        description = "Builds tisma-engine for $abi with the NDK"
        val sdk = android.sdkDirectory
        val ndk = File(sdk, "ndk/$engineNdk")
        val work = layout.buildDirectory.dir("engine/$abi").get().asFile
        val out = engineLibs.get().dir(abi).file("libtisma_engine.so").asFile
        inputs.dir(File(repoRoot, "src"))
        inputs.file(File(repoRoot, "CMakeLists.txt"))
        outputs.file(out)
        val cmake = "cmake"
        commandLine("bash", "-c", listOf(
            "set -e",
            "$cmake -S '$repoRoot' -B '$work' -G Ninja -DCMAKE_TOOLCHAIN_FILE='$ndk/build/cmake/android.toolchain.cmake' " +
                "-DANDROID_ABI=$abi -DANDROID_PLATFORM=28 -DCMAKE_BUILD_TYPE=Release " +
                "-DTISMA_BUILD_TESTS=OFF -DTISMA_GPU=OFF -DTISMA_BUILD_UI=OFF > '$work.configure.log'",
            "$cmake --build '$work' --target tisma-engine",
            "mkdir -p '${out.parentFile}'",
            "'$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip' -o '$out' '$work/tisma-engine'",
        ).joinToString(" && "))
    }
}

val copyCatalog = tasks.register<Copy>("copyCatalog") {
    from(File(repoRoot, "resources/catalog")) { include("printers.json", "filaments.json") }
    into(catalogAssets.map { it.dir("catalog") })
}
// The credits and license notices (docs/spec/features/credits.md): the engine's, the same as the
// desktop's, and the app's own.
val copyCredits = tasks.register<Copy>("copyCredits") {
    from(File(repoRoot, "resources")) { include("credits.json", "licenses/**") }
    from(rootProject.file("credits_android.json"))
    from(rootProject.file("licenses")) { into("licenses") }
    into(catalogAssets.map { it.dir("credits") })
}
tasks.named("preBuild") { dependsOn(copyCatalog, copyCredits) }

if (buildEngine) {
    tasks.named("preBuild") { dependsOn(engineTasks) }
}

dependencies {
    implementation(project(":cloud"))
    implementation(project(":printers"))
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.androidx.core)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)

    testImplementation(project(":mockcloud"))
    testImplementation(project(":mockprinters"))
    testImplementation(libs.junit)
    testImplementation(libs.kotlin.test)
    testImplementation(libs.robolectric)
    testImplementation(libs.androidx.test.core)
    testImplementation(libs.kotlinx.coroutines.test)
    testImplementation(platform(libs.compose.bom))
    testImplementation(libs.compose.ui.test.junit4)
    debugImplementation(libs.compose.ui.test.manifest)
}
