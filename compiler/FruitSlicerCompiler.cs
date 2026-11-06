// FruitSlicerCompiler: builds Fruit Slicer 3DS (.3dsx and .cia) from source with devkitPro on Windows.
// Compiled with the csc.exe that ships with Windows, so the .exe has no runtime to install.
//
// Usage: FruitSlicerCompiler.exe [--source DIR] [--output DIR] [--devkitpro DIR] [--bash EXE] [--no-ui]
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

static class Program
{
    const string Title = "Fruit Slicer Compiler";
    const string MakeromUrl = "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-win_x86_64.zip";
    const string BannertoolUrl = "https://github.com/carstene1ns/3ds-bannertool/releases/download/1.2.3/bannertool-1.2.3-windows.zip";
    const string DevkitProUrl = "https://devkitpro.org/wiki/Getting_Started";

    static bool ui = true;

    [STAThread]
    static int Main(string[] argv)
    {
        var args = Parse(argv);
        ui = !args.ContainsKey("no-ui");
        try
        {
            return Run(args);
        }
        catch (Exception e)
        {
            Fail("The build stopped:\n\n" + e.Message);
            return 1;
        }
    }

    static Dictionary<string, string> Parse(string[] argv)
    {
        var d = new Dictionary<string, string>();
        for (int i = 0; i < argv.Length; i++)
        {
            if (!argv[i].StartsWith("--")) continue;
            string k = argv[i].Substring(2);
            bool hasValue = i + 1 < argv.Length && !argv[i + 1].StartsWith("--");
            d[k] = hasValue ? argv[++i] : "";
        }
        return d;
    }

    static int Run(Dictionary<string, string> args)
    {
        if (ui) Application.EnableVisualStyles();

        string output = Get(args, "output") ?? PickFolder("Choose where to save FruitSlicer3DS.3dsx and FruitSlicer3DS.cia");
        if (output == null) return 2;
        Directory.CreateDirectory(output);

        string bash = Get(args, "bash");
        string dkp = Get(args, "devkitpro") ?? Environment.GetEnvironmentVariable("DEVKITPRO_WIN") ?? @"C:\devkitPro";
        if (bash == null)
        {
            bash = Path.Combine(dkp, @"msys2\usr\bin\bash.exe");
            if (!File.Exists(bash))
            {
                Fail("devkitPro was not found at " + dkp + ".\n\nInstall it with the 3ds-dev group (" + DevkitProUrl + "), then run this tool again.");
                return 3;
            }
        }

        string work = Path.Combine(Path.GetTempPath(), "FruitSlicerBuild");
        string source = Get(args, "source") ?? ExtractEmbeddedSource(work);
        if (source == null) source = PickFolder("Choose the Fruit Slicer 3DS source folder (it contains the Makefile)");
        if (source == null) return 2;
        if (!File.Exists(Path.Combine(source, "Makefile"))) throw new Exception("No Makefile in " + source);

        string tools = Path.Combine(work, "tools");
        Directory.CreateDirectory(tools);
        EnsureTool(tools, "makerom.exe", MakeromUrl);
        EnsureTool(tools, "bannertool.exe", BannertoolUrl);

        string log = Path.Combine(output, "build.log");
        string script = "export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM; " +
                        "export PATH=\"" + MsysPath(tools) + ":/opt/devkitpro/tools/bin:/opt/devkitpro/devkitARM/bin:$PATH\"; " +
                        "cd \"" + MsysPath(source) + "\" && make -j4 && make cia";
        int code = RunBash(bash, script, log);
        if (code != 0) throw new Exception("make failed with exit code " + code + ". See " + log);

        string[] products = { "FruitSlicer3DS.3dsx", "FruitSlicer3DS.cia" };
        foreach (string p in products)
        {
            string from = Path.Combine(source, p);
            if (!File.Exists(from)) throw new Exception("The build finished but did not produce " + p + ". See " + log);
            File.Copy(from, Path.Combine(output, p), true);
        }
        Say("Built FruitSlicer3DS.3dsx and FruitSlicer3DS.cia in:\n" + output);
        return 0;
    }

    static string Get(Dictionary<string, string> a, string k)
    {
        string v;
        return a.TryGetValue(k, out v) && v.Length > 0 ? Path.GetFullPath(v) : null;
    }

    // The release .exe carries the game's source as an embedded zip, so it works on its own.
    static string ExtractEmbeddedSource(string work)
    {
        using (Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream("source.zip"))
        {
            if (s == null) return null;
            string dir = Path.Combine(work, "source");
            if (Directory.Exists(dir)) Directory.Delete(dir, true);
            Directory.CreateDirectory(dir);
            using (var zip = new ZipArchive(s)) zip.ExtractToDirectory(dir);
            return dir;
        }
    }

    static void EnsureTool(string dir, string exe, string url)
    {
        string target = Path.Combine(dir, exe);
        if (File.Exists(target)) return;
        ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
        string zipPath = Path.Combine(dir, exe + ".zip");
        Console.WriteLine("Downloading " + url);
        using (var c = new WebClient()) c.DownloadFile(url, zipPath);
        using (ZipArchive zip = ZipFile.OpenRead(zipPath))
        {
            foreach (ZipArchiveEntry e in zip.Entries)
            {
                if (e.Name.Equals(exe, StringComparison.OrdinalIgnoreCase))
                {
                    e.ExtractToFile(target, true);
                    break;
                }
            }
        }
        File.Delete(zipPath);
        if (!File.Exists(target)) throw new Exception(exe + " was not in " + url);
    }

    // C:\a\b becomes /c/a/b, the form MSYS2 bash expects.
    static string MsysPath(string p)
    {
        string full = Path.GetFullPath(p).Replace('\\', '/');
        return "/" + char.ToLowerInvariant(full[0]) + full.Substring(2);
    }

    static int RunBash(string bash, string script, string logPath)
    {
        var psi = new ProcessStartInfo(bash, "-lc \"" + script.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"")
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        psi.EnvironmentVariables["MSYSTEM"] = "MSYS";
        psi.EnvironmentVariables["CHERE_INVOKING"] = "1";
        using (var log = new StreamWriter(logPath, false, Encoding.UTF8) { AutoFlush = true })
        using (Process p = Process.Start(psi))
        {
            object gate = new object();
            DataReceivedEventHandler sink = (s, e) =>
            {
                if (e.Data == null) return;
                lock (gate)
                {
                    log.WriteLine(e.Data);
                    Console.WriteLine(e.Data);
                }
            };
            p.OutputDataReceived += sink;
            p.ErrorDataReceived += sink;
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            p.WaitForExit();
            return p.ExitCode;
        }
    }

    static string PickFolder(string prompt)
    {
        if (!ui) return null;
        using (var d = new FolderBrowserDialog { Description = prompt, ShowNewFolderButton = true })
            return d.ShowDialog() == DialogResult.OK ? d.SelectedPath : null;
    }

    static void Say(string msg)
    {
        Console.WriteLine(msg);
        if (ui) MessageBox.Show(msg, Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
    }

    static void Fail(string msg)
    {
        Console.Error.WriteLine(msg);
        if (ui) MessageBox.Show(msg, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
    }
}
