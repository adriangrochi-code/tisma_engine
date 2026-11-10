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
        versionCode = 2
        versionName = "0.1.0-beta.1"
        // The project's Ed25519 public key for the updates (app_updates.md, rules 5 and 13), as 64 hex digits:
        // -Ptisma.updatesKey=…; empty until the owner makes the key, and then the updater stays off.
        buildConfigField("String", "UPDATES_KEY", "\"" + (project.findProperty("tisma.updatesKey") as String? ?: "") + "\"")
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
        // For testers: shrunk by R8 (the arm64 APK goes from about 33 MB to 9 MB, without SolidCraft) and signed
        // with the debug key until the owner's release key exists. The tests run on debug: a beta is smoke-tested
        // on an emulator before it is handed out.
        create("beta") {
            initWith(getByName("release"))
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
            signingConfig = signingConfigs.getByName("debug")
            matchingFallbacks += "release"
        }
    }

    // One APK per architecture, as well as the one with both: phones only need arm64-v8a, which leaves out the
    // other copy of the engine and of SolidCraft (about half the APK); x86_64 is for emulators.
    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "x86_64")
            isUniversalApk = true
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
        // The Bambu Lab mock's TLS under Robolectric reads java.net internals (the camera test, CM2).
        // Arguments to the engine's process in UTF-8, as on Android (a note «Salió bien», local_library.md rule 1).
        unitTests.all {
            it.jvmArgs("--add-opens", "java.base/java.net=ALL-UNNAMED")
            it.environment("LC_ALL", "C.UTF-8")
        }
    }
}

kotlin {
    compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17); optIn.add("kotlin.time.ExperimentalTime") }
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

// ---- SolidCraft for Design (owner's decision, 2026-11-04) ----
//
// The `solidcraft-cli` the workflow «SolidCraft for Android» builds (.github/workflows/solidcraft-android.yml):
// -Ptisma.solidcraft=<the downloaded artifacts' folder, with <abi>/solidcraft-cli> packs it as
// lib/<abi>/libsolidcraft.so beside the engine, and its licence files with the credits. Without it, the app
// offers the engine's own parts only.
val solidcraftDir = project.findProperty("tisma.solidcraft")?.toString()?.let { File(it) }
if (solidcraftDir != null) {
    // Its own folder, so a later build without the property does not carry it.
    val solidcraftLibs = layout.buildDirectory.dir("solidcraft/jniLibs")
    android.sourceSets["main"].jniLibs.srcDir(solidcraftLibs)
    val copySolidcraft = tasks.register<Copy>("copySolidcraft") {
        engineAbis.forEach { abi ->
            from(File(solidcraftDir, "$abi/solidcraft-cli")) { into(abi); rename { "libsolidcraft.so" } }
        }
        into(solidcraftLibs)
    }
    val copySolidcraftLicences = tasks.register<Copy>("copySolidcraftLicences") {
        from(solidcraftDir) { include("LICENSE-MIT", "LICENSE-APACHE", "NOTICE", "THIRD-PARTY-LICENSES.txt"); into("licenses/solidcraft") }
        into(catalogAssets.map { it.dir("credits") })
    }
    tasks.named("preBuild") { dependsOn(copySolidcraft, copySolidcraftLicences) }
    copySolidcraftLicences.configure { mustRunAfter(copyCredits) }
}

dependencies {
    implementation(project(":shared"))
    implementation(project(":cloud"))
    implementation(project(":printers"))
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.androidx.core)
    implementation(libs.androidsvg)
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
