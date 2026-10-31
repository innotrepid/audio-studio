package com.innotrepid.audiostudio

import android.content.Context
import android.util.AtomicFile
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.io.FileOutputStream
import java.io.ByteArrayOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.nio.charset.StandardCharsets
import java.time.Instant
import java.util.UUID

internal class CreatorProjectStore(context: Context) {
    data class Section(
        val sectionId: String,
        val title: String,
        val text: String,
        val order: Int,
        val notes: String = "",
        val cues: List<String> = emptyList()
    )

    data class Script(
        val scriptId: String,
        val title: String,
        val createdUtc: String,
        val updatedUtc: String,
        val sections: List<Section>
    )

    data class Project(
        val projectId: String,
        val title: String,
        val createdUtc: String,
        val updatedUtc: String,
        val scripts: List<Script>,
        val takes: List<JSONObject> = emptyList()
    )

    private val file = File(context.filesDir, "creator/projects/default-project.json")
    private val atomicFile = AtomicFile(file)
    private val previousFile = File(file.parentFile, file.name + ".previous")
    private val maxDocumentBytes = 1_048_576
    private val maxTitleLength = 200
    private val maxSectionTextLength = 100_000

    @Synchronized
    @Throws(IOException::class)
    fun loadOrCreate(): Project {
        if (!file.exists() && !previousFile.exists() && !File(file.path + ".bak").exists()) {
            val now = Instant.now().toString()
            val project = Project(
                projectId = UUID.randomUUID().toString(),
                title = "My first project",
                createdUtc = now,
                updatedUtc = now,
                scripts = listOf(
                    Script(
                        scriptId = UUID.randomUUID().toString(),
                        title = "First script",
                        createdUtc = now,
                        updatedUtc = now,
                        sections = listOf(Section(UUID.randomUUID().toString(), "Opening", "", 0))
                    )
                )
            )
            save(project)
            return project
        }

        var primaryError: Exception? = null
        if (file.exists() || File(file.path + ".bak").exists()) {
            try {
                return decode(String(readPrimaryBytes(), StandardCharsets.UTF_8))
            } catch (error: Exception) {
                primaryError = error
            }
        }

        if (previousFile.exists()) {
            try {
                val recovered = decode(previousFile.readText(StandardCharsets.UTF_8))
                writePrimary(encode(recovered).toByteArray(StandardCharsets.UTF_8))
                return recovered
            } catch (backupError: Exception) {
                throw IOException(
                    "The Creator project and its recovery copy are invalid. Both files were preserved. Primary: ${primaryError?.message ?: "missing"}. Recovery: ${backupError.message}",
                    backupError
                )
            }
        }

        throw IOException(
            "The saved Creator project is invalid and was preserved: ${primaryError?.message ?: "unknown error"}",
            primaryError
        )
    }

