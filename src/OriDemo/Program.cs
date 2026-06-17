using System.Text;
using Ori;

namespace Ori.Demo;

static class Program
{
    [STAThread]
    static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}

/// <summary>
/// A minimal Windows window with one button. Every click compiles (once) and
/// runs <c>program.ori</c> through the Ori VM by way of the encrypted
/// <c>program.orx</c> image — proving the language + VM + format end to end.
/// </summary>
public sealed class MainForm : Form
{
    private readonly Label _output;
    private readonly Button _runButton;
    private readonly TextBox _log;
    private readonly Label _status;

    private int _clicks;
    private byte[] _image;        // the encrypted .orx we execute
    private string _orxPath;

    public MainForm()
    {
        Text = "Ori VM — Demo";
        Width = 720;
        Height = 520;
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 10f);
        BackColor = Color.FromArgb(24, 26, 32);

        var title = new Label
        {
            Text = "The Ori language running on its own VM",
            ForeColor = Color.White,
            Font = new Font("Segoe UI Semibold", 15f, FontStyle.Bold),
            AutoSize = false,
            Dock = DockStyle.Top,
            Height = 48,
            TextAlign = ContentAlignment.MiddleCenter
        };

        _output = new Label
        {
            Text = "Click the button to run program.orx on the Ori VM",
            ForeColor = Color.FromArgb(120, 230, 180),
            Font = new Font("Consolas", 13f, FontStyle.Bold),
            AutoSize = false,
            Dock = DockStyle.Top,
            Height = 70,
            TextAlign = ContentAlignment.MiddleCenter
        };

        _runButton = new Button
        {
            Text = "▶  Run Ori VM",
            Dock = DockStyle.Top,
            Height = 56,
            ForeColor = Color.White,
            BackColor = Color.FromArgb(48, 120, 220),
            FlatStyle = FlatStyle.Flat,
            Font = new Font("Segoe UI Semibold", 12f, FontStyle.Bold),
            Cursor = Cursors.Hand
        };
        _runButton.FlatAppearance.BorderSize = 0;
        _runButton.Click += OnRunClicked;

        _log = new TextBox
        {
            Multiline = true,
            ReadOnly = true,
            Dock = DockStyle.Fill,
            ScrollBars = ScrollBars.Vertical,
            BackColor = Color.FromArgb(16, 18, 22),
            ForeColor = Color.FromArgb(200, 205, 215),
            Font = new Font("Consolas", 9.5f),
            BorderStyle = BorderStyle.None
        };

        _status = new Label
        {
            Dock = DockStyle.Bottom,
            Height = 26,
            ForeColor = Color.Gray,
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(8, 0, 0, 0)
        };

        var logHost = new Panel { Dock = DockStyle.Fill, Padding = new Padding(12, 6, 12, 6) };
        logHost.Controls.Add(_log);

        Controls.Add(logHost);
        Controls.Add(_status);
        Controls.Add(_runButton);
        Controls.Add(_output);
        Controls.Add(title);

        BuildImage();
    }

    /// <summary>Compile program.ori -> program.orx once at startup.</summary>
    private void BuildImage()
    {
        try
        {
            string dir = AppContext.BaseDirectory;
            string srcPath = Path.Combine(dir, "program.ori");
            _orxPath = Path.Combine(dir, "program.orx");

            string source = File.Exists(srcPath) ? File.ReadAllText(srcPath) : DefaultSource;

            var prog = Compiler.Compile(source);
            _image = Ori.Container.Pack(prog);
            File.WriteAllBytes(_orxPath, _image);

            Log($"Compiled program.ori -> program.orx ({_image.Length} bytes, ChaCha20 + HMAC encrypted).");
            Log($"Image header: {BitConverter.ToString(_image, 0, 8)} ...  (magic 'ORIX')");
            Log("Ready. Each click decrypts and runs this image on the VM.\r\n");
            _status.Text = _orxPath;
        }
        catch (Exception e)
        {
            Log("Failed to build image: " + e.Message);
        }
    }

    private void OnRunClicked(object sender, EventArgs e)
    {
        _clicks++;
        try
        {
            // Host functions exposed to the Ori program for this run.
            var hosts = HostRegistry.CreateStandard(s => Log("    " + s));
            hosts.Register("get_clicks", _ => Value.Number(_clicks));
            hosts.Register("ui_set_text", args =>
            {
                _output.Text = args.Length > 0 ? args[0].Display() : "";
                return Value.Nil;
            });

            Log($"--- Run #{_clicks}: VM decrypts program.orx, then executes ---");
            VirtualMachine.RunImage(_image, hosts);
        }
        catch (Exception ex)
        {
            _output.Text = "Runtime error";
            Log("Error: " + ex.Message);
        }
    }

    private void Log(string line)
    {
        _log.AppendText(line + "\r\n");
    }

    // Fallback if program.ori is missing next to the exe.
    private const string DefaultSource = @"
fold fib(n) { when n < 2 { give n } give fib(n-1) + fib(n-2) }
hold c = get_clicks()
ui_set_text(""Click #"" + str(c) + ""  fib="" + str(fib(c)))
say(""ran"")
";
}
