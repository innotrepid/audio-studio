package com.innotrepid.audiostudio

import android.content.Context
import android.util.AtomicFile
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
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
    private val maxDocumentBytes = 1_048_576
    private val maxTitleLength = 200
    private val maxSectionTextLength = 100_000

    @Synchronized
    @Throws(IOException::class)
    fun loadOrCreate(): Project {
        if (!file.exists() && !File(file.path + ".bak").exists()) {
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
                        sections = listOf(
                            Section(UUID.randomUUID().toString(), "Opening", "", 0)
                        )
                    )
                )
            )
            save(project)
            return project
        }

        val bytes = atomicFile.openRead().use { input ->
            val output = input.readBytes()
            if (output.size > maxDocumentBytes) throw IOException("Project file exceeds the 1 MB safety limit.")
            output
        }
        return try {
            decode(String(bytes, StandardCharsets.UTF_8))
        } catch (error: Exception) {
            throw IOException("The saved Creator project is invalid. The original file was preserved: ${error.message}", error)
        }
    }

    @Synchronized
    @Throws(IOException::class)
    fun save(project: Project): Project {
        val updated = project.copy(updatedUtc = Instant.now().toString())
        validate(updated)
        val bytes = encode(updated).toByteArray(StandardCharsets.UTF_8)
        if (bytes.size > maxDocumentBytes) throw IOException("Project file exceeds the 1 MB safety limit.")

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
        return updated
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