    private fun readPrimaryBytes(): ByteArray {
        return atomicFile.openRead().use { input ->
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(8192)
            var total = 0
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                total += count
                if (total > maxDocumentBytes) throw IOException("Project file exceeds the 1 MB safety limit.")
                output.write(buffer, 0, count)
            }
            output.toByteArray()
        }
    }

    @Synchronized
    @Throws(IOException::class)
    fun save(project: Project): Project {
        val updated = project.copy(updatedUtc = Instant.now().toString())
        validate(updated)
        val bytes = encode(updated).toByteArray(StandardCharsets.UTF_8)
        if (bytes.size > maxDocumentBytes) throw IOException("Project file exceeds the 1 MB safety limit.")

        // Preserve the last valid generation independently of AtomicFile's in-flight backup.
        if (file.exists()) {
            try {
                val current = readPrimaryBytes()
                decode(String(current, StandardCharsets.UTF_8))
                writePrevious(current)
            } catch (_: Exception) {
                // Never replace a valid recovery generation with a malformed primary file.
            }
        }
        writePrimary(bytes)
        return updated
    }

    private fun writePrevious(bytes: ByteArray) {
        val temporary = File(file.parentFile, file.name + ".previous.tmp")
        FileOutputStream(temporary).use { output ->
            output.write(bytes)
            output.fd.sync()
        }
        Files.move(
            temporary.toPath(), previousFile.toPath(),
            StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING
        )
    }

    private fun writePrimary(bytes: ByteArray) {
        var stream: java.io.FileOutputStream? = null
        try {
            stream = atomicFile.startWrite()
            stream.write(bytes)
            stream.fd.sync()
            atomicFile.finishWrite(stream)
        } catch (error: Exception) {
            if (stream != null) atomicFile.failWrite(stream)
            throw IOException("Could not safely save the Creator project: ${error.message}", error)
        }
    }

    private fun validate(project: Project) {
        require(project.projectId.isNotBlank()) { "Project ID is required." }
        require(project.title.isNotBlank() && project.title.length <= maxTitleLength) { "Project title must contain 1–200 characters." }
        require(project.scripts.isNotEmpty()) { "At least one script is required." }
        val ids = mutableSetOf(project.projectId)
        project.scripts.forEach { script ->
            require(script.scriptId.isNotBlank() && ids.add(script.scriptId)) { "Script IDs must be unique." }
            require(script.title.isNotBlank() && script.title.length <= maxTitleLength) { "Script title must contain 1–200 characters." }
            require(script.sections.isNotEmpty()) { "Each script must contain at least one section." }
            script.sections.forEach { section ->
                require(section.sectionId.isNotBlank() && ids.add(section.sectionId)) { "Section IDs must be unique." }
                require(section.title.isNotBlank() && section.title.length <= maxTitleLength) { "Section title must contain 1–200 characters." }
                require(section.order >= 0) { "Section order cannot be negative." }
                require(section.text.length <= maxSectionTextLength) { "Section text exceeds the 100,000 character limit." }
            }
        }
        project.takes.forEach { take ->
            val id = take.optString("takeId")
            require(id.isNotBlank() && ids.add(id)) { "Take IDs must be present and unique." }
            val sourceUri = take.optString("sourceUri").takeIf { it.isNotBlank() && it != "null" }
            val sourcePath = take.optString("sourcePath").takeIf { it.isNotBlank() && it != "null" }
            require(sourceUri != null || sourcePath != null) { "A take must reference its existing audio file." }
            val durationMs = take.optLong("durationMs", -1L)
            val sampleRateHz = take.optLong("sampleRateHz", -1L)
            val channels = take.optInt("channels", -1)
            require(durationMs > 0L && sampleRateHz > 0L && channels in 1..2) {
                "A take must contain valid verified audio metadata."
            }
            val scriptId = take.optString("scriptId").takeIf { it.isNotBlank() && it != "null" }
            val sectionId = take.optString("sectionId").takeIf { it.isNotBlank() && it != "null" }
            require(scriptId == null || project.scripts.any { it.scriptId == scriptId }) {
                "A take references a missing script."
            }
            if (sectionId != null) {
                require(scriptId != null) { "A section-linked take must also reference its script." }
                val referencedScript = project.scripts.firstOrNull { it.scriptId == scriptId }
                require(referencedScript?.sections?.any { it.sectionId == sectionId } == true) {
                    "A take section must exist inside its referenced script."
                }
            }
        }
    }

    private fun encode(project: Project): String {
        val scripts = JSONArray()
        project.scripts.forEach { script ->
            val sections = JSONArray()
            script.sections.sortedBy { it.order }.forEach { section ->
                sections.put(JSONObject()
                    .put("sectionId", section.sectionId)
                    .put("title", section.title)
                    .put("text", section.text)
                    .put("order", section.order)
                    .put("notes", section.notes)
                    .put("cues", JSONArray(section.cues)))
            }
            scripts.put(JSONObject()
                .put("scriptId", script.scriptId)
                .put("title", script.title)
                .put("createdUtc", script.createdUtc)
                .put("updatedUtc", script.updatedUtc)
                .put("sections", sections))
        }
        return JSONObject()
            .put("schemaVersion", 1)
            .put("projectId", project.projectId)
            .put("title", project.title)
            .put("createdUtc", project.createdUtc)
            .put("updatedUtc", project.updatedUtc)
            .put("scripts", scripts)
            .put("takes", JSONArray(project.takes))
            .toString(2)
    }

    /** Export the complete native project document. Linked takes remain references; audio files are untouched. */
    @Synchronized
    @Throws(IOException::class)
    fun exportJson(project: Project): String {
        validate(project)
        return encode(project)
    }

    /** Parse and validate a complete document before callers replace the active project. */
    @Synchronized
    @Throws(IOException::class)
    fun importJson(json: String): Project {
        if (json.toByteArray(StandardCharsets.UTF_8).size > maxDocumentBytes) {
            throw IOException("Project file exceeds the 1 MB safety limit.")
        }
        return try {
            decode(json)
        } catch (error: Exception) {
            throw IOException("Import rejected; the current project was not changed: ${error.message}", error)
        }
    }

    fun exportNarration(script: Script): String =
        script.sections.sortedBy { it.order }.joinToString("\n\n") { it.text.trim() }.trim() + "\n"

    fun exportProductionCopy(script: Script): String = buildString {
        appendLine(script.title)
        appendLine("=".repeat(script.title.length.coerceAtLeast(1)))
        appendLine()
        script.sections.sortedBy { it.order }.forEachIndexed { index, section ->
            appendLine("[SECTION ${index + 1}] ${section.title}")
            section.cues.forEach { appendLine("[CUE] $it") }
            if (section.notes.isNotBlank()) appendLine("[PRODUCTION NOTES] ${section.notes}")
            appendLine()
            appendLine(section.text.trim())
            appendLine()
        }
    }

    private fun decode(json: String): Project {
        val root = JSONObject(json)
        val version = root.optInt("schemaVersion", -1)
        require(version == 1) { "Unsupported project schema version: $version." }
        val scriptsJson = root.getJSONArray("scripts")
        val scripts = (0 until scriptsJson.length()).map { index ->
            val script = scriptsJson.getJSONObject(index)
            val sectionsJson = script.getJSONArray("sections")
            val sections = (0 until sectionsJson.length()).map { sectionIndex ->
                val section = sectionsJson.getJSONObject(sectionIndex)
                val cuesJson = section.optJSONArray("cues") ?: JSONArray()
                Section(
                    sectionId = section.getString("sectionId"),
                    title = section.getString("title"),
                    text = section.optString("text", ""),
                    order = section.getInt("order"),
                    notes = section.optString("notes", ""),
                    cues = (0 until cuesJson.length()).map { cuesJson.getString(it) }
                )
            }
            Script(
                scriptId = script.getString("scriptId"),
                title = script.getString("title"),
                createdUtc = script.getString("createdUtc"),
                updatedUtc = script.getString("updatedUtc"),
                sections = sections
            )
        }
        val takesJson = root.optJSONArray("takes") ?: JSONArray()
        val takes = (0 until takesJson.length()).map { takesJson.getJSONObject(it) }
        return Project(
            projectId = root.getString("projectId"),
            title = root.getString("title"),
            createdUtc = root.getString("createdUtc"),
            updatedUtc = root.getString("updatedUtc"),
            scripts = scripts,
            takes = takes
        ).also(::validate)
    }
}
