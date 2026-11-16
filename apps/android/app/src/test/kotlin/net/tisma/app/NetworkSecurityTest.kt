package net.tisma.app

import java.io.File
import javax.xml.parsers.DocumentBuilderFactory
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * The store and beta builds' network rules (src/main, not the debug override): plain HTTP to printers on the local
 * network (printers.md), HTTPS only to Tisma Cloud (cloud.md, rule 1). The release gate runs the debug build, which
 * allows everything, so this checks what the people's builds get.
 */
class NetworkSecurityTest {
    @Test
    fun `printers may speak plain HTTP and Tisma Cloud may not`() {
        val doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(File("src/main/res/xml/network_security_config.xml"))
        val base = doc.getElementsByTagName("base-config").item(0) as org.w3c.dom.Element
        assertEquals("true", base.getAttribute("cleartextTrafficPermitted"))
        val domain = doc.getElementsByTagName("domain-config").item(0) as org.w3c.dom.Element
        assertEquals("false", domain.getAttribute("cleartextTrafficPermitted"))
        val d = domain.getElementsByTagName("domain").item(0) as org.w3c.dom.Element
        assertEquals("tismaservidores.net", d.textContent.trim())
        assertEquals("true", d.getAttribute("includeSubdomains"))
        assertEquals("https://tismaservidores.net", net.tisma.cloud.CloudClient.DEFAULT_SERVER)
    }
}
