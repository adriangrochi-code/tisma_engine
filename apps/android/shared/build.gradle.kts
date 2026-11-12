import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// Logic shared by the Android and iPhone apps (apps/ios/PLAN.md): no Android and no java.* in commonMain.
plugins {
    alias(libs.plugins.kotlin.multiplatform)
    alias(libs.plugins.kotlin.serialization)
}

kotlin {
    jvm { compilerOptions { jvmTarget.set(JvmTarget.JVM_17) } }
    // The iPhone app links this as a static framework named TismaShared (apps/ios/PLAN.md, step 3).
    listOf(iosArm64(), iosSimulatorArm64()).forEach { t ->
        t.binaries.framework { baseName = "TismaShared"; isStatic = true }
    }
    compilerOptions { freeCompilerArgs.add("-Xexpect-actual-classes"); optIn.addAll("kotlin.time.ExperimentalTime", "kotlin.io.encoding.ExperimentalEncodingApi") }

    sourceSets {
        commonMain.dependencies {
            api(libs.kotlinx.coroutines.core)
            api(libs.kotlinx.serialization.json)
        }
        commonTest.dependencies {
            implementation(kotlin("test"))
            implementation(libs.kotlinx.coroutines.test)
        }
    }
}

