using Android.App;
using Android.OS;
using Android.Views;
using Android.Widget;
using Android.Graphics;
using Ori;

namespace Ori.Droid;

/// <summary>
/// An Android screen with one button. Each tap decrypts an embedded program.orx
/// and runs it on the Ori VM (the same OriLang core as the desktop tools) —
/// computing fib(tapCount) and updating the UI through the ui_set_text host fn.
/// </summary>
[Activity(Label = "Ori VM", MainLauncher = true, Theme = "@android:style/Theme.Material")]
public class MainActivity : Activity
{
    private int _clicks;
    private byte[] _image;
    private TextView _output;
    private TextView _log;

    protected override void OnCreate(Bundle savedInstanceState)
    {
        base.OnCreate(savedInstanceState);

        var root = new LinearLayout(this) { Orientation = Orientation.Vertical };
        root.SetPadding(32, 32, 32, 32);
        root.SetBackgroundColor(Color.Rgb(24, 26, 32));
        // Inset content below the status/navigation bars so the header is visible.
        root.SetFitsSystemWindows(true);

        var title = new TextView(this) { Text = "Ori VM on Android" };
        title.SetTextColor(Color.White);
        title.TextSize = 22f;
        title.SetPadding(0, 0, 0, 20);

        _output = new TextView(this) { Text = "Tap the button to run program.orx on the Ori VM" };
        _output.SetTextColor(Color.Rgb(120, 230, 180));
        _output.TextSize = 18f;
        _output.SetPadding(0, 0, 0, 20);

        var button = new Button(this) { Text = "▶  Run Ori VM" };
        button.SetBackgroundColor(Color.Rgb(48, 120, 220));
        button.SetTextColor(Color.White);
        button.Click += OnRunClicked;

        _log = new TextView(this) { Text = "" };
        _log.SetTextColor(Color.Rgb(200, 205, 215));
        _log.TextSize = 12f;
        _log.SetTypeface(Typeface.Monospace, TypefaceStyle.Normal);
        _log.SetPadding(0, 20, 0, 0);

        var scroll = new ScrollView(this);
        scroll.AddView(_log);

        AddRow(root, title);
        AddRow(root, _output);
        AddRow(root, button);
        // the log scroll fills the remaining space
        var scrollLp = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MatchParent, 0) { Weight = 1f };
        scroll.LayoutParameters = scrollLp;
        root.AddView(scroll);

        SetContentView(root);

        BuildImage();
    }

    private static void AddRow(LinearLayout parent, View v)
    {
        v.LayoutParameters = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MatchParent, ViewGroup.LayoutParams.WrapContent);
        parent.AddView(v);
    }

    /// <summary>Compile the embedded Ori source into an encrypted .orx once.</summary>
    private void BuildImage()
    {
        try
        {
            var prog = Compiler.Compile(Source);
            _image = Container.Pack(prog);
            Log($"Compiled program.ori -> program.orx ({_image.Length} bytes, ChaCha20 + HMAC encrypted).");
            Log($"Image header: {System.BitConverter.ToString(_image, 0, 8)} ...  (magic 'ORIX')");
            Log("Ready. Each tap decrypts and runs this image on the VM.\n");
        }
        catch (System.Exception e)
        {
            Log("Failed to build image: " + e.Message);
        }
    }

    private void OnRunClicked(object sender, System.EventArgs e)
    {
        _clicks++;
        try
        {
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
        catch (System.Exception ex)
        {
            _output.Text = "Runtime error";
            Log("Error: " + ex.Message);
        }
    }

    private void Log(string line) => _log.Text += line + "\n";

    // The Ori program executed on every tap.
    private const string Source = @"
fold fib(n) {
    when n < 2 { give n }
    give fib(n - 1) + fib(n - 2)
}
fold greet(n) {
    when n == 1 { give ""First tap!"" }
    give ""You tapped "" + str(n) + "" times""
}
hold clicks = get_clicks()
hold msg = greet(clicks) + ""   |   fib("" + str(clicks) + "") = "" + str(fib(clicks))
ui_set_text(msg)
say(""[Ori VM] "" + msg)
";
}
