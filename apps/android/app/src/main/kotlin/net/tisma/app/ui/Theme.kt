package net.tisma.app.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.foundation.layout.RowScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color

// The colors of interface.md («Colores»), from the Tisma logo (assets/brand/README.md).

/** Colors of the table that Material 3 has no slot for. */
data class TismaColors(
    /** Text in the accent color: #7A24C9 on light, #C9A2F5 on dark. */
    val accentText: Color,
    val accentHover: Color,
    val accentPressed: Color,
    val mutedBackground: Color,
    val mutedText: Color,
    /** The 3D view: the same dark gradient in both themes. */
    val viewTop: Color,
    val viewBottom: Color,
    val success: Color,
)

private val Accent = Color(0xFF7A24C9)

private val LightScheme: ColorScheme = lightColorScheme(
    primary = Accent,
    onPrimary = Color.White,
    primaryContainer = Color(0xFFECE1F9),
    onPrimaryContainer = Accent,
    secondary = Accent,
    onSecondary = Color.White,
    secondaryContainer = Color(0xFFECE1F9),
    onSecondaryContainer = Accent,
    background = Color(0xFFF7F7F9),
    onBackground = Color(0xFF1A1A1F),
    surface = Color(0xFFF7F7F9),
    onSurface = Color(0xFF1A1A1F),
    surfaceVariant = Color(0xFFECECF0),
    onSurfaceVariant = Color(0xFF6B6B76),
    surfaceContainerLowest = Color(0xFFFFFFFF),
    surfaceContainerLow = Color(0xFFFFFFFF),
    surfaceContainer = Color(0xFFFFFFFF),
    surfaceContainerHigh = Color(0xFFFFFFFF),
    surfaceContainerHighest = Color(0xFFFFFFFF),
    outline = Color(0xFFE2E2E8),
    outlineVariant = Color(0xFFE2E2E8),
    error = Color(0xFFB3261E),
)

private val DarkScheme: ColorScheme = darkColorScheme(
    primary = Accent,
    onPrimary = Color.White,
    primaryContainer = Color(0xFF3B2A55),
    onPrimaryContainer = Color(0xFFC9A2F5),
    secondary = Accent,
    onSecondary = Color.White,
    secondaryContainer = Color(0xFF3B2A55),
    onSecondaryContainer = Color(0xFFC9A2F5),
    background = Color(0xFF1F1F24),
    onBackground = Color(0xFFECECF0),
    surface = Color(0xFF1F1F24),
    onSurface = Color(0xFFECECF0),
    surfaceVariant = Color(0xFF303038),
    onSurfaceVariant = Color(0xFF9A9AA6),
    surfaceContainerLowest = Color(0xFF27272D),
    surfaceContainerLow = Color(0xFF27272D),
    surfaceContainer = Color(0xFF27272D),
    surfaceContainerHigh = Color(0xFF27272D),
    surfaceContainerHighest = Color(0xFF27272D),
    outline = Color(0xFF34343C),
    outlineVariant = Color(0xFF34343C),
    error = Color(0xFFF2B8B5),
)

private val LightExtra = TismaColors(
    accentText = Accent, accentHover = Color(0xFF8E34DF), accentPressed = Color(0xFF5B0FA7),
    mutedBackground = Color(0xFFD9D9DF), mutedText = Color(0xFF9A9AA4),
    viewTop = Color(0xFF1C1C21), viewBottom = Color(0xFF141417), success = Color(0xFF2E7D32),
)

private val DarkExtra = LightExtra.copy(
    accentText = Color(0xFFC9A2F5), mutedBackground = Color(0xFF3A3A42), mutedText = Color(0xFF7A7A84),
    success = Color(0xFF81C784),
)

val LocalTismaColors = staticCompositionLocalOf { LightExtra }

object Tisma {
    val colors: TismaColors
        @Composable get() = LocalTismaColors.current
}

@Composable
fun TismaTheme(dark: Boolean = isSystemInDarkTheme(), content: @Composable () -> Unit) {
    CompositionLocalProvider(LocalTismaColors provides if (dark) DarkExtra else LightExtra) {
        MaterialTheme(colorScheme = if (dark) DarkScheme else LightScheme, content = content)
    }
}

/** A text button in the theme's accent text color (readable on the dark panels too). */
@Composable
fun AccentTextButton(onClick: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true,
                     content: @Composable RowScope.() -> Unit) {
    TextButton(onClick, modifier, enabled,
        colors = ButtonDefaults.textButtonColors(contentColor = Tisma.colors.accentText), content = content)
}
