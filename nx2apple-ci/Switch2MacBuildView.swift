import SwiftUI
import AppKit
import Foundation

/// A separate experimental Mac path. The stock NX2Apple flow stays unchanged.
struct Switch2MacBuildView: View {
    @State private var gameFolder = ""
    @State private var kartFolder = NSHomeDirectory() + "/KartNative"
    @State private var gameName = "Mario Kart World"
    @State private var iconPath = ""
    @State private var outputFolder = NSHomeDirectory() + "/Desktop"
    @State private var active = false
    @State private var status = "Selecciona tu volcado descifrado de Switch 2."
    @State private var outputLog = ""

    private var validDump: Bool {
        let root = URL(fileURLWithPath: gameFolder)
        return !gameFolder.isEmpty && ["exefs/main", "exefs/main.npdm", "exefs/sdk", "exefs/subsdk0"].allSatisfy {
            FileManager.default.fileExists(atPath: root.appendingPathComponent($0).path)
        } && FileManager.default.fileExists(atPath: root.appendingPathComponent("romfs").path)
    }
    private var validKartNative: Bool {
        FileManager.default.fileExists(atPath: URL(fileURLWithPath: kartFolder).appendingPathComponent("scripts/build_ipa_from_nsp.py").path)
            && FileManager.default.fileExists(atPath: URL(fileURLWithPath: kartFolder).appendingPathComponent("dm/games/0400C3F00006E000/game.tsv").path)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Nintendo Switch 2 · Mac (experimental)").font(.title2.bold())
            Text("Utiliza KartNative para Mario Kart World 1.0.0. No se requieren prod.keys ni un iPhone. El juego y sus archivos permanecen en tu Mac.")
                .foregroundStyle(.secondary)
            HStack {
                Button("Elegir carpeta del juego") { chooseDirectory { gameFolder = $0 } }
                Text(gameFolder.isEmpty ? "Sin seleccionar" : gameFolder).lineLimit(1).truncationMode(.middle)
            }
            Text(validDump ? "✓ Encontrados exefs/ y romfs/" : "La carpeta debe contener exefs/main, main.npdm, sdk, subsdk0 y romfs/")
                .foregroundStyle(validDump ? .green : .secondary)
            Divider()
            HStack {
                Button("Elegir KartNative") { chooseDirectory { kartFolder = $0 } }
                Button("Descargar KartNative") { downloadKartNative() }.disabled(active)
            }
            Text(kartFolder).font(.caption).foregroundStyle(.secondary)
            Text(validKartNative ? "✓ Motor KartNative reconocido" : "No se ha encontrado el motor. Descárgalo o selecciona su carpeta.")
                .foregroundStyle(validKartNative ? .green : .secondary)
            Divider()
            TextField("Nombre de la app", text: $gameName).textFieldStyle(.roundedBorder)
            HStack {
                Button("Elegir icono (PNG, JPEG, ICNS)") { chooseIcon() }
                if !iconPath.isEmpty {
                    if let icon = NSImage(contentsOfFile: iconPath) {
                        Image(nsImage: icon).resizable().aspectRatio(contentMode: .fit).frame(width: 48, height: 48)
                    }
                    Button("Quitar") { iconPath = "" }
                }
            }
            HStack {
                Button("Guardar app en…") { chooseDirectory { outputFolder = $0 } }
                Text(outputFolder).lineLimit(1).truncationMode(.middle)
            }
            Text("Destino: \(safeName(gameName)).app").font(.caption).foregroundStyle(.secondary)
            Button(active ? "Compilando…" : "Compilar app para Mac") { build() }
                .disabled(active || !validDump || !validKartNative || gameName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                .buttonStyle(.borderedProminent)
            Text(status).font(.callout)
            if !outputLog.isEmpty {
                ScrollView {
                    Text(outputLog).font(.system(size: 11, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading).textSelection(.enabled)
                }.frame(height: 220)
            }
            Text("El modo mac-app está bloqueado actualmente por KartNative para Switch 2. Esta versión activa esa ruta de manera experimental. No garantiza que la aplicación del juego arranque correctamente.")
                .font(.caption).foregroundStyle(.secondary)
        }.padding(16)
    }

    private func safeName(_ name: String) -> String {
        let clean = name.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-")
        return clean.isEmpty ? "Switch2Game" : clean
    }

    private func chooseDirectory(_ receive: (String) -> Void) {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        if panel.runModal() == .OK, let url = panel.url { receive(url.path) }
    }

    private func chooseIcon() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.allowedFileTypes = ["png", "jpeg", "jpg", "icns"]
        if panel.runModal() == .OK, let url = panel.url { iconPath = url.path }
    }

    private func run(_ path: String, _ args: [String], in directory: String? = nil, onLine: ((String) -> Void)? = nil) throws {
        let task = Process()
        task.executableURL = URL(fileURLWithPath: path)
        task.arguments = args
        if let directory { task.currentDirectoryURL = URL(fileURLWithPath: directory) }
        var env = ProcessInfo.processInfo.environment
        env["PATH"] = ["/opt/homebrew/bin", "/usr/local/bin", "/usr/bin", "/bin", env["PATH"] ?? ""].joined(separator: ":")
        task.environment = env
        let pipe = Pipe()
        task.standardOutput = pipe
        task.standardError = pipe
        try task.run()
        let reader = pipe.fileHandleForReading
        while true {
            let bytes = reader.availableData
            if bytes.isEmpty { break }
            onLine?(String(decoding: bytes, as: UTF8.self))
        }
        task.waitUntilExit()
        if task.terminationStatus != 0 {
            throw NSError(domain: "NX2Apple.Switch2", code: Int(task.terminationStatus), userInfo: [NSLocalizedDescriptionKey: "El proceso terminó con código \(task.terminationStatus). Consulta el registro."])
        }
    }

