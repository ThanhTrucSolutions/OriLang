using Ori;

// oric — the Ori toolchain CLI
//   oric build <in.ori> [-o <out.orx>]   compile source -> encrypted image
//   oric run   <file.ori|file.orx>       run source or a compiled image
//   oric dump  <file.ori>                show the disassembled bytecode
//   oric peek  <file.orx>                inspect an image header (proves it's opaque)

return Cli.Dispatch(args);

static class Cli
{
    public static int Dispatch(string[] args)
    {
        try
        {
            if (args.Length == 0) { Usage(); return 1; }
            switch (args[0])
            {
                case "build": return Build(args);
                case "run": return Run(args);
                case "dump": return Dump(args);
                case "peek": return Peek(args);
                case "version" or "-v" or "--version":
                    Console.WriteLine("Ori 0.2  (.ori source / .orx encrypted image)");
                    return 0;
                case "-h" or "--help" or "help": Usage(); return 0;
                default:
                    Console.Error.WriteLine($"unknown command '{args[0]}'");
                    Usage();
                    return 1;
            }
        }
        catch (OriCompileError e) { Console.Error.WriteLine(e.Message); return 2; }
        catch (OriRuntimeError e) { Console.Error.WriteLine(e.Message); return 3; }
        catch (OriImageError e) { Console.Error.WriteLine(e.Message); return 4; }
        catch (Exception e) { Console.Error.WriteLine("fatal: " + e.Message); return 5; }
    }

    static int Build(string[] args)
    {
        if (args.Length < 2) { Console.Error.WriteLine("usage: oric build <in.ori> [-o out.orx]"); return 1; }
        string input = args[1];
        string output = null;
        for (int i = 2; i < args.Length - 1; i++)
            if (args[i] == "-o") output = args[i + 1];
        output ??= Path.ChangeExtension(input, ".orx");

        string src = File.ReadAllText(input);
        var prog = Compiler.Compile(src);
        byte[] image = Container.Pack(prog);
        File.WriteAllBytes(output, image);
        Console.WriteLine($"compiled {input} -> {output} ({image.Length} bytes, encrypted)");
        return 0;
    }

    static int Run(string[] args)
    {
        if (args.Length < 2) { Console.Error.WriteLine("usage: oric run <file.ori|file.orx>"); return 1; }
        string path = args[1];
        var hosts = HostRegistry.CreateStandard(Console.WriteLine);

        if (path.EndsWith(".orx", StringComparison.OrdinalIgnoreCase))
        {
            VirtualMachine.RunImage(File.ReadAllBytes(path), hosts);
        }
        else
        {
            VirtualMachine.RunSource(File.ReadAllText(path), hosts);
        }
        return 0;
    }

    static int Dump(string[] args)
    {
        if (args.Length < 2) { Console.Error.WriteLine("usage: oric dump <file.ori>"); return 1; }
        var prog = Compiler.Compile(File.ReadAllText(args[1]));
        Console.Write(prog.Disassemble());
        return 0;
    }

    static int Peek(string[] args)
    {
        if (args.Length < 2) { Console.Error.WriteLine("usage: oric peek <file.orx>"); return 1; }
        byte[] b = File.ReadAllBytes(args[1]);
        Console.WriteLine($"file       : {args[1]}");
        Console.WriteLine($"size       : {b.Length} bytes");
        string magic = b.Length >= 4 ? System.Text.Encoding.ASCII.GetString(b, 0, 4) : "????";
        Console.WriteLine($"magic      : {magic}");
        Console.WriteLine($"version    : {(b.Length > 4 ? b[4] : 0)}");
        int show = Math.Min(48, b.Length);
        Console.WriteLine("first bytes: " + BitConverter.ToString(b, 0, show));
        Console.WriteLine();
        Console.WriteLine("Body is ChaCha20-encrypted + HMAC-protected: opaque without the VM key.");
        return 0;
    }

    static void Usage()
    {
        Console.WriteLine(@"Ori toolchain (oric)
  oric build <in.ori> [-o out.orx]   compile source -> encrypted .orx image
  oric run   <file.ori|file.orx>     run source directly or a compiled image
  oric dump  <file.ori>              disassemble compiled bytecode
  oric peek  <file.orx>              inspect an .orx header
  oric version                       print toolchain version");
    }
}
