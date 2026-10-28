using System.Diagnostics;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Runtime.Intrinsics.X86;

namespace MkwiiPcBuilder;

internal sealed class RuntimeFiles : IDisposable
{
    public string Root { get; } = Path.Combine(Path.GetTempPath(), "MKWii-PC-Builder", Guid.NewGuid().ToString("N"));
    public string Python => Path.Combine(Root, "python", "python.exe");
    public string Worker => Path.Combine(Root, "payload", "worker", "build_game.py");
    public void Prepare()
    {
        Directory.CreateDirectory(Root);
        Extract("PythonRuntime.zip", Path.Combine(Root, "python"));
        var payload = Path.Combine(Root, "payload"); Extract("BuilderPayload.zip", payload);
        var entries = JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(Path.Combine(payload, "payload-files.json")))!;
        foreach (var (relative, expected) in entries)
        {
            var full = SafeTarget(payload, relative);
            using var input = File.OpenRead(full);
            if (!Convert.ToHexString(SHA256.HashData(input)).Equals(expected, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("El paquete del instalador está dañado: " + relative);
        }
    }
    private static string SafeTarget(string root, string relative)
    {
        if (relative.Contains(':') || Path.IsPathRooted(relative)) throw new InvalidDataException("Ruta de paquete inválida");
        var path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        if (!path.StartsWith(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Ruta fuera del paquete");
        return path;
    }
    private static void Extract(string name, string target)
    {
        using var resource = Assembly.GetExecutingAssembly().GetManifestResourceStream(name) ?? throw new IOException("Falta " + name);
        using var zip = new ZipArchive(resource);
        foreach (var item in zip.Entries)
        {
            if (item.FullName.EndsWith('/')) continue;
            var path = SafeTarget(target, item.FullName); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            using var input = item.Open(); using var output = File.Create(path); input.CopyTo(output);
        }
    }
    public void Dispose()
    {
        var parent = Path.GetFullPath(Path.Combine(Path.GetTempPath(), "MKWii-PC-Builder")) + Path.DirectorySeparatorChar;
        if (!Path.GetFullPath(Root).StartsWith(parent, StringComparison.OrdinalIgnoreCase)) return;
        try { Directory.Delete(Root, recursive: true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
    }
}

internal sealed record BuildOptions(string Iso, string Destination, string? Desktop, string? Cache = null, string Language = "es", int Jobs = 4, bool Offline = false);
internal sealed class BuildRunner : IDisposable
{
    private Process? process;
    public void Cancel() { try { if (process is { HasExited: false }) process.Kill(entireProcessTree: true); } catch (InvalidOperationException) { } }
    public async Task<int> Run(RuntimeFiles runtime, BuildOptions options, Action<string> output, CancellationToken token)
    {
        var start = new ProcessStartInfo(runtime.Python) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true, StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8 };
        start.Environment["PYTHONUTF8"] = "1"; start.Environment["PYTHONIOENCODING"] = "utf-8";
        foreach (var a in new[] { "-I", "-u", runtime.Worker, "--iso", options.Iso, "--install-dir", Environment.ExpandEnvironmentVariables(options.Destination), "--language", options.Language, "--jobs", options.Jobs.ToString() }) start.ArgumentList.Add(a);
        if (options.Desktop is not null) { start.ArgumentList.Add("--desktop-dir"); start.ArgumentList.Add(options.Desktop); }
        if (options.Cache is not null) { start.ArgumentList.Add("--cache-dir"); start.ArgumentList.Add(options.Cache); }
        if (options.Offline) start.ArgumentList.Add("--offline");
        process = Process.Start(start) ?? throw new IOException("No se pudo iniciar el compilador");
        using var registration = token.Register(Cancel);
        async Task Pump(StreamReader reader) { while (await reader.ReadLineAsync() is { } line) output(line); }
        await Task.WhenAll(Pump(process.StandardOutput), Pump(process.StandardError));
        await process.WaitForExitAsync(CancellationToken.None); token.ThrowIfCancellationRequested(); return process.ExitCode;
    }
    public void Dispose() { Cancel(); process?.Dispose(); }
}

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            if (args.Contains("--version")) { Console.WriteLine("1.0.0"); return 0; }
            if (Array.IndexOf(args, "--render-preview") is var preview && preview >= 0 && preview + 1 < args.Length)
            {
                ApplicationConfiguration.Initialize(); using var form = new BuilderForm(); form.ShowInTaskbar = false; form.StartPosition = FormStartPosition.Manual; form.Location = new Point(-12000, -12000); form.Show(); Application.DoEvents();
                using var bitmap = new Bitmap(form.Width, form.Height); form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, form.Size));
                bitmap.Save(args[preview + 1], System.Drawing.Imaging.ImageFormat.Png); return 0;
            }
            if (args.Contains("--self-test"))
            {
                using var runtime = new RuntimeFiles(); runtime.Prepare();
                var p = new ProcessStartInfo(runtime.Python) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
                p.ArgumentList.Add("-I"); p.ArgumentList.Add("-c"); p.ArgumentList.Add("import sys,ssl,hashlib,zipfile;print(sys.version);print('Embedded runtime OK')");
                using var child = Process.Start(p)!; Console.Write(child.StandardOutput.ReadToEnd()); Console.Error.Write(child.StandardError.ReadToEnd()); child.WaitForExit(); return child.ExitCode;
            }
            if (args.Contains("--iso"))
            {
                string? Get(string key) { var i = Array.IndexOf(args, key); return i >= 0 && i + 1 < args.Length ? args[i + 1] : null; }
                var options = new BuildOptions(Get("--iso")!, Get("--install-dir") ?? throw new ArgumentException("Falta --install-dir"), args.Contains("--no-desktop") ? null : Get("--desktop-dir") ?? Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), Get("--cache-dir"), Get("--language") ?? "es", int.TryParse(Get("--jobs"), out var jobs) ? Math.Clamp(jobs, 1, 16) : 4, args.Contains("--offline"));
                using var runtime = new RuntimeFiles(); runtime.Prepare(); using var runner = new BuildRunner();
                return runner.Run(runtime, options, Console.WriteLine, CancellationToken.None).GetAwaiter().GetResult();
            }
            ApplicationConfiguration.Initialize(); Application.Run(new BuilderForm()); return 0;
        }
        catch (Exception error)
        {
            if (args.Length > 0) Console.Error.WriteLine(error);
            else MessageBox.Show(error.Message, "Mario Kart Wii PC Builder", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}

internal sealed class BuilderForm : Form
{
    private readonly TextBox iso = new() { ReadOnly = true, PlaceholderText = "Selecciona tu imagen de Mario Kart Wii PAL (RMCP01)" };
    private readonly TextBox destination = new() { Text = @"%LOCALAPPDATA%\MarioKartWiiPC\Game" };
    private readonly ComboBox language = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly CheckBox desktop = new() { Text = "Crear Mario Kart Wii.exe en el escritorio", Checked = true, AutoSize = true };
    private readonly CheckBox details = new() { Text = "Mostrar detalles", AutoSize = true };
    private readonly Label status = new() { AutoSize = false, Text = "Selecciona tu disco para empezar.", ForeColor = Color.FromArgb(45, 70, 85) };
    private readonly ProgressBar progress = new();
    private readonly RichTextBox log = new() { ReadOnly = true, Visible = false, BorderStyle = BorderStyle.None, BackColor = Color.FromArgb(238, 244, 248), Font = new Font("Consolas", 9) };
    private readonly Button install = new() { Text = "Compilar e instalar", BackColor = Color.FromArgb(0, 133, 190), ForeColor = Color.White, FlatStyle = FlatStyle.Flat };
    private readonly Button cancel = new() { Text = "Cancelar", Enabled = false };
    private readonly Button launch = new() { Text = "Abrir el juego", Visible = false };
    private readonly Button selectIso = new() { Text = "Elegir imagen…" };
    private readonly Button selectFolder = new() { Text = "Cambiar carpeta…" };
    private CancellationTokenSource? cancellation;
    private BuildRunner? runner;
    private bool running;
    private string? game;

    public BuilderForm()
    {
        SuspendLayout(); AutoScaleDimensions = new SizeF(96f, 96f); AutoScaleMode = AutoScaleMode.Dpi;
        Text = "Mario Kart Wii · PC Builder"; ClientSize = new Size(880, 580); MinimumSize = new Size(896, 619); StartPosition = FormStartPosition.CenterScreen;
        Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!);
        Font = new Font("Segoe UI", 10); BackColor = Color.White;
        var header = new Panel { Bounds = new Rectangle(0, 0, 880, 110), Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right };
        header.Paint += (_, e) =>
        {
            using var bg = new System.Drawing.Drawing2D.LinearGradientBrush(header.ClientRectangle, Color.FromArgb(0, 111, 171), Color.FromArgb(29, 174, 210), 0f);
            e.Graphics.FillRectangle(bg, header.ClientRectangle);
            using var square = new SolidBrush(Color.FromArgb(38, Color.White));
            for (int y = 0; y < 6; y++) for (int x = 0; x < 12; x++) if ((x + y) % 2 == 0) e.Graphics.FillRectangle(square, header.Width - 240 + x * 24, y * 24 - 16, 24, 24);
        };
        var title = new Label { Text = "Mario Kart Wii · PC Builder", Font = new Font("Segoe UI", 23, FontStyle.Bold), AutoSize = true, ForeColor = Color.White, BackColor = Color.Transparent, Location = new Point(28, 20) };
        var subtitle = new Label { Text = "Tu disco. Tu compilación para Windows.", AutoSize = true, ForeColor = Color.White, BackColor = Color.Transparent, Location = new Point(32, 70) };
        header.Controls.Add(title); header.Controls.Add(subtitle); Controls.Add(header);
        void Add(Control c, int x, int y, int w, int h) { c.Bounds = new Rectangle(x, y, w, h); Controls.Add(c); }
        Add(new Label { Text = "1   Imagen del juego", Font = new Font(Font, FontStyle.Bold) }, 32, 135, 750, 24);
        Add(iso, 32, 165, 630, 30); Add(selectIso, 680, 163, 166, 34);
        Add(new Label { Text = "2   Carpeta de instalación", Font = new Font(Font, FontStyle.Bold) }, 32, 220, 750, 24);
        Add(destination, 32, 250, 630, 30); Add(selectFolder, 680, 248, 166, 34);
        Add(new Label { Text = "Idioma inicial" }, 32, 310, 120, 28); Add(language, 160, 307, 175, 30);
        language.Items.AddRange(["Español", "English", "Français", "Deutsch", "Italiano"]); language.SelectedIndex = 0;
        Add(desktop, 360, 310, 485, 30);
        Add(new Label { Text = "Se descargarán herramientas y Retro Rewind. Tu imagen y tus partidas permanecen en tu PC.\nNecesita Windows x64 con AVX2, Internet y 20 GiB libres para compilar.", ForeColor = Color.DimGray }, 32, 355, 814, 50);
        Add(status, 32, 418, 814, 42); Add(progress, 32, 465, 814, 12);
        Add(details, 32, 502, 175, 28); Add(install, 596, 495, 250, 42); Add(cancel, 462, 495, 120, 42); Add(launch, 596, 495, 250, 42);
        Add(log, 32, 550, 814, 180);
        var credit = new Label { Text = "Basado en WiiCompiled y Retro Rewind · No incluye el juego", AutoSize = true, ForeColor = Color.Gray, Location = new Point(32, 549) };
        Controls.Add(credit);
        details.CheckedChanged += (_, _) => { log.Visible = details.Checked; credit.Visible = !details.Checked; ClientSize = new Size(ClientSize.Width, (int)Math.Round((details.Checked ? 755 : 580) * DeviceDpi / 96f)); };
        selectIso.Click += (_, _) => { using var dialog = new OpenFileDialog { Filter = "Imagen de Wii|*.iso;*.wbfs;*.rvz", CheckFileExists = true, Title = "Selecciona tu Mario Kart Wii PAL" }; if (dialog.ShowDialog(this) == DialogResult.OK) iso.Text = dialog.FileName; };
        selectFolder.Click += (_, _) => { using var dialog = new FolderBrowserDialog { Description = "Carpeta para el juego y los archivos de compilación", UseDescriptionForTitle = true }; if (dialog.ShowDialog(this) == DialogResult.OK) destination.Text = dialog.SelectedPath; };
        install.Click += async (_, _) => await Install();
        cancel.Click += (_, _) => { cancellation?.Cancel(); status.Text = "Deteniendo la compilación…"; };
        launch.Click += (_, _) => { if (game is not null) Process.Start(new ProcessStartInfo(game) { UseShellExecute = true, WorkingDirectory = Path.GetDirectoryName(game) }); };
        FormClosing += (_, e) => { if (running) { cancellation?.Cancel(); runner?.Cancel(); } };
        ResumeLayout(performLayout: true);
    }

    private void Output(string line)
    {
        if (IsDisposed || !IsHandleCreated) return;
        try { BeginInvoke((Action)(() =>
        {
            if (log.TextLength > 150000) log.Clear(); log.AppendText(line + Environment.NewLine);
            try
            {
                using var doc = JsonDocument.Parse(line); var root = doc.RootElement;
                var stage = root.GetProperty("stage").GetString();
                if (root.TryGetProperty("percent", out var p)) { progress.Style = ProgressBarStyle.Continuous; progress.Value = Math.Clamp(p.GetInt32(), 0, 100); status.Text = root.GetProperty("message").GetString(); }
                else if (stage == "download") status.Text = root.GetProperty("message").GetString();
                else if (stage is "compile" or "translate-base" or "translate-retro" or "configure" or "shards") { progress.Style = ProgressBarStyle.Marquee; status.Text = stage == "compile" ? "Compilando el juego para Windows. Puede tardar varios minutos…" : "Preparando y traduciendo localmente tu juego…"; }
                if (stage == "complete") game = root.GetProperty("game_exe").GetString();
                if (stage == "error") status.Text = root.GetProperty("message").GetString();
            }
            catch (JsonException) { }
        })); } catch (InvalidOperationException) { }
    }

    private async Task Install()
    {
        if (!File.Exists(iso.Text)) { status.Text = "Selecciona una imagen ISO, WBFS o RVZ antes de continuar."; return; }
        if (!Avx2.IsSupported) { status.Text = "Esta compilación necesita un procesador x64 con AVX2."; return; }
        running = true; install.Enabled = false; cancel.Enabled = true; selectIso.Enabled = selectFolder.Enabled = destination.Enabled = language.Enabled = desktop.Enabled = false;
        cancellation = new CancellationTokenSource(); runner = new BuildRunner();
        var options = new BuildOptions(iso.Text, destination.Text, desktop.Checked ? Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory) : null, Language: new[] { "es", "en", "fr", "de", "it" }[language.SelectedIndex]);
        try
        {
            status.Text = "Preparando el instalador…"; using var runtime = new RuntimeFiles(); await Task.Run(runtime.Prepare);
            var code = await runner.Run(runtime, options, Output, cancellation.Token);
            if (code == 0) { game ??= Path.Combine(Environment.ExpandEnvironmentVariables(destination.Text), "Mario Kart Wii.exe"); status.Text = "Listo. Tu juego está instalado y ya puedes abrirlo."; progress.Style = ProgressBarStyle.Continuous; progress.Value = 100; install.Visible = false; launch.Visible = true; }
            else { status.Text += "  Los detalles están en la carpeta .mkwii-builder de la instalación."; details.Checked = true; }
        }
        catch (OperationCanceledException) { status.Text = "Compilación cancelada. Puedes volver a intentarlo; las descargas se conservan."; }
        catch (Exception error) { status.Text = error.Message; details.Checked = true; }
        finally
        {
            running = false; runner.Dispose(); runner = null; cancellation.Dispose(); cancellation = null; cancel.Enabled = false; install.Enabled = true;
            selectIso.Enabled = selectFolder.Enabled = destination.Enabled = language.Enabled = desktop.Enabled = true;
            if (progress.Style == ProgressBarStyle.Marquee) { progress.Style = ProgressBarStyle.Continuous; progress.Value = 0; }
        }
    }
}