    private func report(_ message: String) {
        DispatchQueue.main.async {
            outputLog += message
            if outputLog.count > 16000 { outputLog = String(outputLog.suffix(16000)) }
        }
    }

    private func downloadKartNative() {
        guard !active else { return }
        active = true
        status = "Descargando KartNative…"
        let path = kartFolder
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                if FileManager.default.fileExists(atPath: path) {
                    throw NSError(domain: "NX2Apple.Switch2", code: 1, userInfo: [NSLocalizedDescriptionKey: "La carpeta ya existe. Selecciona una carpeta diferente o usa el motor existente."])
                }
                try run("/usr/bin/git", ["clone", "--depth", "1", "https://github.com/pahorema/kartnative.git", path], onLine: report)
                DispatchQueue.main.async { status = "Motor KartNative descargado."; active = false }
            } catch {
                DispatchQueue.main.async { status = "Error: \(error.localizedDescription)"; active = false }
            }
        }
    }

    private func pythonExecutable() -> String {
        let bundled = Bundle.main.resourceURL?.appendingPathComponent("python/bin/python3").path ?? ""
        if FileManager.default.isExecutableFile(atPath: bundled) { return bundled }
        for path in ["/opt/homebrew/bin/python3", "/usr/local/bin/python3", "/usr/bin/python3"] {
            if FileManager.default.isExecutableFile(atPath: path) { return path }
        }
        return "/usr/bin/python3"
    }

    private func enableMacPackaging(_ engine: String) throws {
        let script = URL(fileURLWithPath: engine).appendingPathComponent("scripts/build_ipa_from_nsp.py")
        let old = "if g.get('switch2', 'off') == 'on' and platform != 'mac':"
        let new = "if g.get('switch2', 'off') == 'on' and platform not in ('mac', 'mac-app'):"
        let data = try String(contentsOf: script, encoding: .utf8)
        if data.contains(new) { return }
        guard data.contains(old) else {
            throw NSError(domain: "NX2Apple.Switch2", code: 2, userInfo: [NSLocalizedDescriptionKey: "La versión de KartNative no coincide con la esperada. No se ha modificado su código."])
        }
        try data.replacingOccurrences(of: old, with: new).write(to: script, atomically: true, encoding: .utf8)
    }

    private func brandApp(_ app: URL, name: String, icon: String) throws {
        let contents = app.appendingPathComponent("Contents")
        let plist = contents.appendingPathComponent("Info.plist")
        guard var data = NSDictionary(contentsOf: plist) as? [String: Any] else {
            throw NSError(domain: "NX2Apple.Switch2", code: 3, userInfo: [NSLocalizedDescriptionKey: "Info.plist ausente en la app generada."])
        }
        data["CFBundleName"] = name
        data["CFBundleDisplayName"] = name
        if !icon.isEmpty {
            let out = contents.appendingPathComponent("Resources/AppIcon.icns")
            if FileManager.default.fileExists(atPath: out.path) { try FileManager.default.removeItem(at: out) }
            if URL(fileURLWithPath: icon).pathExtension.lowercased() == "icns" {
                try FileManager.default.copyItem(at: URL(fileURLWithPath: icon), to: out)
            } else {
                let tmp = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString + ".iconset")
                try FileManager.default.createDirectory(at: tmp, withIntermediateDirectories: true)
                defer { try? FileManager.default.removeItem(at: tmp) }
                for size in [16, 32, 128, 256, 512] {
                    for factor in [1, 2] {
                        let suffix = factor == 2 ? "@2x" : ""
                        let dest = tmp.appendingPathComponent("icon_\(size)x\(size)\(suffix).png")
                        try run("/usr/bin/sips", ["-s", "format", "png", "-z", String(size * factor), String(size * factor), icon, "--out", dest.path])
                    }
                }
                try run("/usr/bin/iconutil", ["-c", "icns", tmp.path, "-o", out.path])
            }
            data["CFBundleIconFile"] = "AppIcon"
        }
        guard (data as NSDictionary).write(to: plist, atomically: true) else {
            throw NSError(domain: "NX2Apple.Switch2", code: 4, userInfo: [NSLocalizedDescriptionKey: "No se pudo actualizar Info.plist."])
        }
        try run("/usr/bin/codesign", ["--force", "--deep", "--sign", "-", app.path])
    }

    private func build() {
        guard !active && validDump && validKartNative else { return }
        let folder = gameFolder, engine = kartFolder, name = safeName(gameName), icon = iconPath, output = outputFolder
        let app = URL(fileURLWithPath: output).appendingPathComponent(name + ".app")
        if FileManager.default.fileExists(atPath: app.path) {
            status = "Ya existe \(name).app. Cámbiale el nombre o elige otro destino para no sobrescribirla."
            return
        }
        active = true
        outputLog = ""
        status = "Compilación Switch 2 en curso…"
        let python = pythonExecutable()
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try enableMacPackaging(engine)
                let command = [engine + "/scripts/build_ipa_from_nsp.py", folder, "--platform", "mac-app", "--no-heap-check", "--allow-other-metal", "--progress", "json", "--app-name", name, "-o", app.path]
                try run(python, command, in: engine, onLine: report)
                guard FileManager.default.fileExists(atPath: app.path) else {
                    throw NSError(domain: "NX2Apple.Switch2", code: 5, userInfo: [NSLocalizedDescriptionKey: "KartNative terminó, pero no generó la app esperada."])
                }
                try brandApp(app, name: name, icon: icon)
                DispatchQueue.main.async { status = "App creada: \(app.path)"; active = false }
            } catch {
                DispatchQueue.main.async { status = "Error: \(error.localizedDescription)"; active = false }
            }
        }
    }
}
